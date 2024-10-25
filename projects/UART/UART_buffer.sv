module UART_buffer
    (
        input clk,
        input reset,
        input enable,
        input wire [7:0] byte_in,
        output reg [7:0] byte_out,
        output reg ready
    );

always @(posedge clk) begin
    if (reset) begin
        byte_out <= '0;
        ready <= 0;
    end else begin
        if (enable) begin
            byte_out <= byte_in;
            ready <= 1;
        end
    end
end
endmodule
