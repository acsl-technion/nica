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

#include "threshold.hpp"
#include "threshold-impl.hpp"
#include "hls_helper.h"

#include <algorithm>
using std::min;
using std::max;

using namespace hls_ik;

ring_id_t threshold_contexts::find_ring(const ikernel_id_t& id)
{
    return (*this)[id].ring_id;
}

int threshold_contexts::rpc(int address, int *v, ikernel_id_t ikernel_id, bool read)
{
#pragma HLS inline
    uint32_t index = ikernel_id;

    switch (address) {
    case THRESHOLD_VALUE:
	return gateway_access_field<value, &threshold_context::threshold_value>(index, v, read);
    case THRESHOLD_RING_ID:
	return gateway_access_field<ring_id_t, &threshold_context::ring_id>(index, v, read);
    case THRESHOLD_FLAGS:
        return gateway_access_field<ap_uint<32>, &threshold_context::flags>(index, v, read);
    case THRESHOLD_RESET:
        return gateway_rmw(index, [](threshold_context c) -> threshold_context {
            c.threshold_value = 0;
            c.ring_id = 0;
            c.flags = 0;
            return c;
        });
        break;
    default:
        if (read)
            *v = -1;
        return GW_FAIL;
    }

    return GW_DONE;
}

int threshold_stats_contexts::rpc(int address, int *v, ikernel_id_t ikernel_id, bool read)
{
#pragma HLS inline
    uint32_t index = ikernel_id;

    switch (address) {
    case THRESHOLD_MIN:
	return gateway_access_field<value, &threshold_stats_context::min>(index, v, read);
    case THRESHOLD_MAX:
	return gateway_access_field<value, &threshold_stats_context::max>(index, v, read);
    case THRESHOLD_COUNT:
	return gateway_access_field<value, &threshold_stats_context::count>(index, v, read);
    case THRESHOLD_SUM_LO:
        return gateway_rmw(index, [read, v](const threshold_stats_context& c) -> threshold_stats_context {
            if (read)
                *v = c.sum(31, 0);
            else
                c.sum(31, 0) = *v;
            return c;
        });
    case THRESHOLD_SUM_HI:
        return gateway_rmw(index, [read, v](const threshold_stats_context& c) -> threshold_stats_context {
            if (read)
                *v = c.sum(63, 32);
            else
                c.sum(63, 32) = *v;
            return c;
        });
        break;
    case THRESHOLD_DROPPED:
	return gateway_access_field<value, &threshold_stats_context::dropped>(index, v, read);
    case THRESHOLD_DROPPED_BACKPRESSURE:
	return gateway_access_field<value, &threshold_stats_context::dropped_backpressure>(index, v, read);
    case THRESHOLD_RESET:
        return gateway_rmw(index, [](threshold_stats_context c) -> threshold_stats_context {
            c.min = 0;
            c.max = 0;
            c.count = 0;
            c.dropped = 0;
            c.dropped_backpressure = 0;
            c.sum = 0;
            return c;
        });
        break;
    default:
        if (read)
            *v = -1;
        return GW_FAIL;
    }

    return GW_DONE;
}

threshold::threshold() :
    parsed("parsed"),
    data_dup_to_parser("data_dup_to_parser"),
    data_dup_to_egress("data_dup_to_egress"),
    _data_egress_to_filter("_data_egress_to_filter"),
    net_to_parser("net_to_parser"),
    parser1_to_parsed("parser1_to_parsed"),
    decisions("decisions"),
    _decision_pass("_decision_pass"),
    parser_message(-1)
{}

void threshold::net_ingress(hls_ik::pipeline_ports& p)
{
#pragma HLS pipeline enable_flush ii=3
    if (contexts.update())
        return;

    if (p.metadata_input.empty() || net_to_parser.full())
        return;

    hls_ik::metadata meta = p.metadata_input.read();
    ring_id_t ring_id = contexts.find_ring(meta.ikernel_id);
    value threshold_value = contexts[meta.ikernel_id].threshold_value;
    ap_uint<1> flags = contexts[meta.ikernel_id].flags;

    if (ring_id != 0) {
        custom_ring_metadata cr;
        meta.ring_id = ring_id;
        cr.end_of_message = 1;
        meta.var = cr;
        meta.length = 4;
        meta.verify();
    }

    threshold_metadata threshold_m = {
        meta, threshold_value, flags };
    net_to_parser.write_nb(threshold_m);
}

void threshold::ingress_parsed(hls_ik::pipeline_ports& p,
                               hls_ik::tc_pipeline_data_counts& tc)
{
#pragma HLS pipeline enable_flush ii=3
    if (update())
        return;
    if (stats.update())
        return;

    if (parser1_to_parsed.empty() || parsed.empty() || decisions.full() ||
        _decision_pass.full())
        return;

    threshold_metadata meta;
    parser1_to_parsed.read_nb(meta);

    value v = parsed.read();
    bool backpressure = !can_transmit(tc, meta.meta.ikernel_id, meta.meta.ring_id, 4, HOST);
    bool drop = v < meta.threshold_value || backpressure;
    if (!drop) {
        if (meta.meta.ring_id != 0) {
            new_message(meta.meta.ring_id, HOST);
        }
        p.metadata_output.write(meta.meta);
    }

    decisions.write(decision_t{v, meta.meta.ring_id, meta.flags});
    _decision_pass.write(!drop);
    update_stats(meta.meta.ikernel_id, v, drop, backpressure);
}

void threshold::egress()
{
    axi_data d;
#pragma HLS pipeline enable_flush ii=1
    // TODO support UDP mode by tracking the relations of decisions and
    // data_dup_to_egress packets.
    ap_uint<axi_data::width> raw;
    data_dup_to_egress.read_nb(raw);

    switch (egress_state)
    {
    case IDLE:
        if (decisions.empty() || _data_egress_to_filter.full())
            return;

        decisions.read_nb(egress_last_decision);

        if (egress_last_decision.ring_id != 0) {
            d = axi_data((egress_last_decision.v, ap_uint<256 - value::width>()), 0xf0000000, true);
            _data_egress_to_filter.write_nb(d);
        }
        break;

    case STREAM:
        break;
    }
}

void threshold::parser0()
{
#pragma HLS pipeline enable_flush ii=1
    ap_uint<axi_data::width> raw;
    hls_ik::ikernel_id_t ikernel_id;

    if (parser_reset.read_nb(ikernel_id)) {
        parser_state = PARSER_IDLE;
        parser_offset = 0;
        parser_message_offset = 0;
        parser_len = 0;
        parser_flit = {};
        parser_metadata = {};
        return;
    }

    switch (parser_state) {
    case PARSER_IDLE:
        if (net_to_parser.empty())
            return;

        net_to_parser.read_nb(parser_metadata);

        parser_state = PARSER_NEW_FLIT;
        parser_offset = 0;

        tcp = parser_metadata.flags & THRESHOLD_FLAG_TCP;
        break;

    case PARSER_NEW_FLIT:
        if (data_dup_to_parser.empty())
            return;

        data_dup_to_parser.read_nb(raw);

        parser_flit = raw;
        parser_len = parser_flit.num_kept_bytes();
        parser_state = PARSER_LOOP;

        break;

    case PARSER_LOOP: {
        if (parser0_to_1.full())
            return;

        ap_uint<6> begin = parser_offset;
        ap_uint<6> end = min(parser_len, ap_uint<6>(int(begin) + 18 - int(parser_message_offset)));
        ap_uint<6> cur_len = end - begin;

        parser0_to_1.write_nb(parser0_out{
            begin, end,
            parser_message_offset, cur_len + parser_message_offset,
            parser_metadata,
            parser_flit.data});

        if (tcp) {
            /* std::cout << "tracing parser offset " << parser_offset << ", message offset " << parser_message_offset << 
                         ", cur len " << cur_len << "\n"; */
            if (end == parser_len) {
                parser_state = parser_flit.last ? PARSER_IDLE : PARSER_NEW_FLIT;
            }
            parser_offset += cur_len;
            if (parser_offset >= 32)
                parser_offset -= 32;
            parser_message_offset += cur_len;
            if (parser_message_offset >= 18)
                parser_message_offset -= 18;
        } else {
            parser_state = parser_flit.last ? PARSER_IDLE : PARSER_REST;
            parser_message_offset = 0;
        }
        break;
    }
    case PARSER_REST:
        if (!data_dup_to_parser.read_nb(raw))
            return;

        parser_flit = raw;
        parser_state = parser_flit.last ? PARSER_IDLE : PARSER_REST;
        break;
    }
}

void threshold::parser1()
{
#pragma HLS pipeline enable_flush ii=1
    if (parser0_to_1.empty() || parsed.full() || parser1_to_parsed.full())
        return;

    parser0_out p;
    parser0_to_1.read_nb(p);

    int hi = 8 * (18 - int(p.output_hi)) - 1;
    int lo = 8 * (18 - int(p.output_lo));
    assert(hi > 0 && hi < 8 * 18);
    assert(lo >= 0 && lo < 8 * 18 - 1);
    parser_message(hi, lo) = p.data(255 - p.input_hi * 8, 256 - p.input_lo * 8);
    if (p.output_lo == 18) {
        value v = parser_message(31, 0);
        // std::cout << "parser output: " << v << "\n";
        parsed.write_nb(v);
        parser1_to_parsed.write_nb(p.meta);
        parser_message = ap_uint<18 * 8>(-1);
    }
}

void threshold::update_stats(ikernel_id_t id, value v, bool drop, bool backpressure)
{
#pragma HLS inline
    threshold_stats_context& c = stats[id];
    c.min = std::min(c.min, v);
    c.max = std::max(c.max, v);
    c.sum += v;
    ++c.count;
    if (drop && !backpressure)
        ++c.dropped;
    if (backpressure) {
        ++c.dropped_backpressure;
    }
}

void threshold::step(hls_ik::ports& p, hls_ik::tc_ikernel_data_counts& tc)
{
#pragma HLS inline

    DO_PRAGMA(HLS stream variable=data_dup_to_parser depth=2)
    DO_PRAGMA(HLS stream variable=data_dup_to_egress depth=15)

    memory_unused(p.mem, dummy_update);
    pass_packets(p.host);
    hls_helpers::dup(p.net.data_input, data_dup_to_parser, data_dup_to_egress);
    parser0();
    parser1();
    ingress_parsed(p.net, tc.net);
    net_ingress(p.net);
    egress();
    _dropper.filter(_decision_pass, _data_egress_to_filter, p.net.data_output);
}

int threshold::rpc(int address, int* value, ikernel_id_t ikernel_id, bool read)
{
#pragma HLS inline
    int ret;

    if (ikernel_id >= NUM_THRESHOLD_CONTEXTS) {
        if (read)
            *value = -1;
        return GW_FAIL;
    }

    switch (address) {
    case THRESHOLD_VALUE:
    case THRESHOLD_RING_ID:
    case THRESHOLD_FLAGS:
        return contexts.rpc(address, value, ikernel_id, read);
    case THRESHOLD_MIN:
    case THRESHOLD_MAX:
    case THRESHOLD_COUNT:
    case THRESHOLD_SUM_LO:
    case THRESHOLD_SUM_HI:
    case THRESHOLD_DROPPED:
    case THRESHOLD_DROPPED_BACKPRESSURE:
    default:
        return stats.rpc(address, value, ikernel_id, read);
    case THRESHOLD_RESET:
        if (!reset_done(0, 0)) {
            ret = contexts.rpc(address, value, ikernel_id, read);
            if (ret == GW_DONE)
                reset_done(0, 0) = 1;
        }
        if (!reset_done(1, 1)) {
            ret = stats.rpc(address, value, ikernel_id, read);
            if (ret == GW_DONE)
                reset_done(1, 1) = 1;
        }
        if (!reset_done(2, 2)) {
            if (parser_reset.write_nb(ikernel_id)) {
                reset_done(2, 2) = 1;
            }
        }
        return (reset_done(0, 0) && reset_done(1, 1) && reset_done(2, 2)) ? GW_DONE : GW_BUSY;
    }
}

int threshold::reg_write(int address, int value, ikernel_id_t ikernel_id)
{
#pragma HLS inline
    return rpc(address, &value, ikernel_id, false);
}

int threshold::reg_read(int address, int* value, ikernel_id_t ikernel_id)
{
#pragma HLS inline
    return rpc(address, value, ikernel_id, true);
}

DEFINE_TOP_FUNCTION(threshold_top, threshold, THRESHOLD_UUID)
