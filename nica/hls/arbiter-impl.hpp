/* * Copyright (c) 2016-2018 Haggai Eran, Gabi Malka, Lior Zeno, Maroun Tork
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 *  * Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation and/or
 * other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <hls_stream.h>
#include <functional>

#include "gateway.hpp"
#include "arbiter.hpp"
#include "nica-top.hpp"
#include "scheduler.hpp"
#include "tc-ports.hpp"

class arbiter : public hls_ik::gateway_impl<arbiter>
{
public:
    static const unsigned num_streams_width = hls_helpers::log2(NUM_TC);
    typedef hls_ik::data_stream stream;
    maybe<ap_uint<udp::udp_builder_metadata::width> > peek_metadata[NUM_TC];

    typedef scheduler<num_streams_width> scheduler_t;
    typedef typename scheduler_t::index_t index_t;
    scheduler_t sched;

    arbiter() : meta_state(META_IDLE), data_state(DATA_IDLE), last_stream(0), stats(),
        quota(0)
    {
    }

    /* Accept a variable length list of arbiter_input_stream structs */
    void arbiter_step(tc_ports& tc, udp::udp_builder_metadata_stream& metadata_out, stream& out, arbiter_stats<NUM_TC>* s,
        hls_ik::gateway_registers& g, trace_event events[4])
    {
#pragma HLS inline
#pragma HLS array_partition variable=s->port complete
#pragma HLS array_partition variable=s->tx_port complete
        pick_next_packet(s, g);
        tx_meta(tc, metadata_out, events);
        tx_data(tc, s, out);
    }

    void pick_next_packet(arbiter_stats<NUM_TC>* s,
        hls_ik::gateway_registers& g)
    {
#pragma HLS pipeline II=3
        /* Inline the gateway here */
#pragma HLS inline region
        hls_ik::gateway_impl<arbiter>::gateway(this, g);

        if (sched.update())
            return;

        index_t req;
        if (!tx_requests.empty()) {
            req = tx_requests.read();
            sched.schedule(req);

            // TODO ++stats.port[schedule_ports_last].not_empty;
        }

        arbiter_stats_output:
        for (int i = 0; i < NUM_TC; ++i)
#pragma HLS unroll
            s->port[i] = stats.port[i];

        if (scheduler_decision.full())
            return;

#pragma HLS array_partition variable=stats.port complete

        index_t selected_stream;
        uint32_t quota;
        if (!sched.next_flow(&selected_stream, &quota))
            return;

        scheduler_decision.write(scheduler_cmd{selected_stream, quota});
    }

    void schedule_ports()
    {
#pragma HLS inline
        if (tx_requests.full())
            return;

        if (!empty_metadata(schedule_ports_last) && !interrupt_sent(schedule_ports_last, schedule_ports_last)) {
            tx_requests.write(schedule_ports_last);
            interrupt_sent(schedule_ports_last, schedule_ports_last) = 1;
        }

        ++schedule_ports_last;
        if (schedule_ports_last == NUM_TC)
            schedule_ports_last = 0;
    }

    enum { META_IDLE, NEW_PACKET } meta_state;
    typedef std::tuple<udp::udp_builder_metadata, index_t> data_status_t;
    hls::stream<data_status_t> meta_to_data;

    void tx_meta(tc_ports& tc, udp::udp_builder_metadata_stream& metadata_out,
                 trace_event events[4])
    {
#pragma HLS pipeline II=3 enable_flush
#pragma HLS array_partition variable=peek_metadata complete

#pragma HLS stream variable=meta_to_data depth=15
        for (int i = 0; i < 4; ++i)
            events[i] = 0;

        schedule_ports();
        update_peek(tc);

        /* State machine */
        switch (meta_state) {
        case META_IDLE: {
            if (scheduler_decision.empty())
                break;

            auto decision = scheduler_decision.read();
            selected_port = decision.port;
            quota = decision.quantum;

            assert(selected_port < NUM_TC);
            // Make sure only 0-2 are accessed
            switch (selected_port) {
            case 0:
            case 1:
            case 2:
                events[selected_port] = 1;
                break;
            default:
                break;
            }

            meta_state = NEW_PACKET;
            break; /* TODO optimize */
        }
        case NEW_PACKET: {
            if (metadata_out.full() || meta_to_data.full())
                break;

            uint32_t len;
            bool non_empty = peek_stream_packet_length(selected_port, &len);

            if (non_empty && len <= quota) {
                quota -= len; // TODO more accurate packet length
                assert(!empty_metadata(selected_port));
                udp::udp_builder_metadata m = read_metadata(selected_port);
                if (!m.empty_packet())
                    meta_to_data.write_nb(std::make_tuple(m, selected_port));
                metadata_out.write_nb(m);
            } else {
                events[TRACE_ARBITER_EVICTED] = 1;
                meta_state = META_IDLE;
                interrupt_sent(selected_port, selected_port) = 0;
                sched.update_flow(selected_port, non_empty, quota);
                return;
            }
            break;
        }
        }
    }

    enum { DATA_IDLE, DATA_STREAM } data_state;
    data_status_t data_status;

    void tx_data(tc_ports& tc, arbiter_stats<NUM_TC>* s, stream& out)
    {
#pragma HLS pipeline II=1 enable_flush
#pragma HLS array_partition variable=stats.tx_port complete
        /* Update statistics */
        for (int i = 0; i < NUM_TC; ++i)
            s->tx_port[i] = stats.tx_port[i];
        s->out_full = stats.out_full;

        switch (data_state) {
        case DATA_IDLE: {
            if (!meta_to_data.read_nb(data_status))
                break;

            data_state = DATA_STREAM;
            break; /* TODO optimize */
        }
        case DATA_STREAM:
            // TODO close packet if ikernel is misbehaving even in middle of
            // a packet.

            if (out.full())
                break;

            ap_uint<hls_ik::axi_data::width> raw_word;
            index_t selected_port = std::get<1>(data_status);
            switch (selected_port) {
#define BOOST_PP_LOCAL_MACRO(i) \
            case i: \
                if (!(tc.data ## i).read_nb(raw_word)) \
                    return; \
                break;
#define BOOST_PP_LOCAL_LIMITS (0, NUM_TC - 1)
%:include BOOST_PP_LOCAL_ITERATE()
            }
            out.write_nb(raw_word);

            auto& p = stats.tx_port[selected_port];
            ++p.words;
            hls_ik::axi_data word = raw_word;
            if (word.last) {
                ++p.packets;
                data_state = DATA_IDLE;
            }
            break;
        }
    }

    bool decode_gateway_address(int address, int& flow_id, int& cmd)
    {
        if (address >= ARBITER_SCHEDULER) {
            int offset = address - ARBITER_SCHEDULER;
            flow_id = offset >> 1;
            cmd = offset & 1;
            return true;
        }
        return false;
    }

    int reg_write(int address, int value)
    {
#pragma HLS inline
        int flow_id, cmd;
        if (decode_gateway_address(address, flow_id, cmd))
            return sched.rpc(cmd, &value, flow_id, false);

        return GW_DONE;
    }

    int reg_read(int address, int* value)
    {
#pragma HLS inline
        int flow_id, cmd;
        if (decode_gateway_address(address, flow_id, cmd))
            return sched.rpc(cmd, value, flow_id, true);

        switch (address) {
        case ARBITER_NUM_TC:
            *value = NUM_TC;
            break;
        default:
            *value = -1;
            return GW_FAIL;
        }
        return GW_DONE;
    }

    void gateway_update() {}

private:
    void update_peek(tc_ports& tc) {
#pragma HLS inline
#define BOOST_PP_LOCAL_MACRO(port) \
        if (!(tc.meta ## port).empty() && !peek_metadata[port].valid()) { \
            ap_uint<udp::udp_builder_metadata::width> raw; \
            (tc.meta ## port).read_nb(raw); \
            peek_metadata[port] = raw; \
        }
#define BOOST_PP_LOCAL_LIMITS (0, NUM_TC - 1)
%:include BOOST_PP_LOCAL_ITERATE()
    }

    bool peek_stream_packet_length(index_t port, uint32_t* len) {
#pragma HLS inline
        if (peek_metadata[port].valid()) {
            udp::udp_builder_metadata m = peek_metadata[port].value();
            *len = uint32_t(ALIGN(m.length, 32)) >> 5;
            return true;
        }
        return false;
    }

    udp::udp_builder_metadata read_metadata(index_t port) {
#pragma HLS inline
        auto ret = peek_metadata[port].value();
        peek_metadata[port].reset();
        return ret;
    }

    bool empty_metadata(index_t port) {
#pragma HLS inline
        return !peek_metadata[port].valid();
    }

    typedef ap_uint<NUM_TC> port_bitmap_t;
    hls::stream<index_t> tx_requests;

    index_t schedule_ports_last;
    port_bitmap_t interrupt_sent;

    index_t last_stream;
    index_t selected_port;
    struct scheduler_cmd {
        index_t port;
        uint32_t quantum;
    };
    hls::stream<scheduler_cmd> scheduler_decision;
    arbiter_stats<NUM_TC> stats;
    ap_uint<32> cycle_counter;
    /* Number of bytes to charge this port when evicting it */
    int accumulated_charge;
    /* Number of flits a port is allowed to send before it is evicted */
    uint32_t quota;
};

void arbiter_top(udp::udp_builder_metadata_stream& hdr_out, hls_ik::data_stream& out,
                 udp::udp_builder_metadata_stream meta[NUM_TC],
                 hls_ik::data_stream port[NUM_TC],
                 arbiter_stats<NUM_TC>* stats, hls_ik::gateway_registers& arbiter_gateway);
void demux_arb_top(udp::udp_builder_metadata_stream& meta_in, hls_ik::data_stream& data_in,
                   udp::udp_builder_metadata_stream& passthrough_meta_in, hls_ik::data_stream& passthrough_data_in,
                   udp::udp_builder_metadata_stream& hdr_out, hls_ik::data_stream& out,
                   arbiter_stats<NUM_TC>* stats, hls_ik::gateway_registers& arbiter_gateway,
                   tc_ports& tc_out, tc_ports& tc_in);
