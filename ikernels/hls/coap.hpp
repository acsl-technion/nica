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

#ifndef COAP_HPP
#define COAP_HPP

// 2793e332-bbcf-418b-94a3-f6a2ce75fb5c
#define COAP_UUID { 0x27, 0x93, 0xe3, 0x32, 0xbb, 0xcf, 0x41, 0x8b, 0x94, 0xa3, 0xf6, 0xa2, 0xce, 0x75, 0xfb, 0x5c }

#include <ikernel.hpp>
#include <gateway.hpp>
#include <hls_helper.h>
#include <flow_table.hpp>
#include <ntl/context_manager.hpp>
#include <ntl/scheduler.hpp>
#include <ntl/constexpr.hpp>
#include <boost/preprocessor/repetition/repeat.hpp>
#include <boost/preprocessor/repetition/enum.hpp>
#include <boost/preprocessor/arithmetic/inc.hpp>
#include <boost/preprocessor/cat.hpp>

#define IPAD 0x36
#define OPAD 0x5C
#define SHA256_HASH_BITS  256
#define SHA256_HASH_BYTES (SHA256_HASH_BITS / 8)
#define SHA256_BLOCK_BITS 512
#define SHA256_BLOCK_BYTES (SHA256_BLOCK_BITS/8)
#define SIGNATURE_BASE64_LENGTH 43
#define SIGNATURE_BASE64_LENGTH_BITS (SIGNATURE_BASE64_LENGTH * 8)

#define LOG_NUM_COAP_CONTEXTS 6
#define NUM_COAP_CONTEXTS (1 << LOG_NUM_COAP_CONTEXTS)

// 0x10 - 0x1f
#define COAP_KEY 0x10
#define COAP_RING_ID 0x20
#define COAP_DROPPED_COUNT 0x21
#define COAP_PASSED_COUNT 0x22
#define COAP_FIRST_PASS_SHA_UNIT_REQ_COUNT 0x23
#define COAP_FIRST_PASS_SHA_UNIT_RES_COUNT 0x24
#define COAP_SECOND_PASS_SHA_UNIT_REQ_COUNT 0x25
#define COAP_SECOND_PASS_SHA_UNIT_RES_COUNT 0x26
#define COAP_PARSED_PACKETS_COUNT 0x27
#define COAP_DROPPER_UNIT_STATE 0x28
#define COAP_PARSER_UNIT_STATE 0x29
#define COAP_FIRST_PASS_UNIT_STATE 0x30
#define COAP_SECOND_PASS_UNIT_STATE 0x31
#define COAP_BACKPRESSURE_DROP_COUNT 0x32
#define COAP_SCHEDULER 0x33

#define MESSAGE_LENGTH_IN_BITS (81*8)
#define FIRST_PASS_BLOCKS (MESSAGE_LENGTH_IN_BITS / SHA256_BLOCK_BITS)
#define FIRST_PASS_LAST_BLOCK_SIZE (MESSAGE_LENGTH_IN_BITS - FIRST_PASS_BLOCKS * SHA256_BLOCK_BITS)

char basis_64(ap_uint<6> val);

static constexpr const char* JWT_BASE64_HEADER = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.";

struct coap_hmac_key {
    ap_uint<SHA256_BLOCK_BITS> buff;
    hls_ik::ikernel_id_t tc;
};

struct coap_parsed_data {
    ap_uint<SHA256_BLOCK_BITS> first_word;
    ap_uint<SHA256_BLOCK_BITS> second_word;

    coap_parsed_data() {
        for (int i = 0; i < 37; ++i) {
#pragma HLS unroll
            hls_helpers::write_byte<SHA256_BLOCK_BITS>(first_word, i, JWT_BASE64_HEADER[i]);
        }
    }
};

struct coap_sig {
    ap_uint<SIGNATURE_BASE64_LENGTH_BITS> buff;
};

struct coap_decision {
    ap_uint<1> pass;
    hls_ik::ring_id_t ring_id;
};

struct coap_context {
    hls_ik::ring_id_t ring_id;

    coap_context() : ring_id(0) {}
};

struct coap_stats_context {
    ap_uint<32> dropped_count,
            passed_count,
            first_pass_sha_unit_req_count,
            first_pass_sha_unit_res_count,
            second_pass_sha_unit_req_count,
            second_pass_sha_unit_res_count,
            parsed_packets_count,
            dropper_unit_state,
            parser_unit_state,
            first_pass_unit_state,
            second_pass_unit_state,
            backpressure_drop_count;

    coap_stats_context() :
                     dropped_count(0),
                     passed_count(0),
                     first_pass_sha_unit_req_count(0),
                     first_pass_sha_unit_res_count(0),
                     second_pass_sha_unit_req_count(0),
                     second_pass_sha_unit_res_count(0),
                     parsed_packets_count(0),
                     dropper_unit_state(0),
                     parser_unit_state(0),
                     first_pass_unit_state(0),
                     second_pass_unit_state(0),
                     backpressure_drop_count(0)
    {}
};

struct coap_sha_request {
    ap_uint<SHA256_BLOCK_BITS> data;
    ap_uint<1> last;
};

struct coap_sha_response {
    ap_uint<SHA256_HASH_BITS> data;
};

struct coap_verify_sig_request {
    hls_ik::ring_id_t ring_id;
    hls_ik::ikernel_id_t tc;
};

struct coap_packet_action {
    ap_uint<1> pass;
    hls_ik::ring_id_t ring_id;
    ap_uint<16> src_port;
    ap_uint<32> src_ip;
};

void coap_ikernel(hls_ik::ports& ik, hls_ik::ikernel_id& uuid,
                  hls_ik::virt_gateway_registers& gateway,
                  hls_ik::tc_ikernel_data_counts& tc,
                  hls::stream<coap_sha_request>& first_pass_sha_unit_input_stream,
                  hls::stream<coap_sha_response>& first_pass_sha_unit_output_stream,
                  hls::stream<coap_sha_request>& second_pass_sha_unit_input_stream,
                  hls::stream<coap_sha_response>& second_pass_sha_unit_output_stream);

class coap_contexts : public ntl::base_context_manager<coap_context, LOG_NUM_COAP_CONTEXTS>
{
public:
    hls_ik::ring_id_t find_ring(const hls_ik::ikernel_id_t& ikernel_id);
};

class coap_stats_contexts : public ntl::context_manager<coap_stats_context, LOG_NUM_COAP_CONTEXTS>
{
public:
    int rpc(int address, int *v, hls_ik::ikernel_id_t ikernel_id, bool read);
};

typedef ntl::base_context_manager<ap_uint<32>, LOG_NUM_COAP_CONTEXTS> coap_key_parts;

class coap : public hls_ik::ikernel {
public:
    void step(hls_ik::ports& ports, hls_ik::tc_ikernel_data_counts& tc);
    int reg_write(int address, int value, hls_ik::ikernel_id_t ikernel_id);
    int reg_read(int address, int* value, hls_ik::ikernel_id_t ikernel_id);
    int rpc(int address, int *value, hls_ik::ikernel_id_t ikernel_id, bool read);
    coap();
    void first_pass(hls::stream<coap_sha_request>& first_pass_sha_unit_input_stream);
    void second_pass(hls::stream<coap_sha_response>& first_pass_sha_unit_output_stream,
                     hls::stream<coap_sha_request>& second_pass_sha_unit_input_stream);
    void verify_sig(hls::stream<coap_sha_response>& second_pass_sha_unit_output_stream);

private:
    void drop_or_pass(hls_ik::pipeline_ports& in);
    void parse_packet();
    void base64_encode(ap_uint<SIGNATURE_BASE64_LENGTH_BITS>& encoded, const ap_uint<SHA256_HASH_BITS>& string);
    void action_resolution(hls_ik::pipeline_ports& p,
                           hls_ik::tc_pipeline_data_counts& tc);
    void read_key();
    template <unsigned Length, uint64_t MessageLength, unsigned Width>
    void pad_block(ap_uint<SHA256_BLOCK_BITS>& dest, ap_uint<Width>& src);
    void update_stats();
    int calc_tc(const hls_ik::metadata& metadata);


    enum { IN_METADATA, FIRST_WORD, SECOND_WORD, THIRD_WORD, FOURTH_WORD, FIFTH_WORD, OTHER_WORDS } _in_state;
    enum { DECISION, DATA } _dropper_state;
    enum { READ_REQUEST, FIRST_BLOCK, SECOND_BLOCK } _first_pass_state;
    enum { HASH_KEY, HASH_FIRST_PASS } _second_pass_state;
    hls_helpers::duplicator<1, ap_uint<hls_ik::axi_data::width> > _raw_dup;
    hls_helpers::duplicator<1, ap_uint<hls_ik::metadata::width> > _metadata_dup;
    hls_ik::data_stream _parser_data, _buffer_data;
    hls_ik::metadata_stream _parser_metadata, _buffer_metadata;
    hls::stream<coap_decision> _decision_stream;
    hls::stream<coap_packet_action> _action_stream;
    coap_packet_action _dropper_packet_action;
    coap_parsed_data _parsed_data, _first_pass_data;
    coap_sig _sig;
    hls_ik::metadata _meta;

    coap_sha_request _request;

    hls::stream<coap_hmac_key> _read_key_output_stream_to_first_pass_unit, _read_key_output_stream_to_second_pass_unit;

    hls::stream<coap_verify_sig_request> _verify_sig_request_stream;

    coap_contexts _contexts;
    coap_key_parts _keys[16];
    coap_stats_contexts _stats_contexts;

    hls::stream<std::tuple<hls_ik::ikernel_id_t, bool> > _backpressure_drop, _action_count;
    hls::stream<bool> _first_pass_sha_unit_req_count, _first_pass_sha_unit_res_count, _second_pass_sha_unit_req_count, _second_pass_sha_unit_res_count;
    hls::stream<hls_ik::ikernel_id_t> _parsed_packets;
    hls::stream<ap_uint<32> > _dropper_unit_state, _parser_unit_state, _first_pass_unit_state, _second_pass_unit_state;

    static const unsigned log_num_tc = ntl::log2(NUM_TC);
    ntl::scheduler<log_num_tc> _sched;
    ap_uint<log_num_tc> _parser_curr_tc, _sched_curr_tc, _first_pass_curr_tc;
    hls::stream<ap_uint<log_num_tc> > _parsed_packet_tc;
    hls::stream<coap_parsed_data> _parsed_data_stream[NUM_TC];
    hls::stream<coap_sig> _sig_stream[NUM_TC];
    hls::stream<hls_ik::ikernel_id_t> _ikernel_id_per_tc[NUM_TC];
    enum SCHEDULER_STATE { FIND_NEXT_IKERNEL, PASS_REQUESTS, UPDATE_QUOTA } _sched_state;
    uint32_t _sched_quota;
    bool _tc_empty;
};


#endif //COAP_HPP
