`timescale 1ns / 1ps

// parameters handle the sample total as a function 
module sr_driver_gen #(
    parameter int CLK_FREQ_HZ    = 12_000_000,  

    parameter int PULSE_TIME_US  = 2 ,             // S/R pulse duration
    parameter int SETTLE_TIME_US = 10,            // Post-pulse settling (5*tau)
    parameter int WINDOW_TIME_US = 2480,          // Sampling window duration
    parameter int GAP_TIME_US    = 20             // Off-period between phases
)(
    input  logic clk,
    input  logic rst,
    input logic  start,

    
    output logic set_sig,
    output logic reset_sig,

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

    state_t      state_q, state_d;
    logic [31:0] timer_q, timer_d;

    // Combinatorial next-state logic
    always_comb begin
        // Defaults
        state_d   = state_q;
        timer_d = start ? (timer_q + 1) : timer_q;
        set_sig   = 1'b0;
        reset_sig = 1'b0;
        sample_en = 1'b0;
        phase     = 1'b0;

        case (state_q)

            ST_IDLE: begin
                if (timer_q >= PERIOD_TICKS - 1) begin
                    state_d = ST_SET_GAP;
                    timer_d = '0;
                end
            end

            ST_SET_GAP: begin
                // Both signals low - off period before Set pulse
                
                if (timer_q >= GAP_TICKS - 1) begin
                    state_d = ST_SET_PULSE;
                    timer_d = '0;
                end
            end

            ST_SET_PULSE: begin
                // Assert Set signal for PULSE_TICKS
                set_sig = 1'b1;
               
                
                if (timer_q >= PULSE_TICKS - 1) begin
                    state_d = ST_SET_SETTLE;
                    timer_d = '0;
                end
            end

            ST_SET_SETTLE: begin
                set_sig = 1'b1;
                
                // Both signals low - wait for current spike to decay (5*tau)
                if (timer_q >= SETTLE_TICKS - 1) begin
                    state_d = ST_SET_WINDOW;
                    timer_d = '0;
                end
            end

            ST_SET_WINDOW: begin
                // Sampling window - ADC controller captures samples here
                 sample_en = 1'b1;
                 set_sig = 1'b1;
                 phase     = 1'b0;  // SET phase
                if (timer_q >= (WINDOW_TICKS-SETTLE_TICKS-PULSE_TICKS) - 1) begin
                    state_d = ST_RST_GAP;
                    timer_d = '0;
                end
            end

            ST_RST_GAP: begin
                // Both signals low - off period before Reset pulse
                
                if (timer_q >= GAP_TICKS - 1) begin
                    state_d = ST_RST_PULSE;
                    timer_d = '0;
                end
            end

            ST_RST_PULSE: begin
                // Assert Reset signal for PULSE_TICKS
                reset_sig = 1'b1;
                if (timer_q >= PULSE_TICKS - 1) begin
                    state_d = ST_RST_SETTLE;
                    timer_d = '0;
                end
            end

            ST_RST_SETTLE: begin
                // Both signals low - wait for current spike to decay (5*tau)
                reset_sig = 1'b1;
                if (timer_q >= SETTLE_TICKS - 1) begin
                    state_d = ST_RST_WINDOW;
                    timer_d = '0;
                end
            end

            ST_RST_WINDOW: begin
                // Sampling window - ADC controller captures samples here
                sample_en = 1'b1;
                reset_sig = 1'b1;
                phase     = 1'b1;  // RESET phase
                if (timer_q >= (WINDOW_TICKS-SETTLE_TICKS-PULSE_TICKS) - 1) begin
                    state_d = ST_SET_GAP;
                    timer_d = '0;
                end
            end

            default: begin
                state_d = ST_IDLE;
                timer_d = '0;
            end

        endcase
    end

    // -------------------------------------------------------------------------
    // Sequential block
    // -------------------------------------------------------------------------
    always_ff @(posedge clk or posedge rst) begin
        if (rst) begin
            state_q <= ST_IDLE;
            timer_q <= '0;
        end else begin
            state_q <= state_d;
            timer_q <= timer_d;
        end
    end

endmodule