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

#include <ap_int.h>

#include "flow_table.hpp"

#define CUSTOM_RINGS_LOG_NUM 6
#define LOG_NUM_IKERNELS 6
#define LOG_NUM_VMS 6
#define LOG_NUM_ENGINES 1
#ifndef NUM_TC
#  define NUM_TC 8
#endif

namespace hls_ik {
    /* One more bit for the UDP traffic */
    typedef ap_uint<CUSTOM_RINGS_LOG_NUM + 1> ring_id_t;
    typedef ap_uint<LOG_NUM_IKERNELS> ikernel_id_t;
    typedef ap_uint<LOG_NUM_ENGINES> engine_id_t;
    typedef ap_uint<LOG_NUM_VMS> vm_id_t;
    /* One more bit for the miss flows */
    typedef ap_uint<FLOW_TABLE_LOG_SIZE + 1> flow_id_t;

    typedef ap_uint<1> direction_t;
    #define HOST (0)
    #define NET (1)

    typedef ap_uint<11> pkt_len_t;
}
