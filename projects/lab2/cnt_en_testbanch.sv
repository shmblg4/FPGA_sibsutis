//Тестируем счетчик с разрешением счета
//`timescale 1ns / 100ps                           //
module cnt_en_testbanch;                       // модуль не имеет входных и выходных параметров

localparam cnt_width = 4;                      // задаем разрядность счетчика равную 4

reg clk;                                                   // объявляем тип переменной clk
reg [cnt_width:0] cnt_taktov;                //счетчик для генерации управляющих сигналов

logic enable;                                         //Вход тестируемого модуля (разрешение счета сумматора)
logic [cnt_width-1:0] out_counter;       //Выход тестируемого модуля (счетчика)

initial begin                                           // Сброс счетчика тактов перед началом работы
    clk = 0;
    cnt_taktov = 0;
end

always
  #10 clk = ~ clk;                                 //задаем тактовый сигнал с периодом 20 единиц времени

always @(posedge clk) begin
    cnt_taktov <= cnt_taktov + 1;          // счетчик тактов для задания управляющих сигналов
end
// Управляющие сигналы
wire reset = (cnt_taktov == 3);          // сигнал установки триггера активного состояния в 0
wire start = (cnt_taktov == 5);          // сигнал установки триггера активного состояния в 1
wire active;                                       //сигнал разрешения работы счетчика

wire pause = (cnt_taktov == 10) || (cnt_taktov == 15);    //сигналы остановки счета

//Триггер активного состояния
trigger_x  tr ( 
    .clk(clk),  .reset(reset),                //сброс осуществляется только по сигналу  reset
    .set(start),  .out(active));

assign enable =  active & ~ pause; //формирование сигнала разрешения счета

//Тестируемые модули ______________________________________

counter_enbl  # ( .N(cnt_width)) cnt_inst 
    (
    .clk(clk),
    .reset(reset),
    .enable(enable),
    .counter(out_counter)
    );
//________________________________________________________
 
endmodule