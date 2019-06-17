// swap bytes for endianess change

module byte_swap #(
    parameter N = 32
)(
    input [8 * N - 1:0] in,
    output [8 * N - 1:0] out
);

genvar i;
generate for (i = 0; i < N; i = i + 1) begin : assign_byte
    assign out[8 * i + 7:8 * i] = in[8 * (N - i) - 1 : 8 * (N - 1 - i)];
end endgenerate

endmodule
