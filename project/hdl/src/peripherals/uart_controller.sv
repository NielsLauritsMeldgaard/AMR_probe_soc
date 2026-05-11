`timescale 1ns / 1ps

// UART controller for fixed baud rate. REMEMBER to set correct clock freq
// The module currently latches the received data to rx_data_o whenever rx_valid is high
module uart_controller   #(parameter BAUD_RATE = 115200, CLK_FREQ = 12_000_000)
(
    input  logic        clk, rst,
    input  logic [7:0]  tx_data_i,   // Byte to send
    input  logic        tx_we_i,     // Start transmission
    input  logic        rx_read_i,   // Read strobe from CPU to clear valid flag
    output logic [7:0]  rx_data_o,   // Byte received
    output logic        rx_valid_o,  // New data available (latched until read)
    output logic        tx_busy_o,   // UART is currently sending
    output logic        uart_tx_pin, // Physical TX wire
    input  logic        uart_rx_pin  // Physical RX wire
);
        
    uart_tx #(
        .BAUD_RATE(BAUD_RATE),
        .CLK_FREQ(CLK_FREQ)
    ) tx (
        .clk(clk),
        .rst(rst),
        .tx_data_i(tx_data_i),
        .tx_we_i(tx_we_i),
        .tx_busy_o(tx_busy_o),
        .uart_tx_pin(uart_tx_pin)
    );
    
    logic rx_valid;
    logic [7:0] rx_data_o_wire;
    uart_rx #(
        .BAUD_RATE(BAUD_RATE),
        .CLK_FREQ(CLK_FREQ)
    ) rx (
        .clk(clk),
        .rst(rst),
        .uart_rx_pin(uart_rx_pin),
        .rx_valid_o(rx_valid),
        .rx_data_o(rx_data_o_wire)
    );
    
    // Latch rx_valid and rx_data until CPU reads
    logic rx_valid_latched;
    
    always_ff @(posedge clk) begin
        if (rst) begin
            rx_valid_latched <= 1'b0;
            rx_data_o <= 8'h0;
        end else begin
            // New byte arrived: latch data and set valid flag
            if (rx_valid) begin
                rx_data_o <= rx_data_o_wire;
                rx_valid_latched <= 1'b1;
            end 
            // CPU read: clear valid flag
            else if (rx_read_i) begin
                rx_valid_latched <= 1'b0;
            end
        end
    end
    
    assign rx_valid_o = rx_valid_latched;    
    
endmodule

module uart_rx #(
    parameter BAUD_RATE = 115200,
    parameter CLK_FREQ  = 100_000_000
)(
    input  logic        clk, rst,
    input  logic        uart_rx_pin,
    output logic        rx_valid_o,
    output logic [7:0]  rx_data_o
);

    // Baud rate calculations
    localparam integer BAUD_CNT      = CLK_FREQ / BAUD_RATE;
    localparam integer BAUD_CNT_HALF = BAUD_CNT / 2;
    localparam integer CNT_WIDTH     = $clog2(BAUD_CNT);

    // State machine
    typedef enum logic [1:0] {IDLE, START, DATA, STOP} rx_state_t;
    rx_state_t rx_state, rx_next_state;

    // Shift register and bit counter
    logic [7:0] rx_data_shiftreg, rx_data_shiftreg_next;
    logic [3:0] rx_bit_idx, rx_bit_idx_next;

    // Baud timing
    logic [CNT_WIDTH-1:0] baud_counter, baud_counter_next;
    logic baud_tick, mid_baud_tick;

    // Input synchronization
    logic uart_rx_reg, uart_rx_reg_d;

    // Edge detection
    logic rx_pin_negedge;
    assign rx_pin_negedge = (!uart_rx_reg && uart_rx_reg_d);

    // Output registers
    logic rx_valid_next;
    logic [7:0] rx_data_next;

    
    always_comb begin
        // Defaults
        rx_next_state = rx_state;

        baud_tick = 0;
        mid_baud_tick = 0;

        rx_data_shiftreg_next = rx_data_shiftreg;
        rx_bit_idx_next = rx_bit_idx;

        rx_valid_next = 0;
        rx_data_next = rx_data_o;

        // Baud counter logic
        if (rx_state == IDLE) begin
            baud_counter_next = 0;
        end else if (baud_counter == BAUD_CNT-1) begin
            baud_tick = 1;
            baud_counter_next = 0;
        end else if (baud_counter == BAUD_CNT_HALF-1) begin
            mid_baud_tick = 1;
            baud_counter_next = baud_counter + 1;
        end else begin
            baud_counter_next = baud_counter + 1;
        end

        // FSM
        case (rx_state)

            IDLE: begin
                rx_bit_idx_next = 0;
                rx_data_shiftreg_next = 0;

                if (rx_pin_negedge) begin
                    rx_next_state = START;
                end
            end

            START: begin
                if (mid_baud_tick) begin
                    if (uart_rx_reg == 0) begin
                        rx_next_state = DATA;
                    end else begin
                        // False start bit
                        rx_next_state = IDLE;
                    end
                end
            end

            DATA: begin
                if (mid_baud_tick) begin
                    // Sample data bit in middle of bit period
                    rx_data_shiftreg_next =
                        {uart_rx_reg, rx_data_shiftreg[7:1]};

                    if (rx_bit_idx == 7) begin
                        rx_next_state = STOP;
                    end else begin
                        rx_bit_idx_next = rx_bit_idx + 1;
                    end
                end
            end

            STOP: begin
                if (mid_baud_tick) begin
                    if (uart_rx_reg == 1) begin
                        rx_data_next = rx_data_shiftreg;
                        rx_valid_next = 1;
                    end

                    rx_next_state = IDLE;
                end
            end

        endcase
    end


    always_ff @(posedge clk or posedge rst) begin
        if (rst) begin
            baud_counter      <= 0;
            uart_rx_reg       <= 1;
            uart_rx_reg_d     <= 1;
            rx_state          <= IDLE;
            rx_bit_idx        <= 0;
            rx_data_shiftreg  <= 0;
            rx_valid_o        <= 0;
            rx_data_o         <= 0;

        end else begin
            baud_counter      <= baud_counter_next;
            uart_rx_reg       <= uart_rx_pin;
            uart_rx_reg_d     <= uart_rx_reg;
            rx_state          <= rx_next_state;
            rx_bit_idx        <= rx_bit_idx_next;
            rx_data_shiftreg  <= rx_data_shiftreg_next;
            rx_valid_o        <= rx_valid_next;
            rx_data_o         <= rx_data_next;
        end
    end

endmodule

module uart_tx #(
    parameter BAUD_RATE = 115200,       // Default baud rate
    parameter CLK_FREQ = 100_000_000    // Default clock frequency
)(
    input logic         clk, rst,
    input logic [7:0]   tx_data_i,      // Byte to send
    input logic         tx_we_i,        // start transmission
    output logic        tx_busy_o,      // UART is currently sending
    output logic        uart_tx_pin     // Physical TX wire
);
    
    localparam integer BAUD_CNT = CLK_FREQ / BAUD_RATE;
    localparam integer CNT_WIDTH = $clog2(BAUD_CNT);
    
    logic [7:0] tx_data_shiftreg, tx_data_shiftreg_next;    // Shift reg for data transmission
    logic [3:0] tx_bit_idx, tx_bit_idx_next;
    
    typedef enum logic [1:0] {IDLE, START, DATA, STOP} tx_state_t;
    tx_state_t tx_state, tx_next_state;
    
    logic [CNT_WIDTH - 1:0] baud_counter, baud_counter_next = 0;
    logic baud_tick, uart_tx_reg, uart_tx_reg_next;
    
    always_comb begin

        baud_tick = 0;
        if (baud_counter == BAUD_CNT - 1) begin
            baud_counter_next = 0;
            baud_tick = 1;
        end else begin
            baud_counter_next = baud_counter + 1;    
        end
    
        tx_busy_o = 0;
        uart_tx_reg_next = 1;
    
        tx_bit_idx_next = tx_bit_idx;
        tx_data_shiftreg_next = tx_data_shiftreg;    
        tx_next_state = tx_state;
    
        case (tx_state)
    
            IDLE: begin            
                if (tx_we_i) begin
                    tx_data_shiftreg_next = tx_data_i;
                    tx_bit_idx_next = 0;
                    tx_next_state = START;
                    baud_counter_next = 0; // Reset counter to ensure full Start Bit duration
                end
            end
            
            START: begin
                tx_busy_o = 1;
                uart_tx_reg_next = 0;
    
                if (baud_tick)
                    tx_next_state = DATA;
            end
            
            DATA: begin
                tx_busy_o = 1;
                uart_tx_reg_next = tx_data_shiftreg[0];
    
                if (baud_tick) begin
                    tx_data_shiftreg_next = tx_data_shiftreg >> 1;
    
                    if (tx_bit_idx == 7)
                        tx_next_state = STOP;
                    else
                        tx_bit_idx_next = tx_bit_idx + 1;
                end
            end
            
            STOP: begin
                tx_busy_o = 1;
    
                if (baud_tick)
                    tx_next_state = IDLE;
            end                        
        endcase
        
        uart_tx_pin = uart_tx_reg;
        
    end

       
    always_ff @(posedge clk or posedge rst) begin
        if (rst) begin
            baud_counter <= 0;
            tx_state <= IDLE;
            tx_bit_idx <= 0;
            tx_data_shiftreg <= 0;
            uart_tx_reg <= 1;    
        end else begin
            baud_counter <= baud_counter_next;
            tx_state <= tx_next_state;
            tx_bit_idx <= tx_bit_idx_next;
            tx_data_shiftreg <= tx_data_shiftreg_next;
            uart_tx_reg <= uart_tx_reg_next;
        end
     end                
endmodule
