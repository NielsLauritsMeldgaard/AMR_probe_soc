`timescale 1ns / 1ps
//////////////////////////////////////////////////////////////////////////////////
// Company: 
// Engineer: 
// 
// Create Date: 27.05.2026 08:24:18
// Design Name: 
// Module Name: dac_controller
// Project Name: 
// Target Devices: 
// Tool Versions: 
// Description: 
// 
// Dependencies: 
// 
// Revision:
// Revision 0.01 - File Created
// Additional Comments:
//  [23:20] = command, [19:16] = channel address, [15:0] = data. 
//////////////////////////////////////////////////////////////////////////////////


module dac_controller(
    input logic clk,
    input logic rst,
    
    // inputs to dac 
    output logic reset_n,
    output logic ldac_n,
    
    input logic        send,
    input logic [15:0] data_a, // value for DAC A
    input logic [15:0] data_b, // value for DAC B
    input logic [15:0] data_c, // value for DAC C
    output logic busy, // high while transfer in progress
    // SPI signals
    output logic sclk,
    output logic sdin,
    output logic sync_n
    );
        // local parameters
        localparam logic [3:0] CMD  = 4'b0001; // CMD_WRITE_IN always
        localparam int TOTAL_BITS   = 24; // 3 channels  24 bits
        localparam int TOTAL_FRAMES = 3;   // count of the frames
        
        // states for fsm 
        typedef enum logic [2:0] {
            ST_IDLE, ST_CLK_LO, ST_CLK_HI, ST_LDAC_PULSE
        } state_t;

        // register wires
        state_t      state_q, state_d;
        logic [71:0] shift_reg_q, shift_reg_d ; // {frame_A, frame_B, frame_C} - A sent first
        logic [6:0]  bit_cnt_d, bit_cnt_q; // frame bit counter
        logic        frame_cnt_d, frame_cnt_q;
        
        always_comb begin
            //defaults
                // registers
            shift_reg_d = shift_reg_q;
            state_d     = state_q;

            
                // counters
            bit_cnt_d = bit_cnt_q; //
            frame_cnt_d = frame_cnt_q;
                // wires
            sclk   = 0;
            ldac_n = 1;
            sync_n = 1;
            sdin   = 0;
            busy   = 0;
            case (state_q)
               
                ST_IDLE: begin 
                    
                    if (send) begin 
                        shift_reg_d = {CMD, 4'b0001, data_a,
                                       CMD, 4'b0010, data_b,
                                       CMD, 4'b0100, data_c};
                        bit_cnt_d = 0;
                        busy = 1;
                        sync_n = 0;
                        sclk   = 0;           // start with SCLK low
                        sdin   = 0;
                        state_d = ST_CLK_LO;
                    end
                           
                end
                
                ST_CLK_LO: begin
                    busy    = 1;
                    sclk    = 0;
                    sync_n  = 0;
                    sdin    = shift_reg_q[71];
                    state_d = ST_CLK_HI;
                   
                    
                end
                
                ST_CLK_HI: begin
                    busy = 1;
                    sclk = 1;
                    if (frame_cnt_q == TOTAL_FRAMES -1) begin
                        frame_cnt_d = 0;
                        sync_n      = 1;
                        bit_cnt_d   = 0;
                        state_d     = ST_LDAC_PULSE;
                    end else if (bit_cnt_q == TOTAL_BITS - 1) begin
                            // all 72 bits done - raise SYNC on next cycle
                       frame_cnt_d = frame_cnt_q + 1;
                       sync_n      = 1;         // all 3 input registers latch here
                       bit_cnt_d   = 0;         // reset bit counter
                       state_d     = ST_CLK_LO; // go to pulse ldac state
                       shift_reg_d = shift_reg_q << 1;
                    end else begin
                       bit_cnt_d   = bit_cnt_q + 1; 
                       shift_reg_d = shift_reg_q << 1; // shift register 1 bit to left
                       state_d     = ST_CLK_LO;
                    end 
                end
                
                ST_LDAC_PULSE: begin
                    sclk    = 0;             // SCLK back to idle
                    ldac_n  = 0;      // low for exactly one cycle
                    busy  = 0;
                    state_d = ST_IDLE;   // next cycle IDLE drives ldac_n back to 1
                end
                        
                   
                    
            endcase
            
        end
        
        
        always_ff @(posedge clk or posedge rst) begin
             if (rst) begin
                shift_reg_q <= 0;
                state_q     <= ST_IDLE;
                bit_cnt_q   <= 0;
                frame_cnt_q <= 0;
             end else begin
                shift_reg_q <= shift_reg_d;
                state_q     <= state_d;
                bit_cnt_q   <= bit_cnt_d;
                frame_cnt_q <= frame_cnt_d;
             end
        end
        assign reset_n = 1;
endmodule
