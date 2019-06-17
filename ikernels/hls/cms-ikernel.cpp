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

#include "cms-ikernel.hpp"
#include "hls_helper.h"
#include <iostream>

using namespace hls_ik;

int cms::reg_write(int address, int value, ikernel_id_t ikernel_id)
{
    if (address == READ_TOP_K) { //read_req for topK
	_topK_read_request.write(true);
	return 0;
    }

    address -= HASHES_BASE;
    if (address < 0 || address >= 2*DEPTH) {
	return -1;
    }

    _hashes_addresses.write(address);
    _hashes_values.write(value);

    return 0;
}


int cms::reg_read(int address, int* value, ikernel_id_t ikernel_id)
{
    if (address == READ_K_VALUE) {
	*value = _k;
	return 0;
    }

    if (address != TOPK_READ_NEXT_VALUE) {
	*value = -1;
	return -1;
    }

    switch (_read_state) {
	case INITIAL:
		 if (_topK_values.empty()) return GW_BUSY;
		_next_topK_pair = _topK_values.read();
	case ENTITY:
		*value = _next_topK_pair.entity;
		_read_state = FREQUENCY;
		break;
	case FREQUENCY:
		*value = _next_topK_pair.frequency;
		_read_state = INITIAL;
		break;
    }

    return 0;
}

void cms::net_ingress(hls_ik::pipeline_ports& p) {
#pragma HLS pipeline enable_flush ii=1
    DO_PRAGMA(HLS STREAM variable=_hashes_addresses depth=2*DEPTH);
    DO_PRAGMA(HLS STREAM variable=_hashes_values depth=2*DEPTH);
    DO_PRAGMA(HLS STREAM variable=_topK_read_request depth=1);
    DO_PRAGMA(HLS STREAM variable=_topK_values depth=4)

    switch (_state) {
	case METADATA:
	    if (!p.metadata_input.empty()) {
		metadata m = p.metadata_input.read();
		_state = FIRST_WORD;
	    }

	    break;
	case FIRST_WORD:
	    if (!p.data_input.empty() && !_values_stream.full()) {

		axi_data d = p.data_input.read();
		value v = d.data(255-14*8, 256 - value::width-14*8);
		_values_stream.write(v);

		_state = d.last ? METADATA : OTHER_WORDS;
	    }

	    break;

	case OTHER_WORDS:
	    if (p.data_input.empty()) return;

	    axi_data d = p.data_input.read();
	    if (d.last) _state = METADATA;

	    break;
    }
}

void cms::write_to_heap(value_and_frequency& kv) {
#pragma HLS pipeline enable_flush ii=3
	sketch.setHashes(_hashes_addresses, _hashes_values);

	if (!_values_stream.empty()) {
		value val = _values_stream.read();
		sketch.update(val, 1);

		value_and_frequency val_and_freq;
		val_and_freq.frequency = sketch.estimate(val);
		val_and_freq.entity = val;
		kv = val_and_freq;
	}
}

void cms::read_heap(hls::stream<value_and_frequency>& heap_out, ap_uint<32> k_value) {
	if (!_reading_heap) {
		_k = k_value;
	}

	if (!_topK_read_request.empty() && !_reading_heap) {
		_topK_read_request.read();
		_reading_heap = true;
		_reading_index = 0;
	}

	if (_reading_index == k_value) {
		_reading_heap = false;
		return;
	}

	if (_reading_heap && !_topK_values.full() && !heap_out.empty()) {
		_topK_values.write(heap_out.read());
		++_reading_index;
	}
}

void cms::step(hls_ik::ports& p, hls_ik::tc_ikernel_data_counts& tc)
{
#pragma HLS inline
    pass_packets(p.host);
    net_ingress(p.net);
    hls_helpers::produce(p.net.data_output);
    hls_helpers::produce(p.net.metadata_output);
}

static cms cms_inst;
void cms_ikernel(hls_ik::ports& ik, hls_ik::ikernel_id& uuid,
	hls_ik::virt_gateway_registers& gateway, value_and_frequency& to_heap,
	hls::stream<value_and_frequency>& heap_out, ap_uint<32> k_value,
        hls_ik::tc_ikernel_data_counts& tc)
{
	DO_PRAGMA(HLS dataflow)
	DO_PRAGMA(HLS ARRAY_RESHAPE variable=uuid.uuid complete dim=1)
	IKERNEL_PORTS_PRAGMAS(ik)
	DO_PRAGMA_SYN(HLS interface s_axilite port=uuid offset=0x1000)
	VIRT_GATEWAY_OFFSET(gateway, 0x1014, 0x101c, 0x102c, 0x1034)
	DO_PRAGMA_SYN(HLS interface ap_ctrl_none port=return)
	DO_PRAGMA(HLS interface ap_ovld port=to_heap)
	DO_PRAGMA(HLS data_pack variable=to_heap)
	DO_PRAGMA(HLS interface axis port=heap_out)
	DO_PRAGMA(HLS data_pack variable=heap_out)
	DO_PRAGMA(HLS interface ap_none port=k_value)

	using namespace hls_ik;
    constexpr ikernel_id __constant_uuid = { CMS_UUID };
    cms_inst.step(ik, tc);
    cms_inst.write_to_heap(to_heap);
    cms_inst.read_heap(heap_out, k_value);
    cms_inst.gateway(&cms_inst, gateway);
output_uuid: uuid = __constant_uuid;
}

