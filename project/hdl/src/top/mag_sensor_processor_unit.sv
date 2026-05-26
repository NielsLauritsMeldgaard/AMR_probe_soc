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


    logic [15:0] adc_ch0, adc_ch1, adc_ch2;
    // SR driver -> ADC controller and mag processor
    logic sample_en;
    logic phase;
    logic running;
    
    logic final_samp;
    // ADC controller -> mag processor logic [15:0] adc_ch0, adc_ch1, adc_ch2;
    logic        adc_data_valid;




  

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