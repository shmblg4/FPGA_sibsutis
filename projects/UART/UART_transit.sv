module UART_transit
	(
        input sb0,
		input wire bit_in,
        input clk,
        output wire bit_out,
        output wire [7:0] led_bus
	);

	logic areset;
	assign areset = ~ sb0;
	reg reset;
	always @ (posedge clk)   reset <= areset;
   
	wire [7:0] byte_out1;
    wire [7:0] byte_out2;

    wire ready1;
    wire ready_buf;
	wire xxx;
	wire enbl = ready_buf && ~ xxx;

    assign led_bus = byte_out2;
	wire clk_4;

	my_pll pll_inst
    (
        .areset(1'b0),
        .inclk0(clk),
        .c0(clk_4),
        .locked() 
    );

    UART_reciever reciever (
		.reset(reset), 
		.clk(clk_4),
		.bit_in(bit_in), 
		.byte_out(byte_out1),
		.ready_out(ready1)
	);

	UART_buffer buffer (
		.clk(clk_4), 
		.reset(reset), 
		.enable(ready1),
		.byte_in(byte_out1), 
		.byte_out(byte_out2), 
		.ready(ready_buf)
	);
	
	UART_transmitter transmitter (
		.reset(reset), 
		.clk(clk_4), 
		.byte_in(byte_out2), 
		.enable(enbl),
		.bit_out(bit_out),
		.busy(xxx)
	);
    
endmodule
