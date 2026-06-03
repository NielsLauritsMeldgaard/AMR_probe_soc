`timescale 1ns / 1ps

// -----------------------------------------------------------------------------
// Module: adc_controller
// Description: Controls the LTC2357-16 4-channel simultaneous sampling ADC.
//              Sends configuration word once after reset, then continuously
//              captures 3 channels during the sampling window asserted
//              by the SR driver. Outputs raw 16-bit samples with channel index
//              and data_valid strobe to the magnetometer processor.
//
// SPI Mode: SCK idles low, data captured on rising edge 
// SCK frequency: CLK_FREQ_HZ / (2 * SPI_CLK_DIV) = 80MHz / (2*1) = 40MHz
//
// Configuration word: 000 101 101 101 (differential mode all channels)
// Sent MSB first as 12-bit word padded to 16 bits: 0000_0001_0110_1101


module adc_controller #(
    parameter int CLK_FREQ_HZ    = 12_000_000,  // clk freq 40MHz
    parameter int TCNVH          = 40,    // conversion time 40ns
    parameter int TCONV          = 550,    // busy high: 500xN ns
    parameter int N_CHANNELS     = 3,    // number of channels: 3
    parameter int TQUIET         = 20,   // Quiet time: 20ns 
    parameter int WINDOW_TIME_US = 1478
)(
    input  logic        clk,
    input  logic        rst,

    // SR driver interface
    input  logic        sample_en,             // From sr_driver: sample window (means a valid measurements from HMC)
    input  logic        phase, 

    // ADC physical interface
    output logic        cnv,                   // Convert start pulse
    output logic        sck,                   // SPI clock (40MHz at 80MHz system clk)
    input  logic        busy,                  // ADC busy signal
    output logic        sdi,                   // Configuration data to ADC
    input  logic        sdo0,                  // Channel 0 data
    input  logic        sdo1,                  // Channel 1 data
    input  logic        sdo2,                  // Channel 2 data
    output logic        CS_n,                    // chip select is not used.
    output logic        shdn,                  // shutdown power
    // Output to magnetometer processor
    output logic [15:0] ch0_data,
    output logic [15:0] ch1_data,
    output logic [15:0] ch2_data,
    output logic        data_valid,            // One cycle strobe per conversion
    output logic        final_sample
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

    
// Window time in ns
    localparam real WINDOW_NS      = WINDOW_TIME_US * 1000.0;
    // Max samples that fit in the window
    localparam int SAMPLES_RAW = 256; //int'($floor(WINDOW_NS / tCYC));
    localparam int SAMPLE_BITS  = (SAMPLES_RAW == (1 << $clog2(SAMPLES_RAW))) ?
                                   $clog2(SAMPLES_RAW) :      // already power of 2
                                  $clog2(SAMPLES_RAW) - 1;   // round down
   localparam int SAMPLES     = 1 << SAMPLE_BITS; 
     

    // Configuration word
    // LTC2357-16 config: 000 101 101 101 => unipolar input range: 0V to 2xVREF where VREF=2.5V from DAC bandgap so 0V to 5V
    // Sent as 12 bits MSB first, padded to 16 bits
   // softspan word big 
    
    localparam logic [15:0] SOFTSPAN_WORD = 16'b0001_0010_0100_0000;
    // State machine
    typedef enum logic [3:0] {
        ST_IDLE,            // Wait for sample_en from SR driver
        ST_CNV_PULSE,       // Assert CNV to start conversion
        ST_WAIT_BUSY,        // Wait for BUSY to go low
        ST_SHIFT,           // Shift out 16 bits on all four SDO lines
        ST_QUIET,            // time after sdo out to next cnv
        ST_DISCARD,         // discard the first sdo values after config
        ST_DONE             // Assert data_valid for one cycle
    } state_t;
    
    logic sample_en_d, sample_en_q;
    // internal registers for state, timers, counters, signals and SPI clk
    state_t      state_q,        state_d;
    logic [7:0]  timer_q,        timer_d;            // General purpose tick counter
    logic [4:0]  bit_q,          bit_d;              // Bit counter (0-15)
    logic        scki_q,         scki_d;              // SPI clock register
    logic        startup_done_q, startup_done_d; // Initial config startup check 
    logic        scki_rising,    scki_falling;      // falling and rising edge of scki
    logic [SAMPLE_BITS-1:0]      sample_count_q, sample_count_d;

    // Shift registers for incoming data
    logic [15:0] sr0_q, sr0_d;
    logic [15:0] sr1_q, sr1_d;
    logic [15:0] sr2_q, sr2_d;
    
   

    
    // Output registers
    logic sdi_d, sdi_q;
    logic [15:0] ch0_q, ch0_d;
    logic [15:0] ch1_q, ch1_d;
    logic [15:0] ch2_q, ch2_d;
    logic data_valid_d, data_valid_q;
    

   // logic sample_en_meta, sample_en_sync;
    // in use when using CDC
//    xpm_cdc_single #(
//        .DEST_SYNC_FF  (2),    // two flop synchronizer
//        .SRC_INPUT_REG (0)     // source already registered
//    ) sample_en_cdc (
//        .src_clk  (clk_12mhz),
//        .src_in   (sample_en),
//        .dest_clk (clk_80),
//        .dest_out (sample_en_sync)
//    );

    // FSM flow - the first data after softspan has been sent is garbage, a discard state is used to ignore data.
  // ST_IDLE -> ST_CNV_PULSE -> ST_WAIT_BUSY -> ST_SHIFT -> ST_QUIET -> ST_DISCARD -> ST_IDLE
    //                                                                 
      //                                         (normal loop starts, after one discard state has been used)
  //  ST_IDLE -> ST_CNV_PULSE -> ST_WAIT_BUSY -> ST_SHIFT -> ST_QUIET -> ST_DONE -> ST_IDLE
    always_comb begin
    //Defaults
     state_d     = state_q;
     scki_d       = scki_q;
     bit_d        = bit_q;
     timer_d      = timer_q + 1;
     startup_done_d = startup_done_q;
     sr0_d          = sr0_q;
     sr1_d          = sr1_q;
     sr2_d          = sr2_q;
     ch0_d          = ch0_q;
     ch1_d          = ch1_q;
     ch2_d          = ch2_q;
     final_sample   = 0;
     sample_count_d = sample_count_q;
    sample_en_d = sample_en;
     
     scki_rising  = 1'b0;
     scki_falling = 1'b0;
     cnv          = 1'b0;
     sdi_d        = sdi_q;
     data_valid_d = data_valid_q;
      case (state_q) 
            ST_IDLE: begin
               
               scki_d    = 1'b0;
               data_valid_d = 0;
                if (sample_en &&(sample_en_d != sample_en_q) ) begin
                state_d = ST_CNV_PULSE;
                timer_d = 0;
                
                end else begin
                   state_d = ST_IDLE; 
                   
               end
            end
            
            ST_CNV_PULSE: begin
                cnv = 1'b1;
                scki_d    = 1'b0;
                data_valid_d = 0;
                if (timer_q >= CNV_HIGH_TICKS) begin
                    timer_d = 0;
                    state_d = ST_WAIT_BUSY;
                end else begin
                    state_d = ST_CNV_PULSE;
                end
            end
            
            ST_WAIT_BUSY: begin
                scki_d = 1'b0;
                data_valid_d = 0;
                sdi_d  = SOFTSPAN_WORD[15];
                if (!busy) begin
                    state_d = ST_SHIFT;
                    timer_d = 0;
                    
                    bit_d   = 5'd15;
                end else if (timer_q >= BUSY_TIMEOUT - 1) begin
                    // Hardware fault - BUSY never fell
                    // Return to idle and wait for next window
                    state_d = ST_IDLE;
                    timer_d = 0;
                end else begin
                    state_d = ST_WAIT_BUSY;
                    
                end
            end
            
           ST_SHIFT: begin
                scki_d = ~scki_q;
                data_valid_d = 0;
                scki_rising  = (scki_q == 1'b0);  
                scki_falling = (scki_q == 1'b1);  
                        // On falling SCKI edge, output next SDI bit
                if (scki_falling) begin
                    sdi_d = SOFTSPAN_WORD[bit_q];  // bit_q goes 15 down to 0
                end 
                
                if (scki_rising) begin
                    sr0_d = {sr0_q[14:0], sdo0};
                    sr1_d = {sr1_q[14:0], sdo1};
                    sr2_d = {sr2_q[14:0], sdo2};
                    
                    if (bit_q == 0) begin
                        state_d = ST_QUIET;
                        timer_d = '0;
                        scki_d  = 1'b0; 
                    end else begin
                        bit_d = bit_q - 1;
                       
                    end
                end 
                
            end
            
            ST_QUIET: begin
             // Hold SCKI low, SDI low, CNV low
             // Just count 2 ticks then proceed
                 scki_d    = 1'b0;
                 data_valid_d = 0;
                 if (timer_q >= QUIET_TICKS - 1) begin
                    timer_d = '0;
                    if (!startup_done_q) begin
                        state_d = ST_DISCARD;
                    end else begin
                        state_d = ST_DONE;
                    end
                 end
                    
             end
            
            ST_DISCARD: begin
                // Softspan now loaded, discard this result
                data_valid_d = 0;
                scki_d    = 1'b0;
                startup_done_d = 1'b1;  // flag: from now on ST_DONE instead
                if (sample_en) begin
                    state_d = ST_CNV_PULSE;
                end else begin
                    state_d = ST_IDLE;
                end
            end
            
            ST_DONE: begin
                data_valid_d = 1;
                scki_d    = 1'b0;
                
                ch0_d   = sr0_q[15:0];  // or sr0_q[15:0] if 16-bit
                ch1_d   = sr1_q[15:0];
                ch2_d   = sr2_q[15:0];
             
                if (sample_en && sample_count_q < SAMPLES-1) begin
                    state_d = ST_CNV_PULSE;
                    sample_count_d = sample_count_q + 1;
                end else begin
                    state_d = ST_IDLE;
                    sample_count_d = '0;
                    final_sample   = 1'b1;
                    
                end          
            end 
      endcase 
    end

    // Sequential block
    always_ff @(posedge clk or posedge rst) begin
        if (rst) begin
         state_q        <= ST_IDLE;
         timer_q        <= 0;
         bit_q          <= 15;
         scki_q         <= 0;
         startup_done_q <= 0;
         
         sr0_q          <= 16'b0;
         sr1_q          <= 16'b0;
         sr2_q          <= 16'b0;
         
         ch0_q          <= 16'b0;
         ch1_q          <= 16'b0;
         ch2_q          <= 16'b0; 
         sample_count_q <= 0;
         data_valid_q   <= 0;
         sample_en_q        <= 0;  
         
        end else begin
        state_q        <= state_d;
        timer_q        <= timer_d;
        bit_q          <= bit_d;
        scki_q         <= scki_d;
        startup_done_q <= startup_done_d;
        sample_count_q <= sample_count_d;
        data_valid_q   <= data_valid_d;
        sample_en_q        <= sample_en_d;
       
        //shift registers
        sr0_q <= sr0_d;
        sr1_q <= sr1_d;
        sr2_q <= sr2_d;
        
        //output registers
        ch0_q   <= ch0_d;
        ch1_q   <= ch1_d;
        ch2_q   <= ch2_d;
        sdi_q <= sdi_d;
        
        end
    end

    
    // Output assignments
    assign CS_n = 1'b0;  // Chip select is not in use since no bus sharing
    assign shdn = 1'b0;
    assign ch0_data = ch0_q;
    assign ch1_data = ch1_q;
    assign ch2_data = ch2_q;
    assign sck = scki_q;
    assign data_valid = data_valid_q;
    assign sdi = sdi_q;
    
        initial begin
        $display("=== Parameters ===");
        $display("CLK_FREQ_HZ    = %0d", CLK_FREQ_HZ);
        $display("SAMPLES_RAW    = %0d", SAMPLES_RAW);
        $display("SAMPLE_BITS     = %0d", SAMPLE_BITS);
        $display("SAMPLES        = %0d", SAMPLES);
        $display("tCYC           = %0f ns", tCYC);
        $display("WINDOW_NS      = %0f ns", WINDOW_NS);
        $display("BUSY_TIMEOUT   = %0d ticks", BUSY_TIMEOUT);
        $display("CNV_HIGH_TICKS = %0d ticks", CNV_HIGH_TICKS);
        $display("QUIET_TICKS    = %0d ticks", QUIET_TICKS);
        $display("==================");
    end
endmodule