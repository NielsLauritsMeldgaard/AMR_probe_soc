`timescale 1ns / 1ps

// parameters handle the sample total as a function 
module sr_controller #(
    parameter int CLK_FREQ_HZ    = 12_000_000,  

    parameter int PULSE_TIME_US  = 2 ,             // S/R pulse duration
    parameter int SETTLE_TIME_US = 1000,            // Post-pulse settling 
    parameter int WINDOW_TIME_US = 2480,          // Sampling window duration
    parameter int GAP_TIME_US    = 20             // Off-period between phases
)(
    // Board clock and reset
    input  logic clk,                   
    input  logic rst,
    
    input logic  start,  // signal to start the full magnetic sensing system
    output logic set_sig,   // // Set pulse to SR-driver MOSFET's
    output logic reset_sig, // Reset pulse to SR-driver MOSFET's

    // To ADC controller and magnetometer processor
    output logic sample_en,    // High during valid sampling window
    output logic phase         // 0 = SET phase, 1 = RESET phase
);


    // Timing constants (all in clock ticks)
    localparam int TICKS_PER_US     = CLK_FREQ_HZ / 1_000_000;
    localparam int PULSE_TICKS      = PULSE_TIME_US   * TICKS_PER_US;
    localparam int SETTLE_TICKS     = SETTLE_TIME_US  * TICKS_PER_US;
    localparam int WINDOW_TICKS     = WINDOW_TIME_US  * TICKS_PER_US;
    localparam int GAP_TICKS        = GAP_TIME_US     * TICKS_PER_US;

  
    localparam int HALF_PERIOD_TICKS = GAP_TICKS +  WINDOW_TICKS;
    localparam int PERIOD_TICKS      = 2 * HALF_PERIOD_TICKS ;
    

   
    // State machine
    typedef enum logic [3:0] {
        ST_IDLE,        // Initial startup delay - wait one full period
        ST_SET_GAP,     // Off-period before Set pulse
        ST_SET_PULSE,   // Set pulse active
        ST_SET_SETTLE,  // Post-pulse settling
        ST_SET_WINDOW,  // ADC sampling window (SET phase)
        ST_RST_GAP,     // Off-period before Reset pulse
        ST_RST_PULSE,   // Reset pulse active
        ST_RST_SETTLE,  // Post-pulse settling
        ST_RST_WINDOW   // ADC sampling window (RESET phase)
    } state_t;

    // FSM state register wires and timer register wires.
    state_t      state_q, state_d;
    logic [31:0] timer_q, timer_d;

    // Combinatorial next-state logic
    always_comb begin
        // Defaults
        state_d   = state_q;
        timer_d = start ? (timer_q + 1) : timer_q;  //only start system if start is high
        set_sig   = 1'b0;
        reset_sig = 1'b0;
        sample_en = 1'b0;
        phase     = 1'b0;

        case (state_q)
            ST_IDLE: begin
            // IDLE state allows for chargepump circuit to charge before firing.
            // wiats 5ms then jumps to SET_GAP state
                if (timer_q >= PERIOD_TICKS - 1) begin
                    state_d = ST_SET_GAP;
                    timer_d = '0;
                end
            end

            ST_SET_GAP: begin
                // Both signals low - off period before Set pulse
                //defined by the specs in AN201 from honeywell inc.
                if (timer_q >= GAP_TICKS - 1) begin
                    state_d = ST_SET_PULSE;
                    timer_d = '0;
                end
            end

            ST_SET_PULSE: begin
                // Assert Set signal for PULSE_TICKS
                // signal is sent to SR-driver; a 3A current spike is sent to HMC1001 strap
                // this period is just as long as the pulse period. 
                //made explicitly for debugging
                set_sig = 1'b1;
                if (timer_q >= PULSE_TICKS - 1) begin
                    state_d = ST_SET_SETTLE;
                    timer_d = '0;
                end
            end

            ST_SET_SETTLE: begin
                set_sig = 1'b1;
                // wait for current spike to decay before sampling
                // to ensure correct magnetic alignment in the wheatstone bridge
                // again made explicitly for debugging
                if (timer_q >= SETTLE_TICKS - 1) begin
                    state_d = ST_SET_WINDOW;
                    timer_d = '0;
                end
            end

            ST_SET_WINDOW: begin
                // Sampling window - ADC controller captures samples here
                 sample_en = 1'b1; // sample_en goes high to tell the ADC_controller to start capturing
                 set_sig = 1'b1;
                 phase     = 1'b0;  // SET phase - for mag_datapath to differentiate between SET and RESET samples
                if (timer_q >= ((WINDOW_TICKS-SETTLE_TICKS)-PULSE_TICKS) - 1) begin
                    state_d = ST_RST_GAP;
                    timer_d = '0;
                end
            end

            ST_RST_GAP: begin
                // Both signals low - off period before Reset pulse
                // To ensure both MOSFETS are closed so to not short the SR-driver circuitry
                if (timer_q >= GAP_TICKS - 1) begin
                    state_d = ST_RST_PULSE;
                    timer_d = '0;
                end
            end

            ST_RST_PULSE: begin
                // Assert Reset signal for PULSE_TICKS
                // similar to SET_PULSE state
                reset_sig = 1'b1;
                if (timer_q >= PULSE_TICKS - 1) begin
                    state_d = ST_RST_SETTLE;
                    timer_d = '0;
                end
            end

            ST_RST_SETTLE: begin
                // Both signals low - wait for current spike to decay 
                // similar to SET_SETTLE state
                reset_sig = 1'b1;
                if (timer_q >= SETTLE_TICKS - 1) begin
                    state_d = ST_RST_WINDOW;
                    timer_d = '0;
                end
            end

            ST_RST_WINDOW: begin
                // Sampling window - ADC controller captures samples here
                // similar to SET_WINDOW state
                sample_en = 1'b1; 
                reset_sig = 1'b1;
                phase     = 1'b1;  // RESET phase - mag_datapath knows these samples are RESET samples
                if (timer_q >= ((WINDOW_TICKS-SETTLE_TICKS)-PULSE_TICKS) - 1) begin
                    state_d = ST_SET_GAP;
                    timer_d = '0;
                end
            end

            default: begin
            // default case to ensure Deterministic hardware logic.
                state_d = ST_IDLE;
                timer_d = '0;
            end

        endcase
    end

    // -------------------------------------------------------------------------
    // Sequential block
    // -------------------------------------------------------------------------
    always_ff @(posedge clk or posedge rst) begin
        // flip-flop registers change on system clk.
        if (rst) begin
            state_q <= ST_IDLE;
            timer_q <= '0;
        end else begin
            state_q <= state_d;
            timer_q <= timer_d;
        end
    end

endmodule