`timescale 1ns / 1ps

module mag_sensor_processor_unit (
    // Board clock and reset
    input  logic        clk,          // Onboard 12 MHz oscillator
    input  logic        rst,              // Active-low reset (button)

    // HMC1001 Set/Reset driver outputs
    output logic        set_sig,            // Set pulse to H-bridge
    output logic        reset_sig,          // Reset pulse to H-bridge
    input  logic        toggle,             // toggle to start system
    // LTC2357-16 ADC interface
    output logic        adc_cnv,            // Convert start pulse
    output logic        adc_sck,            // SPI clock to ADC
    output logic        adc_sdi,            // Configuration data to ADC
    output logic        adc_cs_n,           // Chip select held low
    input  logic        adc_busy,           // ADC busy indicator
    input  logic        adc_sdo0,           // Channel 0 data (HMC axis 1)
    input  logic        adc_sdo1,           // Channel 1 data (HMC axis 2)
    input  logic        adc_sdo2,           // Channel 2 data (HMC axis 3)
    
    
    output logic signed [31:0] field_ch0,
    output logic signed [31:0] field_ch1,
    output logic signed [31:0] field_ch2,
    
    output logic               result_valid
);


    // Internal signals

    // Clock and reset
    logic clk80mhz;                              // 80 MHz system clock from PLL
    logic pll_locked;
    logic reset;

    // SR driver -> ADC controller and mag processor
    logic sample_en;
    logic phase;

    // ADC controller -> mag processor logic [15:0] adc_ch0, adc_ch1, adc_ch2;
    logic        adc_data_valid;
    logic        adc_data_phase;

    // Mag processor outputs
    logic [31:0]        n_set;
    logic [31:0]        n_rst;


    // Reset synchronisation
    // Hold in reset until PLL locks
    always_ff @(posedge clk or negedge rst) begin
        if (!rst)
            reset <= 1'b1;
        else if (pll_locked)
            reset <= 1'b0;
        else
            reset <= 1'b1;
    end


    // PLL: 12 MHz -> 80 MHz
    // VCO = 12 * 40 = 480 MHz, CLKOUT0 = 480 / 6 = 80 MHz
    logic clkfb;

    MMCME2_BASE #(
        .CLKFBOUT_MULT_F  (40.0),          // VCO = 12 * 40 = 480 MHz
        .CLKIN1_PERIOD    (83.333),         // 12 MHz = 83.333 ns period
        .CLKOUT0_DIVIDE_F (6.0),            // 480 / 6 = 80 MHz
        .DIVCLK_DIVIDE    (1),
        .CLKOUT0_DUTY_CYCLE(0.5),
        .CLKOUT0_PHASE    (0.0)
    ) pll_inst (
        .CLKIN1   (clk_12mhz),
        .CLKFBIN  (clkfb),
        .CLKFBOUT (clkfb),
        .CLKOUT0  (clk),
        .LOCKED   (pll_locked),
        .PWRDWN   (1'b0),
        .RST      (!rst)
    );


    // SR Driver
    sr_driver_gen #(
        .CLK_FREQ_HZ    (12_000_000),
        .PULSE_TIME_US  (2),
        .SETTLE_TIME_US (10),
        .WINDOW_TIME_US (2460),
        .GAP_TIME_US    (20)
    ) sr_driver_inst (
        .clk       (clk),
        .rst       (reset),
        .set_sig   (set_sig),
        .reset_sig (reset_sig),
        .sample_en (sample_en),
        .phase     (phase),
        .start(toggle)
    );


    // ADC Controller
    adc_controller adc_ctrl_inst (
        .clk          (clk),
        .rst          (reset),
        .sample_en    (sample_en),
        .phase        (phase),
        .cnv          (adc_cnv),
        .sck          (adc_sck),
        .sdi          (adc_sdi),
        .busy         (adc_busy),
        .sdo0         (adc_sdo0),
        .sdo1         (adc_sdo1),
        .sdo2         (adc_sdo2),
        .ch0_data     (adc_ch0),
        .ch1_data     (adc_ch1),
        .ch2_data     (adc_ch2),
        .data_valid   (adc_data_valid),
        .data_phase   (adc_data_phase),
        .CS_n         (adc_cs_n)
    );


    // Magnetometer Processor
    mag_datapath #(
        .ACC_WIDTH      (32),
        .N_CHANNELS     (3),
        .TARGET_SAMPLES (1024)
    ) mag_data_inst (
        .clk          (clk),
        .rst          (reset),
        .ch0_data     (adc_ch0),
        .ch1_data     (adc_ch1),
        .ch2_data     (adc_ch2),

        .data_valid   (adc_data_valid),
        .data_phase   (adc_data_phase),
        .sample_en    (sample_en),
        .field_ch0    (field_ch0),
        .field_ch1    (field_ch1),
        .field_ch2    (field_ch2),
        .n_set        (n_set),
        .n_rst        (n_rst),
        .result_valid (result_valid)
    );
endmodule