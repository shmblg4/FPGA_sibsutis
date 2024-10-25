module UART_reciever
    (
        input reset,
        input clk,
        input bit_in,
        output [7:0] byte_out,
        output ready_out
    );

reg [5:0] clk_1;
reg [5:0] clk_2;
reg [5:0] clk_3;

logic state;
logic read;
wire shift_wire;

assign shift_wire = (clk_2 == 3);
assign ready_out = read;

initial begin
    clk_1 = 0;
    clk_2 = 0;
    clk_3 = 0;
end

shift_reg_serial_in_par_out #(.M(8)) shift_reg (
    .clk(clk), .reset(reset),
    .bit_in(bit_in), .byte_out(byte_out),
    .shift(shift_wire)
);

assign ready_out = read;

always @(posedge clk)
begin
    if (reset) begin
        clk_1 <= 0;
        clk_2 <= 0;
        clk_3 <= 0;
        read <= 0;
    end else begin
        if (bit_in == 0 || state == 1) begin
            clk_1 <= clk_1 + 1;
            if (clk_1 == 35) begin
                state <= 0;
                read <= 0;
                clk_1 <= 0;
            end else begin
                if (clk_1 == 2) begin
                    state <= 1;
                    read <= 0;
                    clk_2 <= 0;
                end else begin
                    clk_2 <= clk_2 + 1;
                    if (clk_2 == 3) begin
                        clk_2 <= 0;
                        if (clk_3 == 7) begin
                            read <= 1;
                            clk_3 <= 0;
                        end else begin
                            clk_3 <= clk_3 + 1;
                        end
                    end
                end
            end
        end
    end
end
endmodule