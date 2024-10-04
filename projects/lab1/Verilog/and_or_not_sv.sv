module and_or_not_sv
	(
	input [1:0] key,
	output [2:0] led
	);
	
		wire a = ~key[0];
		wire b = ~key[1];
		
		assign led[0] = ~a;
		assign led[1] = a|b;
		assign led[2] = a&b;
		
endmodule