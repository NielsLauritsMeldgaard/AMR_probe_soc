`timescale 1ns / 1ps

module IF_stage(
    input  logic        clk,
    input  logic        rst,
    input  logic        stall,         // stall: stops registers when waiting for memory
    
    // --- Branch/Jump Logic (Feedback from EX) ---
    input  logic        pc_sel,        // PC_select: high if branch taken; add PC from EX + imm from EX
    input  logic [31:0] pc_from_ex,    // PC from execute stage: pipelined back to match branch calc.,
    input  logic [31:0] imm_from_ex,   // immidiate value from EX stage: pipelined back
    input  logic [31:0] pc_from_id,
    input  logic        pc_jump_sel,   // JAL taken from ID
    
    // --- NEW: JALR Logic (Feedback from EX) ---
    input  logic        jal_or_jalr_ex_sel,    // NEW: High if JALR is processed in EX
    input  logic [31:0] jal_or_jalr_target_ex, // NEW: Calculated Target (rs1 + imm) from EX
    
    
    // --- Instruction Wishbone Master --- Harvard style with seperate instructions and data BUS 
    output logic [31:0] iwb_adr_o,     // Address to Bus
    output logic        iwb_stb_o,     // Strobe/Request to Bus
    input  logic [31:0] iwb_dat_i,     // Instruction from Bus
    input  logic        iwb_ack_i,     // Acknowledge from Bus
    
    // --- Pipeline Outputs to ID Stage ---
    output logic [31:0] pc_if_o,       // Current PC in instruction fetch stage output
    output logic [31:0] instr_if_o     // instruction bits from IF stage output
    );
    
    logic [31:0] pc_curr, pc_next;
    logic [31:0] adder_op_a, adder_op_b, adder_out;  // PC adder I/O
    
    // --- 1. OPERATOR SHARING ADDER ---
    // Calculates either PC+4 (Normal) or Branch-Target (pc_ex + imm_ex)
       always_comb begin
            // 1. Select Base Address (Operand A)
            adder_op_a = jal_or_jalr_ex_sel ? jal_or_jalr_target_ex : // Highest Priority: JALR from EX (Absolute)
                         pc_sel             ? pc_from_ex     : // Branch from EX (Base for offset)                        
                                              pc_curr;         // Default: Current PC
    
            // 2. Select Offset (Operand B)
            // If JALR or JAL, we add 0 because the target is already calculated.
            // If Branch, we add the Immediate. Otherwise, we add 4.
            adder_op_b = (jal_or_jalr_ex_sel) ? 32'd0 :
                         pc_sel               ? imm_from_ex : 
                                                32'd4;
    
            // 3. The Single Adder
            adder_out = adder_op_a + adder_op_b;
    
            // 4. Final PC Next Assignment
            pc_next = rst ? 32'h0 : adder_out;
        end
    
    // --- 2. THE PC REGISTERS ---
    // pc_curr: The address we are FETCHING now
    // pc_delayed: The address of the instruction arriving from memory NOW
    always_ff @(posedge clk) begin
        if (rst) begin
            pc_curr    <= 32'h0;
        end else if (!stall) begin
            pc_curr    <= pc_next;
        end
    end
    
    // --- 3. WISHBONE BUS ASSIGNMENTS ---
    assign iwb_adr_o  = pc_next;
    assign iwb_stb_o  = 1'b1;         // Always requesting instructions
    
    // --- 4. OUTPUTS TO ID STAGE ---
    // Use pc_delayed so that 'instr_if_o' matches its correct address
    assign pc_if_o    = pc_curr;   // PC associated with the instruction arriving on iwb_dat_i
    assign instr_if_o = iwb_dat_i;    // Bits arriving from memory
endmodule
