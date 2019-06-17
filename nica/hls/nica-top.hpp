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

#ifndef NICA_TOP_HPP
#define NICA_TOP_HPP

#include "ikernel.hpp"
#include "udp.h"
#include "tc-ports.hpp"
#include "arbiter.hpp"

#include <boost/preprocessor/iteration/local.hpp>

struct nica_ikernel_stats {
    ap_uint<32> packets;
};

struct nica_pipeline_stats {
    udp::udp_stats udp;
    arbiter_stats<NUM_TC> arbiter;
#define BOOST_PP_LOCAL_MACRO(i) \
    nica_ikernel_stats ik ## i;
#define BOOST_PP_LOCAL_LIMITS (0, NUM_IKERNELS - 1)
%:include BOOST_PP_LOCAL_ITERATE()
};

struct nica_stats {
    nica_pipeline_stats n2h, h2n;
    int flow_table_size;
};

struct nica_config {
    udp::config_n2h n2h;
    udp::config_h2n h2n;
};

using hls_ik::trace_event;
#define NUM_TRACE_EVENTS 8
enum {
    TRACE_ARBITER_PORT_0 = 0,
    TRACE_ARBITER_PORT_1 = 1,
    TRACE_ARBITER_PORT_2 = 2,
    TRACE_ARBITER_EVICTED = 3,

    TRACE_N2H = 0,
    TRACE_H2N = 4,
};

void nica(mlx::stream& nwp2sbu, mlx::stream& sbu2nwp, mlx::stream& cxp2sbu,
          mlx::stream& sbu2cxp,
          nica_config* cfg, nica_stats* stats,
          trace_event events[NUM_TRACE_EVENTS],
          DECL_IKERNEL_PARAMS(),
          tc_ports& h2n_tc_out, tc_ports& h2n_tc_in,
          tc_ports& n2h_tc_out, tc_ports& n2h_tc_in
    );

#endif
