`timescale 1ns / 1ps

module EX (
    // --- Global Control Signals ---
    input  logic        clk,               // System clock
    input  logic        rst,               // Global synchronous reset
    input  logic        stall,             // From datapath: Freezes all internal registers

    // --- Data Inputs from ID Stage (Pipelined "Next" values) ---
    input  logic [31:0] rs1_val_reg_next,  // Operand 1 from RegFile
    input  logic [31:0] rs2_imm_reg_next,  // Operand 2 (Muxed rs2 or Imm)
    input  logic [31:0] pc_ex_i,           // PC of the current instruction
    input  logic [31:0] imm_ex_i,          // Immediate for branch math
    
    // --- Control Inputs from ID Stage ---
    input  logic [4:0]  aluOP_ex_i,        // ALU opcode (bit 4 is branch)
    input  logic [4:0]  rd_addr_ex_i,      // Destination register address
    input  logic        memToReg_ex_i,     // Control: Select Mem or ALU result
    input  logic        regWrite_ex_i,     // Control: Write-enable for WB
    input  logic [1:0]  aluFwdSrc,         // Forwarding mux selector
    input  logic [1:0]  addr_offset_ex_i, // From ID: addr[1:0] for Load slicing
    input  logic [2:0]  funct3_ex_i,      // From ID: funct3 for Load slicing
    input  logic        is_jal_or_jalr_ex_i,     // Jal or Jalr signal from ID

    // --- Data Bus Input ---
    input  logic [31:0] dwb_dat_i,         // 32-bit word arriving from Data Memory/Bus

    // --- Outputs back to Datapath/Feedback ---
    output logic [31:0] res_ex_o,            // Final result (ALU or Sliced Memory Data)
    output logic [31:0] pc_ex_o,            // Pipelined PC sent back to IF
    output logic [31:0] imm_ex_o,           // Pipelined Imm sent back to IF
    output logic [4:0]  rd_addr_ex_o,       // Destination register for WB
    output logic        regWrite_ex_o,      // Write enable for WB
    output logic        br_dec_ex_o,         // Result of (branch_signal & alu_comparison) OR JAL
    
    // --- NEW: JALR Outputs ---
    output logic [31:0] jal_or_jalr_target_ex_o,   // Calculated Target for IF
    output logic        jal_or_jalr_ex_o    // Signal to take the jump
);

    // --- Internal Pipeline Registers (ID/EX) ---
    logic [31:0] rs1_reg, rs2_reg, pc_reg, imm_reg, ex_res_reg;
    logic [4:0]  aluOP_reg, rd_reg;
    logic [2:0]  funct3_reg;
    logic [1:0]  addr_offset_reg;
    logic        mToReg_reg, rWrite_reg, br_dec_ex;
    logic [1:0]  aluFwdSrc_reg; 
    logic        is_jal_or_jalr_reg; // --- NEW: Register for JALR flag ---

    // --- Internal Wires ---
    logic [31:0] op1, op2, aluRes, load_data;

    // --- 1. ALU INSTANTIATION ---
    ALU ALU_unit (
        .op1   (op1),
        .op2   (op2),
        .aluOP (aluOP_reg),
        .res   (aluRes)
    );

    // --- 2. MAIN COMBINATIONAL LOGIC ---
    always_comb begin
        // A. Forwarding Mux (Selecting ALU Operands)
        case (aluFwdSrc_reg)
            2'b00:   begin op1 = rs1_reg;  op2 = rs2_reg; end 
            2'b01:   begin op1 = rs1_reg;  op2 = ex_res_reg;  end // Forward wire to Op2
            2'b10:   begin op1 = ex_res_reg;   op2 = rs2_reg; end // Forward wire to Op1
            2'b11:   begin op1 = ex_res_reg;   op2 = ex_res_reg;  end // Forward wire to both
            default: begin op1 = rs1_reg;  op2 = rs2_reg; end
        endcase

        // B. Load Slicing and Sign-Extension
        // Processes the 32-bit word from memory based on funct3 and address offset
        case (funct3_reg)
            3'b000: begin // LB (Byte Signed)
                case(addr_offset_reg)
                    2'b00: load_data = {{24{dwb_dat_i[7]}},  dwb_dat_i[7:0]};
                    2'b01: load_data = {{24{dwb_dat_i[15]}}, dwb_dat_i[15:8]};
                    2'b10: load_data = {{24{dwb_dat_i[23]}}, dwb_dat_i[23:16]};
                    2'b11: load_data = {{24{dwb_dat_i[31]}}, dwb_dat_i[31:24]};
                endcase
            end
            3'b001: begin // LH (Halfword Signed)
                load_data = addr_offset_reg[1] ? {{16{dwb_dat_i[31]}}, dwb_dat_i[31:16]} 
                                               : {{16{dwb_dat_i[15]}}, dwb_dat_i[15:0]};
            end
            3'b100: begin // LBU (Byte Unsigned)
                case(addr_offset_reg)
                    2'b00: load_data = {24'b0, dwb_dat_i[7:0]};
                    2'b01: load_data = {24'b0, dwb_dat_i[15:8]};
                    2'b10: load_data = {24'b0, dwb_dat_i[23:16]};
                    2'b11: load_data = {24'b0, dwb_dat_i[31:24]};
                endcase
            end
            3'b101: begin // LHU (Halfword Unsigned)
                load_data = addr_offset_reg[1] ? {16'b0, dwb_dat_i[31:16]} 
                                               : {16'b0, dwb_dat_i[15:0]};
            end
            default: load_data = dwb_dat_i; // LW (Word)
        endcase

        // C. Final Output Mux (ALU or Memory)
        res_ex_o = is_jal_or_jalr_reg ? pc_reg + 4 : mToReg_reg ? load_data : aluRes;

        // D. Branch and Pass-through Assignments
        rd_addr_ex_o     = rd_reg; 
        regWrite_ex_o    = rWrite_reg;
        br_dec_ex        = aluOP_reg[4] & aluRes[0];  // Normal branch taken logic
        
        // --- MODIFIED: JALR Calculation ---
        // For JALR, target = (rs1 + imm) & ~1. 
        // ALUOp was set to ADD, so aluRes contains (rs1 + imm).
        jal_or_jalr_target_ex_o = aluRes & 32'hFFFFFFFE; 
        jal_or_jalr_ex_o  = is_jal_or_jalr_reg;
        
        // --- MODIFIED: Branch Decision ---
        // Includes Conditional Branch (br_dec_ex), JAL (jump from ID), or JALR (jump from EX)
        // This triggers the flush of previous stages.
        br_dec_ex_o      = (br_dec_ex || jal_or_jalr_ex_o);
        
        pc_ex_o          = pc_reg; 
        imm_ex_o         = imm_reg;
    end
    
    // --- 3. SEQUENTIAL LOGIC (Pipeline Registers) ---
    always_ff @(posedge clk) begin
        if (rst || br_dec_ex_o) begin
            // Reset all pipeline registers to zero
            {rs1_reg, rs2_reg, pc_reg, imm_reg} <= '0;
            {aluOP_reg, rd_reg, funct3_reg, addr_offset_reg} <= '0;
            {mToReg_reg, rWrite_reg} <= '0;
            ex_res_reg <= '0;
            aluFwdSrc_reg <= 2'b00;
            is_jal_or_jalr_reg   <= 1'b0; // Reset JALR flag
        end else if (!stall) begin
            // Capture new values from ID stage ONLY if not stalling
            rs1_reg         <= rs1_val_reg_next;
            rs2_reg         <= rs2_imm_reg_next;
            pc_reg          <= pc_ex_i;
            imm_reg         <= imm_ex_i;
            aluOP_reg       <= aluOP_ex_i;
            rd_reg          <= rd_addr_ex_i;
            funct3_reg      <= funct3_ex_i;
            addr_offset_reg <= addr_offset_ex_i;
            mToReg_reg      <= memToReg_ex_i;
            rWrite_reg      <= regWrite_ex_i;
            ex_res_reg      <= res_ex_o;
            aluFwdSrc_reg   <= aluFwdSrc;
            is_jal_or_jalr_reg     <= is_jal_or_jalr_ex_i; // Capture JALR flag
        end
    end

endmodule
