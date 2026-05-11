`timescale 1ns / 1ps

module data_memory #(
    parameter MEM_WORDS = 16384 // 64KB
)(
    input  logic         clk,
    input  logic         rst,        
    input  logic [31:0]  addr,       // Full 32-bit Byte Address
    input  logic [31:0]  Wdata,      
    input  logic [3:0]   sel,        
    input  logic         En,         
    input  logic         We,         
    output logic [31:0]  Rdata,      
    output logic         Ack         
);
    // Calculate how many bits are needed to index the words
    localparam ADDR_LSB = 2; 
    localparam ADDR_MSB = $clog2(MEM_WORDS) + ADDR_LSB - 1;

    // The Memory Array
    (* ram_style = "block" *) 
    logic [31:0] mem [0:MEM_WORDS-1];

    initial begin
        for (int i = 0; i < MEM_WORDS; i++) mem[i] = 32'h0;
    end

    // --- Synchronous Logic Block ---
    always_ff @(posedge clk) begin
        if (rst) begin
            Rdata <= 32'h0;
            Ack   <= 1'b0;
        end else begin
            // 1. Wishbone Handshake
            // Allow multiple cycles if needed (if sw and lw for same address in sequence)
            Ack <= (En | We);

            // 2. Read Logic
            if (En) begin
                // Use only the relevant bits for the 64KB range
                Rdata <= mem[addr[ADDR_MSB:ADDR_LSB]];
            end

            // 3. Write Logic (Byte-Selective)
            if (We) begin
                if (sel[0]) mem[addr[ADDR_MSB:ADDR_LSB]][7:0]   <= Wdata[7:0];
                if (sel[1]) mem[addr[ADDR_MSB:ADDR_LSB]][15:8]  <= Wdata[15:8];
                if (sel[2]) mem[addr[ADDR_MSB:ADDR_LSB]][23:16] <= Wdata[23:16];
                if (sel[3]) mem[addr[ADDR_MSB:ADDR_LSB]][31:24] <= Wdata[31:24];
            end
        end
   end
endmodule
