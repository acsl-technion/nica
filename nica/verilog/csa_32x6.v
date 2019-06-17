module csa_32x6(
                input wire [31:0]  in1,
                input wire [31:0]  in2,
                input wire [31:0]  in3,
                input wire [31:0]  in4,
                input wire [31:0]  in5,
                input wire [31:0]  in6,
                output wire [31:0] sum,
                output wire [31:0] carry
                );

  wire [31:0] 			   sum1;
  wire [31:0] 			   sum2;
  wire [31:0] 			   sum3;
  wire [31:0] 			   carry1;
  wire [31:0] 			   carry2;
  wire [31:0] 			   carry3;
  
csa_32x3 csa_32x3_1(
		    .in1(in1),
		    .in2(in2),
		    .in3(in3),
		    .sum(sum1),
		    .carry(carry1)
		    );

csa_32x3 csa_32x3_2(
		    .in1(in4),
		    .in2(in5),
		    .in3(in6),
		    .sum(sum2),
		    .carry(carry2)
		    );

csa_32x3 csa_32x3_3(
		    .in1(sum1),
		    .in2(sum2),
		    .in3({carry1[30:0], 1'b0}),
		    .sum(sum3),
		    .carry(carry3)
		    );

csa_32x3 csa_32x3_4(
		    .in1(sum3),
		    .in2({carry2[30:0], 1'b0}),
		    .in3({carry3[30:0], 1'b0}),
		    .sum(sum),
		    .carry(carry)
		    );

endmodule // csa_32x6
  
