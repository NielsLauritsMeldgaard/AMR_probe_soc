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
// 1001       | 0x24           | 0x4000_0024       | read/write GPO-register: [22'b0, JA[9:6], adc2_shdn, adc2_cs, lora_rf_sw, lora_cs, accel_cs]
// 1010       | 0x28           | 0x4000_0028       | read only  GPI-register: [23'b0, JA[8:5], adc2_busy, lora_busy, lora_dio, accel_int2, accel_int1]
// 1011       | 0x2C           | 0x4000_002C       | write only (write a 1 to address to toggle)
// 1100       | 0x30           | 0x4000_0030       | read only from 1'st -axis mag-field
// 1101       | 0x34           | 0x4000_0034       | read only from 2'nd -axis mag-field
// 1110       | 0x38           | 0x4000_0038       | read only from 3'rd -axis mag-field
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

module io_manager #(
        parameter GPO_WIDTH = 11, // Number of general purpose output pins
        parameter GPI_WIDTH = 9 // Number of general purpose input pins    
    )(
    input  logic        clk, rst,
    // --- Wishbone Slave Interface (CPU Side) ---
    input  logic [31:0] wb_adr_i,   // Address
    input  logic [31:0] wb_dat_i,   // Data from CPU
    input  logic        wb_stb_i,   // Request
    input  logic        wb_we_i,    // Write Enable
    output logic [31:0] wb_dat_o,   // Data to CPU
    output logic        wb_ack_o,   // Ready

    // --- Physical FPGA Pins ---
    output logic [1:0]              leds,
    input  logic                    buttons,
    output logic                    uart_tx,
    input  logic                    uart_rx,
    input  logic [GPI_WIDTH-1:0]    gpio_in,
    output  logic [GPO_WIDTH-1:0]   gpio_out,         
    
    // SPI Pins
    input  logic        MISO,
    output logic        MOSI,
    output logic        SCLK,
    
    // XADC pins
    input logic vauxp4,
    input logic vauxn4,  
    
    //ADC1 (HMC) pins 
    output logic adc1_cs,  // ADC Chip Select (active low) (SPI slave select)
    input logic  adc1_busy, // ADC busy signal (active high, high when conversion is in progress)
    output logic adc1_sdi, // ADC Serial Data Input (MOSI)
    input logic [2:0]  adc1_sdo, // ADC Serial Data Output (MISO) - 3 bits for 3 channels
    output logic adc1_sclk, // ADC Serial Clock Input
    output logic adc1_cnv, // ADC Convert Start Signal 
    output logic adc1_shdn, // ADC Shutdown Control  
    

    output logic dac_busy, 
    output logic dac_rst,
    output logic dac_sclk,
    output logic dac_sdin,
    output logic dac_sync,
    output logic dac_ldac,
    
    // sr driver pins
    output logic        driver_set, driver_rst,
    output logic        sample_enable,
    output logic        phase_o
);
    // --- Internal Wires ---
    logic [3:0]           word_index;
    logic                 write_stb, read_stb;
    logic                 uart_we, uart_re;
    logic [7:0]           u_rx_data;
    logic                 u_rx_valid, u_tx_busy;
    logic                 d_buttons;
    logic [31:0]          wb_dat_o_next;
    logic [31:0]          dat_o_spi;
    logic                 spi_sel;
    localparam            TIMER_W = 32; // bit width of timer value
    logic [TIMER_W-1:0]   timer_o;    
    logic                 timer_set;
    logic [11:0]          adc_value;
    logic [GPI_WIDTH-1:0] gpio_in_reg;
    logic [GPO_WIDTH-1:0] gpio_out_reg;
    logic                 toggle;
    logic [31:0]          field0_data; // magnetic fields data from mag_datapath
    logic [31:0]          field1_data;
    logic [31:0]          field2_data;
    logic               mag_result_valid;
    logic               sample_en;
    logic               phase;
    logic               dac_off;
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
        .MISO(MISO), .MOSI(MOSI), .SCLK(SCLK)
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
    
        mag_sensor_processor_unit mag_sensor_processor_unit(
        .clk_12mhz(clk),
        .rst(rst),
        .toggle(toggle),
        .set_sig(driver_set),
        .reset_sig(driver_rst),
        .adc_cnv(adc1_cnv),
        .adc_sck(adc1_sclk),
        .adc_sdi(adc1_sdi),
        .adc_cs_n(adc1_cs),
        .adc_busy(adc1_busy),
        .adc_sdo0(adc1_sdo[0]),
        .adc_sdo1(adc1_sdo[1]),
        .adc_sdo2(adc1_sdo[2]),
        .dac_busy(dac_busy), 
        .dac_rst(dac_rst),
        .dac_sclk(dac_sclk),
        .dac_sdin(dac_sdin),
        .dac_sync(dac_sync),
        .dac_ldac(dac_ldac),
        .field_ch0(field0_data),
        .field_ch1(field1_data),
        .field_ch2(field2_data),
        .result_valid(mag_result_valid),
        .sample_enable(sample_en),
        .phase_o(phase),
        .dac_off(dac_off)
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
        //toggle = (write_stb && (word_index == 4'b1011) && (wb_dat_i == 32'h1));
        toggle = 1'b1;
        //timer_set = 0;
        gpio_out = gpio_out_reg;
        dac_off = leds[1]; 

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
            4'b1001: wb_dat_o_next = {21'b0, gpio_out_reg}; // GPO register (read current register output status) 
            4'b1010: wb_dat_o_next = {23'b0, gpio_in_reg};  // GPO register (read GPI status)
            4'b1100: wb_dat_o_next = field0_data;
            4'b1101: wb_dat_o_next = field1_data;
            4'b1110: wb_dat_o_next = field2_data;
            
            default: wb_dat_o_next = 32'h0;      
        endcase 
     end 
     
     // --- 2. REGISTERS & HANDSHAKE (Sequential) ---
    always_ff @(posedge clk) begin
        if (rst) begin
            wb_ack_o <= 1'b0;
            leds     <= 2'h0;
            wb_dat_o <= 32'b0;
            gpio_in_reg <= 9'b0;
            gpio_out_reg <= 11'b0;
        end else begin
            // multi-Cycle Ack logic
            wb_ack_o <= wb_stb_i;
            wb_dat_o <= wb_dat_o_next;
            gpio_in_reg <= gpio_in;

            // Simple Output Registers
            if (write_stb && (word_index == 4'b0000))
                leds <= wb_dat_i[1:0];
            if (write_stb && (word_index == 4'b1001))
                gpio_out_reg <= wb_dat_i[GPO_WIDTH-1:0];
        end
    end
    assign adc1_shdn = 0;
    assign sample_enable = sample_en;
    assign phase_o = phase;
endmodule