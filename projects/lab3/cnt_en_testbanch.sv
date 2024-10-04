module testbench;
    localparam cnt_width = 5;

    reg clk;
    reg [cnt_width-1:0] cnt_taktov;
    reg [cnt_width-1:0] byte_out;

    wire [cnt_width-1:0] byte_1 = 8'h11;
    wire data;
    wire shift;
    wire reset = (cnt_taktov == 3);
    wire active;
    wire start = (cnt_taktov == 5);

    initial begin
        clk = 0;
        cnt_taktov = 0;
        $$display("test");
    end

    always
        #10 clk = ~clk;

    always @(posedge clk) begin
        cnt_taktov <= cnt_taktov + 1;
    end

    trigger_x tr (
        .clk(clk),
        .set(start),
        .reset(reset),
        .out(active)
    );

    trigger_d d2 (
        .clk(clk),
        .set(~clk_d),
        .reset(reset),
        .out(clk_d)
    );

    assign shift = active & clk_d;

    shift_reg_par_in_serial_out # (.M(cnt_width)) cnt_reg_par_ser
    (
        .clk(clk),
        .reset(reset),
        .bus_in(byte_1),
        .set(start),
        .shift(shift),
        .bit_out(data)
    );

    shift_reg_serial_in_par_out # (.M(cnt_width)) cnt_reg_ser_par
    (
        .clk(clk),
        .reset(reset),
        .bit_in(data),
        .shift(shift),
        .byte_out(byte_out)
    );

endmodule
