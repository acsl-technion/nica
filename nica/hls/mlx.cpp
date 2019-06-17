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

#include <mlx.h>
#include <udp.h>
#include <ntl/link.hpp>

namespace mlx {

void dropper::step(stream& in, hls::stream<bool>& pass_stream, stream& out)
{

    DO_PRAGMA(HLS STREAM variable=in depth=FIFO_WORDS);

#pragma HLS pipeline enable_flush
    axi4s word;
    switch (state) {
    case IDLE:
        if (!pass_stream.empty() && !in.empty() && !out.full()) {
            drop = !pass_stream.read();
            in.read(word);
            state = word.last ? IDLE : STREAM;
            if (!drop)
		out.write(word);
        }
        break;
    case STREAM:
        if (!in.empty() && !out.full()) {
            in.read(word);
            state = word.last ? IDLE : STREAM;
            if (!drop)
		out.write(word);
        }
    }
}

metadata_and_data split_metadata(const axi4s& in)
{
    return std::make_tuple(metadata(in.user, in.id),
                           hls_ik::axi_data(in.data, in.keep, in.last));
}

axi4s join_metadata(const metadata_and_data& in)
{
    metadata m;
    hls_ik::axi_data d;
    std::tie(m, d) = in;
    return axi4s(d.data, d.keep, d.last, m.user, m.id);
}

void join_packet_metadata::operator()(metadata_stream& meta_in,
                                      hls_ik::data_stream& data_in, stream& out)
{
#pragma HLS pipeline II=1 enable_flush
    switch (state) {
    case IDLE:
        if (!meta_in.read_nb(m))
            return;

        state = STREAM;
        break;
    case STREAM:
        if (data_in.empty() || out.full())
            return;

        ap_uint<hls_ik::axi_data::width> raw;
        data_in.read_nb(raw);
        hls_ik::axi_data d = raw;
        out.write_nb(join_metadata(std::make_tuple(m, d)));
        state = d.last ? IDLE : STREAM;
        break;
    }
}

std::ostream& operator<<(::std::ostream& out, const metadata& m)
{
    return out << "mlx::metadata(user=" << m.user << ", id=" << m.id << ")";
}

void extract_metadata::step(stream& in)
{
    ntl::link(in, _enum_in);
    _enum.step(_enum_in);
    {
        if (_enum.out.empty() || out_data.full() || out_metadata.full())
            return;

        int count;
        axi4s flit;
        std::tie(count, flit) = _enum.out.read();

        metadata m;
        ntl::axi_data axi_data_flit;
        std::tie(m, axi_data_flit) = split_metadata(flit);

        if (count == 0)
            out_metadata.write(m);
        out_data.write(axi_data_flit);
    }
}

}
