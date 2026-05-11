module ALU(
    input  logic [31:0] op1,
    input  logic [31:0] op2,
    input  logic [4:0]  aluOP, 
    output logic [31:0] res
);

    // ------------------------
    // Shared signals
    // ------------------------
    logic [31:0] op2_xor_sub;
    logic [31:0] sum;
    logic        is_sub;

    logic [31:0] and_logic, or_logic, xor_logic;
    logic [31:0] shl, srl, sra;
    logic [31:0] cmp_signed, cmp_unsigned;
    
    // ------------------------
    // ADD / SUB
    // ------------------------
    assign is_sub     = (aluOP == 5'b00001);
    assign op2_xor_sub = op2 ^ {32{is_sub}};         // XOR B for subtraction
    assign sum        = op1 + op2_xor_sub + is_sub; // Single adder

    // ------------------------
    // Bitwise operations
    // ------------------------
    assign and_logic = op1 & op2;
    assign or_logic  = op1 | op2;
    assign xor_logic = op1 ^ op2;

    // ------------------------
    // Shifts
    // ------------------------
    assign shl = op1 << op2[4:0];
    assign srl = op1 >> op2[4:0];
    assign sra = $signed(op1) >>> op2[4:0];

    // ------------------------
    // Comparisons
    // ------------------------
    assign cmp_signed   = ($signed(op1) < $signed(op2)) ? 32'd1 : 32'd0;
    assign cmp_unsigned = (op1 < op2) ? 32'd1 : 32'd0;

    // ------------------------
    // Result mux
    // ------------------------
    always_comb begin
        casez(aluOP)
            5'b00000: res = sum;       // ADD
            5'b00001: res = sum;       // SUB
            5'b01000: res = xor_logic; // XOR
            5'b01100: res = or_logic;  // OR
            5'b01110: res = and_logic; // AND
            5'b00010: res = shl;       // SLL
            5'b01010: res = srl;       // SRL
            5'b01011: res = sra;       // SRA
            5'b00100: res = cmp_signed;   // SLT
            5'b00110: res = cmp_unsigned; // SLTU
            5'b10000: res = (op1 == op2) ? 32'd1 : 32'd0;  // BEQ
            5'b10010: res = (op1 != op2) ? 32'd1 : 32'd0;  // BNE
            5'b11000: res = cmp_signed;   // BLT
            5'b11010: res = ($signed(op1) >= $signed(op2)) ? 32'd1 : 32'd0; // BGE
            5'b11100: res = cmp_unsigned; // BLTU
            5'b11110: res = (op1 >= op2) ? 32'd1 : 32'd0; // BGEU
            5'b11111: res = op2;          // Pass immediate
            default:  res = 32'b0;
        endcase
    end

endmodule
