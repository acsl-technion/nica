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
#include "passthrough-impl.hpp"

using namespace hls_ik;

namespace {

    class passthrough_test : public ikernel_test
    {
    public:
        passthrough_test() : ikernel_test(15)
        {}
    };

    INSTANTIATE_TEST_CASE_P(ikernel_test_instance, all_ikernels_test,
            ::testing::Values(&passthrough_top));

    TEST_P(passthrough_test, custom_ring) {
        metadata m;
        packet_metadata pkt;
        pkt.eth_dst = 1;
        pkt.eth_src = 2;
        pkt.ip_dst = 3;
        pkt.ip_src = 4;
        pkt.udp_dst = 5;
        pkt.udp_src = 6;
        m.set_packet_metadata(pkt);
        m.length = 32;
        m.ikernel_id = 1;
        m.flow_id = 2;

        write(PASSTHROUGH_RING_ID, 1, m.ikernel_id);

        const int total = 100;
        update_credits(1, total);
	std::vector<axi_data> vec;

        for (int i = 0; i < total; ++i) {
            p.net.metadata_input.write(m);
            ap_uint<32> rand_val = rand();//(rand()%100);
            //   cout << "[" << i << "] : " << rand_val << "\n";
            ap_uint<256> data = (ap_uint<14*8>(0),ap_uint<32>(rand_val), ap_uint<256 - 32 - 14*8>(0));
            axi_data d(data, 0xffffffff, true);
            vec.push_back(d);
            p.net.data_input.write(d);

            for (int i = 0; i < 10; ++i)
                top();
        }

        for (int i = 0; i < total; ++i) {
            m = p.net.metadata_output.read();
            EXPECT_EQ(m.ring_id, 1) << i;
            EXPECT_EQ(m.get_custom_ring_metadata().end_of_message, true) << i;
            EXPECT_EQ(m.ikernel_id, 1) << i;
            EXPECT_EQ(m.flow_id, 2) << i;
            EXPECT_EQ(vec[i], axi_data(p.net.data_output.read())) << i;
        }
    }

    TEST_P(passthrough_test, push_header) {
        metadata m;
        packet_metadata pkt;
        pkt.eth_dst = 1;
        pkt.eth_src = 2;
        pkt.ip_dst = 3;
        pkt.ip_src = 4;
        pkt.udp_dst = 5;
        pkt.udp_src = 6;
        m.set_packet_metadata(pkt);
        m.length = 32;
        m.ikernel_id = 1;
        m.flow_id = 2;

        write(PASSTHROUGH_RING_ID, 1, m.ikernel_id);
        write(PASSTHROUGH_ADD_IP_PORT, 1, m.ikernel_id);

        const int total = 100;
        update_credits(1, total);
	std::vector<axi_data> vec;

        for (int i = 0; i < total; ++i) {
            p.net.metadata_input.write(m);
            ap_uint<32> rand_val = rand();//(rand()%100);
            //   cout << "[" << i << "] : " << rand_val << "\n";
            ap_uint<256> data = (ap_uint<14*8>(0),ap_uint<32>(rand_val), ap_uint<256 - 32 - 14*8>(0));
            axi_data d(data, 0xffffffff, true);
            vec.push_back(d);
            p.net.data_input.write(d);

            for (int i = 0; i < 10; ++i)
                top();
        }

        metadata orig_metadata = m;

        for (int i = 0; i < total; ++i) {
            m = p.net.metadata_output.read();
            EXPECT_EQ(m.ring_id, 1) << i;
            EXPECT_EQ(m.get_custom_ring_metadata().end_of_message, true) << i;
            EXPECT_EQ(m.ikernel_id, 1) << i;
            EXPECT_EQ(m.flow_id, 2) << i;
            EXPECT_EQ(m.length, 32 + 18) << i;
            axi_data data = p.net.data_output.read();
            EXPECT_FALSE(data.last);
            EXPECT_EQ(data.keep, 0xffffffff);
            EXPECT_EQ(data.data(256 - 1, 256 - 32),
                      orig_metadata.get_packet_metadata().ip_src);
            EXPECT_EQ(data.data(256 - 33, 256 - 48),
                      orig_metadata.get_packet_metadata().udp_src);
            EXPECT_EQ(ap_uint<14*8>(data.data(256 - 18 * 8 - 1, 0)),
                      ap_uint<14*8>(vec[i].data(256 - 1, 256 - 14 * 8)));
            data = p.net.data_output.read();
            EXPECT_TRUE(data.last);
            EXPECT_EQ(data.keep, ~((1U << 14) - 1));
            EXPECT_EQ(ap_uint<32>(vec[i].data(256 - 14*8 - 1, 256 - 14*8 - 32)),
                      ap_uint<32>(data.data(256 - 1, 256 - 32))) << i;
        }
    }

    INSTANTIATE_TEST_CASE_P(passthrough_test_instance, passthrough_test,
            ::testing::Values(&passthrough_top));

} // namespace

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}


