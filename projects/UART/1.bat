vlog -work work. testbench_UART.sv
vsim -gui work.testbench -do "add wave sim:/testbench/*;add wave sim:/testbench/UART_reciever/*; run -all"