`timescale 1ns / 1ps

module mag_sensor_processor_unit (
    // Board clock and reset
    input  logic        clk_12mhz,          // Onboard 12 MHz oscillator
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
   
    logic pll_locked;
    logic reset;
    logic [15:0] adc_ch0, adc_ch1, adc_ch2;
    // SR driver -> ADC controller and mag processor
    logic sample_en;
    logic phase;
    logic running;
    logic clkfb;
    logic final_samp;
    // ADC controller -> mag processor logic [15:0] adc_ch0, adc_ch1, adc_ch2;
    logic        adc_data_valid;




    // Reset synchronisation
    // Hold in reset until PLL locks
    always_ff @(posedge clk_12mhz or posedge rst) begin
        if (rst)
            reset <= 1'b1;
        else if (pll_locked)
            reset <= 1'b0;
        else
            reset <= 1'b1;
    end



    

logic clk_unbuf, clk_40_unbuf;
logic clk_40;

MMCME2_BASE #(
    .CLKFBOUT_MULT_F  (60.0),     // VCO = 12 * 60 = 720 MHz 
    .CLKIN1_PERIOD    (83.333),   // 12 MHz
    .CLKOUT0_DIVIDE_F (9.0),      // 720 / 9  = 80 MHz 
    .CLKOUT1_DIVIDE   (18),       // 720 / 18 = 40 MHz 
    .DIVCLK_DIVIDE    (1),
    .CLKOUT0_DUTY_CYCLE(0.5),
    .CLKOUT1_DUTY_CYCLE(0.5),
    .CLKOUT0_PHASE    (0.0),
    .CLKOUT1_PHASE    (0.0)
) pll_inst (
    .CLKIN1   (clk_12mhz),
    .CLKFBIN  (clkfb),
    .CLKFBOUT (clkfb),
    .CLKOUT0  (clk_unbuf),
    .CLKOUT1  (clk_40_unbuf),
    .LOCKED   (pll_locked),
    .PWRDWN   (1'b0),
    .RST      (rst)
);
logic reset_80_meta, reset_80_sync;
logic clk_80;
BUFG clk_buf    (.I(clk_unbuf),    .O(clk_80));
BUFG clk_40_buf (.I(clk_40_unbuf), .O(clk_40));

//always_ff @(posedge clk_80 or posedge reset) begin
//    if (reset) begin
//        reset_80_meta <= 1'b1;
//        reset_80_sync <= 1'b1;
//    end else begin
//        reset_80_meta <= 1'b0;
//        reset_80_sync <= reset_80_meta;
//    end
//end

    

    always_ff @(posedge clk_12mhz or posedge rst) begin
        if (rst)
              running <= 1'b0;
        else if (toggle)      // CPU write lathes for now
             running <= 1'b1;
    end
    // SR Driver
    sr_driver_gen #(
        .CLK_FREQ_HZ    (12_000_000),
        .PULSE_TIME_US  (2),
        .SETTLE_TIME_US (10),
        .WINDOW_TIME_US (2460),
        .GAP_TIME_US    (20)
    ) sr_driver_inst (
        .clk       (clk_12mhz),
        .rst       (rst),
        .set_sig   (set_sig),
        .reset_sig (reset_sig),
        .sample_en (sample_en),
        .phase     (phase),
        .start(running)
    );


    // ADC Controller
    adc_controller#(
        .CLK_FREQ_HZ    (12_000_000),
        .TCNVH          (40),
        .TCONV          (550),
        .N_CHANNELS     (3),
        .TQUIET         (20),
        .WINDOW_TIME_US (2460)
    ) adc_ctrl_inst (
        .clk          (clk_12mhz),
        .rst          (rst),
        .sample_en    (sample_en),
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
        .final_sample(final_samp),
        .CS_n         (adc_cs_n)
    );


    // Magnetometer Processor
    mag_datapath #(
        .ACC_WIDTH      (32),
        .N_CHANNELS     (3),
        .CLK_FREQ_HZ    (12_000_000),
        .TCNVH          (40),
        .TCONV          (550),
        .TQUIET         (20),
        .WINDOW_TIME_US (2460)
    ) mag_data_inst (
        .clk          (clk_12mhz),
        .rst          (rst),
        .ch0_data     (adc_ch0),
        .ch1_data     (adc_ch1),
        .ch2_data     (adc_ch2),

        .data_valid   (adc_data_valid),
        .data_phase   (phase),
        .sample_en    (sample_en),
        .field_ch0    (field_ch0),
        .field_ch1    (field_ch1),
        .field_ch2    (field_ch2),
        .result_valid (result_valid),
        .final_sample (final_samp)
    );
endmodule