module trigger_x   
    (
        input clk,
        input reset,
        input set,
        output reg out
    );

    always @(posedge clk)
    begin
        if (reset) begin
            out <= 0;
        end else begin
            if (set) begin
                out <= 1;
            end
        end
    end
endmodule