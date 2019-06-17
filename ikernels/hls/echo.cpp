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

#include "echo-impl.hpp"
#include "hls_helper.h"
#include <tuple>

#include <ntl/produce.hpp>
#include <ntl/consume.hpp>

using namespace hls_helpers;
using hls_ik::ikernel_id_t;

using std::make_tuple;

echo::echo()
{
}

void echo::echo_pipeline(hls_ik::ports& p, hls_ik::tc_ikernel_data_counts& tc) {
#pragma HLS inline off
#pragma HLS pipeline enable_flush ii=1
    if (!respond_to_sockperf_update.empty())
        respond_to_sockperf = respond_to_sockperf_update.read();

    switch (state) {
        case METADATA: {
            if (p.net.metadata_input.empty())
                return;

            metadata = p.net.metadata_input.read();

            state = DATA;
            first = true;
            break;
        }
        case DATA: {
            if (p.net.data_input.empty())
                return;

            hls_ik::axi_data d = p.net.data_input.read();
            if (first) {
                first = false;

                if (respond_to_sockperf) {
                    short flags = d.data(255 - 8 * 8, 256 - 10 * 8);
                    bool pong_request = flags & 2;
                    // Turn off client bit on the response packet
                    flags &= ~1;
                    d.data(255 - 8 * 8, 256 - 10 * 8) = flags;

                    respond = pong_request;
                } else {
                    respond = true;
                }

                if (respond) {
                    if (can_transmit(tc.host, metadata.ikernel_id, 0, metadata.length, NET)) {
                        p.host.metadata_output.write(metadata.reply(metadata.length));
                    }
                }
            }

            if (respond) {
                p.host.data_output.write(d);
            }
            state = d.last ? METADATA : DATA;
            break;
        }
    }
}

void echo::step(hls_ik::ports& p, hls_ik::tc_ikernel_data_counts& tc) {
#pragma HLS inline
    memory_unused.step(p.mem);
    echo_pipeline(p, tc);

    ikernel::update();

    // TODO: passthrough or drop the host traffic; merge with action stream
    // from the echo_pipeline.
    dummy_produce_consume: {
        bool dummy;

        port_dummy_update.write_nb(false);
        port_dummy_update.read_nb(dummy);

        ntl::consume(p.host.metadata_input, dummy);
        ntl::consume(p.host.data_input, dummy);
        ntl::produce(p.net.metadata_output, dummy);
        ntl::produce(p.net.data_output, dummy);
    }
}

int echo::reg_write(int address, int value, ikernel_id_t ikernel_id)
{
#pragma HLS inline
    switch (address) {
    case ECHO_RESPOND_TO_SOCKPERF:
        if (respond_to_sockperf_update.full())
            return GW_BUSY;
	respond_to_sockperf_cache = value;
        respond_to_sockperf_update.write(value);
	return 0;
    }

    return GW_FAIL;
}


int echo::reg_read(int address, int* value, ikernel_id_t ikernel_id)
{
#pragma HLS inline
    switch (address) {
    case ECHO_RESPOND_TO_SOCKPERF:
	*value = respond_to_sockperf_cache;
	return 0;
    default:
        *value = -1;
        return GW_FAIL;
    }
}


DEFINE_TOP_FUNCTION(echo_top, echo, ECHO_UUID)
