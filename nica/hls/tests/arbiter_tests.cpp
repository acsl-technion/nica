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

#include "arbiter-impl.hpp"
#include "demux.hpp"
#include "ikernel_tests.hpp"
#include "gtest/gtest.h"

typedef arbiter arbiter_t;
using udp::udp_builder_metadata_stream;
using udp::udp_builder_metadata;

using namespace hls_ik;

namespace {

    class arbiter_tests : public ::testing::Test {
    protected:
        gateway_wrapper gateway;
        udp_builder_metadata_stream hdr_out, meta[NUM_TC];
        data_stream out, port[NUM_TC];
        arbiter_stats<NUM_TC> stats;
        gateway_registers regs;

        void progress()
        {
            arbiter_top(hdr_out, out, meta, port, &stats, regs);
        }

        arbiter_tests() :
            gateway([&]() { progress(); }, regs, 5)
        {}

        // SetUp() is run immediately before a test starts.
        virtual void SetUp() {
        }

        // TearDown() is invoked immediately after a test finishes.
        virtual void TearDown() {
        }

        void write_packets(int port_index, int first, int last) {
            for (int i = first; i < last; ++i) {
                udp_builder_metadata m;
                m.length = i * 32;
                m.ip_identification = ip_id(port_index, i);

                meta[port_index].write(m);

                for (int j = 0; j < i; ++j) {
                    port[port_index].write(axi_data(flit_id(m.ip_identification, j), 0xffffffff, j == i - 1));
                }
            }
        }

        int ip_id(int port_index, int pkt_id)
        {
            assert(port_index < NUM_TC);
            assert(pkt_id < 256);
            return port_index << 8 | pkt_id;
        }

        int flit_id(int ip_id, int flit_number)
        {
            assert(flit_number < 256);
            return ip_id << 8 | flit_number;
        }

        void read_packets(int port_index, int first, int last) {
            for (int i = first; i < last; ++i) {
                udp_builder_metadata m;
                m.length = i * 32;
                m.ip_identification = ip_id(port_index, i);

                EXPECT_FALSE(hdr_out.empty()) << i;
                udp_builder_metadata m_out = hdr_out.read();
                EXPECT_EQ(m, m_out) << i;
                EXPECT_EQ(m.ip_identification, m_out.ip_identification) << i;

                for (int j = 0; j < i; ++j) {
                    axi_data d(flit_id(m_out.ip_identification, j), 0xffffffff, j == i - 1);
                    EXPECT_FALSE(out.empty()) << i;
                    axi_data d_out = out.read();
                    EXPECT_EQ(d, d_out) << i;
                }
            }
        }
    };

    TEST_F(arbiter_tests, single_flow)
    {
        int first = 0;
        int last = 33;
        write_packets(1, first, last);

        for (int i = 0; i < (last + first) * (last - first + 1); ++i)
            progress();
        read_packets(1, first, last);
    }

    TEST_F(arbiter_tests, all_ports)
    {
        int first = 0;
        int last = 5;
        for (int port_index = 0; port_index < NUM_TC; ++port_index) {
            gateway.write(ARBITER_SCHEDULER + ARBITER_SCHEDULER_STRIDE * port_index + SCHED_DRR_QUANTUM, 3);
            EXPECT_EQ(3, gateway.read(ARBITER_SCHEDULER + ARBITER_SCHEDULER_STRIDE * port_index + SCHED_DRR_QUANTUM));
            write_packets(port_index, first, last);
        }
        for (int i = 0; i < (last + first) * (last - first + 1) * NUM_TC; ++i)
            progress();
        for (int i = 0; i < (last - first) * NUM_TC; ++i) {
            udp_builder_metadata m;

            EXPECT_FALSE(hdr_out.empty()) << "i = " << i;
            udp_builder_metadata m_out = hdr_out.read();

            int length = std::min(ALIGN(int(m_out.length), 32) / 32, last - first + 1);
            for (int j = 0; j < length; ++j) {
                uint16_t id = m_out.ip_identification;
                axi_data d(flit_id(id, j), 0xffffffff, j == (ALIGN(m_out.length, 32) / 32 - 1));
                EXPECT_FALSE(out.empty()) << "i = " << i << ", id " << id;
                axi_data d_out = out.read();
                EXPECT_EQ(d, d_out) << "i = " << i << ", id " << id;
            }
        }
    }

    class demux_tests : public arbiter_tests {
    protected:
        hls_ik::data_stream passthrough_data_in, demux_data;
        udp_builder_metadata_stream passthrough_meta_in, demux_meta;
        tc_ports tc_out, tc_in;

        void progress()
        {
            demux_arb_top(demux_meta, demux_data, passthrough_meta_in,
                          passthrough_data_in, hdr_out, out, &stats, regs, tc_out, tc_in);
            link_fifo(tc_out, tc_in);
        }

        // SetUp() is run immediately before a test starts.
        virtual void SetUp() {
        }

        // TearDown() is invoked immediately after a test finishes.
        virtual void TearDown() {
        }

        void write_packets(int port_index, int first, int last) {
            for (int i = first; i < last; ++i) {
                udp_builder_metadata m;
                m.length = i * 32;
                m.ip_identification = i;
                m.ikernel_id = port_index;

                demux_meta.write(m);

                for (int j = 0; j < i; ++j) {
                    demux_data.write(axi_data(flit_id(i, j), 0xffffffff, j == i - 1));
                }
            }
        }
    };

    TEST_F(demux_tests, all_ports)
    {
        int first = 0;
        int last = 5;
        for (int port_index = 0; port_index < NUM_TC; ++port_index)
            write_packets(port_index, first, last);
        int num_flits = (last + first) * (last - first + 1) / 2 * NUM_TC;
        for (int i = 0; i < num_flits + num_flits / 3 * 10; ++i)
            progress();
        for (int i = 0; i < (last - first) * NUM_TC; ++i) {
            udp_builder_metadata m;

            EXPECT_FALSE(hdr_out.empty()) << "i = " << i;
            udp_builder_metadata m_out = hdr_out.read();

            for (int j = 0; j < ALIGN(m_out.length, 32) / 32; ++j) {
                uint16_t id = m_out.ip_identification;
                axi_data d(flit_id(id, j), 0xffffffff, j == (ALIGN(m_out.length, 32) / 32 - 1));
                EXPECT_FALSE(out.empty()) << "i = " << i << ", id " << id;
                axi_data d_out = out.read();
                EXPECT_EQ(d, d_out) << "i = " << i << ", id " << id;
            }
        }
    }

}

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
