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

#include <boost/preprocessor/iteration/local.hpp>

#include <udp.h>
#include <mlx.h>
#include "nica-top.hpp"
#include "nica-impl.hpp"
#include "toe_wrapper-impl.hpp"
#include <link_with_reg.hpp>

#include <algorithm>
using std::min;
using std::max;

using udp::header_stream;
using udp::bool_stream;
using udp::header_parser;
using udp::header_buffer;
using udp::udp_builder_metadata_stream;

using hls_ik::packet_metadata;

using namespace hls_helpers;

#define IKERNEL_DELAY 1024

void header_to_metadata_and_private::split_udp_hdr_stream(
    udp::header_stream& hdr_in, result_stream& ft_results,
    hls_ik::metadata_stream& metadata_out)
{
#pragma HLS pipeline enable_flush ii=1
    if (hdr_in.empty() || ft_results.empty() || metadata_out.full())
        return;

    auto ft_res = ft_results.read();
    header_buffer buf = hdr_in.read();
    header_parser hdr = buf.hdr;
    hls_ik::metadata m;
    packet_metadata pkt = m.get_packet_metadata();
    // pkt.port_dst;
    // pkt.port_src;
    pkt.eth_dst = hdr.eth.dest;
    pkt.eth_src = hdr.eth.source;
    pkt.ip_dst = hdr.ip.daddr;
    pkt.ip_src = hdr.ip.saddr;
    pkt.udp_dst = hdr.udp.dest;
    pkt.udp_src = hdr.udp.source;
    m.set_packet_metadata(pkt);
    m.ip_identification = hdr.ip.id;
    m.length = hdr.udp.length - hdr.udp.width / 8;
    m.ikernel_id = ft_res.v.ikernel_id;
    m.flow_id = ft_res.flow_id;
    metadata_out.write(m);
}

void packet_counter::count(
    hls_ik::metadata_stream& metadata_in,
    udp::udp_builder_metadata_stream& header_out, nica_ikernel_stats& ik_stats)
{
#pragma HLS pipeline enable_flush ii=3
    ap_uint<hls_ik::metadata::width> m;

    ik_stats = stats;

    if (metadata_in.empty() || header_out.full())
        return;

    metadata_in.read_nb(m);
    header_out.write_nb(m);

    ++stats.packets;
}

template <hls_ik::pipeline_ports hls_ik::ports::* pipeline>
ikernel_wrapper_common<pipeline>::ikernel_wrapper_common()
{}

template <hls_ik::pipeline_ports hls_ik::ports::* pipeline>
void ikernel_wrapper_common<pipeline>::wrapper(
    hls_ik::ports& ik, udp::header_stream& header_udp_to_ikernel,
    result_stream& ft_results,
    hls_ik::data_stream& data_udp_to_ikernel,
    udp::udp_builder_metadata_stream& hdr_ik_to_demux,
    hls_ik::data_stream& data_ik_to_demux,
    nica_ikernel_stats& ik_stats,
    config& cfg) {
#pragma HLS inline
    hdr_to_meta.split_udp_hdr_stream(header_udp_to_ikernel,
                                     ft_results,
                                     (ik.*pipeline).metadata_input);
    hls_helpers::link_axi_stream(data_udp_to_ikernel, (ik.*pipeline).data_input);
    hls_helpers::link_fifo((ik.*pipeline).data_output, data_ik_to_demux);
    cnt.count((ik.*pipeline).metadata_output, hdr_ik_to_demux, ik_stats);
}

template <hls_ik::pipeline_ports hls_ik::ports::* pipeline>
void ikernel_wrapper<pipeline>::wrapper(
    hls_ik::ports& ik, udp::header_stream& header_udp_to_ikernel,
    result_stream& ft_results,
    hls_ik::data_stream& data_udp_to_ikernel,
    udp::udp_builder_metadata_stream& hdr_ik_to_demux,
    hls_ik::data_stream& data_ik_to_demux,
    nica_ikernel_stats& ik_stats,
    config& cfg) {
#pragma HLS inline
    common.wrapper(ik, header_udp_to_ikernel, ft_results, data_udp_to_ikernel,
                   hdr_ik_to_demux, data_ik_to_demux, ik_stats, cfg);
}

void ikernel_wrapper<&hls_ik::ports::net>::wrapper(
    hls_ik::ports& ik, udp::header_stream& header_udp_to_ikernel,
    result_stream& ft_results,
    hls_ik::data_stream& data_udp_to_ikernel,
    udp::udp_builder_metadata_stream& hdr_ik_to_demux,
    hls_ik::data_stream& data_ik_to_demux,
    nica_ikernel_stats& ik_stats,
    config& cfg) {
#pragma HLS inline
    common.wrapper(ik, header_udp_to_ikernel, ft_results, data_udp_to_ikernel,
                   hdr_ik_to_custom_ring, data_ik_to_custom_ring, ik_stats, cfg);
    custom_ring.custom_ring(hdr_ik_to_custom_ring, data_ik_to_custom_ring,
        hdr_ik_to_demux, data_ik_to_demux, cfg.custom_ring_gateway);
}

#if !defined(__SYNTHESIS__)
void ikernel_wrapper<&hls_ik::ports::net>::verify()
{
    custom_ring.verify();
}
#endif

template <hls_ik::pipeline_ports hls_ik::ports::* pipeline>
nica_state<pipeline>::nica_state() :
    raw_in_to_udp("raw_in_to_udp"),
    raw_in_to_dropper("raw_in_to_dropper"),
    bool_pass_raw("bool_pass_raw"),
    over_threshold("over_threshold"),
    bool_pass_from_steering("bool_pass_from_steering")
{}

template <hls_ik::pipeline_ports hls_ik::ports::* pipeline>
void nica_state<pipeline>::nica_step(
    mlx::stream& port2sbu, mlx::stream& sbu2port,
    config& config, nica_pipeline_stats& s,
    trace_event events[4],
    DECL_IKERNEL_PARAMS(),
    tc_ports& tc_out, tc_ports& tc_in)
{
#pragma HLS inline
    DO_PRAGMA(HLS STREAM variable=raw_in_to_udp depth=FIFO_WORDS);
    DO_PRAGMA(HLS STREAM variable=raw_in_to_dropper depth=FIFO_WORDS);
    DO_PRAGMA(HLS STREAM variable=bool_pass_raw depth=FIFO_PACKETS);
    DO_PRAGMA(HLS STREAM variable=over_threshold depth=FIFO_PACKETS);
    DO_PRAGMA(HLS STREAM variable=bool_pass_from_steering depth=FIFO_PACKETS);
#define BOOST_PP_LOCAL_MACRO(i) \
    DO_PRAGMA_SYN(HLS data_pack variable=&data_udp_to_ikernel ## i); \
    DO_PRAGMA_SYN(HLS data_pack variable=&header_udp_to_ikernel ## i); \
    DO_PRAGMA(HLS stream variable=&header_udp_to_ikernel ## i depth=FIFO_PACKETS); \
    DO_PRAGMA(HLS stream variable=&ft_results_to_ik ## i depth=FIFO_PACKETS);
#define BOOST_PP_LOCAL_LIMITS (0, NUM_IKERNELS - 1)
%:include BOOST_PP_LOCAL_ITERATE()

#define BOOST_PP_LOCAL_MACRO(i) \
    DO_PRAGMA_SIM(HLS stream variable=&tc_intermediate.meta ## i depth=FIFO_WORDS) \
    DO_PRAGMA_SIM(HLS stream variable=&tc_intermediate.data ## i depth=FIFO_WORDS)
#define BOOST_PP_LOCAL_LIMITS (0, NUM_TC - 1)
%:include BOOST_PP_LOCAL_ITERATE()

    DO_PRAGMA_SYN(HLS data_pack variable=raw_in_to_udp);
    DO_PRAGMA_SYN(HLS data_pack variable=raw_in_to_dropper);
    DO_PRAGMA_SYN(HLS data_pack variable=raw_builder_to_pad);
    DO_PRAGMA(HLS STREAM variable=raw_builder_to_pad depth=2);
    DO_PRAGMA(HLS STREAM variable=hdr_ik_to_demux depth=2);
    DO_PRAGMA(HLS STREAM variable=data_ik_to_demux depth=10);

    raw_dup.dup2(port2sbu, raw_in_to_udp, raw_in_to_dropper);
    udp.udp_step(raw_in_to_udp,
        BOOST_PP_ENUM_PARAMS(NUM_IKERNELS, header_udp_to_ikernel),
        BOOST_PP_ENUM_PARAMS(NUM_IKERNELS, ft_results_to_ik),
        BOOST_PP_ENUM_PARAMS(NUM_IKERNELS, data_udp_to_ikernel),
        bool_pass_from_steering, &config.common, &s.udp);

    dropper.step(raw_in_to_dropper, bool_pass_from_steering,
        BOOST_PP_CAT(tc_out.meta, BOOST_PP_DEC(NUM_TC)),
        BOOST_PP_CAT(tc_out.data, BOOST_PP_DEC(NUM_TC)));

#define BOOST_PP_LOCAL_MACRO(i) \
    wrapper ## i.wrapper(ik ## i, header_udp_to_ikernel ## i, \
                           ft_results_to_ik ## i, \
                           data_udp_to_ikernel ## i, \
                           hdr_ik_to_demux, \
                           data_ik_to_demux, \
                           s.ik ## i, \
                           config);
#define BOOST_PP_LOCAL_LIMITS (0, NUM_IKERNELS - 1)
%:include BOOST_PP_LOCAL_ITERATE()

    demux.demux(hdr_ik_to_demux, data_ik_to_demux, tc_out);

#ifdef SIMULATION_BUILD
    link_axi_to_fifo(tc_in, tc_intermediate);
#endif

    arb.arbiter_step(
#ifdef SIMULATION_BUILD
        tc_intermediate,
#else
        tc_in,
#endif
        hdr_arbiter_to_builder,
        data_arbiter_to_builder,
        &s.arbiter,
        config.common.arbiter_gateway,
        events);

    builder.builder_step(hdr_arbiter_to_builder, data_arbiter_to_builder,
        raw_builder_to_pad);

    ethernet_pad.pad(raw_builder_to_pad, sbu2port);
}

#if !defined(__SYNTHESIS__)
template <hls_ik::pipeline_ports hls_ik::ports::* pipeline>
void nica_state<pipeline>::verify()
{
#define BOOST_PP_LOCAL_MACRO(i) \
    wrapper ## i.verify();
#define BOOST_PP_LOCAL_LIMITS (0, NUM_IKERNELS - 1)
%:include BOOST_PP_LOCAL_ITERATE()
}
#endif

class link_pipe
{
public:
    void link(hls_ik::pipeline_ports& ik_buf, hls_ik::pipeline_ports& ik)
    {
#pragma HLS inline
        metadata_input.link(ik_buf.metadata_input, ik.metadata_input);
        data_input.link(ik_buf.data_input, ik.data_input);
        metadata_output.link(ik.metadata_output, ik_buf.metadata_output);
        data_output.link(ik.data_output, ik_buf.data_output);
    }
protected:
    link_with_reg<ap_uint<hls_ik::metadata::width>, false> metadata_input;
    link_with_reg<ap_uint<hls_ik::metadata::width>, true> metadata_output;
    link_with_reg<ap_uint<hls_ik::axi_data::width>, false> data_input;
    link_with_reg<ap_uint<hls_ik::axi_data::width>, true> data_output;
    link_with_reg<ap_uint<2>, true> action;
};

class link_ports
{
public:
    void link(hls_ik::ports& ik_buf, hls_ik::ports& ik)
    {
#pragma HLS inline
        host.link(ik_buf.host, ik.host);
        net.link(ik_buf.net, ik.net);
    }

protected:
    link_pipe host, net;
};

template class nica_state<&hls_ik::ports::net>;
template class nica_state<&hls_ik::ports::host>;

#if !defined(__SYNTHESIS__)
nica_state<&hls_ik::ports::net> n2h;
nica_state<&hls_ik::ports::host> h2n;
#endif

void nica(mlx::stream& prt_nw2sbu, mlx::stream& sbu2prt_nw, mlx::stream& prt_cx2sbu,
          mlx::stream& sbu2prt_cx,
          nica_config* cfg, nica_stats* stats,
          trace_event events[NUM_TRACE_EVENTS],
          DECL_IKERNEL_PARAMS(),
          tc_ports& h2n_tc_out, tc_ports& h2n_tc_in,
          tc_ports& n2h_tc_out, tc_ports& n2h_tc_in,
          toe_app_ports& toe
    )
{
#pragma HLS INTERFACE axis port=prt_nw2sbu
#pragma HLS INTERFACE axis port=sbu2prt_nw
#pragma HLS INTERFACE axis port=prt_cx2sbu
#pragma HLS INTERFACE axis port=sbu2prt_cx
#pragma HLS array_partition variable=events complete
#pragma HLS interface ap_none port=events
#pragma HLS interface ap_fifo port=n2h_tc_out
#pragma HLS interface ap_fifo port=n2h_tc_in
#pragma HLS interface ap_fifo port=h2n_tc_out
#pragma HLS interface ap_fifo port=h2n_tc_in
#ifdef SIMULATION_BUILD
/* For RTL cosimulation we need the function control signals, but for the
 * Mellanox wrapper we don't. The co-simulation code also doesn't work well
 * with AXI4-Lite. */
#  pragma HLS INTERFACE ap_ctrl_hs port=return
#else
#  pragma HLS INTERFACE ap_ctrl_none port=return
#  pragma HLS INTERFACE s_axilite port=cfg->n2h.common.enable offset=0x10
    GATEWAY_OFFSET(cfg->n2h.common.flow_table_gateway, 0x18, 0x20, 0x30)
// #  pragma HLS INTERFACE s_axilite port=cfg->n2h.lossy offset=0x50
    GATEWAY_OFFSET(cfg->n2h.common.arbiter_gateway, 0x58, 0x60, 0x70)
    GATEWAY_OFFSET(cfg->n2h.custom_ring_gateway, 0x78, 0x80, 0x90)
#  pragma HLS INTERFACE s_axilite port=stats->n2h offset=0x100

#  pragma HLS INTERFACE s_axilite port=cfg->h2n.common.enable offset=0x410
    GATEWAY_OFFSET(cfg->h2n.common.flow_table_gateway, 0x418, 0x420, 0x430)
// #  pragma HLS INTERFACE s_axilite port=cfg->h2n.lossy offset=0x450
    GATEWAY_OFFSET(cfg->h2n.common.arbiter_gateway, 0x458, 0x460, 0x470)
#  pragma HLS INTERFACE s_axilite port=stats->h2n offset=0x500
    GATEWAY_OFFSET(cfg->toe, 0x518, 0x520, 0x530)

#  pragma HLS INTERFACE s_axilite port=stats->flow_table_size offset=0x800
#endif
#pragma HLS dataflow

/* Iterate over all ikernel inputs. See:
 * http://www.boost.org/doc/libs/1_63_0/libs/preprocessor/doc/index.html */
#define BOOST_PP_LOCAL_MACRO(n) \
    IKERNEL_PORTS_PRAGMAS(ik ## n)
#define BOOST_PP_LOCAL_LIMITS (0, NUM_IKERNELS - 1)
%:include BOOST_PP_LOCAL_ITERATE()
    TOE_PORTS_PRAGMAS(toe)

        using hls_ik::ports;
#if defined(__SYNTHESIS__)
        static nica_state<&ports::net> n2h;
        static nica_state<&ports::host> h2n;
#endif
        static toe_control toe_c;
#define BOOST_PP_LOCAL_MACRO(n) \
        static hls_ik::ports ik_buf ## n; \
        static link_ports linker ## n; \
        \
        DO_PRAGMA(HLS stream variable=&ik_buf ## n.host.metadata_input depth=FIFO_WORDS); \
        DO_PRAGMA(HLS stream variable=&ik_buf ## n.host.data_input depth=FIFO_WORDS); \
        DO_PRAGMA(HLS stream variable=&ik_buf ## n.net.metadata_input depth=FIFO_WORDS); \
        DO_PRAGMA(HLS stream variable=&ik_buf ## n.net.data_input depth=FIFO_WORDS);
#define BOOST_PP_LOCAL_LIMITS (0, NUM_IKERNELS - 1)
%:include BOOST_PP_LOCAL_ITERATE()

        n2h.nica_step(prt_nw2sbu, sbu2prt_cx,
            cfg->n2h, stats->n2h, &events[TRACE_N2H],
            BOOST_PP_ENUM_PARAMS(NUM_IKERNELS, ik_buf), n2h_tc_out, n2h_tc_in);
        h2n.nica_step(prt_cx2sbu, sbu2prt_nw,
            cfg->h2n, stats->h2n, &events[TRACE_H2N],
            BOOST_PP_ENUM_PARAMS(NUM_IKERNELS, ik_buf), h2n_tc_out, h2n_tc_in);
        toe_c.toe_c(cfg->toe);
        link(toe_c.p, toe);

#define BOOST_PP_LOCAL_MACRO(n) \
        linker ## n.link(ik_buf ## n, ik ## n);
#define BOOST_PP_LOCAL_LIMITS (0, NUM_IKERNELS - 1)
%:include BOOST_PP_LOCAL_ITERATE()

        constant_configuration: {
            stats->flow_table_size = FLOW_TABLE_SIZE;
        }
}
