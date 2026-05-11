`timescale 1ns / 1ps




// ------------------------------------------------------------
// Memory Map (word aligned, wb_adr_i[3:2] used as word_index)
//
// Base Address: 0x4000_0000
//
// word_index | Address Offset | Absolute Address  | Function
// ------------------------------------------------------------
// 0000       | 0x00           | 0x4000_0000       | LEDs
// 0001       | 0x04           | 0x4000_0004       | Timer (1us increments at 12 MHz)
// 0010       | 0x08           | 0x4000_0008       | UART (Rx Valid, Tx Busy, Rx Data)
// 0011       | 0x0C           | 0x4000_000C       | Buttons (Debounced) (only 1 button for cmod A7)
// 0100       | 0x10           | 0x4000_0010       | SPI Register 0
// 0101       | 0x14           | 0x4000_0014       | SPI Register 1
// 0110       | 0x18           | 0x4000_0018       | SPI Register 2
// 0111       | 0x1C           | 0x4000_001C       | SPI Register 3
// 1000       | 0x20           | 0x4000_0020       | XADC readout
// ------------------------------------------------------------
//
// SPI Base Address: 0x4000_0010
// SPI Internal Addressing: wb_adr_i[3:2]
//   0x10 → 2'b00
//   0x14 → 2'b01
//   0x18 → 2'b10
//   0x1C → 2'b11
//
// Note: Addresses are word-aligned (byte offsets increment by 4).
// ------------------------------------------------------------

module io_manager (
    input  logic        clk, rst,
    // --- Wishbone Slave Interface (CPU Side) ---
    input  logic [31:0] wb_adr_i,   // Address
    input  logic [31:0] wb_dat_i,   // Data from CPU
    input  logic        wb_stb_i,   // Request
    input  logic        wb_we_i,    // Write Enable
    output logic [31:0] wb_dat_o,   // Data to CPU
    output logic        wb_ack_o,   // Ready

    // --- Physical FPGA Pins ---
    output logic [1:0]  leds,
    input  logic        buttons,
    output logic        uart_tx,
    input  logic        uart_rx,
    
    // SPI Pins
    input  logic        MISO,
    output logic        MOSI,
    output logic        SCLK,
    output logic [2:0]  SPI_SS,

    // XADC pins
    input logic vauxp4,
    input logic vauxn4    
);
    // --- Internal Wires ---
    logic [3:0] word_index;
    logic       write_stb, read_stb;
    logic       uart_we, uart_re;
    logic [7:0] u_rx_data;
    logic       u_rx_valid, u_tx_busy;
    logic       d_buttons;
    logic [31:0] wb_dat_o_next;
    logic [31:0] dat_o_spi;
    logic spi_sel;
    localparam TIMER_W = 32; // bit width of timer value
    logic [TIMER_W-1:0] timer_o;    
    logic timer_set;
    logic [11:0] adc_value;
    
    // --- 3. SUB-MODULE INSTANTIATIONS ---
    uart_controller uart_unit (
        .clk(clk), .rst(rst),
        .tx_data_i(wb_dat_i[7:0]), .tx_we_i(uart_we),
        .rx_read_i(uart_re),
        .rx_data_o(u_rx_data), .rx_valid_o(u_rx_valid), .tx_busy_o(u_tx_busy),
        .uart_tx_pin(uart_tx), .uart_rx_pin(uart_rx)
    );
    
    debounce button_unit (
        .clk(clk), .rst(rst), .buttons(buttons), .d_buttons(d_buttons)
    );

    spi_controller spi_unit (
        .clk(clk), .rst(rst),
        .dat_i(wb_dat_i), .adr_i(wb_adr_i[3:2]), .stb_i(wb_stb_i && spi_sel), .we_i(wb_we_i && spi_sel), .dat_o(dat_o_spi),
        .MISO(MISO), .MOSI(MOSI), .SCLK(SCLK), .SS(SPI_SS)
    );
    
    // 1us timer at 12 MHz
    nerd_counter #(.MAX_COUNT(12), .WIDTH(TIMER_W)) us_timer_unit (
        .clk(clk), .rst(rst), .set(timer_set), .timer(timer_o)
    );
    
    // XADC for pin A15
    xadc_unit xadc_unit (
        .clk(clk),
        .vauxp4(vauxp4), 
        .vauxn4(vauxn4), 
        .adc_value(adc_value) 
    );
    

     always_comb begin
        // defaults 
        word_index = wb_adr_i[5:2];
        write_stb  = wb_stb_i && wb_we_i;
        read_stb   = wb_stb_i && !wb_we_i;
        
        // write enable signal for lower modules.
        uart_we = (write_stb && (word_index == 4'b10));
        uart_re = (read_stb && (word_index == 4'b10));
        spi_sel = (word_index[3:2] == 2'b01);
        timer_set = (write_stb && (word_index == 4'b0001) && (wb_dat_i == 32'h1));
        //timer_set = 0;
       

        case (word_index) 
            4'b0000: wb_dat_o_next = {30'h0, leds};                      // Addr 0x0
            4'b0001: wb_dat_o_next = timer_o[31:0]; // Addr 0x4 (timer counting us)
            4'b0010: wb_dat_o_next = {22'b0, u_rx_valid, u_tx_busy, u_rx_data}; // Addr 0x8
            4'b0011: wb_dat_o_next = {31'b0, d_buttons}; // Addr 0xC
            4'b0100: wb_dat_o_next = dat_o_spi; // SPI data register
            4'b0101: wb_dat_o_next = dat_o_spi; // SPI control register
            4'b0110: wb_dat_o_next = dat_o_spi; // SPI status register
            4'b0111: wb_dat_o_next = dat_o_spi; // Reserved
            4'b1000: wb_dat_o_next = {20'b0, adc_value}; // XADC readout
            default: wb_dat_o_next = 32'h0;      
        endcase 
     end 
     
     // --- 2. REGISTERS & HANDSHAKE (Sequential) ---
    always_ff @(posedge clk) begin
        if (rst) begin
            wb_ack_o <= 1'b0;
            leds     <= 2'h0;
            wb_dat_o <= 32'b0;
        end else begin
            // multi-Cycle Ack logic
            wb_ack_o <= wb_stb_i;
            wb_dat_o <= wb_dat_o_next;

            // Simple Output Registers
            if (write_stb && (word_index == 4'b0000))
                leds <= wb_dat_i[1:0];
        end
    end
    
endmodule