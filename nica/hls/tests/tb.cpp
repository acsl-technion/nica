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

#include <pcap/pcap.h>
#include <iostream>
#include <string>
#include <unistd.h>
#include <endian.h>
#include <boost/lexical_cast.hpp>
#include <algorithm>
#include "gtest/gtest.h"

#include "udp.h"
#include "tb.h"

using std::cout;
using std::endl;
using std::string;
using boost::lexical_cast;

using namespace udp_tb;

struct packet_handler_context {
    mlx::stream& stream;
    int count;
    int range_start;
    int range_end;
    pkt_id_verifier* verifier;
    hls::stream<mlx::user_t>* user_values;
    testbench::callback_t callback;

    explicit packet_handler_context(mlx::stream& stream,
                                    hls::stream<mlx::user_t>* user_values) :
        stream(stream), count(0), user_values(user_values) {}
};

void packet_handler(u_char *user, const struct pcap_pkthdr *h, const u_char *bytes)
{
    auto context = reinterpret_cast<packet_handler_context*>(user);
    mlx::stream& stream = context->stream;
    const int b = MLX_AXI4_WIDTH_BYTES;
    mlx::user_t mlx_user;
    if (context->user_values) {
        mlx_user = (random() & 1) ? MLX_TUSER_MAGIC : 0;
        if (mlx_user)
            context->user_values->write(mlx_user);
    } else {
        mlx_user = MLX_TUSER_MAGIC;
    }

    if (h->caplen != h->len)
        return;

    if (context->count < context->range_start || context->count >= context->range_end)
        goto end;

    for (unsigned word = 0; word < ALIGN(h->len, b); word += b) {
        mlx::axi4s input(0, 0xffffffff, false);
        for (unsigned byte = 0; byte < b && word + byte < h->len; ++byte)
            input.data(input.data.width - 1 - 8 * byte, input.data.width - 8 - 8 * byte) = bytes[word + byte];
        if ((word + b) >= h->len) {
            input.keep = hls_ik::axi_data::keep_bytes(h->len - word - b);
            input.last = true;
        }

        input.id = context->count & 7;
        input.user = mlx_user;
        if (context->verifier && word == 0)
                context->verifier->inc(input.id);
        stream.write(input);
    }
end:
    ++context->count;

    if (context->callback)
        context->callback();
}

testbench::testbench() :
    cxp2sbu("cxp2sbu"), sbu2cxp("sbu2cxp"),
    nwp2sbu("nwp2sbu"), sbu2nwp("sbu2nwp"),
    count(0)
{}

int testbench::read_pcap(const string& filename, mlx::stream& stream,
        int range_start, int range_end, pkt_id_verifier* verifier,
        hls::stream<mlx::user_t>* user_values)
{
    return read_pcap_callback(filename, stream, range_start, range_end, verifier, user_values, NULL);
}

int testbench::read_pcap_callback(
    const std::string& filename, mlx::stream& stream,
    int range_start, int range_end,
    pkt_id_verifier* verifier,
    hls::stream<mlx::user_t>* user_values,
    testbench::callback_t callback)
{
    char errbuf[PCAP_ERRBUF_SIZE];
    pcap_t *file = pcap_open_offline(filename.c_str(), errbuf);

    if (!file) {
        fprintf(stderr, "%s\n", errbuf);
        return -1;
    }

    auto context = packet_handler_context(stream, user_values);
    context.range_start = range_start; context.range_end = range_end;
    context.verifier = verifier;
    context.callback = callback;
    int ret = pcap_loop(file, 0, &packet_handler, (u_char *)&context);
    if (ret == -1) {
        perror("pcap_loop returned error");
        return -1;
    }

    pcap_close(file);

    return context.count;
}

int testbench::write_pcap(FILE* file, mlx::stream& stream, bool expected_lossy,
                          pkt_id_verifier* verifier, hls::stream<mlx::user_t>* user_values)
{
    int ret;
    int count = 0;

    pcap_t *dead = pcap_open_dead(DLT_EN10MB, 65535);
    if (!dead) {
        perror("pcap_open_dead failed");
        return -1;
    }

    pcap_dumper_t *output = pcap_dump_fopen(dead, file);
    if (!output) {
        perror("pcap_dump_open failed");
        return -1;
    }

    u_char buffer[65535];
    pcap_pkthdr h = {};
    h.len = 0;
    mlx::pkt_id_t cur_pkt_id;
    mlx::user_t cur_user;

    while (!stream.empty()) {
        mlx::axi4s w = stream.read();
        const mlx::user_t masked_user = w.user & MLX_TUSER_MAGIC_MASK;

        if (h.len == 0) {
            // new packet
            if (verifier) {
                verifier->dec(w.id);
                cur_pkt_id = w.id;
            }
            if (masked_user && user_values) {
                EXPECT_FALSE(user_values->empty());
                cur_user = user_values->read();
            } else if (masked_user && !user_values) {
                cur_user = MLX_TUSER_MAGIC;
            } else if (!masked_user) {
                cur_user = 0;
            }
        } else {
            // middle of a packet
            if (verifier)
                EXPECT_EQ(w.id, cur_pkt_id);
        }
        EXPECT_EQ(masked_user, cur_user) << "tuser";

        for (int byte = 0; byte < w.data.width / 8; ++byte)
            buffer[h.len + byte] = w.data(w.data.width - 1 - byte * 8,
                                          w.data.width - 8 - byte * 8);
        h.len += w.data.width / 8;
        if (w.last) {
            for (int i = 0; i < w.data.width / 8; ++i)
                if (!w.keep(i, i))
                    --h.len;
                else
                    break;
            /* Minimum Ethernet packet length is 64 bytes including the FCS */
            h.caplen = h.len;
            if (!w.user(0, 0)) /* Drop bit cleared */ {
                pcap_dump((u_char *)output, &h, buffer);
                ++count;
            }
            EXPECT_EQ(bool(w.user(2,2)), expected_lossy);
            h.len = 0;
        } else {
            EXPECT_EQ(~w.keep, 0);
        }
    }

    EXPECT_EQ(h.len, 0);
    EXPECT_TRUE(stream.empty());
    EXPECT_TRUE(!verifier || verifier->verify());

    ret = pcap_dump_flush(output);
    if (ret) {
        perror("pcap_dump_flush returned error");
        return -1;
    }

    pcap_close(dead);
    fdatasync(fileno(file));

    return count;
}

void testbench::run()
{
    // TODO run simulation until it ends?
    for (int i = 0; i < num_extra_clocks(); ++i)
        top();

    EXPECT_TRUE(nwp2sbu.empty());
    EXPECT_TRUE(cxp2sbu.empty());
}

bool testbench::compare_output(const string& pcap1, const string& filter1,
                               const string& pcap2, const string& filter2)
{
    string dump1 = "tcpdump -r '" + pcap1 + "' -xxtn " + filter1;
    string dump2 = "tcpdump -r '" + pcap2 + "' -xxtn " + filter2;
    string compare_output = "bash -c \"diff -u <(" + dump1 + ") <(" + dump2 + ")\"";
    cout << compare_output << endl;
    int ret = system(compare_output.c_str());
    return ret == 0 && !WEXITSTATUS(ret);
}

bool testbench::compare_output(const string& pcap1, const string& filter1,
                               const string& output_txt_file)
{
    string dump1 = "tcpdump -r '" + pcap1 + "' -xxtn '" + filter1 + "'";
    string compare_output = "bash -c \"diff -u <(" + dump1 + ") '" + output_txt_file + "'\"";
    cout << compare_output << endl;
    int ret = system(compare_output.c_str());
    return ret == 0 && !WEXITSTATUS(ret);
}

string testbench::filename(FILE* file)
{
    return "/proc/self/fd/" + lexical_cast<string>(fileno(file));
}

void testbench::TearDown()
{
    EXPECT_TRUE(nwp2sbu.empty());
    EXPECT_TRUE(cxp2sbu.empty());
    EXPECT_TRUE(sbu2nwp.empty());
    EXPECT_TRUE(sbu2cxp.empty());
}

pkt_id_verifier::pkt_id_verifier() : count() {}

void pkt_id_verifier::inc(mlx::pkt_id_t id)
{
    if (id)
        ++count[id];
}

void pkt_id_verifier::dec(mlx::pkt_id_t id)
{
    if (id)
        EXPECT_TRUE(--count[id] >= 0);
}

bool pkt_id_verifier::verify() const
{
    return std::all_of(count, &count[8], [](int x) { return x >= 0; });
}
