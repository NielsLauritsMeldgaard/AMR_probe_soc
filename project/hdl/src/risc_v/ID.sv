`timescale 1ns / 1ps

module ID(
    // --- Global Control Signals ---
    input  logic        clk,
    input  logic        rst,
    input  logic        stall,
    input  logic        branch_taken,  // flush

    // --- Inputs from IF Stage ---
    input  logic [31:0] instr_id_i,
    input  logic [31:0] pc_id_i,

    // --- Feedback from EX Stage (WB) ---
    input  logic [31:0] rd_data_wb,
    input  logic [4:0]  rd_addr_wb,
    input  logic        regWrite_wb,
    input  logic [31:0] ex_res,

    // --- Forwarding Control ---
    input  logic        fwd_mem_wdata,
    output logic        aluSrc2_id_o,
    output logic        branch_id_o,
    output logic [4:0]  rs1_id_o,
    output logic [4:0]  rs2_id_o, 

    // --- Data Wishbone Interface ---
    output logic [31:0] dwb_adr_o,
    output logic [31:0] dwb_dat_o,
    output logic [3:0]  dwb_sel_o,
    output logic        dwb_we_o,
    output logic        dwb_stb_o,
    output logic        dwb_cyc_o,
    input  logic        dwb_ack_i,

    // --- Pipeline Outputs to EX Stage ---
    output logic [31:0] rs1_data_o,
    output logic [31:0] rs2_immData,
    output logic [31:0] imm_id_o,
    output logic [31:0] pc_id_o,
    output logic [4:0]  aluCtrl_id_o,
    output logic        memToReg_id_o,
    output logic        regWrite_id_o,
    output logic [4:0]  rd_addr_id_o,
    output logic [2:0]  funct3_id_o,
    output logic [1:0]  addr_offset_id_o,
    output logic        is_jal_or_jalr_id_o
    );

    // --- Internal Typedefs and Logic ---
    typedef enum logic [2:0] { IMM_I, IMM_S, IMM_B, IMM_U, IMM_J } imm_sel_t;

    logic [6:0]  opcode;
    logic [4:0]  rs1, rs2, rd;
    logic aluSrc1, aluSrc2, memToReg, regWrite, memWrite, branch, is_jal_or_jalr;
    logic [4:0] aluCtrl;
    logic [2:0] imm_sel;
    logic [31:0] rs2_data, rs1_data;
    logic [31:0] IR_next;
    logic [31:0] Dmem_data;
    logic [31:0] jal_target_id; 
    // logic [31:0] jalr_target_id; // Removed from ID, moved to EX

    // --- Pipeline registers ---
    logic [31:0] IR, pc_id;

    always_ff @(posedge clk) begin
        if (rst) begin
            IR    <= 32'h00000013;  // NOP
            pc_id <= 0;
        end else if (!stall) begin
            IR    <= IR_next;
            pc_id <= pc_id_i;
        end
    end

    // --- Register File ---
    regFile regFile (
     .rst(rst),
     .clk(clk),
     .we(regWrite_wb),
     .rs1_addr(IR[19:15]), .rs2_addr(IR[24:20]),
     .rd_addr(rd_addr_wb), .rd_data(rd_data_wb),
     .rs1_data(rs1_data), .rs2_data(rs2_data)
    );

    // --- Main Decoder and Data Logic ---
    always_comb begin
        // 1. Flush logic
        IR_next = (branch_taken) ? 32'h00000013 : instr_id_i;

        // 2. Partition IR
        rs1    = IR[19:15];
        rs2    = IR[24:20];
        rd     = IR[11:7];
        opcode = IR[6:0];

        rs1_id_o = rs1;
        rs2_id_o = rs2;

        // 3. Defaults
        aluSrc1 = 0; aluSrc2 = 0; memToReg = 0; regWrite = 0;
        memWrite = 0; branch = 0; is_jal_or_jalr = 0;
        aluCtrl = 5'b0; imm_sel = IMM_I;

        // 4. Instruction decoding
        case (opcode)
            7'b0110011: begin regWrite = 1; aluCtrl = {1'b0,IR[14:12],IR[30]}; end
            7'b0010011: begin aluSrc2 = 1; regWrite = 1;
                aluCtrl = {1'b0, IR[14:12], (IR[14:12]==3'b101)? IR[30]:1'b0}; imm_sel=IMM_I; end
            7'b0000011: begin aluSrc2 = 1; memToReg = 1; regWrite = 1; end
            7'b0100011: begin aluSrc2 = 1; memWrite = 1; imm_sel = IMM_S; end
            7'b1100011: begin branch = 1; aluCtrl = {1'b1,IR[14:12],1'b0}; imm_sel=IMM_B; end
            7'b0110111: begin regWrite=1; aluSrc2=1; imm_sel=IMM_U; aluCtrl=5'b11111; end
            7'b0010111: begin regWrite=1; aluSrc2=1; imm_sel=IMM_U; aluCtrl=5'b00000; aluSrc1=1; end
            7'b1101111: begin regWrite=1; imm_sel=IMM_J; is_jal_or_jalr=1; aluCtrl = 5'b00000; aluSrc1 = 1; aluSrc2 = 1; end
            7'b1100111: begin regWrite=1; imm_sel=IMM_I; is_jal_or_jalr=1; aluCtrl = 5'b00000; aluSrc2 = 1; end  
            default: ;
        endcase

        // 5. Immediate generation
        case (imm_sel)
            IMM_I: imm_id_o = {{20{IR[31]}}, IR[31:20]};
            IMM_S: imm_id_o = {{20{IR[31]}}, IR[31:25], IR[11:7]};
            IMM_B: imm_id_o = {{19{IR[31]}}, IR[31], IR[7], IR[30:25], IR[11:8], 1'b0};
            IMM_U: imm_id_o = {IR[31:12],12'b0};
            IMM_J: imm_id_o = {{12{IR[31]}}, IR[19:12], IR[20], IR[30:21], 1'b0};
            default: imm_id_o = 32'b0;
        endcase

        // --- JAL / JALR target calculation ---
        dwb_adr_o = rs1_data + imm_id_o;                // reused adder for Load/Store address
        
        //jal_target_id   = pc_id + imm_id_o;             // PC-relative for JAL
        
        // --- MODIFIED: Target Selection ---
        // If JAL, we redirect immediately in ID.
        // If JALR, we DO NOT redirect in ID (regs not ready). We wait for EX.
        
        pc_id_o = pc_id;

        // 7. Output signals
        aluCtrl_id_o    = aluCtrl;
        memToReg_id_o   = memToReg;
        regWrite_id_o   = regWrite;
        rd_addr_id_o    = rd;
        funct3_id_o     = IR[14:12];
        addr_offset_id_o= dwb_adr_o[1:0];
        rs2_immData     = aluSrc2 ? imm_id_o : rs2_data;
        rs1_data_o      = aluSrc1 ? pc_id : rs1_data;
        aluSrc2_id_o    = aluSrc2;
        branch_id_o     = branch;
        
        // --- MODIFIED: Jump Flags ---
        is_jal_or_jalr_id_o    = is_jal_or_jalr;  // Pass JALR decision to EX
          
       
        // --- Wishbone store data
        case (IR[14:12])
            3'b000: Dmem_data = {4{rs2_data[7:0]}};
            3'b001: Dmem_data = {2{rs2_data[15:0]}};
            default: Dmem_data = rs2_data;
        endcase
        dwb_dat_o = fwd_mem_wdata ? ex_res : Dmem_data;

        // Wishbone control
        dwb_we_o  = memWrite & !branch_taken; 
        dwb_stb_o = (memToReg | memWrite) & !branch_taken;
        dwb_cyc_o = dwb_stb_o;
        case (IR[14:12])
            3'b000: dwb_sel_o = 4'b0001 << dwb_adr_o[1:0];
            3'b001: dwb_sel_o = dwb_adr_o[1] ? 4'b1100 : 4'b0011;
            default: dwb_sel_o = 4'b1111;
        endcase
    end
endmodule
