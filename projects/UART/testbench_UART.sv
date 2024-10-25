module testbench;
    localparam cnt_width = 8;

    reg clk;
    reg [cnt_width-1:0] counter;

    wire [cnt_width-1:0] byte_out;
    wire ready_out;

    initial begin
        clk = 0;
        counter = 0;
    end

    always @(posedge clk) begin
        counter <= counter + 1;
        if (counter == 50) begin
            $stop;
        end
    end

    always
        #10 clk = ~clk;

    wire reset = (counter == 3);
    wire start = (counter == 5);
    wire active;
    reg [52-1:0]test_in = 52'b11111111_0000_1111_0000_0000_1111_0000_1111_0000_1111_11111111; //10010101

    trigger_x trigger_x (
        .clk(clk), .set(start),
        .reset(reset), .out(active)
    );

    always @(posedge clk) begin
        if (active) begin
            test_in <= {test_in[52-1:0], 1'b0};
        end
    end

    assign bit_in = test_in[52-1];

    UART_reciever # ( .N(cnt_width)) UART_reciever
    (
        .reset(reset),
        .clk(clk),
        .bit_in(bit_in),
        .byte_out(byte_out),
        .ready_out(ready_out)
    );

endmodule