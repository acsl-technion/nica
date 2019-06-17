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

#include "push_suffix.hpp"
#include "gtest/gtest.h"

namespace {

    class push_suffix_tests : public ::testing::TestWithParam<unsigned> {
    protected:
        // SetUp() is run immediately before a test starts.
        virtual void SetUp() {
        }

        // TearDown() is invoked immediately after a test finishes.
        virtual void TearDown() {
        }

        push_suffix<4> push;
    };

    void write_sized_packet(hls_ik::data_stream& s, unsigned size)
    {
	char c = 0;

        for (unsigned i = 0; i < size; i += 32) {
            char buf[32];
	    for (unsigned cur_char = 0; cur_char < 32u; ++cur_char)
		buf[cur_char] = ++c;

            hls_ik::axi_data out;
            out.set_data(buf, std::min(32u, size - i));
	    out.last = i + 32 >= size;
            s.write(out);
        }
    }

    unsigned get_packet_size(hls_ik::data_stream& s, unsigned orig_size)
    {
        unsigned size = 0;
	char c = 0;
        const unsigned orig_size_aligned = (orig_size + 3) & ~0x3;

        while (!s.empty()) {
            hls_ik::axi_data out = s.read();
            char buf[32];	
            unsigned cur_size = out.get_data(buf);

	    for (unsigned cur_char = 0; cur_char < cur_size; ++cur_char) {
		if (size + cur_char < orig_size)
                    EXPECT_EQ(buf[cur_char], ++c);
                else if (size + cur_char < orig_size_aligned)
                    EXPECT_EQ(buf[cur_char], 0);
		else
                    EXPECT_EQ(buf[cur_char], char(0xa1 + size + cur_char - orig_size_aligned));
	    }

            size += cur_size;
            if (out.last)
                return size;
        }

        return size;
    }

    TEST_P(push_suffix_tests, one_packet)
    {
        hls_ik::data_stream in("in"), out("out");
        hls::stream<bool> enable_stream("enable"), empty_packet("empty");
	using suffix_t = push_suffix<4>::suffix_t;
        hls::stream<suffix_t> suffix("suffix");

        enable_stream.write(true);
        empty_packet.write(GetParam() == 0);
        suffix.write(suffix_t(0xa1a2a3a4));
        write_sized_packet(in, GetParam());
	for (unsigned i = 0; i < (2 + GetParam()) * 2; ++i)
	    push.reorder(in, empty_packet, enable_stream, suffix, out);
        unsigned expected_size = (GetParam() + 4 + 3) & ~3; /* 4 byte aligned */
	EXPECT_EQ(expected_size, get_packet_size(out, GetParam()));
    }

    TEST_P(push_suffix_tests, disabled)
    {
        hls_ik::data_stream in("in"), out("out");
        hls::stream<bool> enable_stream("enable"), empty_packet("empty");
	using suffix_t = push_suffix<4>::suffix_t;
        hls::stream<suffix_t> suffix("suffix");

        empty_packet.write(GetParam() == 0);
        enable_stream.write(false);
        write_sized_packet(in, GetParam());
	for (unsigned i = 0; i < 1 + GetParam() * 2; ++i)
	    push.reorder(in, empty_packet, enable_stream, suffix, out);
	EXPECT_EQ(GetParam(), get_packet_size(out, GetParam()));
    }
}

INSTANTIATE_TEST_CASE_P(push_list, push_suffix_tests,
			::testing::Range(0u, 3 * 32u));

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
