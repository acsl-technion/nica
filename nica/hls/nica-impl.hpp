//
// Copyright (c) 2016-2018 Haggai Eran, Gabi Malka, Lior Zeno, Maroun Tork
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

#pragma once

#include "arbiter-impl.hpp"
#include "demux.hpp"
#include "custom_rx_ring-impl.hpp"

class header_to_metadata_and_private
{
public:
    void split_udp_hdr_stream(udp::header_stream& hdr_in, result_stream& ft_results,
                              hls_ik::metadata_stream& metadata_out);
};

class packet_counter {
public:
    void count(
        hls_ik::metadata_stream& metadata_in,
        udp::udp_builder_metadata_stream& header_out, nica_ikernel_stats& ik_stats);

private:
    nica_ikernel_stats stats;
};

/* Choose the right config based on the direction */
template <hls_ik::pipeline_ports hls_ik::ports::* pipeline>
struct udp_config
{
};

template <>
struct udp_config<&hls_ik::ports::host>
{
    typedef udp::config_h2n type;
};

template <>
struct udp_config<&hls_ik::ports::net>
{
    typedef udp::config_n2h type;
};

/** Necessary logic to duplicate around each ikernel.
 *
 * pipeline points to the member variable in the ports structs of the desired
 * pipeline (host or net). */
template <hls_ik::pipeline_ports hls_ik::ports::* pipeline>
class ikernel_wrapper_common {
public:
    ikernel_wrapper_common();

    typedef typename udp_config<pipeline>::type config;

    void wrapper(hls_ik::ports& ik, udp::header_stream& header_udp_to_ikernel,
                 result_stream& ft_results,
                 hls_ik::data_stream& data_udp_to_ikernel,
                 udp::udp_builder_metadata_stream& hdr_ik_to_demux,
                 hls_ik::data_stream& data_ik_to_demux,
                 nica_ikernel_stats& ik_stats,
                 config& cfg);

#if !defined(__SYNTHESIS__)
    void verify();
#endif

private:
    header_to_metadata_and_private hdr_to_meta;
    packet_counter cnt;
};

template <hls_ik::pipeline_ports hls_ik::ports::* pipeline>
class ikernel_wrapper {
public:
    typedef typename ikernel_wrapper_common<pipeline>::config config;
    void wrapper(hls_ik::ports& ik, udp::header_stream& header_udp_to_ikernel,
                 result_stream& ft_results,
                 hls_ik::data_stream& data_udp_to_ikernel,
                 udp::udp_builder_metadata_stream& hdr_ik_to_demux,
                 hls_ik::data_stream& data_ik_to_demux,
                 nica_ikernel_stats& ik_stats,
                 config& cfg);

    void verify() {}
protected:
    ikernel_wrapper_common<pipeline> common;
};

/* Custom ring only in n2h */
template <>
class ikernel_wrapper<&hls_ik::ports::net> {
public:
    typedef udp::config_n2h config;
    void wrapper(hls_ik::ports& ik, udp::header_stream& header_udp_to_ikernel,
                 result_stream& ft_results,
                 hls_ik::data_stream& data_udp_to_ikernel,
                 udp::udp_builder_metadata_stream& hdr_ik_to_demux,
                 hls_ik::data_stream& data_ik_to_demux,
                 nica_ikernel_stats& ik_stats,
                 config& cfg);

    void verify();
private:
    ikernel_wrapper_common<&hls_ik::ports::net> common;
    custom_rx_ring custom_ring;
    udp::udp_builder_metadata_stream hdr_ik_to_custom_ring;
    hls_ik::data_stream data_ik_to_custom_ring;
};

static inline hls_ik::axi_data raw_to_data(const mlx::axi4s& w)
{
#pragma HLS inline
    return hls_ik::axi_data(w.data, w.keep, w.last);
}

class passthrough_wrapper
{
public:
    void step(mlx::stream& in, hls::stream<bool>& pass_stream, udp::udp_builder_metadata_stream& hdr_out, hls_ik::data_stream& data_out)
    {
#pragma HLS pipeline enable_flush
        mlx::axi4s word;
        switch (state) {
        case IDLE:
            if (pass_stream.empty() || in.empty() || hdr_out.full() || data_out.full())
                return;

            bool pass;
            pass_stream.read_nb(pass);
            drop = !pass;
            in.read_nb(word);
            state = word.last ? IDLE : STREAM;
            if (!drop) {
                data_out.write_nb(raw_to_data(word));
                mlx_metadata.user = word.user;
                mlx_metadata.id = word.id;
                length = 1;

                if (word.last)
                    generate_metadata(hdr_out);
            }
            break;
        case STREAM:
            if (in.empty() || data_out.full() || hdr_out.full())
		return;

            in.read_nb(word);
            ++length;
            state = word.last ? IDLE : STREAM;
            if (!drop) {
                data_out.write_nb(raw_to_data(word));

                if (word.last)
                    generate_metadata(hdr_out);
            }
        }
    }

private:
    void generate_metadata(udp::udp_builder_metadata_stream& hdr_out)
    {
#pragma HLS inline
        udp::udp_builder_metadata m;
        m.pkt_type = PKT_TYPE_RAW;
        m.length = length << 5;
        m.verify();
        hdr_out.write_nb(m);
    }

    bool drop;
    enum { IDLE, STREAM } state;
    uint16_t length;
    mlx::metadata mlx_metadata;
};

/** The full NICA pipeline of a single direction (host to net or net to host).
 *
 * pipeline points to the member variable in the ports structs of the desired
 * pipeline (host or net). */
template <hls_ik::pipeline_ports hls_ik::ports::* pipeline>
class nica_state {
public:
    nica_state();

    typedef typename udp_config<pipeline>::type config;

    void nica_step(mlx::stream& port2sbu, mlx::stream& sbu2port,
                   config& config, nica_pipeline_stats& s,
                   trace_event events[4],
                   DECL_IKERNEL_PARAMS(),
                   tc_ports& tc_out, tc_ports& tc_in);

#if !defined(__SYNTHESIS__)
    void verify();
#endif

private:
    udp::udp udp;
#define BOOST_PP_LOCAL_MACRO(i) \
    ikernel_wrapper<pipeline> wrapper ## i; \
    udp::header_stream header_udp_to_ikernel ## i; \
    result_stream ft_results_to_ik ## i; \
    hls_ik::data_stream data_udp_to_ikernel ## i;
#define BOOST_PP_LOCAL_LIMITS (0, NUM_IKERNELS - 1)
%:include BOOST_PP_LOCAL_ITERATE()
    demultiplexor<NUM_TC - 1> demux;
    arbiter arb;
    udp::udp_builder builder;
    udp::ethernet_padding ethernet_pad;

    hls_helpers::duplicator<1, mlx::axi4s> raw_dup;
    passthrough_wrapper dropper;
    mlx::stream raw_in_to_udp, raw_in_to_dropper,
        raw_builder_to_pad;
    udp::bool_stream bool_pass_raw, over_threshold,
           bool_pass_from_steering;
    udp::udp_builder_metadata_stream hdr_ik_to_demux, hdr_arbiter_to_builder;
    hls_ik::data_stream data_ik_to_demux, data_arbiter_to_builder;
#ifdef SIMULATION_BUILD
    /* Intermediate FIFO for simulation so that no inputs are dropped due to
     * backpressure (HLS cosim testbench ignores backpressure for some reason). */
    tc_ports tc_intermediate;
#endif
};

#if !defined(__SYNTHESIS__)
extern nica_state<&hls_ik::ports::net> n2h;
extern nica_state<&hls_ik::ports::host> h2n;
#endif
