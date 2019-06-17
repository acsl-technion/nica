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

#include "gtest/gtest.h"
#include "tb.h"
#include "udp.h"
#include "nica-top.hpp"
#include "nica-impl.hpp"
#include "passthrough-impl.hpp"
#include "ikernel_tests.hpp"
#include "threshold.hpp"
#include "threshold-impl.hpp"
#include "pktgen.hpp"
#include "custom_rx_ring.hpp"

#include <uuid/uuid.h>

using std::string;
using std::cout;
using std::endl;

using udp::hds_stats;

hds_stats& operator -= (hds_stats& s1, const hds_stats& s2)
{
    s1.passthrough_disabled -= s2.passthrough_disabled;
    s1.passthrough_not_ipv4 -= s2.passthrough_not_ipv4;
    s1.passthrough_bad_length -= s2.passthrough_bad_length;
    s1.passthrough_not_udp -= s2.passthrough_not_udp;
    s1.ft_action_passthrough -= s2.ft_action_passthrough;
    s1.ft_action_drop -= s2.ft_action_drop;
    s1.ft_action_ikernel -= s2.ft_action_ikernel;

    return s1;
}

nica_ikernel_stats& operator -= (nica_ikernel_stats& s1, const nica_ikernel_stats& s2)
{
    s1.packets -= s2.packets;

    return s1;
}

arbiter_per_port_stats& operator -= (arbiter_per_port_stats& s1, const arbiter_per_port_stats& s2)
{
    s1.not_empty -= s2.not_empty;
    s1.no_tokens -= s2.no_tokens;
    s1.cur_tokens -= s2.cur_tokens;
 
    return s1;
}

arbiter_tx_per_port_stats& operator -= (arbiter_tx_per_port_stats& s1, const arbiter_tx_per_port_stats& s2)
{
    s1.words -= s2.words;
    s1.packets -= s2.packets;
 
    return s1;
}
template <unsigned num_ports>
arbiter_stats<num_ports>& operator -= (arbiter_stats<num_ports>& s1, const arbiter_stats<num_ports>& s2)
{
    for (unsigned i = 0; i < num_ports; ++i) {
        s1.port[i] -= s2.port[i];
        s1.tx_port[i] -= s2.tx_port[i];
    }

    s1.out_full -= s2.out_full;

    return s1;
}

nica_pipeline_stats& operator -= (nica_pipeline_stats& s1, const nica_pipeline_stats& s2)
{
    s1.udp.hds -= s2.udp.hds;
    s1.arbiter -= s2.arbiter;
#define BOOST_PP_LOCAL_MACRO(i) \
    s1.ik ## i -= s2.ik ## i;
#define BOOST_PP_LOCAL_LIMITS (0, NUM_IKERNELS - 1)
%:include BOOST_PP_LOCAL_ITERATE()

    return s1;
}

nica_stats& operator -= (nica_stats& s1, const nica_stats& s2)
{
    s1.n2h -= s2.n2h;
    s1.h2n -= s2.h2n;


    return s1;
}

nica_stats operator -(const nica_stats& s1, const nica_stats& s2)
{
    auto s = s1;
    s -= s2;
    return s;
}

class testbench : public udp_tb::testbench, public ::testing::Test
{
public:
    testbench() : udp_tb::testbench()
#define BOOST_PP_LOCAL_MACRO(n) \
        , ikernel ## n()
#define BOOST_PP_LOCAL_LIMITS (0, NUM_IKERNELS - 1)
%:include BOOST_PP_LOCAL_ITERATE()
    {
        memset(&c, 0, sizeof(c));
        memset(&s, 0, sizeof(s));

#define BOOST_PP_LOCAL_MACRO(n) \
        init(tc ## n);
#define BOOST_PP_LOCAL_LIMITS (0, NUM_IKERNELS - 1)
%:include BOOST_PP_LOCAL_ITERATE()

        /* TODO reset NICA */

        /* Read first stats */
        nica_top();
        first = s;
    }

    void TearDown()
    {
        udp_tb::testbench::TearDown();

	::n2h.verify();
	::h2n.verify();
    }

    int num_extra_clocks() { return 300; }

    void nica_top()
    {
        nica(nwp2sbu, sbu2nwp, cxp2sbu, sbu2cxp,
             &c, &s, events,
             BOOST_PP_ENUM_PARAMS(NUM_IKERNELS, ports),
             h2n_tc_out, h2n_tc_in, n2h_tc_out, n2h_tc_in, toe);
        link_fifo(h2n_tc_out, h2n_tc_in);
        link_fifo(n2h_tc_out, n2h_tc_in);
    }

    nica_stats stats()
    {
        return s - first;
    }

    void top()
    {
        for (int i = 0; i < 15; ++i)
            nica_top();
        hls_ik::ikernel_id id;
#define BOOST_PP_LOCAL_MACRO(n) \
        if (ikernel ## n) \
            ikernel ## n(ports ## n, id, gateway0, tc ## n);
#define BOOST_PP_LOCAL_LIMITS (0, NUM_IKERNELS - 1)
%:include BOOST_PP_LOCAL_ITERATE()
    }

    void reset_ikernel()
    {
        /* TODO reset ikernel */
    }

protected:
    udp::header_stream header;
    hls_ik::data_stream data;
    nica_config c;
    nica_stats s, first;
#define BOOST_PP_LOCAL_MACRO(n) \
    hls_ik::ports ports ## n; \
    hls_ik::tc_ikernel_data_counts tc ## n; \
    top_function ikernel ## n;
#define BOOST_PP_LOCAL_LIMITS (0, NUM_IKERNELS - 1)
%:include BOOST_PP_LOCAL_ITERATE()
    hls_ik::virt_gateway_registers gateway0, gateway1;
    tc_ports h2n_tc_out, h2n_tc_in, n2h_tc_out, n2h_tc_in;
};

TEST_F(testbench, n2h)
{
    const char *input_filename = "input.pcap";
    FILE* temp_file = tmpfile();
    EXPECT_TRUE(temp_file) << "cannot create temporary file for output.";

    c.n2h.common.enable = true;
    flow_table_wrapper ft_gateway([&]() { nica_top(); }, c.n2h.common.flow_table_gateway);
    EXPECT_TRUE(ft_gateway.add_flow(0, FT_IKERNEL));

    udp_tb::pkt_id_verifier n2h_verifier;
    hls::stream<mlx::user_t> user_values;
    read_pcap(input_filename, nwp2sbu, 0, std::numeric_limits<int>::max(),
              &n2h_verifier, &user_values);
    ikernel0 = passthrough_top;
    reset_ikernel();
    run();
    write_pcap(temp_file, sbu2cxp, false, &n2h_verifier, &user_values);

    EXPECT_TRUE(compare_output(filename(temp_file), "", "input-padded.pcap", ""));
    int count = 0;
    while (!sbu2nwp.empty()) {
        mlx::axi4s w = sbu2nwp.read();
        if (w.last && !w.user(0, 0))
            ++count;
    }
    nica_stats diff = stats();
    EXPECT_EQ(count, 0) << "number of packets";
    EXPECT_EQ(diff.n2h.udp.hds.ft_action_ikernel, 100) << "packets in matched statistic";
    EXPECT_EQ(diff.n2h.udp.hds.passthrough_not_ipv4, 0) << "!ipv4";
    EXPECT_EQ(diff.n2h.udp.hds.passthrough_not_udp, 0) << "!udp";
    EXPECT_EQ(diff.n2h.udp.hds.passthrough_bad_length, 0) << "bad length";
    EXPECT_EQ(diff.n2h.udp.hds.passthrough_disabled, 0) << "disabled";
    EXPECT_EQ(diff.n2h.ik0.packets, 100) << "packets";
}

TEST_F(testbench, all_packet_sizes)
{
    const char *input_filename = "all_sizes.pcap";
    FILE* temp_file = tmpfile();
    EXPECT_TRUE(temp_file) << "cannot create temporary file for output.";

    c.n2h.common.enable = true;
    flow_table_wrapper ft_gateway([&]() { nica_top(); }, c.n2h.common.flow_table_gateway);
    EXPECT_TRUE(ft_gateway.add_flow(0, FT_IKERNEL));

    udp_tb::pkt_id_verifier n2h_verifier;
    hls::stream<mlx::user_t> user_values;
    read_pcap(input_filename, nwp2sbu, 0, std::numeric_limits<int>::max(),
              &n2h_verifier, &user_values);
    ikernel0 = passthrough_top;
    reset_ikernel();
    run();
    write_pcap(temp_file, sbu2cxp, false, &n2h_verifier, &user_values);

    EXPECT_TRUE(compare_output(filename(temp_file), "", "all_sizes-padded.pcap", ""));
    int count = 0;
    while (!sbu2nwp.empty()) {
        mlx::axi4s w = sbu2nwp.read();
        if (w.last && !w.user(0, 0))
            ++count;
    }
    nica_stats diff = stats();
    EXPECT_EQ(count, 0) << "number of packets";
    EXPECT_EQ(diff.n2h.udp.hds.ft_action_ikernel, 38) << "packets in matched statistic";
    EXPECT_EQ(diff.n2h.udp.hds.passthrough_not_ipv4, 0) << "!ipv4";
    EXPECT_EQ(diff.n2h.udp.hds.passthrough_not_udp, 0) << "!udp";
    EXPECT_EQ(diff.n2h.udp.hds.passthrough_bad_length, 0) << "bad length";
    EXPECT_EQ(diff.n2h.udp.hds.passthrough_disabled, 0) << "disabled";
    EXPECT_EQ(diff.n2h.ik0.packets, 38) << "packets";
}
/* Test passthrough for non-ikernel traffic */
TEST_F(testbench, h2n_nica_passthrough)
{
    const char *input_filename = "passthrough.pcap";
    FILE* temp_file = tmpfile();
    EXPECT_TRUE(temp_file) << "cannot create temporary file for output.";

    udp_tb::pkt_id_verifier verifier;
    hls::stream<mlx::user_t> user_values;
    read_pcap(input_filename, cxp2sbu, 0, std::numeric_limits<int>::max(),
              &verifier, &user_values);
    ikernel0 = passthrough_top;
    reset_ikernel();
    run();
    write_pcap(temp_file, sbu2nwp, false, &verifier, &user_values);

    EXPECT_TRUE(compare_output(filename(temp_file), "", "passthrough-padded.pcap", ""));
    int count = 0;
    while (!sbu2cxp.empty()) {
        mlx::axi4s w = sbu2cxp.read();
        if (w.last && !w.user(0, 0))
            ++count;
    }
    nica_stats diff = stats();
    EXPECT_EQ(count, 0) << "number of packets";
    EXPECT_EQ(diff.h2n.udp.hds.ft_action_passthrough, 8) << "packets in matched statistic";
    EXPECT_EQ(diff.h2n.udp.hds.passthrough_not_ipv4, 8) << "!ipv4";
    EXPECT_EQ(diff.h2n.udp.hds.passthrough_not_udp, 8) << "!udp";
    EXPECT_EQ(diff.h2n.udp.hds.passthrough_disabled, 8) << "disabled";
    EXPECT_EQ(diff.h2n.ik0.packets, 0) << "packets";
}

/* TODO: not supported currently in the TCP version 
TEST_F(testbench, drop)
{
    const char *input_filename = "input.pcap";
    FILE* temp_file = tmpfile();
    EXPECT_TRUE(temp_file) << "cannot create temporary file for output.";

    c.n2h.common.enable = true;
    flow_table_wrapper ft_gateway([&]() { nica_top(); }, c.n2h.common.flow_table_gateway);
    EXPECT_TRUE(ft_gateway.add_flow(0, FT_IKERNEL));

    udp_tb::pkt_id_verifier n2h_verifier;
    hls::stream<mlx::user_t> user_values;
    ikernel0 = ::threshold_top;
    reset_ikernel();
    virt_gateway_wrapper gw([&]() { top(); }, gateway0, 15);
    gw.write(THRESHOLD_VALUE, 0xffffffff);
    read_pcap(input_filename, nwp2sbu, 0, std::numeric_limits<int>::max(), &n2h_verifier, &user_values);
    run();
    write_pcap(temp_file, sbu2cxp, false, &n2h_verifier, &user_values);

    EXPECT_TRUE(compare_output(filename(temp_file), "", input_filename, "!ip || !udp"));
    int count = 0;
    while (!sbu2nwp.empty()) {
        mlx::axi4s w = sbu2nwp.read();
        if (w.last && !w.user(0, 0))
            ++count;
    }
    EXPECT_EQ(count, 0) << "number of packets";
    nica_stats diff = stats();
    EXPECT_EQ(diff.n2h.udp.hds.ft_action_ikernel, 100) << "packets in matched statistic";
    EXPECT_EQ(diff.n2h.udp.hds.passthrough_not_ipv4, 0) << "!ipv4";
    EXPECT_EQ(diff.n2h.udp.hds.passthrough_not_udp, 0) << "!udp";
    EXPECT_EQ(diff.n2h.udp.hds.passthrough_bad_length, 0) << "bad length";
    EXPECT_EQ(diff.n2h.udp.hds.passthrough_disabled, 0) << "disabled";
    EXPECT_EQ(diff.n2h.ik0.packets, 0) << "packets";
}
*/

TEST_F(testbench, custom_rx_ring)
{
    const char *input_filename = "input.pcap";
    FILE* temp_file = tmpfile();
    EXPECT_TRUE(temp_file) << "cannot create temporary file for output.";

    c.n2h.common.enable = true;
    flow_table_wrapper ft_gateway([&]() { nica_top(); }, c.n2h.common.flow_table_gateway);
    hls_ik::flow_id_t flow_id = ft_gateway.add_flow(0, FT_IKERNEL);
    EXPECT_NE(flow_id, 0);
    gateway_wrapper cr_gateway([&]() { nica_top(); }, c.n2h.custom_ring_gateway);
    cr_gateway.write(CR_SRC_IP, 0x7f000001);
    cr_gateway.write(CR_DST_IP, 0x7f000001);
    cr_gateway.write(CR_DST_MAC_LO, 0xffffffff);
    cr_gateway.write(CR_DST_MAC_HI, 0xffff);
    cr_gateway.write(CR_SRC_MAC_LO, 0x00000000);
    cr_gateway.write(CR_SRC_MAC_HI, 0x0000);
    cr_gateway.write(CR_SRC_UDP, 61453);
    cr_gateway.write(CR_DST_UDP, 4791);
    cr_gateway.write(CR_DST_QPN, 1);
    cr_gateway.write(CR_WRITE_CONTEXT, 1);
    EXPECT_EQ(cr_gateway.read(CR_NUM_CONTEXTS), 1 << CUSTOM_RINGS_LOG_NUM);

    udp_tb::pkt_id_verifier n2h_verifier;
    hls::stream<mlx::user_t> user_values;
    ikernel0 = ::threshold_top;
    reset_ikernel();
    // Passthrough
    auto gw = virt_gateway_wrapper([&]() { top(); }, gateway0, 15);
    gw.write(THRESHOLD_VALUE, 0);
    gw.write(THRESHOLD_RING_ID, 1);
    ports0.host_credit_regs.ring_id = 1;
    ports0.host_credit_regs.max_msn = 100;
    read_pcap_callback(input_filename, nwp2sbu, 0, std::numeric_limits<int>::max(), &n2h_verifier, &user_values, [&]() {
        for (int i = 0; i < 5; ++i)
            top();
    });
    run();
    write_pcap(temp_file, sbu2cxp, false, &n2h_verifier, &user_values);

    EXPECT_TRUE(compare_output(filename(temp_file), "", "input-bth.pcap", ""));
    int count = 0;
    while (!sbu2nwp.empty()) {
        mlx::axi4s w = sbu2nwp.read();
        if (w.last && !w.user(0, 0))
            ++count;
    }
    EXPECT_EQ(count, 0) << "number of packets";
    nica_stats diff = stats();
    EXPECT_EQ(diff.n2h.udp.hds.ft_action_ikernel, 100) << "packets in matched statistic";
    EXPECT_EQ(diff.n2h.udp.hds.passthrough_not_ipv4, 0) << "!ipv4";
    EXPECT_EQ(diff.n2h.udp.hds.passthrough_not_udp, 0) << "!udp";
    EXPECT_EQ(diff.n2h.udp.hds.passthrough_bad_length, 0) << "bad length";
    EXPECT_EQ(diff.n2h.udp.hds.passthrough_disabled, 0) << "disabled";
    EXPECT_EQ(diff.n2h.ik0.packets, 100) << "packets";
}

TEST_F(testbench, mix_passthrough_and_generated)
{
    const int burst_size = 3;
    FILE* temp_file = tmpfile();
    EXPECT_TRUE(temp_file) << "cannot create temporary file for output.";

    c.h2n.common.enable = true;
    flow_table_wrapper ft_gateway([&]() { nica_top(); }, c.h2n.common.flow_table_gateway);
    ft_gateway.set_fields(FT_FIELD_DST_PORT);
    EXPECT_TRUE(ft_gateway.add_flow(0xbad, FT_IKERNEL));

    gateway_wrapper arb_gateway([&]() { nica_top(); }, c.h2n.common.arbiter_gateway);
    arb_gateway.write(ARBITER_SCHEDULER + SCHED_DRR_QUANTUM, 47, 5);
    nica_top();

    udp_tb::pkt_id_verifier h2n_verifier;
    ikernel0 = ::pktgen_top;
    reset_ikernel();
    virt_gateway_wrapper([&]() { top(); }, gateway0, 15).write(PKTGEN_BURST_SIZE, burst_size);
    read_pcap("0bad.pcap", cxp2sbu, 0, std::numeric_limits<int>::max(), &h2n_verifier);
    read_pcap("f00d.pcap", cxp2sbu, 0, std::numeric_limits<int>::max(), &h2n_verifier);
    run();
    write_pcap(temp_file, sbu2nwp, false, &h2n_verifier);

    EXPECT_TRUE(compare_output(filename(temp_file), "udp port 61453",
                               "f00d-padded.pcap",  "udp port 61453"));
    EXPECT_TRUE(compare_output(filename(temp_file), "-c1 udp port 2989",
                               "0bad-padded.pcap",  "-c1 udp port 2989"));
    int count = 0;
    while (!sbu2cxp.empty()) {
        mlx::axi4s w = sbu2cxp.read();
        if (w.last && !w.user(0, 0))
            ++count;
    }
    EXPECT_EQ(count, 0) << "number of packets";
    nica_stats diff = stats();
    EXPECT_EQ(diff.h2n.arbiter.tx_port[NUM_TC - 1].packets, 16) << "passthrough packets";
    EXPECT_EQ(diff.h2n.arbiter.tx_port[0].packets, 1 + burst_size) << "ikernel passthrough + gen. packets";
    for (int i = 1; i < NUM_TC - 1; ++i)
        EXPECT_EQ(diff.h2n.arbiter.tx_port[i].packets, 0);

    EXPECT_EQ(diff.h2n.ik0.packets, 1 + burst_size) << "packets";
}

TEST_F(testbench, flow_table)
{
    flow_table_wrapper ft_gateway([&]() { nica_top(); }, c.h2n.common.flow_table_gateway);
    ft_gateway.set_fields(FT_FIELD_DST_PORT);
    for (int port = 1; port < 10; ++port)
        EXPECT_TRUE(ft_gateway.add_flow(port, FT_IKERNEL)) << port;
    for (int port = 1; port < 10; ++port)
        EXPECT_FALSE(ft_gateway.add_flow(port, FT_IKERNEL)) << port;
    for (int port = 1; port < 10; ++port)
        EXPECT_TRUE(ft_gateway.delete_flow(port)) << port;
    for (int port = 1; port < 10; ++port)
        EXPECT_FALSE(ft_gateway.delete_flow(port)) << port;
}

#if NUM_IKERNELS >= 2
TEST_F(testbench, two_ikernels)
{
    const char *input_filename = "input.pcap";
    FILE* temp_file = tmpfile();
    EXPECT_TRUE(temp_file) << "cannot create temporary file for output.";

    c.n2h.enable = true;
    flow_table_wrapper ft_gateway([&]() { nica_top(); }, c.n2h.flow_table_gateway);
    ft_gateway.set_fields(FT_FIELD_DST_PORT);
    EXPECT_TRUE(ft_gateway.add_flow(2989, FT_IKERNEL, 0));
    EXPECT_TRUE(ft_gateway.add_flow(47824, FT_IKERNEL, 1));

    udp_tb::pkt_id_verifier n2h_verifier;
    ikernel0 = ::threshold_top;
    ikernel1 = ::passthrough_top;
    reset_ikernel();
    virt_gateway_wrapper gw([&]() { top(); }, gateway0);
    gw.write(THRESHOLD_VALUE, 0);
    gw.write(THRESHOLD_WRITE_CONTEXT, 0, 0, 15);
    read_pcap(input_filename, nwp2sbu, 0, std::numeric_limits<int>::max(), &n2h_verifier);
    run();
    write_pcap(temp_file, sbu2cxp, false, &n2h_verifier);

    EXPECT_TRUE(compare_output(filename(temp_file), "udp port 47824",
                               "input-padded.pcap", "udp port 47824"));
    EXPECT_TRUE(compare_output(filename(temp_file), "udp port 2989",
                               "input-padded.pcap", "udp port 2989"));
    int count = 0;
    while (!sbu2nwp.empty()) {
        mlx::axi4s w = sbu2nwp.read();
        if (w.last && !w.user(0, 0))
            ++count;
    }
    EXPECT_EQ(count, 0) << "number of packets";
    nica_stats diff = stats();
    EXPECT_EQ(diff.n2h.udp.hds.ft_action_ikernel, 100) << "packets in matched statistic";
    EXPECT_EQ(diff.n2h.udp.hds.passthrough_not_ipv4, 0) << "!ipv4";
    EXPECT_EQ(diff.n2h.udp.hds.passthrough_not_udp, 0) << "!udp";
    EXPECT_EQ(diff.n2h.udp.hds.passthrough_bad_length, 0) << "bad length";
    EXPECT_EQ(diff.n2h.udp.hds.passthrough_disabled, 0) << "disabled";

    EXPECT_EQ(diff.n2h.ik0.actions[hls_ik::PASS], 24) << "PASS packets";
    EXPECT_EQ(diff.n2h.ik0.actions[hls_ik::GENERATE], 0) << "GENERATE packets";

    EXPECT_EQ(diff.n2h.ik1.actions[hls_ik::PASS], 76) << "PASS packets";
    EXPECT_EQ(diff.n2h.ik1.actions[hls_ik::GENERATE], 0) << "GENERATE packets";
}
#endif

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
