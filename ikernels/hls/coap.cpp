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

#include "coap.hpp"
#include <mlx.h>

using namespace hls_ik;

// jwt implementation is based on libjwt
// HMAC SHA256 implementation is based on avr-crypto-lib

// 0-15: coap header
// 16-181: cbor data
// 16: 0xa2 (map (2))
// 17: 0x65 (text (5))
// 18-22: "token"
// 23-24: 78-7D (text (125))
// 25-149: token bytes. HEAD = 25-60 (36 bytes), BODY = 62-105 (44 bytes), sig = 107-149 (43 bytes)
// 150: 0x67 (text (7))
// 151-157: "payload"
// 158-159: 78-19 (text (25))
// 160-181: "containing nothing useful"

coap::coap() {
#pragma HLS stream variable=_parser_data depth=512*5
#pragma HLS stream variable=_buffer_data depth=512*5
#pragma HLS stream variable=_parser_metadata depth=512
#pragma HLS stream variable=_buffer_metadata depth=512
#pragma HLS stream variable=_decision_stream depth=15
#pragma HLS stream variable=_action_stream depth=15
#pragma HLS stream variable=_parsed_data_stream depth=512
#pragma HLS stream variable=_sig_stream depth=512
#pragma HLS stream variable=_ikernel_id_per_tc depth=512
#pragma HLS stream variable=_parsed_packet_tc depth=512
#pragma HLS stream variable=_read_key_output_stream_to_first_pass_unit depth=512
#pragma HLS stream variable=_read_key_output_stream_to_second_pass_unit depth=512
#pragma HLS stream variable=_verify_sig_request_stream depth=512
#pragma HLS stream variable=_backpressure_drop depth=32
#pragma HLS stream variable=_action_count depth=32
#pragma HLS stream variable=_first_pass_sha_unit_req_count depth=32
#pragma HLS stream variable=_first_pass_sha_unit_res_count depth=32
#pragma HLS stream variable=_second_pass_sha_unit_req_count depth=32
#pragma HLS stream variable=_second_pass_sha_unit_res_count depth=32
#pragma HLS stream variable=_parsed_packets depth=32
#pragma HLS stream variable=_dropper_unit_state depth=32
#pragma HLS stream variable=_parser_unit_state depth=32
#pragma HLS stream variable=_first_pass_unit_state depth=32
#pragma HLS stream variable=_second_pass_unit_state depth=32
}

template <typename T>
char basis_64(T val)
{
#pragma HLS inline
    if (val <= 25)
        return 'A' + val;
    else if (val >= 26 && val <= 51)
        return 'a' + (val - 26);
    else if (val >= 52 && val <= 61)
        return '0' + (val - 52);
    else if (val == 62)
        return '-';
    else if (val == 63)
        return '_';

    assert(0);
    return 0;
}

void coap::base64_encode(ap_uint<SIGNATURE_BASE64_LENGTH_BITS>& encoded, const ap_uint<SHA256_HASH_BITS>& string) {
#pragma HLS inline
    int offset = 0;
    for (int i = 0; i < SHA256_HASH_BYTES - 2; i += 3) {
#pragma HLS unroll
        hls_helpers::write_byte<SIGNATURE_BASE64_LENGTH_BITS>(encoded, offset++, basis_64(
                (hls_helpers::get_byte<SHA256_HASH_BITS>(string, i) >> 2) & 0x3F));
        hls_helpers::write_byte<SIGNATURE_BASE64_LENGTH_BITS>(encoded, offset++, basis_64(
                ((hls_helpers::get_byte<SHA256_HASH_BITS>(string, i) & 0x3) << 4) |
                ((int) (hls_helpers::get_byte<SHA256_HASH_BITS>(string, i + 1) & 0xF0) >> 4)));
        hls_helpers::write_byte<SIGNATURE_BASE64_LENGTH_BITS>(encoded, offset++, basis_64(
                ((hls_helpers::get_byte<SHA256_HASH_BITS>(string, i + 1) & 0xF) << 2) |
                ((int) (hls_helpers::get_byte<SHA256_HASH_BITS>(string, i + 2) & 0xC0) >> 6)));
        hls_helpers::write_byte<SIGNATURE_BASE64_LENGTH_BITS>(encoded, offset++, basis_64(
                hls_helpers::get_byte<SHA256_HASH_BITS>(string, i + 2) & 0x3F));
    }

    hls_helpers::write_byte<SIGNATURE_BASE64_LENGTH_BITS>(encoded, offset++, basis_64(
            (hls_helpers::get_byte<SHA256_HASH_BITS>(string, SHA256_HASH_BYTES - 2) >> 2) & 0x3F));
    hls_helpers::write_byte<SIGNATURE_BASE64_LENGTH_BITS>(encoded, offset++, basis_64(
            ((hls_helpers::get_byte<SHA256_HASH_BITS>(string, SHA256_HASH_BYTES - 2) & 0x3) << 4) |
            ((int) (hls_helpers::get_byte<SHA256_HASH_BITS>(string, SHA256_HASH_BYTES - 1) & 0xF0) >> 4)));
    hls_helpers::write_byte<SIGNATURE_BASE64_LENGTH_BITS>(encoded, offset, basis_64((
            (hls_helpers::get_byte<SHA256_HASH_BITS>(string, SHA256_HASH_BYTES - 1) & 0xF) << 2)));
}

void coap::drop_or_pass(pipeline_ports& in) {
#pragma HLS pipeline enable_flush ii=1
    switch (_dropper_state) {
        case DECISION:
            if (!_action_stream.empty() &&
                !_dropper_unit_state.full()) {
                _dropper_packet_action = _action_stream.read();

                if (_dropper_packet_action.pass && _dropper_packet_action.ring_id != 0) {
                    hls_ik::axi_data d;
                    char buff[32] = {};
                    hls_helpers::ap_uint_to_char<2>(&buff[0], _dropper_packet_action.src_port);
                    hls_helpers::ap_uint_to_char<4>(&buff[2], _dropper_packet_action.src_ip);
                    d.set_data(&buff[0], 32);
                    d.last = 0;
                    in.data_output.write(d);
                }

                _dropper_unit_state.write_nb(0);
                _dropper_state = DATA;
            }

            break;

        case DATA:
            if (!_buffer_data.empty() &&
                !_dropper_unit_state.full()) {
                axi_data d = _buffer_data.read();

                if (_dropper_packet_action.pass) {
                    in.data_output.write(d);
                }

                if (d.last) {
                    _dropper_state = DECISION;
                }

                _dropper_unit_state.write_nb(1);
            }

            break;
    }
}

int coap::calc_tc(const metadata& metadata)
{
#pragma HLS inline
    static_assert(NUM_TC == 1 << ntl::log2(NUM_TC), "NUM_TC must be power of two");
    return metadata.ikernel_id & (NUM_TC - 1);
}

void coap::parse_packet() {
#pragma HLS pipeline enable_flush ii=1
    _parser_unit_state.write_nb(_in_state);

    switch (_in_state) {
        case IN_METADATA:
            if (!_parser_metadata.empty()) {
                _meta = _parser_metadata.read();
                _parser_curr_tc = calc_tc(_meta);

                _in_state = FIRST_WORD;
            }
            break;

        case FIRST_WORD:
            if (!_parser_data.empty()) {
                _parser_data.read();

                _in_state = SECOND_WORD;
            }

            break;

        case SECOND_WORD:
            if (!_parser_data.empty()) {
                axi_data d = _parser_data.read();

                hls_helpers::write_byte<SHA256_BLOCK_BITS>(_parsed_data.first_word, 37, hls_helpers::get_byte<MLX_AXI4_WIDTH_BITS>(d.data,30));
                hls_helpers::write_byte<SHA256_BLOCK_BITS>(_parsed_data.first_word, 38, hls_helpers::get_byte<MLX_AXI4_WIDTH_BITS>(d.data, 31));

                _in_state = THIRD_WORD;
            }

            break;

        case THIRD_WORD:
            if (!_parser_data.empty()) {
                axi_data d = _parser_data.read();

                for (int i = 0; i < 32; ++i) {
#pragma HLS unroll
                    const unsigned char data = hls_helpers::get_byte<MLX_AXI4_WIDTH_BITS>(d.data, i);

                    if (i <= 24) {
                        hls_helpers::write_byte<SHA256_BLOCK_BITS>(_parsed_data.first_word, 39 + i, data);
                    }

                    if (i >= 25) {
                        hls_helpers::write_byte<SHA256_BLOCK_BITS>(_parsed_data.second_word, i - 25, data);
                    }
                }

                _in_state = FOURTH_WORD;
            }

            break;

        case FOURTH_WORD:
            if (!_parser_data.empty() &&
                !_parsed_data_stream[_parser_curr_tc].full()) {
                axi_data d = _parser_data.read();

                for (int i = 0; i < 32; ++i) {
#pragma HLS unroll
                    const unsigned char data = hls_helpers::get_byte<MLX_AXI4_WIDTH_BITS>(d.data, i);

                    if (i <= 9) {
                        hls_helpers::write_byte<SHA256_BLOCK_BITS>(_parsed_data.second_word, i + 7, data);
                    }

                    if (i >= 11) {
                        hls_helpers::write_byte<SIGNATURE_BASE64_LENGTH_BITS>(_sig.buff, i - 11, data);
                    }
                }

                _parsed_data_stream[_parser_curr_tc].write(_parsed_data);
                _in_state = FIFTH_WORD;
            }

            break;

        case FIFTH_WORD:
            if (!_parser_data.empty() &&
                !_sig_stream[_parser_curr_tc].full() &&
                !_ikernel_id_per_tc[_parser_curr_tc].full() &&
                !_parsed_packet_tc.full()) {
                axi_data d = _parser_data.read();

                for (int i = 0; i <= 21; ++i) {
#pragma HLS unroll
                    hls_helpers::write_byte<SIGNATURE_BASE64_LENGTH_BITS>(_sig.buff, i + 21,
                                                                          hls_helpers::get_byte<MLX_AXI4_WIDTH_BITS>(d.data, i));
                }

                _sig_stream[_parser_curr_tc].write(_sig);
                _parsed_packet_tc.write(_parser_curr_tc);
                _ikernel_id_per_tc[_parser_curr_tc].write(_meta.ikernel_id);

                _in_state = OTHER_WORDS;
            }

            break;

        case OTHER_WORDS:
            if (!_parser_data.empty() &&
                !_parsed_packets.full()) {
                axi_data d = _parser_data.read();

                if (d.last) {
                    _parsed_packets.write_nb(_meta.ikernel_id);
                    _in_state = IN_METADATA;
                }

            }

    }
}

template <unsigned Length, uint64_t MessageLength, unsigned Width>
// This function pads blocks that are smaller than 64 - 8 Bytes.
// Lengths are in bits.
void coap::pad_block(ap_uint<SHA256_BLOCK_BITS>& dest, ap_uint<Width>& src) {
#pragma HLS inline
    hls_helpers::memcpy<Length / 8, SHA256_BLOCK_BITS, Width>(dest, src);
    hls_helpers::write_byte<SHA256_BLOCK_BITS>(dest, Length / 8, 0x80);

    for (unsigned int i = 0; i < 56 - Length / 8; ++i) {
#pragma HLS unroll
        hls_helpers::write_byte<SHA256_BLOCK_BITS>(dest, 1 + (Length / 8) + i, (unsigned char) 0);
    }

    for (unsigned int i = 1; i <= 8; ++i) {
#pragma HLS unroll
        hls_helpers::write_byte<SHA256_BLOCK_BITS>(dest, 55 + i, (unsigned char) (MessageLength >> (64 - 8 * i)));
    }
}

void coap::update_stats() {
#pragma HLS inline
    ikernel_id_t id;

    if (!_backpressure_drop.empty()) {
        bool hit;
        std::tie(id, hit) = _backpressure_drop.read();
        coap_stats_context& c = _stats_contexts[id];

        ++c.backpressure_drop_count;
    }

    if (!_action_count.empty()) {
        bool action;
        std::tie(id, action) = _action_count.read();
        coap_stats_context& c = _stats_contexts[id];

        if (action) {
            ++c.passed_count;
        } else {
            ++c.dropped_count;
        }
    }

    if (!_first_pass_sha_unit_req_count.empty()) {
        _first_pass_sha_unit_req_count.read();
        coap_stats_context& c = _stats_contexts[0];

        ++c.first_pass_sha_unit_req_count;
    }

    if (!_first_pass_sha_unit_res_count.empty()) {
        _first_pass_sha_unit_res_count.read();
        coap_stats_context& c = _stats_contexts[0];

        ++c.first_pass_sha_unit_res_count;
    }

    if (!_second_pass_sha_unit_req_count.empty()) {
        _second_pass_sha_unit_req_count.read();
        coap_stats_context& c = _stats_contexts[0];

        ++c.second_pass_sha_unit_req_count;
    }

    if (!_second_pass_sha_unit_res_count.empty()) {
        _second_pass_sha_unit_res_count.read();
        coap_stats_context& c = _stats_contexts[0];

        ++c.second_pass_sha_unit_res_count;
    }

    if (!_parsed_packets.empty()) {
        id = _parsed_packets.read();
        coap_stats_context& c = _stats_contexts[id];

        ++c.parsed_packets_count;
    }

    if (!_dropper_unit_state.empty()) {
        coap_stats_context& c = _stats_contexts[0];
        c.dropper_unit_state = _dropper_unit_state.read();
    }

    if (!_parser_unit_state.empty()) {
        coap_stats_context& c = _stats_contexts[0];
        c.parser_unit_state = _parser_unit_state.read();
    }

    if (!_first_pass_unit_state.empty()) {
        coap_stats_context& c = _stats_contexts[0];
        c.first_pass_unit_state = _first_pass_unit_state.read();
    }

    if (!_second_pass_unit_state.empty()) {
        coap_stats_context& c = _stats_contexts[0];
        c.second_pass_unit_state = _second_pass_unit_state.read();
    }
}

void coap::read_key() {
#pragma HLS pipeline ii=5 enable_flush
    update_stats();

    if (_sched.update())
        return;

    bool bram_updates = false;
    if (_contexts.update())
        bram_updates = true;
    if (_stats_contexts.update())
        bram_updates = true;
    for (int n = 0; n < 16; ++n)
        if (_keys[n].update())
            bram_updates = true;
    if (bram_updates)
        return;

    if (!_parsed_packet_tc.empty()) {
        _sched.schedule(_parsed_packet_tc.read());
    }

    switch (_sched_state) {
        case FIND_NEXT_IKERNEL:
            if (!_sched.next_flow(&_sched_curr_tc, &_sched_quota))
                break;

            _sched_state = PASS_REQUESTS;

        case PASS_REQUESTS:
            _tc_empty = _ikernel_id_per_tc[_sched_curr_tc].empty();
            if (_tc_empty || _sched_quota <= 0)
                _sched_state = UPDATE_QUOTA;
            else if (!_read_key_output_stream_to_first_pass_unit.full() &&
                     !_read_key_output_stream_to_second_pass_unit.full() &&
                     !_verify_sig_request_stream.full()) {
                ikernel_id_t ikernel_id = _ikernel_id_per_tc[_sched_curr_tc].read();

                coap_hmac_key key;
                for (int n = 0; n < 16; ++n)
                    hls_helpers::int_to_bytes<SHA256_BLOCK_BITS>(_keys[n][ikernel_id], key.buff, n);

                key.tc = _sched_curr_tc;
                _read_key_output_stream_to_first_pass_unit.write(key);
                _read_key_output_stream_to_second_pass_unit.write(key);
                --_sched_quota;

                coap_verify_sig_request req;
                req.ring_id = _contexts.find_ring(ikernel_id);
                req.tc = _sched_curr_tc;
                _verify_sig_request_stream.write(req);
            }

            break;

        case UPDATE_QUOTA:
            _sched.update_flow(_sched_curr_tc, _tc_empty, _sched_quota);
            _sched_state = FIND_NEXT_IKERNEL;
            break;
    }

}

void coap::first_pass(hls::stream<coap_sha_request>& first_pass_sha_unit_input_stream) {
#pragma HLS pipeline ii=1 enable_flush
    _first_pass_unit_state.write_nb(_first_pass_state);

    switch (_first_pass_state) {
        case READ_REQUEST:
            if (!_read_key_output_stream_to_first_pass_unit.empty()) {
                coap_hmac_key key = _read_key_output_stream_to_first_pass_unit.read();
                _first_pass_curr_tc = key.tc;
                hls_helpers::memcpy<SHA256_BLOCK_BYTES, SHA256_BLOCK_BITS, SHA256_BLOCK_BITS>(_request.data, key.buff);

                for (int i = 0; i < SHA256_BLOCK_BYTES; ++i) {
#pragma HLS unroll
                    hls_helpers::write_byte<SHA256_BLOCK_BITS>(_request.data, i,
                                                               hls_helpers::get_byte<SHA256_BLOCK_BITS>(_request.data,
                                                                                                        i) ^ IPAD);
                }

                _request.last = 0;
                first_pass_sha_unit_input_stream.write(_request);

                _first_pass_sha_unit_req_count.write_nb(true);
                _first_pass_state = FIRST_BLOCK;
            }

            break;

        case FIRST_BLOCK:
            if (!_parsed_data_stream[_first_pass_curr_tc].empty()) {
                _first_pass_data = _parsed_data_stream[_first_pass_curr_tc].read();
                coap_sha_request req;
                hls_helpers::memcpy<SHA256_BLOCK_BYTES, SHA256_BLOCK_BITS, SHA256_BLOCK_BITS>(req.data,
                                                                                              _first_pass_data.first_word);
                req.last = 0;
                first_pass_sha_unit_input_stream.write(req);

                _first_pass_sha_unit_req_count.write_nb(true);
                _first_pass_state = SECOND_BLOCK;
            }

            break;

        case SECOND_BLOCK:
            coap_sha_request req;
            pad_block<FIRST_PASS_LAST_BLOCK_SIZE,
                    2 * SHA256_BLOCK_BITS + FIRST_PASS_LAST_BLOCK_SIZE,
                    SHA256_BLOCK_BITS>(req.data, _first_pass_data.second_word);
            req.last = 1;

            first_pass_sha_unit_input_stream.write(req);

            _first_pass_sha_unit_req_count.write_nb(true);
            _first_pass_state = READ_REQUEST;
    }
}

void coap::second_pass(hls::stream<coap_sha_response>& first_pass_sha_unit_output_stream,
                       hls::stream<coap_sha_request>& second_pass_sha_unit_input_stream) {
#pragma HLS pipeline ii=2 enable_flush
    _second_pass_unit_state.write_nb(_second_pass_state);

    switch (_second_pass_state) {
        case HASH_KEY:
            if (!_read_key_output_stream_to_second_pass_unit.empty()) {
                coap_hmac_key key = _read_key_output_stream_to_second_pass_unit.read();
                coap_sha_request req;
                hls_helpers::memcpy<SHA256_BLOCK_BYTES, SHA256_BLOCK_BITS, SHA256_BLOCK_BITS>(req.data, key.buff);

                for (int i = 0; i < SHA256_BLOCK_BYTES; ++i) {
#pragma HLS unroll
                    hls_helpers::write_byte<SHA256_BLOCK_BITS>(req.data, i, hls_helpers::get_byte<SHA256_BLOCK_BITS>(req.data, i) ^ OPAD);
                }

                req.last = 0;
                second_pass_sha_unit_input_stream.write(req);

                _second_pass_sha_unit_req_count.write_nb(true);
                _second_pass_state = HASH_FIRST_PASS;
            }

            break;

        case HASH_FIRST_PASS:
            if (!first_pass_sha_unit_output_stream.empty()) {
                coap_sha_response response = first_pass_sha_unit_output_stream.read();
                coap_sha_request req;
                pad_block<SHA256_HASH_BITS, SHA256_BLOCK_BITS + SHA256_HASH_BITS, SHA256_HASH_BITS>(req.data,
                                                                                                    response.data);
                req.last = 1;
                second_pass_sha_unit_input_stream.write(req);

                _first_pass_sha_unit_res_count.write_nb(true);
                _second_pass_sha_unit_req_count.write_nb(true);
                _second_pass_state = HASH_KEY;
            }

            break;
    }
}

void coap::verify_sig(hls::stream<coap_sha_response>& second_pass_sha_unit_output_stream) {
#pragma HLS pipeline ii=5 enable_flush
    if (second_pass_sha_unit_output_stream.empty() ||
        _verify_sig_request_stream.empty() ||
        _decision_stream.full())
        return;

    coap_sha_response digest = second_pass_sha_unit_output_stream.read();
    coap_verify_sig_request req = _verify_sig_request_stream.read();
    coap_sig sig = _sig_stream[req.tc].read();

    ap_uint<SIGNATURE_BASE64_LENGTH_BITS> encoded;
    base64_encode(encoded, digest.data);

    bool match = encoded == sig.buff;

    coap_decision d;
    d.pass = match;
    d.ring_id = req.ring_id;
    _decision_stream.write(d);

    _second_pass_sha_unit_res_count.write_nb(true);
}

void coap::action_resolution(pipeline_ports& p, tc_pipeline_data_counts& tc) {
#pragma HLS pipeline enable_flush ii=5
    if (update())
        return;

    if (!_decision_stream.empty()
        && !_buffer_metadata.empty()
        && !_action_stream.full()
        && !_backpressure_drop.full()
        && !_action_count.full()) {
        hls_ik::metadata metadata = _buffer_metadata.read();
        coap_decision d = _decision_stream.read();

        ring_id_t ring_id = d.ring_id;
        bool action = d.pass;

        if (action) {
            bool backpressure = !can_transmit(tc, metadata.ikernel_id, ring_id, metadata.length + 32, HOST);

            if (backpressure) {
                action = false;
                _backpressure_drop.write_nb(std::make_tuple(metadata.ikernel_id, true));
            }

        }

        coap_packet_action pa;

        if (action) {
            if (ring_id != 0) {
                new_message(ring_id, HOST);
                metadata.ring_id = ring_id;
                custom_ring_metadata cr;
                cr.end_of_message = 1;
                pa.src_port = metadata.get_packet_metadata().udp_src;
                pa.src_ip = metadata.get_packet_metadata().ip_src;
                metadata.length += 32;
                metadata.var = cr;
                metadata.verify();
            }

            p.metadata_output.write(metadata);
        }

        pa.ring_id = ring_id;
        pa.pass = action;
        _action_stream.write(pa);
        _action_count.write_nb(std::make_tuple(metadata.ikernel_id, action));
    }
}

void coap::step(ports& p, tc_ikernel_data_counts& tc) {
#pragma HLS inline
    pass_packets(p.host);
    _raw_dup.dup2(p.net.data_input, _parser_data, _buffer_data);
    _metadata_dup.dup2(p.net.metadata_input, _parser_metadata, _buffer_metadata);
    drop_or_pass(p.net);
    action_resolution(p.net, tc.net);
    read_key();
    parse_packet();
}

int coap::reg_write(int address, int value, ikernel_id_t ikernel_id)
{
#pragma HLS inline
    if (address >= COAP_SCHEDULER)
        return _sched.rpc(address - COAP_SCHEDULER, &value, ikernel_id, false);

    return rpc(address, &value, ikernel_id, false);
}

int coap::reg_read(int address, int* value, ikernel_id_t ikernel_id)
{
#pragma HLS inline
    if (address >= COAP_SCHEDULER)
        return _sched.rpc(address - COAP_SCHEDULER, value, ikernel_id, true);

    return rpc(address, value, ikernel_id, true);
}

ring_id_t coap_contexts::find_ring(const ikernel_id_t& id)
{
    return (*this)[id].ring_id;
}

int coap_stats_contexts::rpc(int address, int *v, hls_ik::ikernel_id_t ikernel_id, bool read) {
    uint32_t index = ikernel_id;

    switch(address) {
        case COAP_DROPPED_COUNT:
            return gateway_access_field<ap_uint<32>, &coap_stats_context::dropped_count>(index, v, read);
        case COAP_PASSED_COUNT:
            return gateway_access_field<ap_uint<32>, &coap_stats_context::passed_count>(index, v, read);
        case COAP_FIRST_PASS_SHA_UNIT_REQ_COUNT:
            return gateway_access_field<ap_uint<32>, &coap_stats_context::first_pass_sha_unit_req_count>(index, v, read);
        case COAP_FIRST_PASS_SHA_UNIT_RES_COUNT:
            return gateway_access_field<ap_uint<32>, &coap_stats_context::first_pass_sha_unit_res_count>(index, v, read);
        case COAP_SECOND_PASS_SHA_UNIT_REQ_COUNT:
            return gateway_access_field<ap_uint<32>, &coap_stats_context::second_pass_sha_unit_req_count>(index, v, read);
        case COAP_SECOND_PASS_SHA_UNIT_RES_COUNT:
            return gateway_access_field<ap_uint<32>, &coap_stats_context::second_pass_sha_unit_res_count>(index, v, read);
        case COAP_PARSED_PACKETS_COUNT:
            return gateway_access_field<ap_uint<32>, &coap_stats_context::parsed_packets_count>(index, v, read);
        case COAP_DROPPER_UNIT_STATE:
            return gateway_access_field<ap_uint<32>, &coap_stats_context::dropper_unit_state>(index, v, read);
        case COAP_PARSER_UNIT_STATE:
            return gateway_access_field<ap_uint<32>, &coap_stats_context::dropper_unit_state>(index, v, read);
        case COAP_FIRST_PASS_UNIT_STATE:
            return gateway_access_field<ap_uint<32>, &coap_stats_context::first_pass_unit_state>(index, v, read);
        case COAP_SECOND_PASS_UNIT_STATE:
            return gateway_access_field<ap_uint<32>, &coap_stats_context::second_pass_unit_state>(index, v, read);
        case COAP_BACKPRESSURE_DROP_COUNT:
            return gateway_access_field<ap_uint<32>, &coap_stats_context::backpressure_drop_count>(index, v, read);
        default:
            if (read)
                *v = -1;
            return GW_FAIL;
    }
}


int coap::rpc(int address, int *v, ikernel_id_t ikernel_id, bool read)
{
#pragma HLS inline
#pragma HLS inline region
    uint32_t index = ikernel_id;

    if (ikernel_id >= NUM_COAP_CONTEXTS) {
        return GW_FAIL;
    }

    if (address <= COAP_RING_ID && read) {
        *v = -1;
        return GW_FAIL;
    }

    if (address >= COAP_KEY && address < COAP_KEY + 16) {
        int n = address - COAP_KEY;
        coap_key_parts &c = _keys[n];
        c.gateway_context = *v;
        return c.gateway_set(index);
    }

    if (address == COAP_RING_ID) {
        _contexts.gateway_context.ring_id = *v;
        return _contexts.gateway_set(index);
    }

    return _stats_contexts.rpc(address, v, ikernel_id, read);
}

static coap coap_inst;
void coap_ikernel(hls_ik::ports& ik, hls_ik::ikernel_id& uuid,
                  hls_ik::virt_gateway_registers& gateway,
                  hls_ik::tc_ikernel_data_counts& tc,
                  hls::stream<coap_sha_request>& first_pass_sha_unit_input_stream,
                  hls::stream<coap_sha_response>& first_pass_sha_unit_output_stream,
                  hls::stream<coap_sha_request>& second_pass_sha_unit_input_stream,
                  hls::stream<coap_sha_response>& second_pass_sha_unit_output_stream)
{
    DO_PRAGMA(HLS dataflow)
    DO_PRAGMA(HLS ARRAY_RESHAPE variable=uuid.uuid complete dim=1)
    IKERNEL_PORTS_PRAGMAS(ik)
    DO_PRAGMA_SYN(HLS interface s_axilite port=uuid offset=0x1000)
    IKERNEL_TC_PORTS_PRAGMAS(tc)
    VIRT_GATEWAY_OFFSET(gateway, 0x1014, 0x101c, 0x102c, 0x1034)
    DO_PRAGMA_SYN(HLS interface ap_ctrl_none port=return)
    DO_PRAGMA(HLS interface axis port=first_pass_sha_unit_input_stream)
    DO_PRAGMA(HLS interface axis port=first_pass_sha_unit_output_stream)
    DO_PRAGMA(HLS interface axis port=second_pass_sha_unit_input_stream)
    DO_PRAGMA(HLS interface axis port=second_pass_sha_unit_output_stream)

    using namespace hls_ik;
    static const ikernel_id __constant_uuid = { COAP_UUID };
    coap_inst.host_credits_update(ik.host_credit_regs); \
    coap_inst.step(ik, tc);
    coap_inst.first_pass(first_pass_sha_unit_input_stream);
    coap_inst.second_pass(first_pass_sha_unit_output_stream, second_pass_sha_unit_input_stream);
    coap_inst.verify_sig(second_pass_sha_unit_output_stream);
    coap_inst.gateway.gateway(gateway.common, [=](ap_uint<31> addr, int& data) -> int {
        DO_PRAGMA(HLS inline)
        if (addr & GW_WRITE)
            return coap_inst.reg_write(addr & ~GW_WRITE, data, gateway.ikernel_id);
        else
            return coap_inst.reg_read(addr & ~GW_WRITE, &data, gateway.ikernel_id);
    });
    output_uuid: uuid = __constant_uuid;
}
