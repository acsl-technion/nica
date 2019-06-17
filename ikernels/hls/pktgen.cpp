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

#include "pktgen.hpp"
#include "hls_helper.h"

using namespace hls_helpers;
using namespace hls_ik;

using std::make_tuple;

pktgen::pktgen() :
    contexts()
{
}

void pktgen::step(hls_ik::ports& p, hls_ik::tc_ikernel_data_counts& tc)
{
#pragma HLS inline
    data_plane(p.host, tc.host);
    sched_wrapper();
    pass_packets(p.net);
    ikernel::update();
}

void pktgen::sched_wrapper()
{
#pragma HLS pipeline enable_flush ii=2
    if (contexts.update())
        return;
    if (sched.update())
        return;

    if (!context_updates.empty()) {
        pktgen_context context = context_updates.read();
        ikernel_id_t ik = context.metadata.ikernel_id;

        contexts[ik].data_length = context.data_length;
        contexts[ik].cur_packet = std::min(context.cur_packet, contexts[ik].burst_size);
        contexts[ik].metadata = context.metadata;
        if (contexts[ik].cur_packet)
            sched.schedule(ik);
        return;
    }

    switch (scheduler_state) {
    case FIND_NEXT_IKERNEL: {
        if (!sched.next_flow(&sched_ikernel_id, &quota_from_sched))
            break;

        scheduler_state = TRANSMIT_COMMAND;
        /* Fall through */
    }
    case TRANSMIT_COMMAND:
        if (transmit_commands.full())
            break;

        transmit_commands.write(make_tuple(contexts[sched_ikernel_id], quota_from_sched));
        scheduler_state = FIND_NEXT_IKERNEL;
        break;
    }
}

void pktgen::check_quantum_complete(hls_ik::pipeline_ports& p,
                                    hls_ik::tc_pipeline_data_counts& tc)
{
#pragma HLS inline
    if (context.data_length <= quota && context.cur_packet &&
        can_transmit(tc, context.metadata.ikernel_id, 0, context.data_length << 5, NET)) {
        data_offset = 0;
        state = DUPLICATE;
    } else {
        context_updates.write(context);
        sched.update_flow(context.metadata.ikernel_id, context.cur_packet, quota);
        state = IDLE;
    }
}

void pktgen::data_plane(hls_ik::pipeline_ports& p,
                        hls_ik::tc_pipeline_data_counts& tc)
{
#pragma HLS pipeline enable_flush ii=1
    switch (state) {
    case IDLE: {
        if (!p.metadata_input.empty()) {
            metadata = p.metadata_input.read();
            p.metadata_output.write(metadata);

            state = INPUT_PACKET;
            data_offset = 0;
            goto input_packet;
        }

        if (!transmit_commands.empty()) {
            std::tie(context, quota) = transmit_commands.read();
            check_quantum_complete(p, tc);
        }
        break;
    }
    case INPUT_PACKET: {
input_packet:
        if (p.data_input.empty())
            break;

        hls_ik::axi_data d = p.data_input.read();
        /* TODO handle packet too large */
        data[metadata.ikernel_id][data_offset++] = d;
        p.data_output.write(d);

        if (d.last) {
            pktgen_context context;
            context.cur_packet = -1;
            context.data_length = data_offset;
            context.metadata = metadata;
            context_updates.write(context);
            state = IDLE;
        }
        break;
    }
    case DUPLICATE:
duplicate:
        if (data_offset == 0) {
            hls_ik::metadata m(context.metadata);
            m.ip_identification = context.cur_packet--;
            p.metadata_output.write(m);
        }

        p.data_output.write(data[context.metadata.ikernel_id][data_offset]);
        if (++data_offset >= context.data_length) {
            quota -= context.data_length;
            check_quantum_complete(p, tc);
        }
        break;
    }
}

int pktgen_contexts::rpc(int address, int *value, hls_ik::ikernel_id_t ikernel_id, bool read)
{
#pragma HLS inline
    uint32_t index = ikernel_id;

    switch (address) {
    case PKTGEN_BURST_SIZE:
        return gateway_access_field<uint32_t, &pktgen_context::burst_size>(index, value, read);
    case PKTGEN_CUR_PACKET:
        return gateway_access_field<uint32_t, &pktgen_context::cur_packet>(index, value, read);
    default:
        *value = -1;
        return GW_FAIL;
    }

    return GW_DONE;
}

int pktgen::reg_write(int address, int value, ikernel_id_t ikernel_id)
{
#pragma HLS inline
    if (address > PKTGEN_SCHEDULER)
	return sched.rpc(address - PKTGEN_SCHEDULER, &value, ikernel_id, false);

    switch (address) {
    case PKTGEN_BURST_SIZE:
    case PKTGEN_CUR_PACKET:
        return contexts.rpc(address, &value, ikernel_id, false);
    }

    return GW_FAIL;
}

int pktgen::reg_read(int address, int* value, ikernel_id_t ikernel_id)
{
#pragma HLS inline
    if (address > PKTGEN_SCHEDULER)
	return sched.rpc(address - PKTGEN_SCHEDULER, value, ikernel_id, false);

    switch (address) {
    case PKTGEN_BURST_SIZE:
    case PKTGEN_CUR_PACKET:
        return contexts.rpc(address, value, ikernel_id, true);
    }

    return GW_FAIL;
}

DEFINE_TOP_FUNCTION(pktgen_top, pktgen, PKTGEN_UUID)
