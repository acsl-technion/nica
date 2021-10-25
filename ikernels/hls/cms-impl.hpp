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

#pragma once

#include "cms-ikernel.hpp"
#include "cms.hpp"
#include <ikernel.hpp>
#include <gateway.hpp>

typedef ap_uint<32> value;

struct value_and_frequency {
    value entity;
    value frequency;
};

void cms_ikernel(hls_ik::ports& ik, hls_ik::ikernel_id& uuid,
                 hls_ik::virt_gateway_registers& gateway,
                 value_and_frequency& to_heap,
                 hls::stream<value_and_frequency>& heap_out,
                 ap_uint<32> k_value,
                 hls_ik::tc_ikernel_data_counts& tc);

class cms : public hls_ik::ikernel {
public:

    void step(hls_ik::ports& ports, hls_ik::tc_ikernel_data_counts& tc);
    int reg_write(int address, int value, hls_ik::ikernel_id_t ikernel_id);
    int reg_read(int address, int* value, hls_ik::ikernel_id_t ikernel_id);

    void write_to_heap(value_and_frequency& kv);
    void read_heap(hls::stream<value_and_frequency>& heap_out, ap_uint<32> k_value);

protected:
    enum { METADATA, FIRST_WORD, OTHER_WORDS } _state;
    enum { INITIAL, ENTITY, FREQUENCY } _read_state;

    hls::stream<value> _values_stream;
    hls::stream<int> _hashes_addresses;
    hls::stream<int> _hashes_values;
    hls::stream<bool> _topK_read_request;
    hls::stream<value_and_frequency> _topK_values;

    bool _reading_heap;
    unsigned  _reading_index, _k;
    value_and_frequency _next_topK_pair;

    CountMinSketch sketch;

    void net_ingress(hls_ik::pipeline_ports&);
};

