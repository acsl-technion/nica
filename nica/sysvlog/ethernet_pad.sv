// pad Ethernet frames to 60 bytes (minimum size without FCS)

interface axi_vif #(
    parameter integer C_AXI_DATA_BYTES = 32
)();

wire tvalid;
wire tready;
wire [C_AXI_DATA_BYTES * 8 - 1 : 0] tdata;
wire [C_AXI_DATA_BYTES - 1 : 0] tkeep;
wire tlast;
// wire [2:0] tid;
// wire [11:0] tuser;

modport master (
    output tvalid,
    input tready,
    output tdata, tkeep, /* tid, tuser */ tlast
);

modport slave (
    input tvalid,
    output tready,
    input tdata, tkeep, /* tid, tuser */ tlast
);

endinterface : axi_vif

module ethernet_pad(
    input clk,
    input reset,
    axi_vif.slave in,
    axi_vif.master out
);

assign out.tvalid = in.tvalid;

reg [1:0] count_flits;
wire strobe = in.tvalid && in.tready;
wire pad = count_flits == 1 && in.tlast && strobe;

always @(posedge clk) begin
    if (reset) begin
        count_flits <= 0;
    end else begin
        if (strobe) begin
            if (in.tlast) begin
                count_flits <= 0;
            end else begin
                if (count_flits < 2)
                    count_flits <= count_flits + 1;
            end
        end
    end
end

assign out.tvalid = in.tvalid;
assign in.tready = out.tready;

wire [255:0] pad_mask;
genvar i;
generate for (i = 0; i < 32; i = i + 1) begin : gen_pad_mask
    assign pad_mask[i * 8 + 7 : i * 8] = (pad & ~in.tkeep[i]) ? 8'h00 : 8'hff;
end endgenerate
assign out.tdata = pad_mask & in.tdata;

wire [31:0] pad_keep_mask = 32'h0fffffff;
assign out.tkeep = in.tkeep | (pad ? pad_keep_mask : 32'h0);

assign out.tlast = in.tlast;
// assign out.tid = in.tid;
// assign out.tuser = in.tuser;

endmodule : ethernet_pad
