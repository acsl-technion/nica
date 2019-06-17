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
#include "coap.hpp"
#include <openssl/sha.h>
#include "tb.h"

using namespace std;
using namespace hls_ik;

typedef void (* coap_top_function)(hls_ik::ports &, hls_ik::ikernel_id &, hls_ik::virt_gateway_registers&,
                                   hls_ik::tc_ikernel_data_counts& tc,
                                   hls::stream<coap_sha_request>& first_pass_sha_unit_input_stream,
                                   hls::stream<coap_sha_response>& first_pass_sha_unit_output_stream,
                                   hls::stream<coap_sha_request>& second_pass_sha_unit_input_stream,
                                   hls::stream<coap_sha_response>& second_pass_sha_unit_output_stream);

namespace {

    class coap_test : public ::testing::TestWithParam<coap_top_function>, public udp_tb::testbench
    {
    protected:
        hls_ik::ports p;
        hls_ik::tc_ikernel_data_counts tc;
        hls_ik::ikernel_id id;
        hls_ik::virt_gateway_registers gateway;
        hls_ik::ikernel_id_t ik_id;
        int retries;
        nica_config c;
        nica_stats s;
        tc_ports h2n_tc, n2h_tc;

        hls::stream<coap_sha_request> first_pass_sha_unit_input_stream;
        hls::stream<coap_sha_response> first_pass_sha_unit_output_stream;
        hls::stream<coap_sha_request> second_pass_sha_unit_input_stream;
        hls::stream<coap_sha_response> second_pass_sha_unit_output_stream;
        SHA256_CTX ctx_a, ctx_b;
        char buff[SHA256_BLOCK_BYTES];

        void top() {
            ::nica(nwp2sbu, sbu2nwp, cxp2sbu, sbu2cxp,
                   &c, &s, events, p, h2n_tc, h2n_tc, n2h_tc, n2h_tc);

            GetParam()(p, id, gateway, tc, first_pass_sha_unit_input_stream, first_pass_sha_unit_output_stream,
                       second_pass_sha_unit_input_stream, second_pass_sha_unit_output_stream);

            calc_sha(first_pass_sha_unit_input_stream, first_pass_sha_unit_output_stream, ctx_a);
            calc_sha(second_pass_sha_unit_input_stream, second_pass_sha_unit_output_stream, ctx_b);
        }

        void write_buff(char buff[SHA256_BLOCK_BYTES], const ap_uint<SHA256_BLOCK_BITS>& vec) {
            for (int i = 0; i < SHA256_BLOCK_BYTES; ++i) {
                buff[i] = hls_helpers::get_byte<SHA256_BLOCK_BITS>(vec, i);
            }
        }

        void calc_sha(hls::stream<coap_sha_request>& req_stream,
                      hls::stream<coap_sha_response>& res_stream,
                      SHA256_CTX& ctx) {
            if (!req_stream.empty()) {
                coap_sha_request req = req_stream.read();
                write_buff(buff, req.data);
                SHA256_Update(&ctx, buff, SHA256_BLOCK_BYTES);

                coap_sha_response response;
                if (req.last) {
                    for (int i = 0; i < 8; ++i)
                        hls_helpers::int_to_bytes<SHA256_HASH_BITS>(ctx.h[i], response.data, i);

                    res_stream.write(response);
                    SHA256_Init(&ctx);
                }
            }
        }

        int read(int address) {
            return virt_gateway_wrapper([&]() { top(); }, gateway).read(address, ik_id, retries);
        }

        void write(int address, int data) {
            virt_gateway_wrapper([&]() { top(); }, gateway).write(address, data, ik_id, retries);
        }

        coap_test() : retries(100) {
            SHA256_Init(&ctx_a);
            SHA256_Init(&ctx_b);
        }
    };

    TEST_P(coap_test, set_key) {
        const unsigned char key[SHA256_BLOCK_BYTES] = { 's', 'e', 'c', 'r', 'e', 't' };
        int first_word = hls_helpers::bytes_to_int(key, 0);
        int second_word = hls_helpers::bytes_to_int(key, 1);

        for (uint32_t i = 0; i < SHA256_BLOCK_BYTES / sizeof(int); ++i) {
            if (i == 0) {
                write(COAP_KEY, first_word);
                ASSERT_EQ(read(COAP_KEY), -1);
            } else if (i == 1) {
                write(COAP_KEY + 1, second_word);
                ASSERT_EQ(read(COAP_KEY + 1), -1);
            } else {
                write(COAP_KEY + i, 0);
                ASSERT_EQ(read(COAP_KEY + i), -1);
            }
        }
    }

    TEST_P(coap_test, pcap) {
        FILE* cxp_output = tmpfile();
        ASSERT_TRUE(cxp_output) << "cannot create temporary file for output.";

        memset(&c, 0, sizeof(c));
        flow_table_wrapper n2h_ft_gateway([&]() { top(); }, c.n2h.common.flow_table_gateway);
        flow_table_wrapper h2n_ft_gateway([&]() { top(); }, c.h2n.common.flow_table_gateway);
        EXPECT_TRUE(n2h_ft_gateway.add_flow(0, FT_IKERNEL));
        EXPECT_TRUE(h2n_ft_gateway.add_flow(0, FT_IKERNEL));
        c.n2h.common.enable = true;
        c.h2n.common.enable = true;

        ASSERT_GE(read_pcap("coap-requests.pcap", nwp2sbu), 0);
        run();

        write_pcap(cxp_output, sbu2cxp, false);
        EXPECT_TRUE(compare_output(filename(cxp_output), "", "coap-filtered.pcap", ""));
        EXPECT_TRUE(first_pass_sha_unit_input_stream.empty());
        EXPECT_TRUE(first_pass_sha_unit_output_stream.empty());
        EXPECT_TRUE(second_pass_sha_unit_input_stream.empty());
        EXPECT_TRUE(second_pass_sha_unit_output_stream.empty());
    }

    INSTANTIATE_TEST_CASE_P(coap_ikernel_tests_instance, coap_test,
                            ::testing::Values(&coap_ikernel));

} // namespace

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}


