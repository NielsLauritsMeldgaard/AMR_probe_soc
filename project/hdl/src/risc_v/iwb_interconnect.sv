`timescale 1ns / 1ps

// Interconnect between wishbone slaves, bootloader bram and instruction memory 
module iwb_interconnect(
    // Instruction wishbone master bus
    input logic [31:0]      m_iwb_adr_i,    // Address to Bus (from master IF stage)
    input logic             m_iwb_stb_i,    // Strobe/Request to Bus (decoded based on adr[31:28] from master IF stage)
    output  logic [31:0]    m_iwb_dat_o,    // Instruction from Bus (to master IF stage)
    output  logic           m_iwb_ack_o,    // Acknowledge from Bus (to master IF stage)
    
    // --- slave 0: bootloader bram
    output logic [31:0]     s0bb_adr_o,     // Address to s0 (too bootloader rom)
    output logic            s0bb_stb_o,     // strobe to s0 (too bootloader rom)
    input logic [31:0]      s0bb_dat_i,     // Data from s0 to bootloader rom (instruction)
    input logic             s0bb_ack_i,     // Acknowledge from bootloader
    
    // --- slave 1: instruction memory
    output logic [31:0]     s1im_adr_o,     // Address to s1 (too instruction memory)
    output logic            s1im_stb_o,     // strobe to s1
    input logic [31:0]      s1im_dat_i,     // Data from s1 to CPU (instruction)
    input logic             s1im_ack_i      // Acknowledge from instruction memory 
    );
    
    always_comb begin 
        // Default: All slaves disabled
        s0bb_stb_o = 1'b0;
        s1im_stb_o = 1'b0;
        m_iwb_dat_o  = 32'h0;
        m_iwb_ack_o  = 1'b0;

        s0bb_adr_o = m_iwb_adr_i;
        s1im_adr_o = m_iwb_adr_i;
        
        // Decode based on address bits [31:28]
        unique case (m_iwb_adr_i[31:28])
            4'h0: begin
                // Slave 0: bootloader bram
                s0bb_stb_o     = m_iwb_stb_i;
                m_iwb_dat_o    = s0bb_dat_i;
                m_iwb_ack_o    = s0bb_ack_i;
            end
            
            4'h1: begin
                // Slave 1: instruction memory
                s1im_stb_o     = m_iwb_stb_i;
                m_iwb_dat_o    = s1im_dat_i;
                m_iwb_ack_o    = s1im_ack_i;
            end
            
            default: begin
                // No slave selected
                m_iwb_dat_o    = 32'h00000013; // NOP instruction
                m_iwb_ack_o    = 1'b1;         // immediate ack
            end
        endcase
    end
    
endmodule

