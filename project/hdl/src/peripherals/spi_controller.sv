`timescale 1ps/1ps

module spi_controller #(
    parameter MAX_DIV  = 128,
    parameter N_SLAVES = 3
)(
    input  logic clk, rst,

    // Interface
    input  logic [31:0] dat_i,
    input  logic [1:0]  adr_i,   // ONLY 2 bits now
    input  logic        stb_i,
    input  logic        we_i,
    output logic [31:0] dat_o,

    // SPI lines
    input  logic MISO,
    output logic MOSI,
    output logic SCLK,
    output logic [N_SLAVES-1:0] SS
);

    /*================ REGISTERS ================*/

    logic [15:0] tx_reg, tx_reg_next;
    logic [15:0] rx_reg, rx_reg_next;

    logic [1:0]  clk_mode_reg,  clk_mode_reg_next;
    logic        data_mode_reg, data_mode_reg_next;
    logic [$clog2(MAX_DIV)-1:0]  div_reg, div_reg_next;
    logic [$clog2(N_SLAVES)-1:0] slave_sel_reg, slave_sel_reg_next;

    logic start_reg, start_reg_next;
    logic busy_reg, busy_reg_next;
    logic data_valid_reg, data_valid_reg_next;

    logic busy_o, done_o;
    logic [15:0] rx_dat;

    /*================ ADDRESS MAP ================*/

    localparam CTRL_ADDR   = 2'd0; // Control register: [0] Start, [2:1] clk_mode, [3] data_mode, [15:8] div, [23:16] slave_sel
    localparam STATUS_ADDR = 2'd1; // Status register: [0] busy, [1] data_valid
    localparam TX_ADDR     = 2'd2; // Transmit data register: [15:0] data to transmit
    localparam RX_ADDR     = 2'd3; // Receive data register: [15:0] received data (read-only, clears data_valid on read)

    /*================ SPI INSTANCE ================*/

    spi_master #(
        .MAX_DIV(MAX_DIV)
    ) spi (
        .clk(clk),
        .rst(rst),
        .clk_mode_i(clk_mode_reg),
        .data_mode_i(data_mode_reg),
        .en_i(start_reg),
        .div_i(div_reg),
        .tx_data_i(tx_reg),
        .rx_data_o(rx_dat),
        .SCLK(SCLK),
        .MOSI(MOSI),
        .MISO(MISO),
        .busy_o(busy_o),
        .done_o(done_o)
    );

    /*================ COMBINATORIAL ================*/

    always_comb begin

        /* ---- defaults ---- */
        tx_reg_next         = tx_reg;
        rx_reg_next         = rx_reg;
        clk_mode_reg_next   = clk_mode_reg;
        data_mode_reg_next  = data_mode_reg;
        div_reg_next        = div_reg;
        slave_sel_reg_next  = slave_sel_reg;

        start_reg_next      = 1'b0;      // pulse
        busy_reg_next       = busy_reg;
        data_valid_reg_next = data_valid_reg;

        dat_o = 32'h0;

        /*================ WRITE =================*/
        if (stb_i && we_i) begin
            case (adr_i)

                CTRL_ADDR: begin
                    start_reg_next      = dat_i[0];                 // W1P
                    clk_mode_reg_next   = dat_i[2:1];
                    data_mode_reg_next  = dat_i[3];
                    div_reg_next        = dat_i[15:8];
                    slave_sel_reg_next  = dat_i[23:16];
                end

                TX_ADDR:
                    tx_reg_next = dat_i[15:0];

            endcase
        end

        /*================ READ =================*/
        case (adr_i)

            CTRL_ADDR:
                dat_o = {
                    8'b0,
                    slave_sel_reg,
                    div_reg,
                    4'b0,
                    data_mode_reg,
                    clk_mode_reg,
                    1'b0
                };

            STATUS_ADDR:
                dat_o = {30'b0, data_valid_reg, busy_reg};

            RX_ADDR: begin
                dat_o = {16'b0, rx_reg};
                if (stb_i && !we_i)
                    data_valid_reg_next = 1'b0; // clear on read
            end

        endcase

        /*================ STATUS UPDATE =================*/

        busy_reg_next = busy_o;

        if (done_o) begin
            rx_reg_next         = rx_dat;
            data_valid_reg_next = 1'b1;
        end

        /*================ SLAVE SELECT =================*/

        SS = ~(1'b1 << slave_sel_reg);   // active low, direct control
    end


    /*================ SEQUENTIAL ================*/

    always_ff @(posedge clk or posedge rst) begin
        if (rst) begin
            tx_reg         <= 0;
            rx_reg         <= 0;
            clk_mode_reg   <= 0;
            data_mode_reg  <= 0;
            div_reg        <= 1;
            slave_sel_reg  <= 0;
            start_reg      <= 0;
            busy_reg       <= 0;
            data_valid_reg <= 0;
        end else begin
            tx_reg         <= tx_reg_next;
            rx_reg         <= rx_reg_next;
            clk_mode_reg   <= clk_mode_reg_next;
            data_mode_reg  <= data_mode_reg_next;
            div_reg        <= div_reg_next;
            slave_sel_reg  <= slave_sel_reg_next;
            start_reg      <= start_reg_next;
            busy_reg       <= busy_reg_next;
            data_valid_reg <= data_valid_reg_next;
        end
    end

endmodule


module spi_master #(
    parameter MAX_DIV = 128
)(
    input logic clk,
    input logic rst,

    // SPI signals
    input   logic [1:0]                     clk_mode_i,     // CPOL and CPHA
    input   logic                           data_mode_i,    // 0 = 8-bit, 1 = 16-bit
    input   logic                           en_i,           // Start a transaction
    input   logic [$clog2(MAX_DIV) - 1:0]   div_i,        // Clock divider for SCLK (1 -> clk/2, 2 -> clk/4, etc.)
    input   logic [15:0]                    tx_data_i,      // Data to be transmitted
    output  logic [15:0]                    rx_data_o,      // Received data    

    // SPI physical lines
    output  logic SCLK,
    output  logic MOSI,
    input   logic MISO,

    // Status signals
    output  logic busy_o,
    output  logic done_o
);

    typedef enum logic [1:0] {IDLE, LOAD, TRANSFER, DONE} state_t;
    state_t state, next_state;
    
    // Internal wires
    logic strobe; // clk enable
    logic lead_sclk; // leading edge of the sclk
    logic trail_sclk; // trailing edge of the sclk
    logic sample_sclk, shift_sclk; // SCLK edges for sampling and shifting
    logic CPOL, CPHA; // Clock polarity and phase 
    logic mosi_bit;   // Current bit to shift out   

    // Internal registers
    logic [$clog2(MAX_DIV) - 1:0] div_cntr_reg, div_cntr_reg_next; // Counter for clock division
    logic sclk_reg, sclk_reg_next; // internal SCLK reg
    logic [4:0] bit_idx_reg, bit_idx_reg_next; // Counts bits transferred, max 16 for 16-bit mode
    logic [15:0] tx_shift_reg, tx_shift_reg_next; // Shift register for MOSI
    logic [15:0] rx_shift_reg, rx_shift_reg_next; // Shift register for MISO

    // Hardwire CPOL and CPHA from clk_mode_i
    assign CPOL = clk_mode_i[1];
    assign CPHA = clk_mode_i[0];


    always_comb begin        
        /*------------------FSM------------------*/
        // control signals default values
        next_state = IDLE;
        strobe = 0;

        case(state)
            IDLE: begin
                if (en_i) next_state = LOAD;
            end
            LOAD: begin
                next_state = TRANSFER;
                strobe = 0;
            end
            TRANSFER: begin                
                strobe = 1;
                if (bit_idx_reg == (data_mode_i ? 16 : 8) && shift_sclk) begin
                    next_state = DONE;                
                end else begin
                    next_state = TRANSFER;
                end                 
            end
            DONE: begin
                next_state = IDLE;
            end
             default: next_state = IDLE;
        endcase

        /*------------------Clock gen------------------*/        
        if (strobe) begin
            if (div_cntr_reg == div_i - 1) begin
                div_cntr_reg_next = 0;
                sclk_reg_next = ~sclk_reg;
            end else begin
                div_cntr_reg_next = div_cntr_reg + 1;
                sclk_reg_next = sclk_reg;
            end
        end else begin
            div_cntr_reg_next = 0;
            sclk_reg_next = CPOL; // Idle state of SCLK is determined by CPOL
        end
        
        // Generate leading and trailing edge signals
        lead_sclk = (sclk_reg == CPOL) && (sclk_reg_next != CPOL);
        trail_sclk = (sclk_reg != CPOL) && (sclk_reg_next == CPOL);

        /*------------------Bit indexing------------------*/
        sample_sclk = (CPHA == 0) ? lead_sclk  : trail_sclk; // CPHA=0 -> sample on leading, CPHA=1 -> sample on trailing
        shift_sclk  = (CPHA == 0) ? trail_sclk || state == LOAD: lead_sclk;  // CPHA=0 -> shift on trailing, CPHA=1 -> shift on leading

        // index counter logic
        bit_idx_reg_next = bit_idx_reg;
        if (state == IDLE) begin
            bit_idx_reg_next = 0; // Reset bit index at the start of transfer
        end else if (shift_sclk) begin
            bit_idx_reg_next = bit_idx_reg + 1;
        end

        // Shift register logic
        if (state == LOAD) begin
            tx_shift_reg_next = tx_data_i; // Load data into shift register at the start of transfer
        end else if (shift_sclk && bit_idx_reg_next > 1) begin // After the first shift, start shifting the register. This ensures the first bit is available on MOSI before shifting
            tx_shift_reg_next = {tx_shift_reg[14:0], 1'b0};
        end else begin
            tx_shift_reg_next = tx_shift_reg;
        end

        // MISO sampling logic
        if (sample_sclk && bit_idx_reg > 0) begin
            rx_shift_reg_next = {rx_shift_reg[14:0], MISO};
        end else begin
            rx_shift_reg_next = rx_shift_reg;
        end

        mosi_bit = (data_mode_i ? tx_shift_reg[15] : tx_shift_reg[7]) && (bit_idx_reg > 0 && strobe); // Shift out MSB for both 8-bit and 16-bit modes
        
        // Output assignments
        MOSI = mosi_bit;
        SCLK = strobe ? sclk_reg : CPOL; // Drive SCLK only during transfer, otherwise keep it at idle state
        rx_data_o = data_mode_i ? rx_shift_reg : {8'b0, rx_shift_reg[7:0]}; // Output either 8 or 16 bits based on data mode
        busy_o = (state == TRANSFER || state == LOAD);
        done_o = (state == DONE);
    end

    always_ff @( posedge clk or posedge rst ) begin
        if (rst) begin
            state <= IDLE;
            div_cntr_reg <= 0;
            sclk_reg <= CPOL;
            bit_idx_reg <= 0;
            tx_shift_reg <= 16'b0;
            rx_shift_reg <= 16'b0;
        end else begin
            state <= next_state;
            div_cntr_reg <= div_cntr_reg_next;
            sclk_reg <= sclk_reg_next;
            bit_idx_reg <= bit_idx_reg_next; 
            tx_shift_reg <= tx_shift_reg_next;
            rx_shift_reg <= rx_shift_reg_next;       
        end        
    end

endmodule