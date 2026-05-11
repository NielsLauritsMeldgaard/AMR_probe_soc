`timescale 1ns / 1ps


module ps2_controller (
    input  logic        clk, rst,
    input  logic        ps2_clk,     // Physical Clock pin
    input  logic        ps2_data,    // Physical Data pin
    output logic [7:0]  key_code_o,  // Last scan code received
    output logic        data_ready_o // New key pressed pulse
);
    // TODO: Implement 11-bit shift register and parity check
    assign key_code_o   = 8'h1C;     // Constant 'A' key for testing
    assign data_ready_o = 1'b0;
endmodule
