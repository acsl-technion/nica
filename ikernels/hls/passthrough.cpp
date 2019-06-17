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

#include "passthrough.hpp"
#include "passthrough-impl.hpp"

using namespace hls_ik;

namespace hls_ik {
    void pass_packets(pipeline_ports& p)
    {
    #pragma HLS pipeline enable_flush ii=1
        if (!p.metadata_input.empty()) {
            metadata m = p.metadata_input.read();
            p.metadata_output.write(m);
        }

        if (!p.data_input.empty()) {
            axi_data d = p.data_input.read();
            p.data_output.write(d);
        }
    }
}

void passthrough::intercept_in(pipeline_ports& p, tc_pipeline_data_counts& tc) {
#pragma HLS pipeline enable_flush ii=3
    if (contexts.update())
        return;
    if (update())
        return;

    if (!p.metadata_input.empty() && !_decisions.full()) {
        metadata m = p.metadata_input.read();
        passthrough_context& c = contexts[m.ikernel_id];
        bool backpressure = false;
        // CR Mode
        if (c.ring_id != 0) {
            backpressure = !c.ignore_credits && !can_transmit(tc, m.ikernel_id, c.ring_id, m.length, HOST);

            if (!backpressure) {
                new_message(c.ring_id, HOST);
                m.ring_id = c.ring_id;
                custom_ring_metadata cr;
                cr.end_of_message = 1;
                m.var = cr;
                m.length = m.length;
                m.verify();
            }
        }

        if (!backpressure) {
            p.metadata_output.write(m);
        }

        _decisions.write(!backpressure);
    }
}

void passthrough::step(ports& p, tc_ikernel_data_counts& tc) {
#pragma HLS inline
    memory_unused.step(p.mem);
    pass_packets(p.host);
    intercept_in(p.net, tc.net);
    dropper.filter(_decisions, p.net.data_input, p.net.data_output);
}

int passthrough::reg_write(int address, int value, ikernel_id_t ikernel_id)
{
#pragma HLS inline
    if (ikernel_id >= NUM_PASSTHROUGH_CONTEXTS) {
        return GW_FAIL;
    }

    return contexts.rpc(address, &value, ikernel_id, false);
}

int passthrough::reg_read(int address, int* value, ikernel_id_t ikernel_id)
{
#pragma HLS inline
    if (ikernel_id >= NUM_PASSTHROUGH_CONTEXTS) {
        *value = -1;
        return GW_FAIL;
    }

    return contexts.rpc(address, value, ikernel_id, true);
}

int passthrough_contexts::rpc(int address, int *v, ikernel_id_t ikernel_id, bool read)
{
#pragma HLS inline
    uint32_t index = ikernel_id;

    switch (address) {
        case PASSTHROUGH_RING_ID:
            return gateway_access_field<ring_id_t, &passthrough_context::ring_id>(index, v, read);
        case PASSTHROUGH_IGNORE_CREDITS:
            return gateway_access_field<bool, &passthrough_context::ignore_credits>(index, v, read);
        default:
            if (read)
                *v = -1;
            return GW_FAIL;
    }

    return GW_DONE;
}

DEFINE_TOP_FUNCTION(passthrough_top, passthrough, PASSTHROUGH_UUID)
