`timescale 1ns / 1ps

// True dual port instruction memory
// Read transactions is controlled with the WB-I interface (slave 1)
// Write transactions is controlled with the WB-D interface (slave 2)
module instruction_memory #(
    parameter MEM_WORDS = 32768 // 128KB             
)(
    input  logic            clk,
    input  logic            rst,              
    
    // PORT A: Write Port (from data memory WB-D interface)
    // --- Instruction Write Wishbone Slave Interface (write only) ---
    input logic [31:0]      a_dwb_adr_i,         // Write address
    input logic [31:0]      a_dwb_dat_i,         // Write data
    input logic [3:0]       a_dwb_sel_i,         // Byte select
    input logic             a_dwb_we_i,          // Write enable
    input logic             a_dwb_stb_i,         // Strobe (request)
    output logic [31:0]     a_dwb_dat_o,         // Read data (not used)
    output logic            a_dwb_ack_o,         // Acknowledge (ready)

    // PORT B: Read Port (from instruction fetch WB-I interface)
    // --- Instruction Fetch Wishbone Slave Interface (read only) ---
    input  logic [31:0]     b_iwb_adr_i,         // Byte Address (PC)
    input  logic            b_iwb_stb_i,         // Strobe (Request)
    output logic [31:0]     b_iwb_dat_o,         // Instruction Data
    output logic            b_iwb_ack_o          // Acknowledge (Ready)
);

    // Calculate bits needed for indexing
    // 32768 words -> 15 bits. Byte address bits [16:2]
    localparam ADDR_BITS = $clog2(MEM_WORDS);
    
    // Calculate how many bits are needed to index the words (used for write port)
    localparam ADDR_LSB = 2; 
    localparam ADDR_MSB = $clog2(MEM_WORDS) + ADDR_LSB - 1;

    // Force Block RAM inference
    (* rom_style = "block" *) 
    logic [31:0] mem [0:MEM_WORDS-1];

    assign a_dwb_dat_o = 32'h0; // Not used for write port

    initial begin
        for (int i = 0; i < MEM_WORDS; i++) mem[i] = 32'h0;
    end

    logic We_dwb, En_iw;
    assign We_dwb = a_dwb_stb_i && a_dwb_we_i;
    assign En_iwb = b_iwb_stb_i;

    // Port A:
    always_ff @(posedge clk) begin
        if (rst) begin
            a_dwb_ack_o <= 1'b0;
        end else begin
            // Wishbone Handshake
            a_dwb_ack_o <= We_dwb; // multi-cycle write ack

            // Write Logic (Byte-Selective)
            if (We_dwb) begin
                if (a_dwb_sel_i[0]) mem[a_dwb_adr_i[ADDR_BITS+1 : 2]][7:0]   <= a_dwb_dat_i[7:0];
                if (a_dwb_sel_i[1]) mem[a_dwb_adr_i[ADDR_BITS+1 : 2]][15:8]  <= a_dwb_dat_i[15:8];
                if (a_dwb_sel_i[2]) mem[a_dwb_adr_i[ADDR_BITS+1 : 2]][23:16] <= a_dwb_dat_i[23:16];
                if (a_dwb_sel_i[3]) mem[a_dwb_adr_i[ADDR_BITS+1 : 2]][31:24] <= a_dwb_dat_i[31:24];
            end   
        end
    end

    // Port B:
    always_ff @(posedge clk) begin
        if (rst) begin
            b_iwb_ack_o <= 1'b0;
            b_iwb_dat_o <= 32'h00000013; 
        end else begin
            // Wishbone Handshake
            b_iwb_ack_o <= En_iwb;

            // Read Logic
            if (En_iwb) begin               
                b_iwb_dat_o <= mem[b_iwb_adr_i[ADDR_BITS+1 : 2]];
            end
        end
    end    

endmodule
