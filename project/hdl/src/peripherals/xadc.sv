`timescale 1ns / 1ps

module xadc_unit (
    input  logic clk,

    input  logic vauxp4,
    input  logic vauxn4,

    output logic [11:0] adc_value
);

logic [15:0] do_out;
logic [4:0]  channel_out;
logic        eoc_out;
logic        drdy_out;

logic        den_in;

xadc_wiz_0 xadc_inst (
    .di_in(16'h0000),
    .daddr_in(7'h14), // VAUX4 data register

    .den_in(den_in),
    .dwe_in(1'b0),

    .drdy_out(drdy_out),
    .do_out(do_out),

    .dclk_in(clk),

    .vp_in(1'b0),
    .vn_in(1'b0),

    .vauxp4(vauxp4),
    .vauxn4(vauxn4),

    .channel_out(channel_out),
    .eoc_out(eoc_out),

    .alarm_out(),
    .eos_out(),
    .busy_out()
);

always_ff @(posedge clk)
begin
    den_in <= eoc_out;

    if (drdy_out)
        adc_value <= do_out[15:4];
end

endmodule