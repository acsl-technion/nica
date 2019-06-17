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

#ifndef PASSTHROUGH_IMPL_HPP
#define PASSTHROUGH_IMPL_HPP

#include <ikernel.hpp>
#include <gateway.hpp>
#include <flow_table.hpp>
#include <context_manager.hpp>

#define LOG_NUM_PASSTHROUGH_CONTEXTS 6
#define NUM_PASSTHROUGH_CONTEXTS (1 << LOG_NUM_PASSTHROUGH_CONTEXTS)

#include "passthrough.hpp"

DECLARE_TOP_FUNCTION(passthrough_top);

struct passthrough_context {
    passthrough_context() : ring_id(0), ignore_credits(false) {}

    hls_ik::ring_id_t ring_id;
    bool ignore_credits;
};

class passthrough_contexts : public context_manager<passthrough_context, LOG_NUM_PASSTHROUGH_CONTEXTS>
{
public:
    int rpc(int address, int *value, hls_ik::ikernel_id_t ikernel_id, bool read);
};

class passthrough : public hls_ik::ikernel, public hls_ik::virt_gateway_impl<passthrough>
{
public:
    virtual int reg_write(int address, int value, hls_ik::ikernel_id_t ikernel_id);
    virtual int reg_read(int address, int* value, hls_ik::ikernel_id_t ikernel_id);
    virtual void step(hls_ik::ports& p, hls_ik::tc_ikernel_data_counts& tc);
protected:
    enum { DECISION, STREAM } _state;

    hls::stream<bool> _decisions;
    bool _action;
    void intercept_in(hls_ik::pipeline_ports& p, hls_ik::tc_pipeline_data_counts& tc);
    void drop_or_pass(hls_ik::pipeline_ports& p);
    passthrough_contexts contexts;
};

#endif // PASSTHROUGH_IMPL_HPP
