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

#include "ikernel_tests.hpp"
#include "cms-ikernel.hpp"
#include <vector>

#define NUM_OF_MSGS 400

typedef void (* cms_top_function)(hls_ik::ports &, hls_ik::ikernel_id &, hls_ik::virt_gateway_registers&,
	value_and_frequency& to_heap, hls::stream<value_and_frequency>& heap_out, ap_uint<32> k_value);

namespace {

    class cms_ikernel_tests : public ::testing::TestWithParam<cms_top_function> {
	protected:
	    hls_ik::ports p;
	    hls_ik::ikernel_id id;
	    hls_ik::virt_gateway_registers gateway;
	    value_and_frequency to_heap;
	    hls::stream<value_and_frequency> heap_out;
	    int top_call_count;
	    int hashes[DEPTH][2];
	    std::vector<int> vals;
	    ap_uint<32> k_value;

	    void top() {
		GetParam()(p, id, gateway, to_heap, heap_out, k_value);
		++top_call_count;
	    }

	    int read(int address) {
		return virt_gateway_wrapper([&]() { top(); }, gateway).read(address);
	    }

	    void write(int address, int data) {
		virt_gateway_wrapper([&]() { top(); }, gateway).write(address, data);
	    }

	    cms_ikernel_tests() : top_call_count(), k_value(256) {}

	    void setHashes() {
		CountMinSketch::generateHashes(hashes);
		for (int i = 0; i < DEPTH; ++i) {
		    for (int j = 0; j < 2; ++j) {
			write(HASHES_BASE + 2 * i + j, hashes[i][j]);
		    }
		}
	    }

	    void SetUp() {
		setHashes();
		for (int i = 0; i < 64; ++i) top();
	    }
    };

    TEST_P(cms_ikernel_tests, _N_Packets) {
	hls_ik::metadata m;
        hls_ik::packet_metadata pkt = m.get_packet_metadata();
        pkt.eth_dst = 1;
        pkt.eth_src = 2;
        pkt.ip_dst = 3;
        pkt.ip_src = 4;
        pkt.udp_dst = 5;
        pkt.udp_src = 6;
        m.set_packet_metadata(pkt);
	m.length = 32;

	for (int i = 0; i < NUM_OF_MSGS; ++i) {
	    p.net.metadata_input.write(m);
	    ap_uint<32> val = i + 1;
	    vals.push_back(val);
	    ap_uint<256> data = (ap_uint<14 * 8>(0), ap_uint<32>(val), ap_uint<256 - 32 - 14 * 8>(0));
	    hls_ik::axi_data d(data, 0xffffffff, true);
	    p.net.data_input.write(d);
	}

	std::vector<int> ikernel_out;
	for (int i = 0; i < 3 * NUM_OF_MSGS; ++i) {
	    top();

	    // Unlike hardware we cannot read the valid bit of the to_heap signal in order
	    // to determine whether it can be sampled. We ignore zeroes as an alternative
	    // to that approach in the cosimulation.
	    if (i % 2 == 1 && to_heap.entity != 0 && ikernel_out.size() < NUM_OF_MSGS) {
		ASSERT_TRUE(to_heap.frequency > 0);
		ikernel_out.push_back(to_heap.entity);
	    }
	}

	ASSERT_EQ(vals, ikernel_out);
    }

    INSTANTIATE_TEST_CASE_P(cms_ikernel_tests_instance, cms_ikernel_tests,
	    ::testing::Values(&cms_ikernel));

} // namespace

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
