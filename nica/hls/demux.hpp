/* * Copyright (c) 2016-2018 Haggai Eran, Gabi Malka, Lior Zeno, Maroun Tork
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 *  * Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation and/or
 * other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <hls_stream.h>
#include "tc-ports.hpp"

template <unsigned num_ports>
class demultiplexor
{
public:
    static const unsigned log_num_ports = hls_helpers::log2(num_ports);
    /* Minimum width of 1 since HLS doesn't deal with 0 width values */
    static const unsigned num_streams_width = log_num_ports ? log_num_ports : 1;
    typedef hls_ik::data_stream stream;
    typedef udp::udp_builder_metadata_stream metadata_stream;

    typedef ap_uint<num_streams_width> index_t;

    demultiplexor() : state(IDLE)
    {}

    void demux(metadata_stream& metadata_in, stream& data_in, tc_ports& tc)
    {
#pragma HLS inline
        static_assert(num_ports == NUM_TC - 1, "invalid number of ports. only NUM_TC - 1 is supported");

        hls_helpers::dup(metadata_in, meta_to_select_port, meta_to_output);
        port_selector();
        hls_helpers::dup(selected_ports, port_to_meta_output, port_to_data_output);
        demux_meta(tc);
        demux_data(data_in, tc);
    }

private:
    enum { META_IDLE, WRITE_METADATA } meta_state;
    index_t meta_port;
    metadata_stream meta_to_select_port, meta_to_output;
    hls::stream<bool> empty_stream;
    hls::stream<index_t> selected_ports, port_to_meta_output, port_to_data_output;

    void port_selector()
    {
#pragma HLS pipeline II=3 enable_flush
        ap_uint<udp::udp_builder_metadata::width> raw;
        if (selected_ports.full() || empty_stream.full() || !meta_to_select_port.read_nb(raw))
            return;

        udp::udp_builder_metadata metadata = raw;
        bool empty = metadata.empty_packet();
        index_t meta_port = select_port(metadata);
        selected_ports.write_nb(meta_port);
        empty_stream.write_nb(empty);
    }

    void demux_meta(tc_ports& tc)
    {
#pragma HLS pipeline II=2 enable_flush
        switch (meta_state) {
        case META_IDLE:
            if (!port_to_meta_output.read_nb(meta_port))
                return;

            meta_state = WRITE_METADATA;
            break;

        case WRITE_METADATA:
            if (meta_to_output.empty())
                return;

            switch (meta_port) {
#define BOOST_PP_LOCAL_MACRO(i) \
            case i: \
                if (tc.meta ## i.full()) \
                    return; \
                break;
#define BOOST_PP_LOCAL_LIMITS (0, NUM_TC - 2)
%:include BOOST_PP_LOCAL_ITERATE()
            }

            ap_uint<udp::udp_builder_metadata::width> raw;
            meta_to_output.read_nb(raw);

            assert(meta_port < num_ports);
            switch (meta_port) {
#define BOOST_PP_LOCAL_MACRO(i) \
            case i: \
                tc.meta ## i.write_nb(raw); \
                break;
#define BOOST_PP_LOCAL_LIMITS (0, NUM_TC - 2)
%:include BOOST_PP_LOCAL_ITERATE()
            }
            meta_state = META_IDLE;
            break;
        }
    }

    enum { IDLE, STREAM } state;
    index_t data_port;

    void demux_data(stream& data_in, tc_ports& tc)
    {
#pragma HLS pipeline II=1 enable_flush
        switch (state) {
        case IDLE:
            if (empty_stream.empty() || port_to_data_output.empty())
                return;

            port_to_data_output.read_nb(data_port);
            bool empty;
            empty_stream.read_nb(empty);

            state = empty ? IDLE : STREAM;
            break;

        case STREAM: {
            if (data_in.empty())
                return;

            switch (data_port) {
#define BOOST_PP_LOCAL_MACRO(i) \
            case i: \
                if (tc.data ## i.full()) \
                    return; \
                break;
#define BOOST_PP_LOCAL_LIMITS (0, NUM_TC - 2)
%:include BOOST_PP_LOCAL_ITERATE()
            }

            ap_uint<hls_ik::axi_data::width> raw_flit;
            data_in.read_nb(raw_flit);
            hls_ik::axi_data flit = raw_flit;
            switch (data_port) {
#define BOOST_PP_LOCAL_MACRO(i) \
            case i: \
                tc.data ## i.write_nb(raw_flit); \
                break;
#define BOOST_PP_LOCAL_LIMITS (0, NUM_TC - 2)
%:include BOOST_PP_LOCAL_ITERATE()
            }
            state = flit.last ? IDLE : STREAM;
            break;
        }
        }
    }

    index_t select_port(const udp::udp_builder_metadata& metadata)
    {
#pragma HLS inline
        static_assert(NUM_TC == 1 << hls_helpers::log2(NUM_TC), "NUM_TC must be power of two");
        int ik_id = metadata.ikernel_id & (NUM_TC - 1);
        if (ik_id >= num_ports) {
            ik_id -= num_ports;
        }
        return ik_id;
    }
};

void demux_top(udp::udp_builder_metadata_stream& meta_in, hls_ik::data_stream& data_in,
               tc_ports& tc);
