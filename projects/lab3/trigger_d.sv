module trigger_d   
   (input clk,
    input reset,
    input set,
    output reg out
    );
    always @(posedge clk) begin
        if (reset) begin
            out <= 0;
        end else begin
                out <= set;
        end
    end
endmodule