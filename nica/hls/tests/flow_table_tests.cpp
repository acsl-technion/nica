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

#define CACHE_ENABLE_DEBUG_COMMANDS

#include "flow_table_impl.hpp"
#include "ikernel_tests.hpp"
#include "gtest/gtest.h"

typedef hash_flow_table_t::value_type value_type;
typedef hash_flow_table_t::maybe_value_t maybe_value_t;
typedef hash_flow_table_t::tag_type tag_type;

/* Instantiate here to make sure the code instantiated notices the
 * CACHE_ENABLE_DEBUG_COMMANDS definition */
template class ntl::hash_table_wrapper<flow, flow_table_value, FLOW_TABLE_SIZE>;

using std::make_tuple;
using std::get;

using udp::header_stream;

using hls_ik::gateway_registers;

using ntl::maybe;
using ntl::make_maybe;

void flow_table_top(header_stream& header, result_stream& result,
                    gateway_registers& g);
namespace {
    class flow_table_tests : public ::testing::Test {
    protected:
        flow_table_wrapper gateway;
        header_stream header;
        result_stream result;
        gateway_registers regs;

        flow_table_tests() :
            gateway([&]() { flow_table_top(header, result, regs); }, regs)
        {}

        // SetUp() is run immediately before a test starts.
        virtual void SetUp() {
        }

        // TearDown() is invoked immediately after a test finishes.
        virtual void TearDown() {
        }

        int add_flow(const hash_flow_table_t::value_type& value)
        {
            flow f;
            flow_table_value result;

            std::tie(f, result) = value;

            gateway.write(FT_KEY_SADDR, f.saddr);
            gateway.write(FT_KEY_DADDR, f.daddr);
            gateway.write(FT_KEY_SPORT, f.source_port);
            gateway.write(FT_KEY_DPORT, f.dest_port);
            gateway.write(FT_RESULT_ACTION, result.action);
            gateway.write(FT_RESULT_ENGINE, result.engine_id);
            gateway.write(FT_RESULT_IKERNEL_ID, result.ikernel_id);

            return gateway.read(FT_ADD_FLOW, 15);
        }

        int delete_flow(const hash_flow_table_t::tag_type& f)
        {
            gateway.write(FT_KEY_SADDR, f.saddr);
            gateway.write(FT_KEY_DADDR, f.daddr);
            gateway.write(FT_KEY_SPORT, f.source_port);
            gateway.write(FT_KEY_DPORT, f.dest_port);

            return gateway.read(FT_DELETE_FLOW, 15);
        }

        maybe_value_t get_entry(uint32_t address)
        {
            gateway.write(FT_READ_ENTRY, address, 15);
            bool valid = gateway.read(FT_VALID);
            uint32_t saddr = gateway.read(FT_KEY_SADDR);
            uint32_t daddr = gateway.read(FT_KEY_DADDR);
            uint32_t sport = gateway.read(FT_KEY_SPORT);
            uint32_t dport = gateway.read(FT_KEY_DPORT);
            uint32_t action = gateway.read(FT_RESULT_ACTION);
            uint32_t ikernel = gateway.read(FT_RESULT_ENGINE);
            uint32_t ikernel_id = gateway.read(FT_RESULT_IKERNEL_ID);

            return make_maybe(valid, make_tuple(
                flow(sport, dport, saddr, daddr),
                flow_table_value(flow_table_action(action), ikernel, ikernel_id)));
        }

        void progress()
        {
            flow_table_top(header, result, regs);
        }
    };

    TEST_F(flow_table_tests, add_delete)
    {
        for (int j = 0; j < 3; ++j) {
            for (int i = 0; i < 100; ++i) {
                flow f = { i };
                flow_table_value value(FT_IKERNEL, i);
                uint32_t index = add_flow(make_tuple(f, value));
                EXPECT_NE(0, index);
                EXPECT_EQ(0, add_flow(make_tuple(f, value)));

                maybe_value_t entry = get_entry(index - 1);
                EXPECT_TRUE(entry.valid());
                EXPECT_EQ(make_tuple(f, value), entry.value());

                EXPECT_TRUE(delete_flow(f));
                EXPECT_FALSE(delete_flow(f));

                entry = get_entry(index - 1);
                EXPECT_FALSE(entry.valid());
            }
        }
    }
}

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
