// swap bit order (e.g. for tkeep reversal)

module bit_swap #(
    parameter N = 32
)(
    input [N - 1:0] in,
    output [N - 1:0] out
);

genvar i;
generate for (i = 0; i < N; i = i + 1) begin : assign_bit
    assign out[i] = in[N - 1 - i];
end endgenerate

endmodule
