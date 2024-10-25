module UART_transmitter
    (
        input reset,
        input clk,
        input reg [7:0] byte_in,
        input enable,
        output bit_out
    );

reg [2:0] cnt_2;
reg [4:0] cnt_3;

logic sw;
logic tr_next;

wire shift_wire;
wire set_wire;
wire uart_0;
wire ready_out;
wire next_byte;

initial begin
    cnt_2 = 0;
    cnt_3 = 0;
end

shift_reg_par_in_serial_out # ( .M(8)) par_in
    (
        .clk(clk),
        .reset(reset),
        .bus_in(byte_in),
        .set(set_wire),
        .shift(shift_wire),
        .bit_out(uart_0)
    );

assign shift_wire = (cnt_2 == 3);
assign set_wire = (enable == 1 && sw == 0);
assign bit_out = sw ? uart_0 : 1;
assign ready_out = sw;
assign next_byte = tr_next;

always @(posedge clk) begin
    if (reset) begin
        cnt_2 <= 0;
        cnt_3 <= 0;
        sw <= 0;
        tr_next <= 0;
    end else begin
        if (enable == 1 && sw == 0) begin
            sw <= 1;
            cnt_2 <= 0;
            tr_next <= 1;
        end
        if (sw == 1) begin
            cnt_2 <= cnt_2 + 1;
            tr_next <= 0;
            if (cnt_2 == 3) begin
                cnt_2 <= 0;
                cnt_3 <= cnt_3 + 1;
            end
            if (cnt_3 == 8) begin
                sw <= 0;
                cnt_3 <= 0;
            end
        end
    end
end

endmodule