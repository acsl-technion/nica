/* * Copyright (c) 2016-2017 Haggai Eran, Gabi Malka, Lior Zeno, Maroun Tork
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

struct arbiter_per_port_stats {
    arbiter_per_port_stats() :
        not_empty(),
        no_tokens(),
        cur_tokens()
    {}

    ap_uint<64> not_empty;
    ap_uint<64> no_tokens;
    int cur_tokens;
};

struct arbiter_tx_per_port_stats {
    arbiter_tx_per_port_stats() : words(), packets()
    {}

    ap_uint<64> words;
    ap_uint<64> packets;
};

template <unsigned num_ports>
struct arbiter_stats {
    arbiter_stats() : out_full() {}

    arbiter_per_port_stats port[num_ports];
    arbiter_tx_per_port_stats tx_port[num_ports];
    ap_uint<64> out_full;
};

/* Each port has the two SCHED_DRR_* registers at offsets starting from
 * ARBITER_SCHEDULER and with stride ARBITER_SCHEDULER_STRIDE */
#define ARBITER_NUM_TC 0x0
#define ARBITER_SCHEDULER 0x10
#define ARBITER_SCHEDULER_STRIDE 0x2
