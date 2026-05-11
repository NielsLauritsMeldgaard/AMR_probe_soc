`timescale 1ns / 1ps

module seven_seg_controller (
    input  logic        clk, rst,
    input  logic [31:0] data_i,    // 32-bit data from CPU (e.g., 4x8-bit hex codes)
    input  logic        we,        // Write enable from io_manager
    output logic [6:0]  segments,  // Physical pins: A, B, C, D, E, F, G, DP
    output logic [3:0]  anodes     // Physical pins: Digit selector
);
    // Placeholder for internal register
    logic [31:0] display_reg;

    always_ff @(posedge clk) begin
        if (rst) display_reg <= 32'h0;
        else if (we) display_reg <= data_i;
    end
    
    // Initialise the seven seg driver    
    seven_seg_driver driver(
        .clk(clk),
        .rst(rst),
        .data(display_reg[13:0]),   // 14-bit data input, FROM SYSTEM (MAX 9999)
        .dual(0),                   // Hardcode to single display mode (use all 4 seven segs to represent full data)
        .data2(0),                  // N/A (hardcoded in single display mode)
        .segment(segments),
        .anode(anodes)
    );
    
endmodule

// Driver for the basys 3 boards seven segments displays
module seven_seg_driver (
    input logic clk,
    input logic rst,

    input logic [13:0] data,            // 14-bit data input, FROM SYSTEM (MAX 9999)
    input logic dual,                   // Dual display mode
    input logic [6:0] data2,            // 7-segment display data for second display

    output logic [6:0] segment,         // 7-segment display output
    output logic [3:0] anode            // Anode control for display
);

    localparam COUNTER_MAX = 32'd50000;

    // Internal signals
    logic [31:0] counter;
    logic [1:0] anodeSelect;
    logic [3:0] bcd_data;



    // COUNTER STAGE
    always_ff @(posedge clk or posedge rst) 
    begin
        if (rst) begin
            counter <= 0;
            anodeSelect <= 0;   
        end else begin
            if (counter == COUNTER_MAX) begin
                counter <= 0;
                anodeSelect <= (anodeSelect == 3) ? 0 : anodeSelect + 1;
            end else begin
                counter <= counter + 1;
            end
        end
    end



    // MULTIPLEXER STAGE
    always_comb 
    begin
        case (anodeSelect)
            2'b00: begin
                anode = 4'b0111;
                if (dual) begin
                    bcd_data = (data2 % 100) / 10; // Thousands place
                end else begin
                    bcd_data = data / 1000; // Thousands place
                end
            end
            2'b01: begin
                anode = 4'b1011;
                if (dual) begin
                    bcd_data = data2 % 10; // Units place
                end else begin
                    bcd_data = (data % 1000) / 100; // Hundreds place
                end
            end
            2'b10: begin
                anode = 4'b1101;
                bcd_data = (data % 100) / 10; // Tens place
            end
            2'b11: begin
                anode = 4'b1110;
                bcd_data = data % 10; // Units place
            end
        endcase
    end



    // DECODE STAGE
    always_comb 
    begin
        case (bcd_data)
            4'b0000: segment = 7'b0000001;
            4'b0001: segment = 7'b1001111;
            4'b0010: segment = 7'b0010010;
            4'b0011: segment = 7'b0000110;
            4'b0100: segment = 7'b1001100;
            4'b0101: segment = 7'b0100100;
            4'b0110: segment = 7'b0100000;
            4'b0111: segment = 7'b0001111;
            4'b1000: segment = 7'b0000000;
            4'b1001: segment = 7'b0000100;
            default: segment = 7'b0000001;
        endcase
    end
endmodule
