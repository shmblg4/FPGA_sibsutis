module shift_reg_par_in_serial_out # (parameter M = 5)
    (
        input clk,
        input reset,
        input [M-1:0] bus_in,
        input set,
        input shift,
        output bit_out
    );

    reg [M-1:0] register;
    assign bit_out = register[0];

    always @(posedge clk) begin
        if (reset) begin
            register <= 0;
        end else begin
            if (set) begin
                register <= bus_in;
            end else begin
                if (shift) begin
                    register <= {1'b1, register[M-1:1]};
                end
            end
        end
    end
endmodule
