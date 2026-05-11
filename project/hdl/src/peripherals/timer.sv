`timescale 1ns / 1ps

module nerd_counter #(
    MAX_COUNT = 12, // 12 cycles at 12 MHz = 1us
    WIDTH =  32
)(
    input logic clk,
    input logic rst,
    input logic set,
    output logic [WIDTH-1:0] timer
);

    localparam COUNT_BITS = $clog2(MAX_COUNT);
    
    logic [COUNT_BITS:0] cnt_reg, cnt_reg_next;
    logic max_count_reached, reset, set_reg, set_reg_next;
    logic [WIDTH-1:0] timer_reg, timer_reg_next;
    
    // Check if cnt reg reached -1
    assign max_count_reached = cnt_reg[COUNT_BITS];
    // reset counter if rst high or rising edge on set
    assign reset = (rst || (!set_reg && set_reg_next));
    
    always_comb begin        
        cnt_reg_next = (max_count_reached ? MAX_COUNT-2 : cnt_reg-1);
        timer_reg_next = max_count_reached ? timer_reg + 1 : timer_reg;
        timer = timer_reg;
        set_reg_next = set;   
    end
    
    always_ff @( posedge clk or posedge reset ) begin
        if (reset) begin
            cnt_reg <= MAX_COUNT-2;
            timer_reg <= 0;
            set_reg <= 0;
        end else begin
            cnt_reg <= cnt_reg_next;
            timer_reg <= timer_reg_next;
            set_reg <= set_reg_next;
        end    
    end
    
endmodule
