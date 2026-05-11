module dwb_interconnect(
    // --- master interface (from cpu EX stage) --- //
    input  logic [31:0] m_adr_i,
    input  logic [31:0] m_dat_i,
    input  logic [3:0]  m_sel_i,
    input  logic        m_we_i,
    input  logic        m_stb_i,
    output logic [31:0] m_dat_o,
    output logic        m_ack_o,
    
    // --- slave 0: Data ram --- //
    output logic [31:0] s0_adr_o,
    output logic [31:0] s0_dat_o,
    output logic [3:0]  s0_sel_o,
    output logic        s0_we_o,
    output logic        s0_stb_o,
    input  logic [31:0] s0_dat_i,
    input  logic        s0_ack_i,
    
    // --- slave 1: I/O peripherals --- //
    output logic [31:0] s1_adr_o,
    output logic [31:0] s1_dat_o,
    output logic [3:0]  s1_sel_o,
    output logic        s1_we_o,
    output logic        s1_stb_o,
    input  logic [31:0] s1_dat_i,
    input  logic        s1_ack_i,
    
    // --- slave 2: Instruction ram --- //
    output logic [31:0] s2_adr_o,
    output logic [31:0] s2_dat_o,
    output logic [3:0]  s2_sel_o,
    output logic        s2_we_o,
    output logic        s2_stb_o,
    input  logic [31:0] s2_dat_i,
    input  logic        s2_ack_i
);

    // ------------------------------
    // One-hot slave selection
    // ------------------------------
    logic sel_s0, sel_s1, sel_s2;

    assign sel_s2 = (m_adr_i[31:28] == 4'h1); // instruction RAM
    assign sel_s0 = (m_adr_i[31:28] == 4'h2); // data RAM
    assign sel_s1 = (m_adr_i[31:28] == 4'h4); // I/O peripherals

    // ------------------------------
    // Broadcast master signals
    // ------------------------------
    assign s0_adr_o = m_adr_i;
    assign s0_dat_o = m_dat_i;
    assign s0_sel_o = m_sel_i;
    assign s0_we_o  = m_we_i;

    assign s1_adr_o = m_adr_i;
    assign s1_dat_o = m_dat_i;
    assign s1_sel_o = m_sel_i;
    assign s1_we_o  = m_we_i;

    assign s2_adr_o = m_adr_i;
    assign s2_dat_o = m_dat_i;
    assign s2_sel_o = m_sel_i;
    assign s2_we_o  = m_we_i;

    // ------------------------------
    // Strobe gating (demux)
    // ------------------------------
    assign s0_stb_o = m_stb_i & sel_s0;
    assign s1_stb_o = m_stb_i & sel_s1;
    assign s2_stb_o = m_stb_i & sel_s2;

    // ------------------------------
    // Read data and ACK muxing
    // ------------------------------
    always_comb begin
        if      (s0_ack_i) begin m_dat_o = s0_dat_i; m_ack_o = 1'b1; end
        else if (s1_ack_i) begin m_dat_o = s1_dat_i; m_ack_o = 1'b1; end
        else if (s2_ack_i) begin m_dat_o = s2_dat_i; m_ack_o = 1'b1; end
        else begin
            m_dat_o = 32'h0;
            m_ack_o = 1'b0;
        end
    end

endmodule
