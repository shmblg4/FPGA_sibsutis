module cnt_pll
(
    input rst, stop,
    input clk_100,
    output [7:0] led_bus
    );
    logic clk_x;
    assign arst = ~ rst;
    reg reset;
    always @ (posedge clk_100)
    reset <= arst ;

    mypll pll_inst
    ( 
    .inclk0 (clk_100),
    .c0 (clk_x)
    );
    wire [17:0] cnt_bus;

    counter_enbl # (.N(18)) cnt
    (
    .clk (clk_x),
    .reset (reset),
    .enable (stop),
    .counter (cnt_bus)
    );
    assign led_bus = cnt_bus [17:10];
endmodule