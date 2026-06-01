`timescale 1ns / 1ps


// Module: mag_processor
// Description: Synchronous demodulator and accumulator for the AMR 
//              magnetometer. Receives ADC samples from the ADC controller. Accumulates
//              SET and RESET phase samples separately, then on the falling
//              edge of sample_en computes the field estimate by subtracting
//              the two accumulators and outputting the result.
//



module mag_datapath #(
    parameter int ACC_WIDTH     = 32,   // Accumulator width in bits
    parameter int N_CHANNELS    = 3,     // Number of ADC channels
     parameter int CLK_FREQ_HZ   = 12_000_000,  // clk freq 40MHz
    parameter int TCNVH          = 40,    // conversion time 40ns
    parameter int TCONV          = 550,    // busy high: 500xN ns
    parameter int TQUIET         = 20,   // Quiet time: 20ns 
    parameter int WINDOW_TIME_US = 1478,
    parameter int INTEG_SHIFT   = 9    // integrator gain: error >>= INTEG_SHIF
)(
    input  logic        clk,
    input  logic        rst,

    // From ADC controller
    input  logic [15:0] ch0_data,
    input  logic [15:0] ch1_data,
    input  logic [15:0] ch2_data,
    input  logic        data_valid,     // One cycle strobe per conversion
    input  logic        data_phase,     // 0 = SET, 1 = RESET
    input  logic        final_sample,

    // From SR driver
    input  logic        sample_en,      // High during sampling window

    // Output field estimates - one per channel
    output logic  [ACC_WIDTH-1:0] field_ch0,
    output logic  [ACC_WIDTH-1:0] field_ch1,
    output logic  [ACC_WIDTH-1:0] field_ch2,
    output logic        result_valid,  // One cycle strobe
    
    // DAC feedback outputs
    output logic [15:0] dac_ch0,
    output logic [15:0] dac_ch1,
    output logic [15:0] dac_ch2,
    output logic        dac_send

);
    localparam real NS_PER_TICK = 1_000_000_000.0 / CLK_FREQ_HZ;  // 25.0 ns at 40MHz
    localparam int N_BITS         = 16;         // bits per conversion readout
    localparam int CNV_HIGH_TICKS = int'($ceil(TCNVH         / NS_PER_TICK));  // ceil(40/25)  = 2
    localparam int BUSY_TIMEOUT   = int'($ceil((TCONV * N_CHANNELS) / NS_PER_TICK));  // ceil(1500/25) = 60
    localparam int QUIET_TICKS    = int'($ceil(TQUIET        / NS_PER_TICK));  // ceil(20/25)  = 1
    localparam real T_SHIFT_NS    = N_BITS * 2.0 * NS_PER_TICK;   // 2 ticks per bit
    // tCYC in ns
    localparam real tCYC = (CNV_HIGH_TICKS + BUSY_TIMEOUT + (N_BITS * 2) + QUIET_TICKS) 
                       * NS_PER_TICK;
    localparam signed [15:0] ADC_OFFSET = 16'd26843;  // 2.5V midpoint in straight binary

    
// Window time in ns
    localparam real WINDOW_NS      = WINDOW_TIME_US * 1000.0;
    // Max samples that fit in the window
    localparam int SAMPLES_RAW = int'($floor(WINDOW_NS / tCYC));
    localparam int SHIFT_BITS  = (SAMPLES_RAW == (1 << $clog2(SAMPLES_RAW))) ?
                                   $clog2(SAMPLES_RAW) :      // already power of 2
                                  $clog2(SAMPLES_RAW) - 1;   // round down
   localparam int SAMPLES     = 1 << SHIFT_BITS; 

     // integrator clamp: ±half DAC range so midscale ± integ never wraps
    localparam  [ACC_WIDTH-1:0] INT_MAX =  16'd65535;
    localparam  [ACC_WIDTH-1:0] INT_MIN = 16'd0;



    // SET and RESET accumulators for each channel
    logic  [ACC_WIDTH-1:0] acc_set0_q, acc_set0_d;
    logic  [ACC_WIDTH-1:0] acc_set1_q, acc_set1_d;
    logic  [ACC_WIDTH-1:0] acc_set2_q, acc_set2_d;

    logic  [ACC_WIDTH-1:0] acc_rst0_q, acc_rst0_d;
    logic  [ACC_WIDTH-1:0] acc_rst1_q, acc_rst1_d;
    logic  [ACC_WIDTH-1:0] acc_rst2_q, acc_rst2_d;
    
    //demodulated signals
    logic  [ACC_WIDTH-1:0] error0_q, error0_d;
    logic  [ACC_WIDTH-1:0] error1_q, error1_d;
    logic  [ACC_WIDTH-1:0] error2_q, error2_d;
    
    // integrator wires
    logic  [ACC_WIDTH-1:0] integ0_q, integ0_d;
    logic  [ACC_WIDTH-1:0] integ1_q, integ1_d;
    logic  [ACC_WIDTH-1:0] integ2_q, integ2_d;
    // Output registers
    logic       valid_q,  valid_d;
    logic [1:0] final_samp_d, final_samp_q;
    logic       send_q,  send_d;
    
    function automatic logic  [ACC_WIDTH-1:0] sat_add(
        input logic  [ACC_WIDTH-1:0] state,
        input logic  [ACC_WIDTH-1:0] delta
    );
        logic  [ACC_WIDTH:0] sum;
        sum = {state[ACC_WIDTH-1], state} + {delta[ACC_WIDTH-1], delta};
        return sum[ACC_WIDTH-1:0];
    endfunction

    always_comb begin
        // Defaults 
        acc_set0_d   = acc_set0_q;
        acc_set1_d   = acc_set1_q;
        acc_set2_d   = acc_set2_q;
        acc_rst0_d   = acc_rst0_q;
        acc_rst1_d   = acc_rst1_q;
        acc_rst2_d   = acc_rst2_q;
        error0_d     = error0_q;
        error1_d     = error1_q;
        error2_d     = error2_q;
        integ0_d     = integ0_q;
        integ1_d     = integ1_q;
        integ2_d     = integ2_q;
        valid_d      = 1'b0;
        send_d       = 1'b0;
        final_samp_d = final_samp_q;
        


        // Accumulate incoming samples into SET or RESET accumulators
        // based on phase signal from SR driver
        if (data_valid && sample_en) begin
            if (!data_phase) begin
                // SET phase, accumulate positively
                acc_set0_d = acc_set0_q + ACC_WIDTH'(ch0_data);
                acc_set1_d = acc_set1_q + ACC_WIDTH'(ch1_data );
                acc_set2_d = acc_set2_q + ACC_WIDTH'(ch2_data );
                 

            end else begin
                // RESET phase, accumulate into separate register
                acc_rst0_d = acc_rst0_q + ACC_WIDTH'(ch0_data);
                acc_rst1_d = acc_rst1_q + ACC_WIDTH'(ch1_data);
                acc_rst2_d = acc_rst2_q + ACC_WIDTH'(ch2_data);
                
                
                
            end
        end
        
        if (final_sample) begin
            final_samp_d = final_samp_q + 1;
        end
        // On falling edge of sample_en - both windows complete
        // Compute field estimate: (ACC_SET - ACC_RST)
        // Reset accumulators for next cycle
        if (final_samp_q == 2) begin
            // Subtract accumulators - offset cancels, field doubles
            // synchronous demodulation: error = (SET - RESET) >> SHIFT_BITS
            // offset cancels, field signal doubles
            error0_d = ((acc_set0_q - acc_rst0_q) >> SHIFT_BITS)>>2;
            
            error1_d = ((acc_set1_q - acc_rst1_q) >> SHIFT_BITS)>>2;
            error2_d = ((acc_set2_q - acc_rst2_q) >> SHIFT_BITS)>>2;
            
    
            integ0_d = integ0_q + (error0_d >>> INTEG_SHIFT);
            integ1_d = integ1_q + (error1_d >>> INTEG_SHIFT);
            integ2_d = integ2_q + (error2_d >>> INTEG_SHIFT);
        
            // Assert result valid for one cycle
            valid_d  = 1'b1;
            send_d  = 1'b1;   // trigger DAC update
            // clear accumulators and counter for next cycle
            acc_set0_d   = '0;
            acc_set1_d   = '0;
            acc_set2_d   = '0;
            acc_rst0_d   = '0;
            acc_rst1_d   = '0;
            acc_rst2_d   = '0;
            final_samp_d = '0;
        end
    end


    always_ff @(posedge clk or posedge rst) begin
        if (rst) begin
            acc_set0_q   <= '0;
            acc_set1_q   <= '0; 
            acc_set2_q   <= '0;
            acc_rst0_q   <= '0;
            acc_rst1_q   <= '0; 
            acc_rst2_q   <= '0;
            error0_q     <= '0; 
            error1_q     <= '0; 
            error2_q     <= '0;
            integ0_q     <= '0; 
            integ1_q     <= '0; 
            integ2_q     <= '0;
            valid_q      <= 1'b0;
            send_q       <= 1'b0;
            final_samp_q <= '0;
        end else begin
            acc_set0_q   <= acc_set0_d; 
            acc_set1_q   <= acc_set1_d; 
            acc_set2_q   <= acc_set2_d;
            acc_rst0_q   <= acc_rst0_d; 
            acc_rst1_q   <= acc_rst1_d; 
            acc_rst2_q   <= acc_rst2_d;
            error0_q     <= error0_d;   
            error1_q     <= error1_d;   
            error2_q     <= error2_d;
            integ0_q     <= integ0_d;   
            integ1_q     <= integ1_d;   
            integ2_q     <= integ2_d;
            valid_q      <= valid_d;
            send_q       <= send_d;
            final_samp_q <= final_samp_d;
        end
    end
    // -------------------------------------------------------------------------
    // Output assignments
    // -------------------------------------------------------------------------
    // field outputs: integrator state, signed, centred at 0
    assign field_ch0   = integ0_q;
    assign field_ch1   = integ1_q;
    assign field_ch2   = integ2_q;
    assign result_valid = valid_q;

    // DAC outputs: integrator state offset by midscale
    // integ=0 → DAC=32768 → 2.5V → zero feedback current → null
    // integ>0 → DAC>midscale → positive coil current cancels positive field
    // integ<0 → DAC<midscale → negative coil current cancels negative field
    assign dac_ch0 = integ0_q[15:0];
    assign dac_ch1 = integ1_q[15:0];
    assign dac_ch2 =  integ2_q[15:0];
    assign dac_send = send_q;
endmodule