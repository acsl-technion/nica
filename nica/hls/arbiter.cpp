//
// Copyright (c) 2016-2017 Haggai Eran, Gabi Malka, Lior Zeno, Maroun Tork
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without modification,
// are permitted provided that the following conditions are met:
//
//  * Redistributions of source code must retain the above copyright notice, this
// list of conditions and the following disclaimer.
//  * Redistributions in binary form must reproduce the above copyright notice,
// this list of conditions and the following disclaimer in the documentation and/or
// other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ``AS IS''
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
// ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
// LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
// ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//

#include <udp.h>
#include <mlx.h>

#include "arbiter-impl.hpp"
#include "demux.hpp"
#include <hls_helper.h>

using hls_helpers::link_fifo;

void arbiter_top(udp::udp_builder_metadata_stream& hdr_out, hls_ik::data_stream& out,
                 udp::udp_builder_metadata_stream meta[NUM_TC],
                 hls_ik::data_stream port[NUM_TC],
                 arbiter_stats<NUM_TC>* stats, hls_ik::gateway_registers& arbiter_gateway)
{
#pragma HLS INTERFACE ap_fifo port=out
#pragma HLS INTERFACE ap_fifo port=port
    GATEWAY_OFFSET(arbiter_gateway, 0x58, 0x60, 0x70)
#pragma HLS dataflow

    static arbiter arb;
    trace_event events[NUM_TRACE_EVENTS];
    static tc_ports tc;
#define BOOST_PP_LOCAL_MACRO(i) \
    DO_PRAGMA(HLS STREAM variable=tc.data ## i depth=FIFO_WORDS); \
    DO_PRAGMA(HLS STREAM variable=tc.meta ## i depth=FIFO_WORDS); \
    \
    link_fifo(port[i], tc.data ## i); \
    link_fifo(meta[i], tc.meta ## i);
#define BOOST_PP_LOCAL_LIMITS (0, NUM_TC - 1)
%:include BOOST_PP_LOCAL_ITERATE()

    arb.arbiter_step(
        tc,
        hdr_out,
        out,
        stats,
        arbiter_gateway, events
    );
}

void demux_arb_top(udp::udp_builder_metadata_stream& meta_in, hls_ik::data_stream& data_in,
                   udp::udp_builder_metadata_stream& passthrough_meta_in, hls_ik::data_stream& passthrough_data_in,
                   udp::udp_builder_metadata_stream& hdr_out, hls_ik::data_stream& out,
                   arbiter_stats<NUM_TC>* stats, hls_ik::gateway_registers& arbiter_gateway,
                   tc_ports& tc_out, tc_ports& tc_in)
{
#pragma HLS INTERFACE ap_fifo port=out
#pragma HLS INTERFACE ap_fifo port=hdr_out
    static tc_ports tc_axi_to_fifo;
#define BOOST_PP_LOCAL_MACRO(i) \
    DO_PRAGMA_SIM(HLS interface axis port=tc_in.meta ## i) \
    DO_PRAGMA_SIM(HLS interface axis port=tc_in.data ## i) \
    DO_PRAGMA_SIM(HLS stream variable=tc_axi_to_fifo.meta ## i depth=FIFO_WORDS) \
    DO_PRAGMA_SIM(HLS stream variable=tc_axi_to_fifo.data ## i depth=FIFO_WORDS)
#define BOOST_PP_LOCAL_LIMITS (0, NUM_TC - 1)
%:include BOOST_PP_LOCAL_ITERATE()
    GATEWAY_OFFSET(arbiter_gateway, 0x58, 0x60, 0x70)
#pragma HLS dataflow

    static arbiter arb;
    trace_event events[NUM_TRACE_EVENTS];

    demux_top(meta_in, data_in, tc_out);
    link_axi_to_fifo(tc_in, tc_axi_to_fifo);
    arb.arbiter_step(tc_axi_to_fifo, hdr_out, out, stats, arbiter_gateway, events);
}
