/* * Copyright (c) 2016-2017 Haggai Eran, Gabi Malka, Lior Zeno, Maroun Tork
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

#include "custom_rx_ring.hpp"
#include "custom_rx_ring-impl.hpp"
#include "rxe_hdr.h"
#include "ib_pack.h"

using hls_ik::metadata_stream;
using hls_ik::data_stream;
using hls_ik::axi_data;
using hls_ik::ikernel_id_t;

using hls_helpers::dup;

custom_rx_ring::custom_rx_ring() :
    icrc("icrc")
{
#pragma HLS stream variable=empty_packet_icrc depth=10
#pragma HLS stream variable=enable_icrc depth=10
#pragma HLS stream variable=icrc depth=57
    metadata.eth_src = 0x1;
    metadata.ip_src = 0x0a000001;
    metadata.udp_dst = 4791;
    metadata.udp_src = 4791;
    metadata_cache = metadata;
}

void custom_rx_ring::custom_ring(udp::udp_builder_metadata_stream& hdr_in, data_stream& data_in,
                                 udp::udp_builder_metadata_stream& hdr_out, data_stream& data_out,
                                 hls_ik::gateway_registers& r)
{
#pragma HLS inline
    gateway.gateway(r, [=](ap_uint<31> addr, int& data) -> int {
#pragma HLS inline
        if (addr & hls_ik::GW_WRITE)
            return reg_write(addr & ~hls_ik::GW_WRITE, data);
        else
            return reg_read(addr & ~hls_ik::GW_WRITE, &data);
    });

    ring_hdrs(hdr_in, hdr_out);
    dup(enable_stream, enable_bth, enable_icrc);
    dup(empty_packet, empty_packet_bth, empty_packet_icrc);
    push_bth.reorder(bth, empty_packet_bth, enable_bth, data_in, data_bth_to_icrc);
    push_icrc.reorder(data_bth_to_icrc, empty_packet_icrc, enable_icrc, icrc, data_out);
}

hls_ik::axi_data custom_rx_ring::gen_bth(const ring_context& context, ap_uint<16>& len)
{
    rxe_bth bth = {};
    bth.opcode = IB_OPCODE_UC_SEND_ONLY;
    bth.pkey = 0xffff;
    bth.qpn = context.dest_qpn;
    bth.apsn = context.psn;
    const ap_uint<2> pad_count = (-len) & 3;
    len += pad_count;
    bth.flags = ap_uint<8>(pad_count) << 4;

    ap_uint<256> data = (ap_uint<8>(bth.opcode), ap_uint<8>(bth.flags),
		    ap_uint<16>(bth.pkey), ap_uint<8>(0),
		    ap_uint<24>(bth.qpn),
		    ap_uint<32>(bth.apsn), ap_uint<(32 - IB_BTH_BYTES) * 8>(0));
    return hls_ik::axi_data(data, hls_ik::axi_data::keep_bytes(IB_BTH_BYTES), true);
}

void custom_rx_ring::ring_hdrs(udp::udp_builder_metadata_stream& hdr_in, udp::udp_builder_metadata_stream& hdr_out)
{
#pragma HLS pipeline enable_flush ii=3
    if (!metadata_updates.empty())
        metadata = metadata_updates.read();
    if (contexts.update())
        return;

    if (hdr_in.empty() || hdr_out.full() || bth.full() ||
        enable_stream.full() || empty_packet.full() || icrc.full())
        return;

    udp::udp_builder_metadata m = hdr_in.read();
    empty_packet.write(m.empty_packet());
    enable_stream.write(m.ring_id != 0);
    if (m.ring_id != 0) {
        // custom ring
        auto context = contexts.next_packet(m.ring_id);
        auto bth_flit = gen_bth(context, m.length);
        auto packet_metadata = metadata;
        packet_metadata.eth_dst = context.eth_dst;
        packet_metadata.ip_dst = context.ip_dst;
        m.set_packet_metadata(packet_metadata);
        bth.write(bth_flit);
        m.length += IB_BTH_BYTES + 4;
        m.ring_id = 0;
        icrc.write(0); // TODO calc icrc
    }
    hdr_out.write(m);
}

int custom_rx_ring::reg_read(int address, int* value)
{
#pragma HLS inline
    switch (address) {
    case CR_SRC_MAC_LO:
        *value = metadata_cache.eth_src(31, 0);
        break;
    case CR_SRC_MAC_HI:
        *value = metadata_cache.eth_src(47, 32);
        break;
    case CR_SRC_IP:
        *value = metadata_cache.ip_src;
        break;
    case CR_DST_UDP:
        *value = metadata_cache.udp_dst;
        break;
    case CR_SRC_UDP:
        *value = metadata_cache.udp_src;
        break;
    case CR_NUM_CONTEXTS:
        *value = contexts.size;
        break;
    case CR_DST_MAC_LO:
    case CR_DST_MAC_HI:
    case CR_DST_IP:
    case CR_DST_QPN:
    case CR_PSN:
        return contexts.gateway_read(address, value);
    default:
        *value = -1;
        return GW_FAIL;
    }

    return GW_DONE;
}

int custom_rx_ring::reg_write(int address, int value)
{
#pragma HLS inline
    switch (address) {
    case CR_SRC_MAC_LO:
        metadata_cache.eth_src(31, 0) = value;
        break;
    case CR_SRC_MAC_HI:
        metadata_cache.eth_src(47, 32) = value;
        break;
    case CR_SRC_IP:
        metadata_cache.ip_src = value;
        break;
    case CR_DST_UDP:
        metadata_cache.udp_dst = value;
        break;
    case CR_SRC_UDP:
        metadata_cache.udp_src = value;
        break;
    case CR_DST_MAC_LO:
    case CR_DST_MAC_HI:
    case CR_DST_IP:
    case CR_DST_QPN:
    case CR_PSN:
    case CR_WRITE_CONTEXT:
    case CR_READ_CONTEXT:
        return contexts.gateway_write(address, value);
    default:
        return GW_FAIL;
    }

    if (metadata_updates.full())
        return GW_BUSY;
    metadata_updates.write(metadata_cache);

    return GW_DONE;
}

#if !defined(__SYNTHESIS__)
void custom_rx_ring::verify()
{
    assert(icrc.empty());
}
#endif

int ring_context_manager::gateway_write(int address, int value)
{
#pragma HLS inline
    switch (address) {
    case CR_DST_MAC_LO:
        gateway_context.eth_dst(31, 0) = value;
        return GW_DONE;
    case CR_DST_MAC_HI:
        gateway_context.eth_dst(47, 32) = value;
        return GW_DONE;
    case CR_DST_IP:
	gateway_context.ip_dst = value;
        return GW_DONE;
    case CR_DST_QPN:
        gateway_context.dest_qpn = value;
        return GW_DONE;
    case CR_PSN:
        gateway_context.psn = value;
        return GW_DONE;
    case CR_WRITE_CONTEXT:
        return gateway_set(value - 1);
    case CR_READ_CONTEXT:
        return gateway_query(value - 1);
    default:
        return GW_FAIL;
    }
}

int ring_context_manager::gateway_read(int address, int* value)
{
#pragma HLS inline
    switch (address) {
    case CR_DST_MAC_LO:
        *value = gateway_context.eth_dst(31, 0);
        break;
    case CR_DST_MAC_HI:
        *value = gateway_context.eth_dst(47, 32);
        break;
    case CR_DST_IP:
        *value = gateway_context.ip_dst;
        break;
    case CR_DST_QPN:
        *value = gateway_context.dest_qpn;
        break;
    case CR_PSN:
        *value = gateway_context.psn;
        break;
    default:
        *value = -1;
        return GW_FAIL;
    }

    return GW_DONE;
}

ring_context ring_context_manager::next_packet(hls_ik::ring_id_t ring_id)
{
#pragma HLS resource variable=contexts[0].psn core=RAM_T2P_BRAM
    auto ret = (*this)[ring_id - 1];
    (*this)[ring_id - 1].psn++;
    return ret;
}

/* For quick HLS evaluation */
void custom_ring_top(udp::udp_builder_metadata_stream& hdr_in, data_stream& data_in,
    udp::udp_builder_metadata_stream hdr_out, data_stream& data_out,
    hls_ik::gateway_registers& r)
{
#pragma HLS dataflow
    static custom_rx_ring ring;
    ring.custom_ring(hdr_in, data_in, hdr_out, data_out, r);
}
