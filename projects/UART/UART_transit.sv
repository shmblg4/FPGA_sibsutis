module UART_transit
    (
        input wire bit_in,
        input wire clk,
        input wire reset,
        output wire bit_out,
        output wire [7:0] led_bus
    );

    wire [7:0] byte_out1;
    wire [7:0] byte_out2;

    wire ready1;
    wire ready2;
	 
    wire clk_4;

    assign led_bus = byte_out2;

    my_pll pll (
        .inclk0(clk),
        .areset(1'b0),
        .c0(clk_4),
        .locked()
    );
	 
    UART_reciever receiver (
        .clk(clk_4), 
        .reset(reset),
        .bit_in(bit_in), 
        .byte_out(byte_out1),            // Change: consistent named connection
        .ready_out(ready1)
    );

    UART_buffer buffer (
        .clk(clk_4), 
        .reset(reset), 
        .enable(ready1),
        .byte_in(byte_out1), 
        .byte_out(byte_out2), 
        .ready(ready2)
    );

    UART_transmitter transmitter (
        .clk(clk_4), 
        .reset(reset), 
        .enable(ready2),
        .byte_in(byte_out2), 
        .bit_out(bit_out)
    );

endmodule
