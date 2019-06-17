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

#pragma once

#include <ikernel.hpp>
#include <gateway.hpp>
#include <flow_table.hpp>
#include <context_manager.hpp>

#include "threshold.hpp"

DECLARE_TOP_FUNCTION(threshold_top);

typedef ap_uint<32> value;

struct threshold_context {
    threshold_context() :
        threshold_value(0),
        min(-1U), max(0), count(0), dropped(0), sum(0),
        ring_id(0)
    {}

    /** Used by net_ingress to determine whether packets should be passed or
     * dropped */
    value threshold_value;
    value min, max, count, dropped, dropped_backpressure;
    ap_uint<64> sum;
    hls_ik::ring_id_t ring_id;
};

class threshold_contexts : public context_manager<threshold_context, LOG_NUM_THRESHOLD_CONTEXTS>
{
public:
    int rpc(int address, int *value, hls_ik::ikernel_id_t ikernel_id, bool read);

    hls_ik::ring_id_t find_ring(const hls_ik::ikernel_id_t& ikernel_id);
};

class threshold : public hls_ik::ikernel, public hls_ik::virt_gateway_impl<threshold> {
public:
    virtual void step(hls_ik::ports& ports);
    virtual int reg_write(int address, int value, hls_ik::ikernel_id_t ikernel_id);
    virtual int reg_read(int address, int* value, hls_ik::ikernel_id_t ikernel_id);
    void update_stats(hls_ik::ikernel_id_t id, value v, bool drop, bool backpressure);

protected:
    threshold_contexts contexts;

    void net_ingress(hls_ik::pipeline_ports& p);
    void parser();
    void egress(hls_ik::pipeline_ports& p);

    hls::stream<value> parsed;
    hls_ik::data_stream data_dup_to_parser, data_dup_to_egress;

    struct decision_t {
        bool drop;
        value v;
        hls_ik::ring_id_t ring_id;
    };
    hls::stream<decision_t> decisions;
    decision_t egress_last_decision;

    enum { FIRST, REST } parser_state;

    enum { IDLE, STREAM } egress_state;
};
