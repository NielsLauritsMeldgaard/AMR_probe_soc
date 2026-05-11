`timescale 1ns / 1ps

module regFile (
    input  logic         clk,
    input  logic         rst,
    input  logic         we,         
    input  logic [4:0]   rs1_addr,   
    input  logic [4:0]   rs2_addr,   
    input  logic [4:0]   rd_addr,    
    input  logic [31:0]  rd_data,    
    output logic [31:0]  rs1_data,   
    output logic [31:0]  rs2_data    
);

    // 32 x 32-bit registers
    logic [31:0] regs [31:0];
    
   initial begin
    for (int i = 0; i < 32; i++) regs[i] = 32'h0;
end
    
    logic [31:0] we_onehot;
    assign we_onehot = (we && rd_addr != 5'd0) ? (32'b1 << rd_addr) : 32'b0;

    
    genvar i;
    generate
        for (i = 0; i < 32; i = i + 1) begin : reg_write
            always_ff @(posedge clk) begin
                if (rst)
                    regs[i] <= 32'b0;
                else if (we_onehot[i])
                    regs[i] <= rd_data;
            end
        end
    endgenerate

    
    assign rs1_data = (rs1_addr == 5'd0) ? 32'b0 :
                      (we && rs1_addr == rd_addr) ? rd_data : regs[rs1_addr];

    assign rs2_data = (rs2_addr == 5'd0) ? 32'b0 :
                      (we && rs2_addr == rd_addr) ? rd_data : regs[rs2_addr];

endmodule

