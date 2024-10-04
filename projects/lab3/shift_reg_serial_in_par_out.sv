module shift_reg_serial_in_par_out # (parameter M = 5)
    (
        input clk,
        input reset,
        input bit_in,
        input shift,
        output reg [M-1:0] byte_out
    );

    always @(posedge clk)
    begin
        if (reset) begin
            byte_out <= 0;
        end else begin
            if (shift) begin
                byte_out <= {bit_in, byte_out[M-1:1]};
            end
        end
    end
endmodule