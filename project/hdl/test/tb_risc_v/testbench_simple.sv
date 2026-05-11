  `timescale 1ns / 1ps

 module tb_datapath_simple();

    logic clk, rst;
    
    // Simple clock/reset
    initial begin
         clk = 0;
         forever #5 clk = ~clk;
    end
    
    initial begin
        rst = 1'b0;
        #10;             // hold reset low
        rst = 1'b1;      // assert reset
        #10;             // hold reset
        rst = 1'b0;      // deassert reset    
    end
    
    // DUT
    datapath #(.MEM_WORDS(128)) dut (
        .clk(clk),
        .rst(rst)
    );
     
 endmodule
