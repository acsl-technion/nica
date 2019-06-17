//
// Copyright (c) 2016-2018 Haggai Eran, Gabi Malka, Lior Zeno, Maroun Tork
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

#include "ikernel.hpp"

#define TC_META_THRESHOLD 256
#define TC_DATA_THRESHOLD 256

namespace hls_ik {

    bool ikernel::can_transmit(tc_pipeline_data_counts& tc, ikernel_id_t id, ring_id_t ring, pkt_len_t len, direction_t dir)
    {
#pragma HLS inline
        if (dir == HOST && ring != 0 && !host_credits[ring - 1].can_transmit())
            return false;

        const tc_ports_data_counts& meta = tc.tc_meta_counts;
        const tc_ports_data_counts& data = tc.tc_data_counts;
        /* TODO store TC in context */
        int traffic_class = id & (NUM_TC - 1);
        if (traffic_class == NUM_TC - 1)
            traffic_class = 0;
        if (meta[traffic_class] > TC_META_THRESHOLD - 1 || data[traffic_class] > TC_DATA_THRESHOLD - (len >> 5))
            return false;

        return true;
    }

    bool ikernel::update()
    {
        credit_update_registers regs;
        if (!credit_updates.read_nb(regs))
            return false;

        if (regs.reset)
            host_credits[regs.ring_id - 1].msn = 0;

        host_credits[regs.ring_id - 1].max_msn = regs.max_msn;
        return true;
    }

    void ikernel::host_credits_update(credit_update_registers& regs)
    {
        if (credit_updates.full())
            return;

        if (last_host_credit_regs != regs) {
            if (regs.ring_id == 0 || regs.ring_id >= (1 << CUSTOM_RINGS_LOG_NUM)) {
                std::cout << "Warning: got invalid ring ID on the credit update interface.\n";
                last_host_credit_regs = regs;
                return;
            }

            credit_updates.write_nb(regs);
            last_host_credit_regs = regs;
        }
    }

    void ikernel::new_message(ring_id_t ring, direction_t dir)
    {
#pragma HLS inline
        if (dir == HOST)
            host_credits[ring - 1].inc_msn();
    }

    std::ostream& operator<<(::std::ostream& out, const hls_ik::metadata& m)
    {
        out << "hls_ik::metadata("
            << "type=" << m.pkt_type << ", "
            << "flow_id=" << m.flow_id << ", "
            << "ikernel_id=" << m.ikernel_id << ", "
            << "ring_id=" << m.ring_id << ", "
            << "ip id=" << m.ip_identification << ", "
            << "length=" << m.length
            << ")";
        return out;
    }

    void init(hls_ik::tc_ports_data_counts counts)
    {
        for (int i = 0; i < NUM_TC; ++i)
            counts[i] = 0;
    }

    void init(hls_ik::tc_pipeline_data_counts& tc) {
        init(tc.tc_meta_counts);
        init(tc.tc_data_counts);
    }

    void init(hls_ik::tc_ikernel_data_counts& tc) {
        init(tc.host);
        init(tc.net);
    }

    void memory_unused(memory_t& m, hls::stream<bool>& dummy_update)
    {
#pragma HLS inline
        bool dummy;

        dummy_update.write_nb(false);

        if (!dummy_update.read_nb(dummy))
            dummy = false;

        hls_helpers::produce(m.ar, dummy);
        hls_helpers::produce(m.aw, dummy);
        hls_helpers::produce(m.w, dummy);
        hls_helpers::consume(m.r, dummy);
        hls_helpers::consume(m.b, dummy);
    }
}
