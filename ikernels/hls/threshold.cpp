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
    case THRESHOLD_MIN:
	return gateway_access_field<value, &threshold_context::min>(index, v, read);
    case THRESHOLD_MAX:
	return gateway_access_field<value, &threshold_context::max>(index, v, read);
    case THRESHOLD_COUNT:
	return gateway_access_field<value, &threshold_context::count>(index, v, read);
    case THRESHOLD_SUM_LO:
        return gateway_rmw(index, [read, v](const threshold_context& c) -> threshold_context {
            if (read)
                *v = c.sum(31, 0);
            else
                c.sum(31, 0) = *v;
            return c;
        });
    case THRESHOLD_SUM_HI:
        return gateway_rmw(index, [read, v](const threshold_context& c) -> threshold_context {
            if (read)
                *v = c.sum(63, 32);
            else
                c.sum(63, 32) = *v;
            return c;
        });
        break;
    case THRESHOLD_VALUE:
	return gateway_access_field<value, &threshold_context::threshold_value>(index, v, read);
    case THRESHOLD_DROPPED:
	return gateway_access_field<value, &threshold_context::dropped>(index, v, read);
    case THRESHOLD_DROPPED_BACKPRESSURE:
	return gateway_access_field<value, &threshold_context::dropped_backpressure>(index, v, read);
    case THRESHOLD_RING_ID:
	return gateway_access_field<ring_id_t, &threshold_context::ring_id>(index, v, read);
    default:
        if (read)
            *v = -1;
        return GW_FAIL;
    }

    return GW_DONE;
}

void threshold::net_ingress(hls_ik::pipeline_ports& p,
                            hls_ik::tc_pipeline_data_counts& tc)
{
#pragma HLS pipeline enable_flush ii=3
    if (contexts.update())
        return;
    if (update())
        return;

    if (p.metadata_input.empty() || parsed.empty() || decisions.full() ||
        _decision_pass.full())
        return;

    hls_ik::metadata meta = p.metadata_input.read();
    ring_id_t ring_id = contexts.find_ring(meta.ikernel_id);
    bool backpressure = !can_transmit(tc, meta.ikernel_id, ring_id, 4, HOST);

    value v = parsed.read();
    value threshold_value = contexts[meta.ikernel_id].threshold_value;
    bool drop = v < threshold_value || backpressure;
    if (!drop) {
        if (ring_id != 0) {
            new_message(ring_id, HOST);
            meta.ring_id = ring_id;
            custom_ring_metadata cr;
            cr.end_of_message = 1;
            meta.var = cr;
            meta.length = 4;
            meta.verify();
        }
        p.metadata_output.write(meta);
    }

    decisions.write(decision_t{v, ring_id});
    _decision_pass.write(!drop);
    update_stats(meta.ikernel_id, v, drop, backpressure);
}

void threshold::egress()
{
    axi_data d;
#pragma HLS pipeline enable_flush ii=1
    switch (egress_state)
    {
    case IDLE:
        if (decisions.empty() || _data_egress_to_filter.full())
            return;

        egress_last_decision = decisions.read();
        egress_state = STREAM;

        if (egress_last_decision.ring_id != 0) {
            d = axi_data((egress_last_decision.v, ap_uint<256 - value::width>()), 0xf0000000, true);
            _data_egress_to_filter.write(d);
        }
        break;

    case STREAM:
        if (data_dup_to_egress.empty() || _data_egress_to_filter.full())
            return;

        d = data_dup_to_egress.read();
        if (!egress_last_decision.ring_id)
            _data_egress_to_filter.write(d);

        egress_state = d.last ? IDLE : STREAM;
        break;
    }
}

void threshold::parser()
{
#pragma HLS pipeline enable_flush ii=1
    axi_data d;

    switch (parser_state) {
    case FIRST: {
        if (data_dup_to_parser.empty() || parsed.full())
            return;

        d = data_dup_to_parser.read();
        value v = d.data(255-14*8, 256 - value::width-14*8);
//        std::cout << "value: " << d.data(255-14*8, 256 - value::width-14*8) << "\n";
        parsed.write(v);
        break;
    }
    case REST:
        if (data_dup_to_parser.empty())
            return;

        d = data_dup_to_parser.read();
        break;

    }
    parser_state = d.last ? FIRST : REST;
}

void threshold::update_stats(ikernel_id_t id, value v, bool drop, bool backpressure)
{
#pragma HLS inline
    threshold_context& c = contexts[id];
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

    memory_unused.step(p.mem);
    pass_packets(p.host);
    hls_helpers::dup(p.net.data_input, data_dup_to_parser, data_dup_to_egress);
    parser();
    net_ingress(p.net, tc.net);
    egress();
    _dropper.filter(_decision_pass, _data_egress_to_filter, p.net.data_output);
}

int threshold::reg_write(int address, int value, ikernel_id_t ikernel_id)
{
#pragma HLS inline
    if (ikernel_id >= NUM_THRESHOLD_CONTEXTS) {
        return GW_FAIL;
    }

    return contexts.rpc(address, &value, ikernel_id, false);
}

int threshold::reg_read(int address, int* value, ikernel_id_t ikernel_id)
{
#pragma HLS inline
    if (ikernel_id >= NUM_THRESHOLD_CONTEXTS) {
        *value = -1;
        return GW_FAIL;
    }

    return contexts.rpc(address, value, ikernel_id, true);
}

DEFINE_TOP_FUNCTION(threshold_top, threshold, THRESHOLD_UUID)
