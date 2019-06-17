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

#include "echo-impl.hpp"
#include "ikernel_tests.hpp"
#include "tb.h"
#include "nica-top.hpp"

namespace {
    using namespace hls_ik;

    class echo_test : public ikernel_test, public udp_tb::testbench {
    protected:
        nica_config c;
        nica_stats s;
        tc_ports h2n_tc, n2h_tc;

        echo_test() : ikernel_test(15), udp_tb::testbench(), c(), s()
        {}

        void top()
        {
            ::nica(nwp2sbu, sbu2nwp, cxp2sbu, sbu2cxp,
                   &c, &s, events,
                   BOOST_PP_ENUM_PARAMS(NUM_IKERNELS, ports),
                   h2n_tc, h2n_tc, n2h_tc, n2h_tc);
            echo_top(p, id, gateway, tc);
        }

        void test_passthrough(pipeline_ports& in, pipeline_ports& out) {
            hls_ik::metadata m;
            hls_ik::axi_data d;

            ikernel_test::test_passthrough(in, out, m, d);

            hls_ik::metadata metadata_output = out.metadata_output.read();
            hls_ik::axi_data data_output = out.data_output.read();

            EXPECT_EQ(d, data_output);
            const packet_metadata& pkt = m.get_packet_metadata();
            const packet_metadata& out_pkt = metadata_output.get_packet_metadata();
            EXPECT_EQ(pkt.eth_src, out_pkt.eth_dst);
            EXPECT_EQ(pkt.eth_dst, out_pkt.eth_src);
            EXPECT_EQ(pkt.ip_src, out_pkt.ip_dst);
            EXPECT_EQ(pkt.ip_dst, out_pkt.ip_src);
            EXPECT_EQ(pkt.udp_src, out_pkt.udp_dst);
            EXPECT_EQ(pkt.udp_dst, out_pkt.udp_src);
        }
    };

    // Test that the top function returns the correct UUID
    TEST_P(echo_test, correct_uuid) {
        ikernel_id expected = {ECHO_UUID};
        ikernel_test::top();
        EXPECT_EQ(id, expected);
    }
    
    TEST_P(echo_test, net_to_net) {
        test_passthrough(p.net, p.host);
    }

    TEST_P(echo_test, net_to_net_pcap) {
        const char *input_filename = "udp_rr.pcap";

        FILE* temp_file = tmpfile();
        ASSERT_TRUE(temp_file) << "cannot create temporary file for output.";

        memset(&c, 0, sizeof(c));
        flow_table_wrapper ft_gateway([&]() { top(); }, c.n2h.common.flow_table_gateway);
        ft_gateway.set_fields(FT_FIELD_DST_PORT);
        EXPECT_TRUE(ft_gateway.add_flow(49105, FT_IKERNEL));
        c.n2h.common.enable = true;

        udp_tb::pkt_id_verifier n2h_verifier;
        hls::stream<mlx::user_t> user_values;
        ASSERT_GE(read_pcap(input_filename, nwp2sbu, 0, std::numeric_limits<int>::max(),
                            &n2h_verifier, &user_values), 0);

        run();
        write_pcap(temp_file, sbu2nwp, false);
        EXPECT_TRUE(compare_output(filename(temp_file), "", "udp_rr-reverse.pcap", "udp src port 49105"));

        temp_file = tmpfile();
        ASSERT_TRUE(temp_file) << "cannot create temporary file for output.";
        write_pcap(temp_file, sbu2cxp, false, &n2h_verifier, &user_values);
        EXPECT_TRUE(compare_output(filename(temp_file), "", "udp_rr-to-host.pcap", "! udp dst port 49105")); // empty
    }

    TEST_P(echo_test, sockperf) {
        FILE* temp_file = tmpfile();
        ASSERT_TRUE(temp_file) << "cannot create temporary file for output.";

        memset(&c, 0, sizeof(c));
        flow_table_wrapper ft_gateway([&]() { top(); }, c.n2h.common.flow_table_gateway);
        ft_gateway.set_fields(FT_FIELD_DST_PORT);
        EXPECT_TRUE(ft_gateway.add_flow(11111, FT_IKERNEL));
        c.n2h.common.enable = true;

        write(ECHO_RESPOND_TO_SOCKPERF, 1);

        udp_tb::pkt_id_verifier n2h_verifier;
        hls::stream<mlx::user_t> user_values;
        ASSERT_GE(read_pcap("ping.pcap", nwp2sbu, 0, std::numeric_limits<int>::max(),
                            &n2h_verifier, &user_values), 0);

        run();
        write_pcap(temp_file, sbu2nwp, false);
        EXPECT_TRUE(compare_output(filename(temp_file), "", "pong-padded.pcap", ""));

        temp_file = tmpfile();
        ASSERT_TRUE(temp_file) << "cannot create temporary file for output.";
        write_pcap(temp_file, sbu2cxp, false, &n2h_verifier, &user_values);
        EXPECT_TRUE(compare_output(filename(temp_file), "", "pong-padded.pcap", "! udp src port 11111")); // empty
    }

    INSTANTIATE_TEST_CASE_P(echo_test_instance, echo_test,
            ::testing::Values(&echo_top));
} // namespace

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

