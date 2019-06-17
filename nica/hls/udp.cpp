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
#include <hls_helper.h>
#include <boost/preprocessor/iteration/local.hpp>

using namespace hls_helpers;

namespace udp {

/* basic UDP full packet format:
   0x00 - eth.dest[5:2]
   0x04 - eth.dest[0:1] eth.src[5:4]
   0x08 - eth.src[3:0]
   0x0c - ethertype[1:0] ip.ihl ip.version ip.tos
   0x10 - ip.totlen ip.id
   0x14 - ip.frag_off ip.ttl ip.protocol
   0x18 - ip.checksum ip.saddr[3:2]
   0x1c - ip.saddr[1:0] ip.daddr[3:2]
   0x20 - ip.daddr[1:0] udp.source-port
   0x24 - udp.dest-port udp.len
   0x28 - udp.checksum data[2 bytes] */

header_parser::header_parser(const header_buffer& buf) :
    eth(buf.hdr(width - 1, udp_header::width + ip_header::width)),
    ip(buf.hdr(udp_header::width + ip_header::width - 1, udp_header::width)),
    udp(buf.hdr(udp_header::width - 1, 0))
{}

header_parser::operator header_buffer()
{
    HEADER_BUFFER(buf, *this, 0, 0, true);
    return buf;
}

header_data_split::header_data_split() :
    state(IDLE),
    meta(mlx::metadata())
{}

steering::steering() :
    checks_to_stats("checks_to_stats"),
    checks_to_actions("checks_to_actions"),
    hdr_dup_to_dropper("hdr_dup_to_dropper"),
    hdr_dup_to_checks("hdr_dup_to_checks"),
    hdr_dup_to_flow_table("hdr_dup_to_flow_table"),
    matched("matched"),
    dropper(true) /* empty_packets_have_data */
{
    HEADER_BUFFER(buf, 0, -1, -1, true);
}

void steering::hdr_checks(const config& config)
{
#pragma HLS PIPELINE enable_flush ii=1
    if (hdr_dup_to_checks.empty() || checks_to_actions.full())
        return;

    checks c;
    header_buffer buf = hdr_dup_to_checks.read();
    header_parser hdr = buf;

    /* TODO check that all fields are under TKEEP */
    c.disabled = !config.enable;
    c.not_ipv4 = hdr.eth.proto != ETH_P_IP;
    c.bad_length = hdr.ip.tot_len < sizeof(iphdr) + sizeof(udphdr);
    c.not_udp = hdr.ip.protocol != IPPROTO_UDP;

    checks_to_actions.write(c);
    checks_to_stats.write_nb(c);
}

void steering::checks_to_action(const config& cfg, result_stream& result_out)
{
#pragma HLS pipeline enable_flush ii=1
    if (checks_to_actions.empty() || ft_to_action.empty() ||
        ft_results.full() || result_out.full())
        return;

    checks c = checks_to_actions.read();
    c.ft_result = ft_to_action.read();

    if (c.disabled || c.not_ipv4 || c.bad_length || c.not_udp)
        c.ft_result.v.action = FT_PASSTHROUGH;

    ft_results.write(c.ft_result);
    /* If the action is to the ikernel, pass it out to the crossbar */
    if (c.ft_result.v.action == FT_IKERNEL)
        result_out.write(c.ft_result);
}

void steering::update_stats_checks(hds_stats* s)
{
#pragma HLS pipeline enable_flush ii=1
    s->passthrough_disabled = stats.passthrough_disabled;
    s->passthrough_not_ipv4 = stats.passthrough_not_ipv4;
    s->passthrough_bad_length = stats.passthrough_bad_length;
    s->passthrough_not_udp = stats.passthrough_not_udp;

    if (checks_to_stats.empty())
        return;

    checks c = checks_to_stats.read();
    if (c.disabled) {
        ++stats.passthrough_disabled;
    }
    if (c.not_ipv4) {
        ++stats.passthrough_not_ipv4;
    }
    if (c.bad_length) {
        ++stats.passthrough_bad_length;
    }
    if (c.not_udp) {
        ++stats.passthrough_not_udp;
    }
}

void steering::update_stats_actions(bool_stream& pass_raw, hds_stats* s)
{
#pragma HLS pipeline enable_flush ii=1
    s->ft_action_passthrough = stats.ft_action_passthrough;
    s->ft_action_drop = stats.ft_action_drop;
    s->ft_action_ikernel = stats.ft_action_ikernel;

    if (ft_results.empty() || matched.full() || pass_raw.full())
        return;

    flow_table_result ft = ft_results.read();
    matched.write(ft.v.action == FT_IKERNEL);
    pass_raw.write(ft.v.action == FT_PASSTHROUGH);

    switch (ft.v.action) {
    case FT_PASSTHROUGH:
        ++stats.ft_action_passthrough;
        break;
    case FT_DROP:
        ++stats.ft_action_drop;
        break;
    case FT_IKERNEL:
        ++stats.ft_action_ikernel;
        break;
    }
}

void steering::steer(header_stream& hdr_in, hls_ik::data_stream& data_in, bool_stream& pass_raw,
                     header_stream& hdr_out, hls_ik::data_stream& data_out,
                     result_stream& result_out,
                     config* config, hds_stats* s)
{
#pragma HLS inline
    DO_PRAGMA(HLS STREAM variable=hdr_dup_to_dropper depth=16);
    DO_PRAGMA(HLS STREAM variable=hdr_dup_to_flow_table depth=FIFO_FLOW_TABLE_PACKETS);
    DO_PRAGMA(HLS STREAM variable=hdr_dup_to_checks depth=FIFO_FLOW_TABLE_PACKETS);
    DO_PRAGMA(HLS STREAM variable=hdr_dup_to_dropper depth=FIFO_FLOW_TABLE_PACKETS);
    DO_PRAGMA(HLS STREAM variable=matched depth=FIFO_PACKETS);
    DO_PRAGMA(HLS STREAM variable=checks_to_actions depth=FIFO_FLOW_TABLE_PACKETS);
    DO_PRAGMA(HLS STREAM variable=ft_to_action depth=FIFO_FLOW_TABLE_PACKETS);

    DO_PRAGMA_SYN(HLS data_pack variable=hdr_dup_to_dropper);
    DO_PRAGMA_SYN(HLS data_pack variable=hdr_dup_to_checks);
    DO_PRAGMA_SYN(HLS data_pack variable=hdr_dup_to_flow_table);
    DO_PRAGMA_SYN(HLS data_pack variable=checks_to_stats);
    DO_PRAGMA_SYN(HLS data_pack variable=checks_to_actions);
    DO_PRAGMA_SYN(HLS data_pack variable=ft_to_action);
    DO_PRAGMA_SYN(HLS data_pack variable=ft_results);

    hdr_dup.dup3(hdr_in, hdr_dup_to_dropper, hdr_dup_to_checks, hdr_dup_to_flow_table);
    hdr_checks(*config);
    ft.ft_step(hdr_dup_to_flow_table, ft_to_action, config->flow_table_gateway);
    checks_to_action(*config, result_out);
    update_stats_checks(s);
    update_stats_actions(pass_raw, s);
    dropper.udp_dropper_step(matched, hdr_dup_to_dropper, data_in, hdr_out,
                             data_out);
}

void header_data_split::split(header_stream& header, hls_ik::data_stream& data)
{
#pragma HLS PIPELINE enable_flush
    ntl::axi_data cur;
    /* Where does the header end and data start in the second word, in bits */
    const int data_start = 512 - header_buffer::width;

    switch (state) {
    case IDLE:
idle:
        if (!extract_metadata.out_metadata.empty() && !extract_metadata.out_data.empty()) {
            cur = extract_metadata.out_data.read();
            buffer = cur.data;
            meta = extract_metadata.out_metadata.read();
            state = READING_HEADER;
        }
        break;
    case READING_HEADER:
        if (!extract_metadata.out_data.empty() && !header.full()) {
            cur = extract_metadata.out_data.read();

            HEADER_BUFFER(buf,
                (buffer, cur.data(255, data_start)),
                meta.id, meta.user, false);
            buffer = cur.data;
            state = cur.last ? LAST : STREAM;
            header.write(buf);
        }
        break;
    case STREAM:
        if (!extract_metadata.out_data.empty() && !data.full()) {
            cur = extract_metadata.out_data.read();

            hls_ik::axi_data buf = hls_ik::axi_data((ap_uint<256>((buffer(data_start - 1, 0),
                                                    cur.data(255, data_start)))), 0xffffffff, 0);
            data.write(buf);
            buffer = cur.data;
            state = cur.last ? LAST : STREAM;
        }
        break;
    case LAST:
        if (data.full())
            break;

        ap_uint<data_start> last_part = buffer(data_start - 1, 0);
        hls_ik::axi_data buf((last_part, ap_uint<256 - data_start>(0)),
                             hls_ik::axi_data::keep_bytes(32 - data_start / 8), true);
        data.write(buf);
        state = IDLE;
        goto idle;
    }
}

void header_data_split::step(mlx::stream& in,
                             header_stream& header, hls_ik::data_stream& data)
{
#pragma HLS inline
    extract_metadata.step(in);
    split(header, data);
}

length_adjust::length_adjust() :
    state(IDLE),
    data_buf_valid(false),
    packets("length_adjust_packets")
{}

void length_adjust::find_length(header_stream& hdr_in)
{
#pragma HLS pipeline enable_flush

    /* TODO this assumes a fixed size IP header */
	constexpr int header_length = udp_header::width / 8 + ip_header::width / 8;
    header_buffer buf;
    packet_metadata pkt;

    if (hdr_in.empty() || packets.full())
        return;

    hdr_in.read(buf);
    header_parser hdr = buf;

    pkt.tot_len = hdr.ip.tot_len;
    ap_uint<16> data_length = (pkt.tot_len - header_length);
    pkt.last_word_data = data_length(4, 0);
    pkt.word_count = data_length(15, 5) + !!pkt.last_word_data;
    DBG_DECL(pkt.pkt_id = buf.pkt_id);

    packets.write(pkt);
}

void length_adjust::cut_data(hls_ik::data_stream& data_in, hls_ik::data_stream& data_out)
{
#pragma HLS pipeline enable_flush
    switch (state) {
    case IDLE:
        if (!packets.empty()) {
            pkt = packets.read();
            state = pkt.word_count ? COUNT : IDLE;
        }
    	break;
    case COUNT:
        if (data_buf_valid && !data_out.full()) {
            if (!--pkt.word_count) {
                state = data_buf.last ? IDLE : CONSUME;
                data_buf.keep = mlx::last_word_keep_num_bytes_valid(pkt.last_word_data);
                data_buf.last = 1;
            } else {
                if (data_buf.last)
                    state = IDLE;
            }
            data_out.write(data_buf);
            data_buf_valid = false;
        }

        if (!data_buf_valid && !data_in.empty()) {
            data_buf = data_in.read();
            data_buf_valid = true;
        }
        break;
    case CONSUME:
        if (data_buf_valid) {
            if (data_buf.last) {
                state = IDLE;
            }
            data_buf_valid = false;
        }
        if (!data_in.empty()) {
            data_buf = data_in.read();
            data_buf_valid = true;
        }
        break;
   }
}

void length_adjust::adjust(header_stream& hdr_in, hls_ik::data_stream& data_in, hls_ik::data_stream& data_out)
{
#pragma HLS inline

    DO_PRAGMA(HLS STREAM variable=packets depth=1);
    DO_PRAGMA_SYN(HLS data_pack variable=packets);

    find_length(hdr_in);
    cut_data(data_in, data_out);
}

void checksum::ipv4_checksum(ip_header hdr)
{
     /* Clear checksum */
    hdr.check = 0;
    ap_uint<ip_header::width> d = hdr;

    const unsigned bits_per_split = ip_header::width / ip_splits;

    for (unsigned split = 0; split < ip_splits; ++split) {
        cur_checksum.ip_checksum[split] = 0;
        for (unsigned i = split * bits_per_split; i < (split + 1) *
bits_per_split; i += 16)
            cur_checksum.ip_checksum[split] += d(i + 15, i);
    }
}

checksum_t header_parser::checksum_from_packet()
{
    checksum_t from_packet = {
        ip_checksum: ip.check,
        udp_checksum: udp.checksum
    };
    return from_packet;
}

void checksum::udp_pseudoheader_checksum(const header_parser& hdr)
{
    for (int i = 0; i < num_splits; ++i)
        cur_checksum.udp_checksum[i] = 0;

    cur_checksum.udp_checksum[0 % num_splits] += hdr.ip.saddr(15, 0);
    cur_checksum.udp_checksum[1 % num_splits] += hdr.ip.saddr(31, 16);
    cur_checksum.udp_checksum[2 % num_splits] += hdr.ip.daddr(15, 0);
    cur_checksum.udp_checksum[3 % num_splits] += hdr.ip.daddr(31, 16);
    cur_checksum.udp_checksum[4 % num_splits] += (hdr.ip.protocol, ap_uint<8>(0));
    cur_checksum.udp_checksum[5 % num_splits] += hdr.udp.length;
    cur_checksum.udp_checksum[6 % num_splits] += hdr.udp.source;
    cur_checksum.udp_checksum[7 % num_splits] += hdr.udp.dest;
    cur_checksum.udp_checksum[8 % num_splits] += hdr.udp.length;
}

header_parser header_parser::reply() const
{
    header_parser result(*this);

    result.eth.source = eth.dest;
    result.eth.dest = eth.dest;
    result.ip.saddr = ip.daddr;
    result.ip.daddr = ip.saddr;
    result.udp.source = udp.dest;
    result.udp.dest = udp.source;

    return result;
}

void checksum::checksum_step(header_stream& hdr_in, hls_ik::data_stream& data_in,
                             stream& checksum)
{
#pragma HLS inline
    checksum_header_and_data(hdr_in, data_in);
    DO_PRAGMA(HLS STREAM variable=intermediate_stream depth=FIFO_PACKETS);
    finish_checksum(checksum);
}

void checksum::checksum_header_and_data(header_stream& hdr_in, hls_ik::data_stream& data_in)
{
#pragma HLS PIPELINE enable_flush

    switch (state) {
    case IDLE:
        if (!hdr_in.empty() && !intermediate_stream.full()) {
            header_buffer buf = hdr_in.read();
            header_parser hdr = buf;
#ifndef NDEBUG
            pkt_id = buf.pkt_id;
#endif
            ipv4_checksum(hdr.ip);
            udp_pseudoheader_checksum(hdr);
            if (hdr.udp.empty_packet()) {
                intermediate_stream.write(cur_checksum);
            } else {
                state = DATA;
            }
        }
        break;
   case DATA:
        if (!data_in.empty() && !intermediate_stream.full()) {
            hls_ik::axi_data word = data_in.read();
            ap_uint<MLX_AXI4_WIDTH_BITS> masked_word = mask_last_word(word);
            checksum_step_data_loop:
            for (int i = 0; i < MLX_AXI4_WIDTH_BYTES / 2; ++i) {
    #pragma HLS UNROLL
                cur_checksum.udp_checksum[i & (num_splits - 1)] += masked_word(16 * i + 15, 16 * i);
            }

            if (word.last) {
                intermediate_stream.write(cur_checksum);
                state = IDLE;
            }
        }
        break;
    }
}

void checksum::finish_checksum(stream& checksum)
{
#pragma HLS PIPELINE enable_flush ii=3
    if (intermediate_stream.empty() || checksum.full())
        return;

    intermediate_checksum cur = intermediate_stream.read();

    ap_uint<32> ip_result = 0;
    ap_uint<32> udp_result = 0;
    for (int i = 0; i < num_splits; ++i)
        udp_result += cur.udp_checksum[i];
    for (int i = 0; i < ip_splits; ++i)
        ip_result += cur.ip_checksum[i];
    for (int i = 0; i < 2; ++i)
        udp_result = udp_result(15, 0) + udp_result(31, 16);
    for (int i = 0; i < 2; ++i)
        ip_result = ip_result(15, 0) + ip_result(31, 16);

    checksum_t result = {
        ip_checksum: ~ip_result,
        udp_checksum: ~udp_result,
    };

    checksum.write(result);

}

bool checksum_validate::operator()(checksum_t sum, header_buffer buf)
{
    header_parser hdr = buf;

    return sum.ip_checksum == hdr.ip.check &&
           (sum.udp_checksum == hdr.udp.checksum || !hdr.udp.checksum);
}

void udp_dropper::udp_dropper_step(bool_stream& pass,
                       header_stream& header_in, hls_ik::data_stream& data_in,
                       header_stream& header_out, hls_ik::data_stream& data_out)
{
#pragma HLS pipeline enable_flush
    switch (state) {
    case IDLE:
        if (!pass.empty() && !header_in.empty() && !header_out.full()) {
            header_buffer buf = header_in.read();
            header_parser hdr = buf;
            drop = !pass.read();
            if (!drop)
                header_out.write(buf);
            if (!hdr.udp.empty_packet()) {
                state = STREAM;
            } else if (empty_packets_have_data) {
                state = STREAM;
                drop = true;
            }
            DBG_DECL(pkt_id = buf.pkt_id);
        }
        break;
    case STREAM:
        if (data_buf_valid && !data_out.full()) {
            state = data_buf.last ? IDLE : STREAM;
            if (!drop)
                data_out.write(data_buf);
            data_buf_valid = false;
        }
        if (!data_buf_valid && !data_in.empty()) {
            data_buf = data_in.read();
            data_buf_valid = true;
        }
    }
}

udp::udp() :
    udp_dropper_instance(false), /* empty_packets_have_data */
    data_steer_to_length("data_steer_to_length"),
    header_dup_to_length("header_dup_to_length")
{
}

void udp::crossbar(header_stream& hdr_in, hls_ik::data_stream& data_in, result_stream& steer_results,
         BOOST_PP_ENUM_PARAMS(NUM_IKERNELS, header_stream& hdr_out),
         BOOST_PP_ENUM_PARAMS(NUM_IKERNELS, result_stream& ft_results),
         BOOST_PP_ENUM_PARAMS(NUM_IKERNELS, hls_ik::data_stream& data_out))
{
#pragma HLS pipeline enable_flush ii=1

    header_buffer buf;

    switch (state) {
    case IDLE:
        if (steer_results.empty() || hdr_in.empty()
#define BOOST_PP_LOCAL_MACRO(n) \
            || (hdr_out ## n).full() \
            || (ft_results ## n).full()
#define BOOST_PP_LOCAL_LIMITS (0, NUM_IKERNELS - 1)
%:include BOOST_PP_LOCAL_ITERATE()
        )
            return;

        current_steering_decision = steer_results.read();
#define BOOST_PP_LOCAL_MACRO(n) \
        if (current_steering_decision.v.engine_id == n) { \
            buf = hdr_in.read(); \
            (hdr_out ## n).write(buf); \
            (ft_results ## n).write(current_steering_decision); \
        }
#define BOOST_PP_LOCAL_LIMITS (0, NUM_IKERNELS - 1)
%:include BOOST_PP_LOCAL_ITERATE()

        state = header_parser(buf.hdr).udp.empty_packet() ? IDLE : STREAM;
        break;

    case STREAM:
#define BOOST_PP_LOCAL_MACRO(n) \
        if (current_steering_decision.v.engine_id == n) { \
            if (data_in.empty() || (data_out ## n).full()) \
                return; \
            \
            hls_ik::axi_data buf = data_in.read(); \
            (data_out ## n).write(buf); \
            state = buf.last ? IDLE : STREAM; \
        }
#define BOOST_PP_LOCAL_LIMITS (0, NUM_IKERNELS - 1)
%:include BOOST_PP_LOCAL_ITERATE()

        break;
    }
}

void udp::udp_step(mlx::stream& in,
                   BOOST_PP_ENUM_PARAMS(NUM_IKERNELS, header_stream& header_out),
                   BOOST_PP_ENUM_PARAMS(NUM_IKERNELS, result_stream& ft_results),
                   BOOST_PP_ENUM_PARAMS(NUM_IKERNELS, hls_ik::data_stream& data_out),
                   bool_stream& bool_pass_raw,
                   config* config, udp_stats* stats)
{
#pragma HLS interface ap_none port=config
#pragma HLS inline

    DO_PRAGMA_SYN(HLS data_pack variable=data_split_to_steer);
    DO_PRAGMA_SYN(HLS data_pack variable=data_steer_to_length);
    DO_PRAGMA_SYN(HLS data_pack variable=data_length_to_crossbar);
    DO_PRAGMA_SYN(HLS data_pack variable=header_split_to_steer);
    DO_PRAGMA_SYN(HLS data_pack variable=header_steer_to_dup);
    DO_PRAGMA_SYN(HLS data_pack variable=header_dup_to_crossbar);
    DO_PRAGMA_SYN(HLS data_pack variable=header_dup_to_length);

    DO_PRAGMA(HLS STREAM variable=header_split_to_steer depth=FIFO_PACKETS);
    DO_PRAGMA(HLS STREAM variable=data_split_to_steer depth=FIFO_WORDS);
    DO_PRAGMA(HLS STREAM variable=data_steer_to_length depth=FIFO_WORDS);
    DO_PRAGMA(HLS STREAM variable=data_length_to_crossbar depth=FIFO_WORDS);
    DO_PRAGMA(HLS STREAM variable=header_steer_to_dup depth=FIFO_PACKETS);
    DO_PRAGMA(HLS STREAM variable=header_dup_to_length depth=FIFO_PACKETS);
    DO_PRAGMA(HLS STREAM variable=header_dup_to_crossbar depth=FIFO_PACKETS);

    hds.step(in, header_split_to_steer, data_split_to_steer);
    steer.steer(header_split_to_steer, data_split_to_steer, bool_pass_raw,
                header_steer_to_dup, data_steer_to_length, steer_results, config, &stats->hds);
    hdr_dup.dup2(header_steer_to_dup,
        header_dup_to_length,
        header_dup_to_crossbar);
    length_adjuster.adjust(header_dup_to_length, data_steer_to_length,
                           data_length_to_crossbar);
    crossbar(header_dup_to_crossbar, data_length_to_crossbar, steer_results,
             BOOST_PP_ENUM_PARAMS(NUM_IKERNELS, header_out),
             BOOST_PP_ENUM_PARAMS(NUM_IKERNELS, ft_results),
             BOOST_PP_ENUM_PARAMS(NUM_IKERNELS, data_out));
}

udp_builder::udp_builder()
{}

header_parser header_to_mlx::metadata_to_header(const hls_ik::metadata& m)
{
    header_parser hdr;
    hls_ik::packet_metadata pkt = m.get_packet_metadata();
    hdr.eth.dest = pkt.eth_dst;
    hdr.eth.source = pkt.eth_src;
    hdr.eth.proto = ETH_P_IP;
    hdr.ip.version = 4;
    hdr.ip.ihl = ip_header::width / 8 / 4;
    hdr.ip.tot_len = (hdr.udp.width + hdr.ip.width) / 8 + m.length;
    hdr.ip.id = m.ip_identification;
    hdr.ip.ttl = 64;
    hdr.ip.protocol = IPPROTO_UDP;
    hdr.ip.daddr = pkt.ip_dst;
    hdr.ip.saddr = pkt.ip_src;
    hdr.udp.dest = pkt.udp_dst;
    hdr.udp.source = pkt.udp_src;
    hdr.udp.length = hdr.udp.width / 8 + m.length;

    return hdr;
}

void header_to_mlx::hdr_to_mlx(udp_builder_metadata_stream& in, hls_ik::data_stream& out,
                               bool_stream& empty_packet,
                               mlx::metadata_stream& metadata_out, bool_stream& enable_stream)
{
#pragma HLS pipeline enable_flush
    switch (state)
    {
    case IDLE: {
        if (in.empty() || out.full() || empty_packet.full() ||
	    metadata_out.full() || enable_stream.full())
            break;

        udp_builder_metadata m = in.read();
	mlx::metadata mlx_metadata = {};
        metadata_out.write(mlx_metadata);

        header_parser hdr = metadata_to_header(m);
        header_buffer buf = hdr;
        ap_uint<256> word = buf.hdr(hdr.width - 1, hdr.width - 256);
        buffer = buf.hdr(buffer_size - 1, 0);

        hls_ik::axi_data output(word, 0xffffffff, false);
        const bool is_udp = m.pkt_type == PKT_TYPE_UDP;
        if (is_udp)
            out.write(output);

        empty_packet.write(is_udp && hdr.udp.empty_packet());
        enable_stream.write(is_udp);

        state = is_udp ? SECOND: IDLE;
        break;
    }
    case SECOND:
        hls_ik::axi_data out_buf(
            (buffer, ap_uint<MLX_AXI4_WIDTH_BITS - buffer_size>(0)),
            hls_ik::axi_data::keep_bytes(buffer_size / 8),
            true);
        out.write(out_buf);
        state = IDLE;
    }
}

ethernet_padding::ethernet_padding() {}

void ethernet_padding::pad(mlx::stream& in, mlx::stream& out)
{
#pragma HLS pipeline enable_flush ii=1
    if (in.empty() || out.full())
        return;

    mlx::axi4s beat = in.read();
    if (beat.last && beat_count == 1) {
        beat.data = pad_one(beat.data, beat.keep);
        beat.keep = beat.keep | 0xfffffff0;
    }
    out.write(beat);
    if (beat.last)
        beat_count = 0;
    else
        ++beat_count;
}

ap_uint<256> ethernet_padding::pad_one(ap_uint<256> data, ap_uint<32> keep)
{
    for (int i = 0; i < 32; ++i) {
        data(8 * i + 7, 8 * i) = keep(i, i) ? data(8 * i + 7, 8 * i) : 0;
    }

    return data;
}

void udp_builder::builder_step(udp_builder_metadata_stream& header_in, hls_ik::data_stream& data_in,
                               mlx::stream &out)
{
#pragma HLS inline
    DO_PRAGMA(HLS STREAM variable=data_hdr_to_reorder depth=16);
    DO_PRAGMA(HLS STREAM variable=raw_reorder_to_reg depth=FIFO_WORDS);
    DO_PRAGMA(HLS STREAM variable=data_reorder_to_join depth=5);

    DO_PRAGMA_SYN(HLS data_pack variable=data_hdr_to_reorder);
    DO_PRAGMA_SYN(HLS data_pack variable=raw_reorder_to_reg);

    hdr2mlx.hdr_to_mlx(header_in, data_hdr_to_reorder, empty_packet,
                       mlx_metadata, enable_stream);
    merger.reorder(data_hdr_to_reorder, empty_packet, enable_stream, data_in, data_reorder_to_join);
    join_pkt_metadata(mlx_metadata, data_reorder_to_join, raw_reorder_to_reg);
    link.link(raw_reorder_to_reg, out);
}

}

/* For testing C synthesis */
void udp_top(mlx::stream& in,
             udp::header_stream& header_out,
             result_stream& ft_results,
             hls_ik::data_stream& data_out,
             udp::bool_stream& bool_pass_raw,
             udp::config* cfg, udp::udp_stats* stats)
{
#pragma HLS interface axis port=in
#pragma HLS interface axis port=data_out
#pragma HLS INTERFACE s_axilite port=cfg->enable offset=0x10
    GATEWAY_OFFSET(cfg->flow_table_gateway, 0x18, 0x20, 0x30)
    GATEWAY_OFFSET(cfg->arbiter_gateway, 0x58, 0x60, 0x70)
#pragma HLS INTERFACE s_axilite port=stats offset=0x100

    static udp::udp u;

    u.udp_step(in, header_out, ft_results, data_out, bool_pass_raw, cfg, stats);
}
