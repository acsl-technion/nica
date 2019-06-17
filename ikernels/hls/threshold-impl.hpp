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
#include <drop_or_pass.hpp>

#include "threshold.hpp"

DECLARE_TOP_FUNCTION(threshold_top);

typedef ap_uint<32> value;

struct threshold_context {
    threshold_context() :
        threshold_value(0),
        ring_id(0),
        flags(0)
    {}

    /** Used by net_ingress to determine whether packets should be passed or
     * dropped */
    value threshold_value;
    hls_ik::ring_id_t ring_id;

    ap_uint<32> flags;
};

struct threshold_stats_context {
    threshold_stats_context() :
        min(-1U), max(0), count(0), dropped(0), sum(0)
    {}

    value min, max, count, dropped, dropped_backpressure;
    ap_uint<64> sum;
};

class threshold_contexts : public context_manager<threshold_context, LOG_NUM_THRESHOLD_CONTEXTS>
{
public:
    int rpc(int address, int *value, hls_ik::ikernel_id_t ikernel_id, bool read);

    hls_ik::ring_id_t find_ring(const hls_ik::ikernel_id_t& ikernel_id);
};

class threshold_stats_contexts : public context_manager<threshold_stats_context, LOG_NUM_THRESHOLD_CONTEXTS>
{
public:
    int rpc(int address, int *value, hls_ik::ikernel_id_t ikernel_id, bool read);
};

struct threshold_metadata {
    hls_ik::metadata meta;
    value threshold_value;
    ap_uint<1> flags;
};

class threshold : public hls_ik::ikernel, public hls_ik::virt_gateway_impl<threshold> {
public:
    threshold();
    void step(hls_ik::ports& ports, hls_ik::tc_ikernel_data_counts& tc);
    int reg_write(int address, int value, hls_ik::ikernel_id_t ikernel_id);
    int reg_read(int address, int* value, hls_ik::ikernel_id_t ikernel_id);
    void update_stats(hls_ik::ikernel_id_t id, value v, bool drop, bool backpressure);

protected:
    threshold_contexts contexts;
    threshold_stats_contexts stats;

    int rpc(int address, int* value, hls_ik::ikernel_id_t ikernel_id, bool read);

    void net_ingress(hls_ik::pipeline_ports& p);
    void parser0();
    void parser1();
    void ingress_parsed(hls_ik::pipeline_ports& p, hls_ik::tc_pipeline_data_counts& tc);
    void egress();

    hls::stream<value> parsed;
    hls_ik::data_stream data_dup_to_parser, data_dup_to_egress,
                        _data_egress_to_filter;

    hls::stream<threshold_metadata> net_to_parser, parser1_to_parsed;

    struct decision_t {
        value v;
        hls_ik::ring_id_t ring_id;
        ap_uint<1> flags;
    };
    hls::stream<decision_t> decisions;
    hls::stream<bool> _decision_pass;
    decision_t egress_last_decision;

    bool tcp;
    enum { PARSER_IDLE, PARSER_NEW_FLIT, PARSER_LOOP, PARSER_REST } parser_state;
    ap_uint<6> parser_offset;
    ap_uint<6> parser_message_offset;
    ap_uint<6> parser_len;
    hls_ik::axi_data parser_flit;
    threshold_metadata parser_metadata;
    ap_uint<18 * 8> parser_message;
    hls::stream<hls_ik::ikernel_id_t> parser_reset;

    struct parser0_out {
        ap_uint<6> input_hi, input_lo;
        ap_uint<6> output_hi, output_lo;
        threshold_metadata meta;
        ap_uint<256> data;
    };
    hls::stream<parser0_out> parser0_to_1;

    enum { IDLE, STREAM } egress_state;

    drop_or_pass _dropper;

    ap_uint<3> reset_done;
};
