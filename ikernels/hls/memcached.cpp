#include "memcached-ik.hpp"

using namespace hls_ik;

int memcached_cache_contexts::rpc(int address, int *v, ikernel_id_t ikernel_id, bool read)
{
#pragma HLS inline
    uint32_t index = ikernel_id;

    switch(address) {
        case MEMCACHED_REG_CACHE_SIZE:
            return gateway_access_field<size_t, &memcached_cache_context::log_size>(index, v, read);
    }

    return GW_DONE;
}

ring_id_t memcached_contexts::find_ring(const ikernel_id_t& id)
{
    return (*this)[id].ring_id;
}

int memcached_contexts::rpc(int address, int *v, ikernel_id_t ikernel_id, bool read)
{
#pragma HLS inline
    uint32_t index = ikernel_id;

    switch(address) {
        case MEMCACHED_RING_ID:
            return gateway_access_field<ring_id_t, &memcached_context::ring_id>(index, v, read);
        default:
            if (read)
                *v = -1;
            return GW_FAIL;
    }

    return GW_DONE;
}

int memcached_stats_contexts::rpc(int address, int *v, ikernel_id_t ikernel_id, bool read)
{
#pragma HLS inline
    uint32_t index = ikernel_id;

    switch(address) {
        case MEMCACHED_STATS_GET_REQUESTS:
            return gateway_access_field<ap_uint<32>, &memcached_stats_context::get_requests>(index, v, read);
        case MEMCACHED_STATS_GET_REQUESTS_HITS:
            return gateway_access_field<ap_uint<32>, &memcached_stats_context::get_req_hit>(index, v, read);
        case MEMCACHED_STATS_GET_REQUESTS_MISSES:
            return gateway_access_field<ap_uint<32>, &memcached_stats_context::get_req_miss>(index, v, read);
        case MEMCACHED_STATS_SET_REQUESTS:
            return gateway_access_field<ap_uint<32>, &memcached_stats_context::set_requests>(index, v, read);
        case MEMCACHED_STATS_N2H_UNKNOWN:
            return gateway_access_field<ap_uint<32>, &memcached_stats_context::n2h_unknown>(index, v, read);
        case MEMCACHED_STATS_GET_RESPONSE:
            return gateway_access_field<ap_uint<32>, &memcached_stats_context::get_response>(index, v, read);
        case MEMCACHED_STATS_H2N_UNKNOWN:
            return gateway_access_field<ap_uint<32>, &memcached_stats_context::h2n_unknown>(index, v, read);
        case MEMCACHED_DROPPED_BACKPRESSURE:
            return gateway_access_field<ap_uint<32>, &memcached_stats_context::backpressure_drop_count>(index, v, read);
        case MEMCACHED_STATS_DROPPED_TC_BACKPRESSURE:
            return gateway_access_field<ap_uint<32>, &memcached_stats_context::tc_backpressure_drop_count>(index, v, read);
        case MEMCACHED_STATS_GET_REQUESTS_DROPPED_HITS:
            return gateway_access_field<ap_uint<32>, &memcached_stats_context::get_req_dropped_hits>(index, v, read);
        default:
            if (read)
                *v = -1;
            return GW_FAIL;
    }

    return GW_DONE;
}

memcached::memcached() :
    _action_stream(150, 0, "_action_stream"),
    _kv_pairs_stream(150, 0, "_kv_pairs_stream"),
    _req_prs2mem(150, 0, "_req_prs2mem"),
    _req_mem2mem(150, 0, "_req_mem2mem")
{
#pragma HLS stream variable=_buffer_data depth=128
#pragma HLS stream variable=_parser_data depth=30
#pragma HLS stream variable=_buffer_metadata depth=128
#pragma HLS stream variable=_parser_metadata depth=30
#pragma HLS stream variable=_reply_data_stream depth=30
#pragma HLS stream variable=_reply_metadata_stream depth=30
#pragma HLS stream variable=_packet_action depth=30
#pragma HLS stream variable=_h2n_stats depth=30
#pragma HLS stream variable=_n2h_stats depth=30
#pragma HLS stream variable=_stats_cache depth=30
#pragma HLS stream variable=_backpressure_drop depth=30
#pragma HLS stream variable=&_action_stream._stream depth=300
#pragma HLS stream variable=&_kv_pairs_stream._stream depth=300
#pragma HLS stream variable=&_req_prs2mem._stream depth=300
#pragma HLS stream variable=&_req_mem2mem._stream depth=300
#pragma HLS data_pack variable=_reply_data_stream
#pragma HLS data_pack variable=&_req_prs2mem._stream
#pragma HLS data_pack variable=&_req_mem2mem._stream
#pragma HLS data_pack variable=&_kv_pairs_stream._stream
}

void memcached::parse_out_payload(const hls_ik::axi_data &d, ap_uint<1>& word_offset, char key[MEMCACHED_KEY_SIZE], char value[MEMCACHED_VALUE_SIZE]) {
#pragma HLS inline
    for (int i = 0; i < 32; ++i) {
#pragma HLS unroll
        const int bottom = 255 - ((i + 1) * 8 - 1), top = 255 - (i * 8);

        if (word_offset == 0 && i >= 14 && i <= 14 + MEMCACHED_KEY_SIZE - 1) {
            key[i - 14] = d.data.range(top, bottom);
        }

        if (word_offset == 0 && i >= VALUE_POS) {
            value[i - VALUE_POS] = d.data.range(top, bottom);
        }

        if (word_offset == 1 && i >= VALUE_POS - 32 && i <= VALUE_POS + MEMCACHED_VALUE_SIZE - 1 - 32) {
            const int idx = VALUE_POS > 32 ? ((32 + i) - VALUE_POS) : ((32 + i) - VALUE_POS) + (31 - VALUE_POS);
            value[idx] = d.data.range(top, bottom);
        }
    }

}

void memcached::parse_in_payload(const hls_ik::axi_data &d, char udp_header[8], char key[MEMCACHED_KEY_SIZE]) {
#pragma HLS inline
    for (int i = 0; i < 32; ++i) {
#pragma HLS unroll
        const int bottom = 255 - ((i + 1) * 8 - 1), top = 255 - (i * 8);

        if (i < 8) {
            udp_header[i] = d.data.range(top, bottom);
        }

        if (i >= 12 && i <= 12 + MEMCACHED_KEY_SIZE - 1) {
            key[i - 12] = d.data.range(top, bottom);
        }
    }
}

void memcached::reply_cached_value(hls_ik::pipeline_ports &out) {
#pragma HLS pipeline enable_flush ii=1
#pragma HLS array_partition variable=_current_response.data complete
    switch (_reply_state) {
        case REQUEST_METADATA:
            if (!_reply_metadata_stream.empty() && !h2n_arb.m2.full()) {
                h2n_arb.m2.write(_reply_metadata_stream.read());
                _reply_state = READ_REQUEST;
                goto read_request;
            }

            break;

        case READ_REQUEST:
        read_request:
            if (!_reply_data_stream.empty()) {
                _current_response = _reply_data_stream.read();
                _reply_state = GENERATE_RESPONSE;
            }

            break;

        case GENERATE_RESPONSE:
            if (h2n_arb.d2.full()) return;

            hls_ik::axi_data d;
            if (_reply_word == 0) {
                d.set_data(_current_response.data, 32);
                d.last = 0;
                _reply_word = 1;
            } else {
                d.set_data(_current_response.data + 32, REPLY_SIZE - 32);
                d.last = 1;
            }

            h2n_arb.d2.write(d);

            if (d.last) {
                _reply_state = REQUEST_METADATA;
                _reply_word = 0;
            }

            break;
    }
}

void memcached::drop_or_pass(hls_ik::pipeline_ports& in) {
#pragma HLS pipeline enable_flush ii=1
    switch (_dropper_state) {
        case METADATA:
            if (!_packet_action.empty()) {
                packet_action pa = _packet_action.read();
                _dropper_action = pa.action;

                if (_dropper_action) {
                    in.metadata_output.write(pa.metadata);
                }

                if (pa.metadata.ring_id != 0) {
                    hls_ik::axi_data d;
                    char buff[32] = {};
                    hls_helpers::memcpy<2>(&buff[0], (char*)&pa.src_port);
                    hls_helpers::memcpy<4>(&buff[2], (char*)&pa.src_ip);
                    d.set_data(&buff[0], 32);
                    d.last = 0;
                    in.data_output.write(d);
                }

                _dropper_state = DATA;
            }
            break;

        case DATA:
            if (!_buffer_data.empty()) {
                axi_data d = _buffer_data.read();

                if (_dropper_action) {
                    in.data_output.write(d);
                }

                if (d.last) {
                    _dropper_state = METADATA;
                }
            }

            break;
    }

}

void memcached::action_resolution(hls_ik::pipeline_ports& in,
                                  hls_ik::tc_pipeline_data_counts& tc) {
#pragma HLS pipeline enable_flush ii=3
    _action_stream.empty_progress();

    if (ctx.update())
        return;
    if (ikernel::update())
        return;

    if (!_action_stream.empty()
        && !_buffer_metadata.empty()
        && !_backpressure_drop.full()
        && !_packet_action.full()) {
        hls_ik::metadata metadata = _buffer_metadata.read();
        bool action = _action_stream.read();

        ring_id_t ring_id = ctx.find_ring(metadata.ikernel_id);

        if (action) {
            bool backpressure = !can_transmit(tc, metadata.ikernel_id, ring_id, metadata.length + 32, HOST);

            if (backpressure) {
                action = false;
                _backpressure_drop.write_nb(std::make_tuple(metadata.ikernel_id, true));
            }
        }

        packet_action pa;

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
        }

        pa.action = action;
        pa.metadata = metadata;

        _packet_action.write(pa);
    }
}

void memcached::intercept_out(hls_ik::pipeline_ports &out,
                              hls_ik::tc_pipeline_data_counts& tc) {
#pragma HLS pipeline enable_flush ii=1
    _kv_pairs_stream.full_progress();

    switch(_out_state) {
        case METADATA:
            if (!out.metadata_input.empty() && !h2n_arb.m1.full()) {
                _out_metadata = out.metadata_input.read();
                _intercept_tc_backpressure = !can_transmit(tc, _out_metadata.ikernel_id, 0,
                                                           _out_metadata.length, NET);
                if (!_intercept_tc_backpressure) {
                    h2n_arb.m1.write(_out_metadata);
                }
                _parsed_response.ikernel_id = _out_metadata.ikernel_id;
                _out_state = DATA;
            }

            break;
        case DATA:
            if (_kv_pairs_stream.full()) return;

            if (!out.data_input.empty() && !h2n_arb.d1.full()) {
                axi_data d = out.data_input.read();

                if (_out_word == 0) {
                    const int bottom = 255 - ((8 + 1) * 8 - 1), top = 255 - (8 * 8);
                    _response_type_char = d.data.range(top, bottom);
                }

                parse_out_payload(d, _out_word, _parsed_response.key.data, _parsed_response.value.data);
                _out_word = 1;

                // We parse get responses.
                // On get response: cache the key.
                if (d.last) {
                    if (_response_type_char == 'V') {
                        _kv_pairs_stream.write(_parsed_response);
                        _h2n_stats.write(std::make_tuple(_out_metadata.ikernel_id, H2N_GET_RESPONSE));
                    } else {
                        _h2n_stats.write(std::make_tuple(_out_metadata.ikernel_id, H2N_UNKNOWN));
                    }

                    _out_word = 0;
                    _out_state = METADATA;
                }

                if (!_intercept_tc_backpressure)
                    h2n_arb.d1.write(d);
            }
    }
}

void memcached::parse_packet(hls_ik::trace_event events[IKERNEL_NUM_EVENTS]) {
#pragma HLS pipeline enable_flush ii=1
    events[MEMCACHED_EVENT_PARSED_REQUESTS_STREAM_FULL] = false;
    events[MEMCACHED_EVENT_PARSED_REQUESTS_STREAM_FULL_INTERNAL] = false;
    events[MEMCACHED_EVENT_PARSED_REQUESTS_STREAM_WRITE] = false;

    _req_prs2mem.full_progress();

    switch (_in_state) {
        case METADATA:
            if (!_parser_metadata.empty()) {
                _in_metadata = _parser_metadata.read();
                _parsed_request.metadata = _in_metadata;
                _in_state = DATA;
            }
            break;

        case DATA: {
            const bool requests_full = _req_prs2mem.full();
            events[MEMCACHED_EVENT_PARSED_REQUESTS_STREAM_FULL] = requests_full;
            if (!_parser_data.empty() && !requests_full) {
                axi_data d = _parser_data.read();

                if (_in_offset == 0) {
                    const int bottom = 255 - ((8 + 1) * 8 - 1), top = 255 - (8 * 8);
                    _request_type_char = d.data.range(top, bottom);

                    if (_request_type_char == 'g') {
                        _parsed_request.type = GET;
                    } else if (_request_type_char == 's') {
                        _parsed_request.type = SET;
                    } else {
                        _parsed_request.type = OTHER;
                    }

                    parse_in_payload(d, _parsed_request.udp_header, _parsed_request.key.data);
                    ++_in_offset;
                }


                if (d.last) {
                    _n2h_stats.write(std::make_tuple(_parsed_request.metadata.ikernel_id, _parsed_request.type));
                    _req_prs2mem.write(_parsed_request);
                    events[MEMCACHED_EVENT_PARSED_REQUESTS_STREAM_WRITE] = true;
                    _in_offset = 0;
                    _in_state = METADATA;
                }

            }
            break;
        }
    }
}

void memcached::handle_parsed_packet(memory& m, hls_ik::trace_event events[IKERNEL_NUM_EVENTS])
{
#pragma HLS pipeline enable_flush ii=3

    _kv_pairs_stream.empty_progress();
    _req_prs2mem.empty_progress();
    _req_mem2mem.full_progress();

    if (cache_ctx.update()) {
        return;
    }

    const bool parsed_requests_stream_empty = _req_prs2mem.empty();
    events[MEMCACHED_EVENT_PARSED_REQUESTS_STREAM_EMPTY] =
        parsed_requests_stream_empty;
    events[MEMCACHED_EVENT_PARSED_REQUESTS_STREAM_READ] = false;

    if (!_kv_pairs_stream.empty()) {
        memcached_key_value_pair kv = _kv_pairs_stream.read();
        _index.insert(m, kv.key, kv.value, cache_ctx[kv.ikernel_id].log_size, kv.ikernel_id);
        return;
    }

    if (parsed_requests_stream_empty || !_index.can_post_find(m) ||
        _req_mem2mem.full())
    {
        return;
    }

    events[MEMCACHED_EVENT_PARSED_REQUESTS_STREAM_READ] = true;
    memcached_parsed_request parsed_request = _req_prs2mem.read();

    ikernel_id_t id = parsed_request.metadata.ikernel_id;

    if (parsed_request.type == GET) {
        _index.find(m, parsed_request.key, cache_ctx[id].log_size, id);
    } else if (parsed_request.type == SET) {
        _index.erase(m, parsed_request.key, cache_ctx[id].log_size, id);
    }
    _req_mem2mem.write_nb(parsed_request);
}

void memcached::handle_memory_responses(memory& m, hls_ik::tc_pipeline_data_counts& tc_host) {
#pragma HLS pipeline enable_flush ii=3
    bool pass_to_host = true;
    ikernel_id_t id;

    _action_stream.full_progress();
    _req_mem2mem.empty_progress();

    /* Handle write responses */
    if (m.has_write_response()) {
        m.get_write_response();
        // TODO statistics about the repsonses status */
    }

    switch (mem_resp_state) {
    case MEM_RESP_IDLE:
        if (_req_mem2mem.empty() || _action_stream.full())
            return;

        _req_mem2mem.read_nb(mem_resp_req);
        mem_resp_state = MEM_RESP_WAIT;
        /* Fall through */

    case MEM_RESP_WAIT:
        ikernel_id_t id = mem_resp_req.metadata.ikernel_id;

        switch (mem_resp_req.type) {
        case GET: {
            if (!_index.has_find_result(m))
                return;

            ntl::maybe<memcached_value<MEMCACHED_VALUE_SIZE> > found = _index.get_find_result(m);

            if (found.valid()) {
                pass_to_host = false;
                bool tc_backpressure = !can_transmit(tc_host, id, 0, REPLY_SIZE, NET);
                if (tc_backpressure) {
                    _stats_cache.write(std::make_tuple(id, STAT_HIT_TC_BACKPRESSURE));
                } else if (!_reply_metadata_stream.full() && !_reply_data_stream.full()) {
                    _reply_metadata_stream.write(mem_resp_req.metadata.reply(REPLY_SIZE));
                    _reply_data_stream.write(generate_response(mem_resp_req, found.value()));
                    _stats_cache.write(std::make_tuple(id, STAT_HIT_GEN));
                } else {
                    _stats_cache.write(std::make_tuple(id, STAT_HIT_DROP));
                }
            } else {
                _stats_cache.write(std::make_tuple(id, STAT_MISS));
            }
            break;
        }
        case SET:
        case OTHER:
            break;
        }
        _action_stream.write(pass_to_host);
        mem_resp_state = MEM_RESP_IDLE;
        break;
    }
}

memcached_response memcached::generate_response(const memcached_parsed_request& parsed_request, const memcached_value<MEMCACHED_VALUE_SIZE> &value) {
    memcached_response response;

    hls_helpers::memcpy<8>(&response.data[0], &parsed_request.udp_header[0]);
    hls_helpers::memcpy<MEMCACHED_KEY_SIZE>(&response.data[14], &parsed_request.key.data[0]);
    hls_helpers::memcpy<MEMCACHED_VALUE_SIZE>(&response.data[19 + MEMCACHED_KEY_SIZE + VALUE_BYTES_SIZE], &value.data[0]);

    return response;
}

int memcached::reg_write(int address, int value, ikernel_id_t ikernel_id)
{
#pragma HLS inline
    if (ikernel_id >= MEMCACHED_RING_COUNT) {
        return GW_FAIL;
    }

    switch (address) {
    case MEMCACHED_REG_CACHE_SIZE:
        return cache_ctx.rpc(address, &value, ikernel_id, false);
    case MEMCACHED_RING_ID:
        return ctx.rpc(address, &value, ikernel_id, false);
    default:
        return stats_ctx.rpc(address, &value, ikernel_id, false);
    }
}


int memcached::reg_read(int address, int* value, ikernel_id_t ikernel_id) {
#pragma HLS inline
    if (ikernel_id >= MEMCACHED_RING_COUNT) {
        *value = -1;
        return GW_FAIL;
    }

    switch (address) {
    case MEMCACHED_REG_CACHE_SIZE:
        return cache_ctx.rpc(address, value, ikernel_id, true);
    case MEMCACHED_RING_ID:
        return ctx.rpc(address, value, ikernel_id, true);
    default:
        return stats_ctx.rpc(address, value, ikernel_id, true);
    }
}

void memcached::update_stats()
{
#pragma HLS pipeline enable_flush ii=3
    ikernel_id_t id;

    if (stats_ctx.update())
        return;

    if (!_n2h_stats.empty()) {
        request_type req;
        std::tie(id, req) =  _n2h_stats.read();
        memcached_stats_context& c = stats_ctx[id];

        switch (req) {
        case GET:
            ++c.get_requests;
            break;
        case SET:
            ++c.set_requests;
            break;
        case OTHER:
            ++c.n2h_unknown;
            break;
        }
    }

    if (!_h2n_stats.empty()) {
        h2n_packet_type resp;
        std::tie(id, resp) = _h2n_stats.read();
        memcached_stats_context& c = stats_ctx[id];

        switch (resp) {
        case H2N_GET_RESPONSE:
            ++c.get_response;
            break;
        case H2N_UNKNOWN:
            ++c.h2n_unknown;
            break;
        }
    }

    if (!_stats_cache.empty()) {
        cache_stats_type type;
        std::tie(id, type) = _stats_cache.read();
        memcached_stats_context& c = stats_ctx[id];

        switch (type) {
            case STAT_HIT_GEN:
                ++c.get_req_hit;
                break;
            case STAT_HIT_DROP:
                ++c.get_req_dropped_hits;
                break;
            case STAT_HIT_TC_BACKPRESSURE:
                ++c.tc_backpressure_drop_count;
                break;
            case STAT_MISS:
                ++c.get_req_miss;
                break;
        }
    }

    if (!_backpressure_drop.empty()) {
        bool hit;
        std::tie(id, hit) = _backpressure_drop.read();
        memcached_stats_context& c = stats_ctx[id];

        ++c.backpressure_drop_count;
    }
}

void memcached::step(hls_ik::ports& p, hls_ik::tc_ikernel_data_counts& tc)
{
#pragma HLS inline
    _raw_dup.dup2(p.net.data_input, _parser_data, _buffer_data);
    _metadata_dup.dup2(p.net.metadata_input, _parser_metadata, _buffer_metadata);
    drop_or_pass(p.net);
    action_resolution(p.net, tc.net);
    handle_parsed_packet(p.mem, p.events);
    handle_memory_responses(p.mem, tc.host);
    parse_packet(p.events);
    reply_cached_value(p.host);
    intercept_out(p.host, tc.host);
    h2n_arb.arbitrate(p.host.metadata_output, p.host.data_output);
    update_stats();
}

DEFINE_TOP_FUNCTION(memcached_top, memcached, MEMCACHED_UUID)
