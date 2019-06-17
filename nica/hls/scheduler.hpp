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

#pragma once

#include <context_manager.hpp>
#include <cstdint>

/* Deficit round robin quantum */
#define SCHED_DRR_QUANTUM 0
/* Deficit round robin deficit */
#define SCHED_DRR_DEFICIT 1

struct scheduler_flow {
    scheduler_flow() : quantum(ALIGN(1500, 32) / 32), deficit(0) {}

    uint32_t quantum;
    uint32_t deficit;
};

template <unsigned int log_size>
class scheduler : public context_manager<scheduler_flow, log_size>
{
public:
    typedef context_manager<scheduler_flow, log_size> base_t;
    typedef typename base_t::index_t index_t;

    scheduler() :
        active_flows("active_flows"),
        active_flows_bitmap(0)
    {
#if defined(__SYNTHESIS__)
        static const int active_flows_depth=1 << log_size;
#endif
#pragma HLS stream variable=active_flows depth=active_flows_depth
    }

    bool update()
    {
#pragma HLS inline
        if (context_manager<scheduler_flow, log_size>::update())
            return true;

        if (!flow_updates.empty()) {
            auto cmd = flow_updates.read();

            auto& flow = (*this)[cmd.ik];
            flow.deficit = cmd.not_empty ? cmd.remaining_quota : 0;
            return true;
        }

        return false;
    }

    int rpc(int address, int *value, index_t flow_id, bool read)
    {
#pragma HLS inline
        uint32_t index = flow_id;

        switch (address) {
        case SCHED_DRR_QUANTUM:
            return this->template gateway_access_field<uint32_t, &scheduler_flow::quantum>(index, value, read);
        case SCHED_DRR_DEFICIT:
            return this->template gateway_access_field<uint32_t, &scheduler_flow::deficit>(index, value, read);
        default:
            *value = -1;
            return GW_FAIL;
        }

        return GW_DONE;
    }

    void schedule(index_t flow_id)
    {
        if (!is_active(flow_id)) {
            active_flows_bitmap(flow_id, flow_id) = 1;
            active_flows.write(flow_id);
        }
    }

    void update_flow(index_t flow_id, bool not_empty, uint32_t remaining_quota)
    {
        flow_updates.write(flow_update_cmd{flow_id, not_empty, remaining_quota});
    }

    bool next_flow(index_t* flow, uint32_t* quota)
    {
#pragma HLS inline
        index_t next;

        if (!active_flows.empty()) {
            *flow = active_flows.read();
            active_flows_bitmap(*flow, *flow) = 0;
            auto& flow_context = (*this)[*flow];
            *quota = flow_context.deficit + flow_context.quantum;
            return true;
        }

        return false;
    }


private:
    bool is_active(index_t flow_id)
    {
#pragma HLS inline
        const int mask = (1 << LOG_NUM_IKERNELS) - 1;
        return active_flows_bitmap(flow_id & mask, flow_id & mask);
    }


    hls::stream<index_t> active_flows;
    ap_uint<1 << log_size> active_flows_bitmap;

    struct flow_update_cmd {
        index_t ik;
        bool not_empty;
        uint32_t remaining_quota;
    };
    hls::stream<flow_update_cmd> flow_updates;
};

