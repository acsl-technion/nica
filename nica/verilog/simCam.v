// Simple cam for simulation purposes

module simCam(
	      input wire 	  clk,
	      input wire 	  rst,
		
	      input wire 	  lup_req_valid,
	      output wire 	  lup_req_ready,
	      input wire [97:0]   lup_req_din,     // lookupsource(1) + key(96) = 97 bits (lup_req_din[97] is not used)
	                                           // key == {myIP(32), theirIP(32), myport(16), theirport(16)}
                                                   // lookupsource == RX | TX_APP
	      output wire 	  lup_rsp_valid,
	      input wire 	  lup_rsp_ready,
	      output wire [15:0]  lup_rsp_dout,    // lookupsource(1), + sessionID(14) + hit = 16 bits
			
	      input wire 	  upd_req_valid,
	      output wire 	  upd_req_ready,
	      input wire [111:0]  upd_req_din,     // lookupsource(1) + lookupOp(1) + SessionID(14) + key(96) = 112 bits
	                                           // lookupOp == INSERT | DELETE
			
	      output wire 	  upd_rsp_valid,
	      input wire 	  upd_rsp_ready,
	      output wire [15:0]  upd_rsp_dout,    // lookupSource(1), lookupOp(1) + sessionID(14) = 16 bits
			
	      // tbd: Verify these pins direction
	      output wire 	  led0,
	      output wire 	  led1,
	      output wire 	  cam_ready,
	      output wire [255:0] debug
 );

localparam
  IDLE         = 2'd0,
  LUP_CHK      = 2'd1,
  LUP_WAIT     = 2'd2,
  LUP_RSP      = 2'd3,
  UPD_WR       = 2'd1,
  UPD_WAIT     = 2'd2,
  UPD_RSP      = 2'd3;

  reg [97:0] 			  tag0;
  reg [15:0] 			  data0;
  reg 				  data0_valid;

  reg [1:0] 			  lup_state;
  reg [1:0] 			  upd_state;
  
  reg 				  lup_req_readyQ;
  reg 				  lup_rsp_validQ;
  reg [15:0] 			  lup_rsp_doutQ;

  reg 				  upd_req_readyQ;
  reg 				  upd_rsp_validQ;
  reg [15:0] 			  upd_rsp_doutQ;
  reg 				  cam_hit;
  reg 				  lup_source;
  reg 				  upd_source;
  reg 				  upd_op;
  
  assign lup_req_ready = lup_req_readyQ;
  assign lup_rsp_valid = lup_rsp_validQ;
  assign lup_rsp_dout = lup_rsp_doutQ;
  assign upd_req_ready = upd_req_readyQ;
  assign upd_rsp_valid = upd_rsp_validQ;
  assign upd_rsp_dout = upd_rsp_doutQ;

				  
  always @(posedge clk) begin
    if (rst) begin
      lup_req_readyQ <= 1'b1;
      lup_rsp_validQ <= 1'b0;
      upd_req_readyQ <= 1'b1;
      upd_rsp_validQ <= 1'b0;

// Initializing cam entry to first lookup data in testbench 
//      tag0 <= 98'h19f0796560401518140015180;
//      data0 <= 16'h7ff8;
//      data0_valid <= 1'b1;
      data0_valid <= 1'b0;
      
      cam_hit <= 1'b0;
      lup_state <= IDLE;
      upd_state <= IDLE;
    end

    else begin
    
      // Lookup SM:
      case (lup_state)
	IDLE: begin
	  lup_req_readyQ <= 1'b1;
	  lup_rsp_validQ <= 1'b0;
	  cam_hit <= 1'b0;
	  if (lup_req_valid & lup_req_ready) begin
	    lup_state <= LUP_CHK;
	    lup_req_readyQ <= 1'b0;
	  end
	  else
	    lup_state <= IDLE;

	end // case: IDLE

	LUP_CHK: begin
	  // cam lookup:
	  // lookup_request_data == lookupsource(1) + key(96)
	  // key == {myIP(32), theirIP(32), myport(16), theirport(16)}
	  cam_hit <= data0_valid & (tag0[95:0] == lup_req_din[96:1]);
	  lup_source <= lup_req_din[0];
	  
	  if (lup_rsp_ready)	  
	    lup_state <= LUP_RSP;
	  else 	  
	    lup_state <= LUP_WAIT;

	end // case: LUP_CHK
	
	LUP_WAIT: begin
	  if (lup_rsp_ready)	  
	    lup_state <= LUP_RSP;
	  else 	  
	    lup_state <= LUP_WAIT;
	end // case: LUP_WAIT
	
	LUP_RSP: begin
	  lup_state <= IDLE;
	  lup_rsp_validQ <= 1'b1;

	  // lup_response_data == lookupsource(1), + sessionID(14) + hit = 16 bits
	  lup_rsp_doutQ <= {cam_hit, cam_hit ? data0[13:0] : 14'h0, lup_source};
	end // case: LUP_RSP

	default:
	  lup_state <= IDLE;
      endcase


      // Update SM:
      case (upd_state)
	IDLE: begin
	  upd_req_readyQ <= 1'b1;
	  upd_rsp_validQ <= 1'b0;
	  if (upd_req_valid & upd_req_ready) begin
	    upd_state <= UPD_WR;
	    upd_req_readyQ <= 1'b0;
	  end
	  else
	    upd_state <= IDLE;

	end // case: IDLE

	UPD_WR: begin
	  // Update cam entry:
	  upd_req_readyQ <= 1'b0;

	  //upd_request_data ==  lookupsource(1) + lookupOp(1) + SessionID(14) + key(96)
	  tag0 <= {2'b0, upd_req_din[111:16]};
	  data0 <= upd_req_din[15:2];
	  upd_op <= upd_req_din[1];
	  upd_source <= upd_req_din[0];
	  data0_valid <= 1'b1;
	  
	  if (upd_rsp_ready)	  
	    upd_state <= UPD_RSP;
	  else 	  
	    upd_state <= UPD_WAIT;

	end // case: UPD_WR
	
	UPD_WAIT: begin
	  if (upd_rsp_ready)	  
	    upd_state <= UPD_RSP;
	  else 	  
	    upd_state <= UPD_WAIT;
	end // case: UPD_WAIT
	
	UPD_RSP: begin
	  upd_state <= IDLE;
	  upd_rsp_validQ <= 1'b1;

	  // upd_response == lookupSource(1), lookupOp(1) + sessionID(14)
	  upd_rsp_doutQ <= {data0[13:0], upd_op, upd_source};
	end // case: UPD_RSP

	default:
	  upd_state <= IDLE;
      endcase

    end
  end  
endmodule
