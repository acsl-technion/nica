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

#include "ikernel.hpp"
#include "udp.h"
#include "gateway.hpp"
#include "push_header.hpp"
#include "push_suffix.hpp"
#include "context_manager.hpp"

struct ring_context
{
    ap_uint<48> eth_dst;
    ap_uint<32> ip_dst;
    ap_uint<24> dest_qpn;
    ap_uint<24> psn;
};

class ring_context_manager : public context_manager<ring_context, CUSTOM_RINGS_LOG_NUM> {
public:
    int gateway_write(int address, int value);
    int gateway_read(int address, int* value);

    /* Query QPN and increment PSN */
    ring_context next_packet(hls_ik::ring_id_t ring_id);
};

struct hdr_to_data
{
    hls_ik::ring_id_t ring_id;
};

/* Build RoCE UC send packets from ikernel outputs */
class custom_rx_ring : public hls_ik::gateway_impl<custom_rx_ring>
{
public:
    custom_rx_ring();
    /** Builds packets from split header and data streams, and re-calculates
     * the checksum. */
    void custom_ring(udp::udp_builder_metadata_stream& header_in, hls_ik::data_stream& data_in,
                     udp::udp_builder_metadata_stream& header_out, hls_ik::data_stream& data_out,
                     hls_ik::gateway_registers& r);

    int reg_read(int address, int* value);
    int reg_write(int address, int value);
    void gateway_update() {}

#if !defined(__SYNTHESIS__)
    void verify();
#endif

private:
    void ring_hdrs(udp::udp_builder_metadata_stream& hdr_in, udp::udp_builder_metadata_stream& hdr_out);
    hls_ik::axi_data gen_bth(const ring_context& context, ap_uint<16>& len);

    /* Metadata used for trasmitting to the host */
    hls_ik::packet_metadata metadata, metadata_cache;
    hls::stream<hls_ik::packet_metadata> metadata_updates;
    ring_context_manager contexts;
    hls_ik::data_stream bth, data_bth_to_icrc;
    hls::stream<ap_uint<32> > icrc;
    hls::stream<bool> empty_packet, empty_packet_bth, empty_packet_icrc,
                      enable_stream, enable_bth, enable_icrc;
    push_header<12 * 8> push_bth;
    push_suffix<4> push_icrc;

    /* Data-path state */
    enum { IDLE, STREAM } state;
    udp::udp_builder_metadata cur_metadata;
};
