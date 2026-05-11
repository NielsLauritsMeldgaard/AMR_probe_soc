`timescale 1ns / 1ps

module system_clock_gen (
    input  logic clk_in,    // 100 MHz Input
    output logic clk_1x,    // 100 MHz Output (CPU)
    output logic clk_2x,    // 200 MHz Output 
    output logic clk_ram,   // 200 MHz Output ( 180 deg shifted)
    output logic locked     
);

  logic clk_fb;

  `ifdef SYNTHESIS
  // PLLE2_BASE: Base Phase Locked Loop (PLL)
  PLLE2_BASE #(
     .CLKFBOUT_MULT(8),       // VCO = 800 MHz (100 * 8)
     .CLKIN1_PERIOD(10.0),    // Input is 100 MHz
     
     // CLK 0: 200 MHz (Standard)
     .CLKOUT0_DIVIDE(4),      
     .CLKOUT0_PHASE(0.0),
     
     // CLK 1: 100 MHz (CPU)
     .CLKOUT1_DIVIDE(8),      
     .CLKOUT1_PHASE(0.0),
     
     // CLK 2: 200 MHz (RAM - Shifted)
     .CLKOUT2_DIVIDE(4),      
     .CLKOUT2_PHASE(180.0)    // 
  )
  PLLE2_BASE_inst (
     // Outputs
     .CLKOUT0(clk_2x),    // 200 MHz (0 deg)
     .CLKOUT1(clk_1x),    // 100 MHz (0 deg)
     .CLKOUT2(clk_ram),   // 200 MHz (180 deg) 
     .CLKOUT3(),
     .CLKOUT4(),
     .CLKOUT5(),
     
     // Feedback
     .CLKFBOUT(clk_fb),
     .CLKFBIN(clk_fb),
     
     // Control
     .LOCKED(locked),     
     .CLKIN1(clk_in),
     .PWRDWN(1'b0),
     .RST(1'b0)
  );
  `else
  // For simulation, bypass PLL - just pass through input clock
  assign clk_1x = clk_in;
  assign clk_2x = clk_in;  // In sim, use same clock
  assign clk_ram = clk_in;
  assign locked = 1'b1;    // Always locked in simulation
  `endif

endmodule