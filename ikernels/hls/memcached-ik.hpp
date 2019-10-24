#ifndef MEMCACHED_HPP
#define MEMCACHED_HPP

// d68adb30-4d19-4f3e-8542-fc184db75bf7
#define MEMCACHED_UUID { 0xd6,0x8a,0xdb,0x30,0x4d,0x19,0x4f,0x3e,0x85,0x42,0xfc,0x18,0x4d,0xb7,0x5b,0xf7 }

#include <ikernel.hpp>
#include <gateway.hpp>
#include <hls_helper.h>
#include <mlx.h>
#include <flow_table.hpp>
#include <ntl/context_manager.hpp>
#include <ntl/constexpr.hpp>
#include "memcached_cache.hpp"
#include <ntl/programmable_fifo.hpp>

#define MEMCACHED_LOG_RING_COUNT 10
#define MEMCACHED_RING_COUNT (1 << MEMCACHED_LOG_RING_COUNT)
#define BUFFER_SIZE (20 + MEMCACHED_VALUE_SIZE + MEMCACHED_KEY_SIZE)
#define BUFFER_SIZE_WORDS ((BUFFER_SIZE + 31) / 32)
// Value size length. VALUE_BYTES_SIZE and MEMCACHED_VALUE_SIZE should be changed together.
#define VALUE_BYTES_SIZE 2
#ifndef MEMCACHED_VALUE_SIZE
#define MEMCACHED_VALUE_SIZE 10
#endif
#ifndef MEMCACHED_KEY_SIZE
#define MEMCACHED_KEY_SIZE 10
#endif
#define REPLY_SIZE (8 + 6 + MEMCACHED_KEY_SIZE + 3 + VALUE_BYTES_SIZE + 2 + MEMCACHED_VALUE_SIZE + 7)
#define VALUE_POS (19 + MEMCACHED_KEY_SIZE + VALUE_BYTES_SIZE)

DECLARE_TOP_FUNCTION(memcached_top);

#define STRINGIZE(x) #x
#define STRINGIZE_VALUE_OF(x) STRINGIZE(x)

static constexpr const char* ASCII_MEMCACHED_VALUE_SIZE = STRINGIZE_VALUE_OF(MEMCACHED_VALUE_SIZE);
static const unsigned LOG_DEFAULT_CACHE_SIZE = ntl::log2(MEMCACHED_DEFAULT_CACHE_SIZE);

struct memcached_response {
    char data[REPLY_SIZE];

    memcached_response() {
        data[8] = 'V';
        data[9] = 'A';
        data[10] = 'L';
        data[11] = 'U';
        data[12] = 'E';
        data[13] = ' ';
        data[14 + MEMCACHED_KEY_SIZE] = ' ';
        data[15 + MEMCACHED_KEY_SIZE] = '0'; // Flags
        data[16 + MEMCACHED_KEY_SIZE] = ' ';

        // Value size
        hls_helpers::memcpy<VALUE_BYTES_SIZE>(&data[17 + MEMCACHED_KEY_SIZE], &ASCII_MEMCACHED_VALUE_SIZE[0]);

        // Spaces
        data[17 + MEMCACHED_KEY_SIZE + VALUE_BYTES_SIZE] = '\r';
        data[18 + MEMCACHED_KEY_SIZE + VALUE_BYTES_SIZE] = '\n';
        data[19 + MEMCACHED_KEY_SIZE + VALUE_BYTES_SIZE + MEMCACHED_VALUE_SIZE] = '\r';
        data[20 + MEMCACHED_KEY_SIZE + VALUE_BYTES_SIZE + MEMCACHED_VALUE_SIZE] = '\n';
        data[21 + MEMCACHED_KEY_SIZE + VALUE_BYTES_SIZE + MEMCACHED_VALUE_SIZE] = 'E';
        data[22 + MEMCACHED_KEY_SIZE + VALUE_BYTES_SIZE + MEMCACHED_VALUE_SIZE] = 'N';
        data[23 + MEMCACHED_KEY_SIZE + VALUE_BYTES_SIZE + MEMCACHED_VALUE_SIZE] = 'D';
        data[24 + MEMCACHED_KEY_SIZE + VALUE_BYTES_SIZE + MEMCACHED_VALUE_SIZE] = '\r';
        data[25 + MEMCACHED_KEY_SIZE + VALUE_BYTES_SIZE + MEMCACHED_VALUE_SIZE] = '\n';
    }
};

enum request_type { GET, SET, OTHER };

struct memcached_parsed_request {
    char udp_header[8];
    memcached_key<MEMCACHED_KEY_SIZE> key;
    request_type type;
    hls_ik::metadata metadata;
};

struct memcached_key_value_pair {
    memcached_key<MEMCACHED_KEY_SIZE> key;
    memcached_value<MEMCACHED_VALUE_SIZE> value;
    hls_ik::ikernel_id_t ikernel_id;
};

struct packet_action {
    bool action;
    hls_ik::metadata metadata;
    ap_uint<16> src_port;
    ap_uint<32> src_ip;
};

/* Arbiter between two sets of actions/metadata_output/data_output competing
 * over an ikernel's egress interface. */
class ikernel_arbiter
{
public:
    ikernel_arbiter() {}

    hls_ik::metadata_stream m1;
    hls_ik::metadata_stream m2;

    hls_ik::data_stream d1;
    hls_ik::data_stream d2;

    void arbitrate(hls_ik::metadata_stream& mout,
                   hls_ik::data_stream& dout)
    {
#pragma HLS pipeline enable_flush ii=1
#pragma HLS stream variable=m1 depth=15
#pragma HLS stream variable=m2 depth=15
#pragma HLS stream variable=d1 depth=15
#pragma HLS stream variable=d2 depth=15
        hls_ik::axi_data data;

        switch (state) {
        case IDLE:
            /* First port has priority */
            if (!m1.empty()) {
                cur = 0;
                mout.write(m1.read());
            } else if (!m2.empty()) {
                cur = 1;
                mout.write(m2.read());
            } else {
                return;
            }
            state = STREAM;
            /* Fallthrough */
        case STREAM:
            if (cur == 0) {
                if (d1.empty())
                    return;
                data = d1.read();
            } else {
                if (d2.empty())
                    return;
                data = d2.read();
            }
            dout.write(data);
            state = data.last ? IDLE : STREAM;
            break;
        }
    }

    enum { IDLE, STREAM } state;
    ap_uint<1> cur;
};

struct memcached_context {
    memcached_context() :
            ring_id(0)
    {}

    hls_ik::ring_id_t ring_id;
};

struct memcached_stats_context {
    memcached_stats_context() :
            get_requests(0),
            get_req_hit(0),
            get_req_dropped_hits(0),
            get_req_miss(0),
            set_requests(0),
            n2h_unknown(0),
            get_response(0),
            h2n_unknown(0),
            backpressure_drop_count(0),
            tc_backpressure_drop_count(0)
    {}

    ap_uint<32> get_requests,
            get_req_hit,
            get_req_dropped_hits,
            get_req_miss,
            set_requests,
            n2h_unknown,
            get_response,
            h2n_unknown,
            backpressure_drop_count,
            tc_backpressure_drop_count;
};

struct memcached_cache_context {
    memcached_cache_context(): log_size(LOG_DEFAULT_CACHE_SIZE) {}

    size_t log_size;
};

class memcached_cache_contexts : public ntl::context_manager<memcached_cache_context, MEMCACHED_LOG_RING_COUNT>
{
public:
    int rpc(int address, int *value, hls_ik::ikernel_id_t ikernel_id, bool read);
};

class memcached_stats_contexts : public ntl::context_manager<memcached_stats_context, MEMCACHED_LOG_RING_COUNT>
{
public:
    int rpc(int address, int *value, hls_ik::ikernel_id_t ikernel_id, bool read);
};

class memcached_contexts : public ntl::context_manager<memcached_context, MEMCACHED_LOG_RING_COUNT>
{
public:
    int rpc(int address, int *value, hls_ik::ikernel_id_t ikernel_id, bool read);

    hls_ik::ring_id_t find_ring(const hls_ik::ikernel_id_t& ikernel_id);
};

class memcached : public hls_ik::ikernel {
public:
    void step(hls_ik::ports& ports, hls_ik::tc_ikernel_data_counts& tc);
    int reg_write(int address, int value, hls_ik::ikernel_id_t ikernel_id);
    int reg_read(int address, int* value, hls_ik::ikernel_id_t ikernel_id);
    memcached();

    typedef hls_ik::memory_t memory;

private:
    memcached_response generate_response(const memcached_parsed_request &parsed_request, const memcached_value<MEMCACHED_VALUE_SIZE> &value);
    void parse_packet(hls_ik::trace_event events[IKERNEL_NUM_EVENTS]);
    void action_resolution(hls_ik::pipeline_ports& in,
                           hls_ik::tc_pipeline_data_counts& tc);
    void reply_cached_value(hls_ik::pipeline_ports &out);
    void intercept_out(hls_ik::pipeline_ports &out,
                       hls_ik::tc_pipeline_data_counts& tc);
    void handle_parsed_packet(memory& m,
        hls_ik::trace_event events[IKERNEL_NUM_EVENTS]);
    void handle_memory_responses(memory& m,
        hls_ik::tc_pipeline_data_counts& tc);
    void parse_out_payload(const hls_ik::axi_data &d, ap_uint<1>& offset, char key[MEMCACHED_KEY_SIZE], char value[MEMCACHED_VALUE_SIZE]);
    void parse_in_payload(const hls_ik::axi_data &d, char udp_header[8], char key[MEMCACHED_KEY_SIZE]);
    void drop_or_pass(hls_ik::pipeline_ports& in);
    void update_stats();

    enum state { METADATA, DATA };
    enum reply_state { REQUEST_METADATA, READ_REQUEST, GENERATE_RESPONSE };

    state _in_state, _dropper_state, _out_state;
    reply_state _reply_state;
    bool _dropper_action;
    char _request_type_char, _response_type_char;
    int _in_offset;
    memcached_response _current_response;
    memcached_cache<MEMCACHED_KEY_SIZE, MEMCACHED_VALUE_SIZE> _index;
    memcached_parsed_request _parsed_request;
    memcached_key_value_pair _parsed_response;
    hls_ik::metadata _in_metadata, _out_metadata;
    ntl::programmable_fifo<bool, 300> _action_stream;
    ntl::programmable_fifo<memcached_key_value_pair,300> _kv_pairs_stream;
    ntl::programmable_fifo<memcached_parsed_request,300> _req_prs2mem;
    ntl::programmable_fifo<memcached_parsed_request,300> _req_mem2mem;
    hls_ik::metadata_stream _reply_metadata_stream, _parser_metadata, _buffer_metadata;
    hls_ik::data_stream _parser_data, _buffer_data;
    hls_helpers::duplicator<1, ap_uint<hls_ik::axi_data::width> > _raw_dup;
    hls_helpers::duplicator<1, ap_uint<hls_ik::metadata::width> > _metadata_dup;
    hls::stream<memcached_response> _reply_data_stream;
    packet_action _pa;
    ap_uint<1> _reply_word, _out_word;
    bool _intercept_tc_backpressure;

    /* port 1 is for passthrough, 2 is for generated */
    ikernel_arbiter h2n_arb;

    /* Counters */
    enum h2n_packet_type {
        H2N_GET_RESPONSE,
        H2N_UNKNOWN,
    };
    hls::stream<std::tuple<hls_ik::ikernel_id_t, h2n_packet_type> > _h2n_stats;
    hls::stream<std::tuple<hls_ik::ikernel_id_t, request_type> > _n2h_stats;

    enum cache_stats_type {
        STAT_HIT_GEN,
        STAT_HIT_DROP,
        STAT_HIT_TC_BACKPRESSURE,
        STAT_MISS,
    };

    hls::stream<std::tuple<hls_ik::ikernel_id_t, cache_stats_type> > _stats_cache;
    hls::stream<std::tuple<hls_ik::ikernel_id_t, bool> > _backpressure_drop; /* An event counts for a drop caused by CR backpressure */
    hls::stream<packet_action> _packet_action;

    memcached_contexts ctx;
    memcached_cache_contexts cache_ctx;
    memcached_stats_contexts stats_ctx;

    // handle_memory_responses state
    enum { MEM_RESP_IDLE, MEM_RESP_WAIT } mem_resp_state;
    memcached_parsed_request mem_resp_req;
};

#define MEMCACHED_REG_CACHE_SIZE 0

#define MEMCACHED_STATS_GET_REQUESTS 0x10
#define MEMCACHED_STATS_GET_REQUESTS_HITS 0x11
#define MEMCACHED_STATS_GET_REQUESTS_MISSES 0x12
#define MEMCACHED_STATS_SET_REQUESTS 0x13
#define MEMCACHED_STATS_N2H_UNKNOWN 0x14
#define MEMCACHED_STATS_GET_RESPONSE 0x15
#define MEMCACHED_STATS_H2N_UNKNOWN 0x16
#define MEMCACHED_DROPPED_BACKPRESSURE 0x17
#define MEMCACHED_RING_ID 0x18
#define MEMCACHED_STATS_GET_REQUESTS_DROPPED_HITS 0x19
#define MEMCACHED_STATS_DROPPED_TC_BACKPRESSURE 0x20

/* Trace events numbers */
#define MEMCACHED_EVENT_PARSED_REQUESTS_STREAM_FULL 0
#define MEMCACHED_EVENT_PARSED_REQUESTS_STREAM_FULL_INTERNAL 1
#define MEMCACHED_EVENT_PARSED_REQUESTS_STREAM_EMPTY 2
#define MEMCACHED_EVENT_PARSED_REQUESTS_STREAM_READ 3
#define MEMCACHED_EVENT_PARSED_REQUESTS_STREAM_WRITE 4

#endif //MEMCACHED_HPP
