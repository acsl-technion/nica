/* Copyright (C) 2017 Haggai Eran

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>. */

#include "ikernel_tests.hpp"
#include "tc-ports.hpp"
#include "memcached-ik.hpp"
#include "tb.h"

using namespace hls_ik;

namespace {

    class memcached_test : public ikernel_test, public udp_tb::testbench {
    protected:
        nica_config c;
        nica_stats s;
        tc_ports h2n_tc, n2h_tc;

        memcached_test() : ikernel_test(15) {}

        void top()
        {
            ::nica(nwp2sbu, sbu2nwp, cxp2sbu, sbu2cxp,
                   &c, &s, events,
                   BOOST_PP_ENUM_PARAMS(NUM_IKERNELS, ports),
                   h2n_tc, h2n_tc, n2h_tc, n2h_tc);
            memcached_top(p, id, gateway, tc);
            mem.mem(p.mem);
        }

        void SetUp() {
            write(MEMCACHED_STATS_GET_REQUESTS, 0);
            write(MEMCACHED_STATS_GET_REQUESTS_HITS, 0);
            write(MEMCACHED_STATS_GET_RESPONSE, 0);
            write(MEMCACHED_STATS_DROPPED_TC_BACKPRESSURE, 0);
        }
    };

    TEST_P(memcached_test, mem_size_register) {
        size_t size = 0x51a5f % (1 << (DDR_INTERFACE_WIDTH - 6));
        size_t orig = read(MEMCACHED_REG_CACHE_SIZE);
        write(MEMCACHED_REG_CACHE_SIZE, size);
        write(MEMCACHED_REG_CACHE_SIZE, size + 1, 1);
        EXPECT_EQ(read(MEMCACHED_REG_CACHE_SIZE), size);
        EXPECT_EQ(read(MEMCACHED_REG_CACHE_SIZE, 1), size + 1);
        write(MEMCACHED_REG_CACHE_SIZE, orig);
        write(MEMCACHED_REG_CACHE_SIZE, orig , 1);
    }

    TEST_P(memcached_test, pcap_10_10) {
        FILE* nwp_output = tmpfile();
        ASSERT_TRUE(nwp_output) << "cannot create temporary file for output.";
        FILE* cxp_output = tmpfile();
        ASSERT_TRUE(cxp_output) << "cannot create temporary file for output.";

        memset(&c, 0, sizeof(c));
        flow_table_wrapper n2h_ft_gateway([&]() { top(); }, c.n2h.common.flow_table_gateway);
        flow_table_wrapper h2n_ft_gateway([&]() { top(); }, c.h2n.common.flow_table_gateway);
        EXPECT_TRUE(n2h_ft_gateway.add_flow(0, FT_IKERNEL));
        EXPECT_TRUE(h2n_ft_gateway.add_flow(0, FT_IKERNEL));
        c.n2h.common.enable = true;
        c.h2n.common.enable = true;

        if (MEMCACHED_KEY_SIZE == 10 && MEMCACHED_VALUE_SIZE == 10)
            ASSERT_GE(read_pcap("memcached-responses.pcap", cxp2sbu), 0);
        run();
        if (MEMCACHED_KEY_SIZE == 10 && MEMCACHED_VALUE_SIZE == 10)
            ASSERT_GE(read_pcap("memcached-requests.pcap", nwp2sbu), 0);
        run();

        write_pcap(nwp_output, sbu2nwp, false);
        write_pcap(cxp_output, sbu2cxp, false);

        if (MEMCACHED_KEY_SIZE == 10 && MEMCACHED_VALUE_SIZE == 10)
            EXPECT_TRUE(compare_output(filename(nwp_output), "",
                                       "memcached-all-responses.pcap", ""));
        // cxp_output should be empty
        if (MEMCACHED_KEY_SIZE == 10 && MEMCACHED_VALUE_SIZE == 10)
            EXPECT_TRUE(compare_output(filename(cxp_output), "",
                                       "memcached-requests.pcap", "sctp"));

        if (MEMCACHED_KEY_SIZE == 10 && MEMCACHED_VALUE_SIZE == 10) {
            EXPECT_EQ(read(MEMCACHED_STATS_GET_REQUESTS), 90);
            EXPECT_EQ(read(MEMCACHED_STATS_GET_REQUESTS_HITS), 90);
            EXPECT_EQ(read(MEMCACHED_STATS_GET_RESPONSE), 1);
        }

        EXPECT_EQ(read(MEMCACHED_STATS_GET_REQUESTS_MISSES), 0);
        EXPECT_EQ(read(MEMCACHED_STATS_SET_REQUESTS), 0);
        EXPECT_EQ(read(MEMCACHED_STATS_N2H_UNKNOWN), 0);
        EXPECT_EQ(read(MEMCACHED_STATS_H2N_UNKNOWN), 0);
    }

    TEST_P(memcached_test, pcap_16_16) {
        FILE* nwp_output = tmpfile();
        ASSERT_TRUE(nwp_output) << "cannot create temporary file for output.";
        FILE* cxp_output = tmpfile();
        ASSERT_TRUE(cxp_output) << "cannot create temporary file for output.";

        memset(&c, 0, sizeof(c));
        flow_table_wrapper n2h_ft_gateway([&]() { top(); }, c.n2h.common.flow_table_gateway);
        flow_table_wrapper h2n_ft_gateway([&]() { top(); }, c.h2n.common.flow_table_gateway);
        EXPECT_TRUE(n2h_ft_gateway.add_flow(0, FT_IKERNEL));
        EXPECT_TRUE(h2n_ft_gateway.add_flow(0, FT_IKERNEL));
        c.n2h.common.enable = true;
        c.h2n.common.enable = true;

        if (MEMCACHED_KEY_SIZE == 16 && MEMCACHED_VALUE_SIZE == 16)
            ASSERT_GE(read_pcap("memcached-responses-16.pcap", cxp2sbu), 0);
        run();
        if (MEMCACHED_KEY_SIZE == 16 && MEMCACHED_VALUE_SIZE == 16)
            ASSERT_GE(read_pcap("memcached-requests-16.pcap", nwp2sbu), 0);
        run();

        write_pcap(nwp_output, sbu2nwp, false);
        write_pcap(cxp_output, sbu2cxp, false);

        if (MEMCACHED_KEY_SIZE == 16 && MEMCACHED_VALUE_SIZE == 16)
            EXPECT_TRUE(compare_output(filename(nwp_output), "",
                                       "memcached-all-responses-16.pcap", ""));
        // cxp_output should be empty
        if (MEMCACHED_KEY_SIZE == 16 && MEMCACHED_VALUE_SIZE == 16)
            EXPECT_TRUE(compare_output(filename(cxp_output), "",
                                       "memcached-requests-16.pcap", "sctp"));

        if (MEMCACHED_KEY_SIZE == 16 && MEMCACHED_VALUE_SIZE == 16) {
            EXPECT_EQ(read(MEMCACHED_STATS_GET_REQUESTS), 90);
            EXPECT_EQ(read(MEMCACHED_STATS_GET_REQUESTS_HITS), 90);
            EXPECT_EQ(read(MEMCACHED_STATS_GET_RESPONSE), 1);
        }

        EXPECT_EQ(read(MEMCACHED_STATS_GET_REQUESTS_MISSES), 0);
        EXPECT_EQ(read(MEMCACHED_STATS_SET_REQUESTS), 0);
        EXPECT_EQ(read(MEMCACHED_STATS_N2H_UNKNOWN), 0);
        EXPECT_EQ(read(MEMCACHED_STATS_H2N_UNKNOWN), 0);
    }

    TEST_P(memcached_test, tc_backpressure) {
        FILE* nwp_output = tmpfile();
        ASSERT_TRUE(nwp_output) << "cannot create temporary file for output.";
        FILE* cxp_output = tmpfile();
        ASSERT_TRUE(cxp_output) << "cannot create temporary file for output.";

        int prev_meta = tc.host.tc_meta_counts[0];
        int prev_data = tc.host.tc_data_counts[0];
        tc.host.tc_meta_counts[0] = 300;
        tc.host.tc_data_counts[0] = 300;

        memset(&c, 0, sizeof(c));
        flow_table_wrapper n2h_ft_gateway([&]() { top(); }, c.n2h.common.flow_table_gateway);
        flow_table_wrapper h2n_ft_gateway([&]() { top(); }, c.h2n.common.flow_table_gateway);
        EXPECT_TRUE(n2h_ft_gateway.add_flow(0, FT_IKERNEL));
        EXPECT_TRUE(h2n_ft_gateway.add_flow(0, FT_IKERNEL));
        c.n2h.common.enable = true;
        c.h2n.common.enable = true;

        if (MEMCACHED_KEY_SIZE == 10 && MEMCACHED_VALUE_SIZE == 10)
            ASSERT_GE(read_pcap("memcached-responses.pcap", cxp2sbu), 0);
        run();
        if (MEMCACHED_KEY_SIZE == 10 && MEMCACHED_VALUE_SIZE == 10)
            ASSERT_GE(read_pcap("memcached-requests.pcap", nwp2sbu), 0);
        run();

        write_pcap(nwp_output, sbu2nwp, false);
        write_pcap(cxp_output, sbu2cxp, false);

        // nwp_output should be empty
        if (MEMCACHED_KEY_SIZE == 10 && MEMCACHED_VALUE_SIZE == 10)
            EXPECT_TRUE(compare_output(filename(nwp_output), "",
                                       "memcached-responses.pcap", "sctp"));
        // cxp_output should be empty
        if (MEMCACHED_KEY_SIZE == 10 && MEMCACHED_VALUE_SIZE == 10)
            EXPECT_TRUE(compare_output(filename(cxp_output), "",
                                       "memcached-requests.pcap", "sctp"));

        if (MEMCACHED_KEY_SIZE == 10 && MEMCACHED_VALUE_SIZE == 10) {
            EXPECT_EQ(read(MEMCACHED_STATS_GET_REQUESTS), 90);
            EXPECT_EQ(read(MEMCACHED_STATS_GET_REQUESTS_HITS), 0);
            EXPECT_EQ(read(MEMCACHED_STATS_GET_RESPONSE), 1);
            EXPECT_EQ(read(MEMCACHED_STATS_DROPPED_TC_BACKPRESSURE), 90);
        }

        EXPECT_EQ(read(MEMCACHED_STATS_GET_REQUESTS_MISSES), 0);
        EXPECT_EQ(read(MEMCACHED_STATS_SET_REQUESTS), 0);
        EXPECT_EQ(read(MEMCACHED_STATS_N2H_UNKNOWN), 0);
        EXPECT_EQ(read(MEMCACHED_STATS_H2N_UNKNOWN), 0);

        tc.host.tc_meta_counts[0] = prev_meta;
        tc.host.tc_data_counts[0] = prev_data;
    }

    TEST_P(memcached_test, pcap_set) {
        FILE* nwp_output = tmpfile();
        ASSERT_TRUE(nwp_output) << "cannot create temporary file for output.";
        FILE* cxp_output = tmpfile();
        ASSERT_TRUE(cxp_output) << "cannot create temporary file for output.";

        memset(&c, 0, sizeof(c));
        flow_table_wrapper n2h_ft_gateway([&]() { top(); }, c.n2h.common.flow_table_gateway);
        flow_table_wrapper h2n_ft_gateway([&]() { top(); }, c.h2n.common.flow_table_gateway);
        EXPECT_TRUE(n2h_ft_gateway.add_flow(0, FT_IKERNEL));
        EXPECT_TRUE(h2n_ft_gateway.add_flow(0, FT_IKERNEL));
        c.n2h.common.enable = true;
        c.h2n.common.enable = true;

        if (MEMCACHED_KEY_SIZE == 16 && MEMCACHED_VALUE_SIZE == 16)
            ASSERT_GE(read_pcap("memcached-set-16.pcap", nwp2sbu), 0);
        run();

        write_pcap(nwp_output, sbu2nwp, false);
        write_pcap(cxp_output, sbu2cxp, false);

        // nwp output should be empty
        if (MEMCACHED_KEY_SIZE == 16 && MEMCACHED_VALUE_SIZE == 16)
            EXPECT_TRUE(compare_output(filename(nwp_output), "",
                                       "memcached-set-16-padded.pcap", "sctp"));
        if (MEMCACHED_KEY_SIZE == 16 && MEMCACHED_VALUE_SIZE == 16)
            EXPECT_TRUE(compare_output(filename(cxp_output), "",
                                       "memcached-set-16-padded.pcap", ""));

        if (MEMCACHED_KEY_SIZE == 16 && MEMCACHED_VALUE_SIZE == 16) {
            EXPECT_EQ(read(MEMCACHED_STATS_SET_REQUESTS), 1);
        }
    }

    INSTANTIATE_TEST_CASE_P(memcached_test_instance, memcached_test,
            ::testing::Values(&memcached_top));

} // namespace

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
