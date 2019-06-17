/* * Copyright (c) 2016-2017 Haggai Eran, Gabi Malka, Lior Zeno, Maroun Tork
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 *  * Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation and/or
 * other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <ikernel.hpp>
#include <mlx.h>

/** Merges a header and a payload into a single mlx stream, given the mlx
 * streams of the header and the payload. */
template <unsigned header_length_bits>
class push_header
{
public:
    /* TODO use data_stream everywhere instead of relying on mlx::stream */
    void reorder(hls_ik::data_stream& hdr_in, hls::stream<bool>& empty_packet,
		 hls::stream<bool>& enable_stream,
                 hls_ik::data_stream& data_in, hls_ik::data_stream& out)
    {
#pragma HLS pipeline enable_flush
        switch (state)
        {
        case IDLE: {
            if (empty_packet.empty() || enable_stream.empty())
                break;

            empty = empty_packet.read();
	    bool enable = enable_stream.read();
	    if (enable) {
                state = HEADER;
	    } else if (empty) {
                state = IDLE;
                break;
            } else /* not empty */ {
                state = NO_HEADER;
	    }
            break;
        }
        case HEADER: {
header:
            if (hdr_in.empty() || out.full())
                break;

            hls_ik::axi_data cur = hdr_in.read();
	    if (!cur.last) {
	        assert(cur.keep == 0xffffffff);
		out.write_nb(cur);
		break;
	    }
            assert(empty || cur.keep == ~((1U << (32 - buffer_size / 8)) - 1));
            buffer = cur.data(MLX_AXI4_WIDTH_BITS - 1, MLX_AXI4_WIDTH_BITS - buffer_size);
            last_word_keep = cur.keep >> ((MLX_AXI4_WIDTH_BITS - buffer_size) / 8);
            if (empty) {
                state = IDLE;
                out.write(cur);
		break;
            } else {
                state = DATA;
                /* Passthrough */
            }
        }
        case DATA: {
            if (data_in.empty() || out.full())
                break;

            hls_ik::axi_data word = data_in.read();

            ap_uint<256> out_data((buffer, word.data(word.data.width - 1, buffer_size)));
            last_word_keep = word.keep;

            /* Check if the amount of new bytes in the input word is larger
             * than the available after writing out the buffer.
             * Another way to look at it is that the amount of padding bytes
             * in the input word is smaller than size of the buffer, and we
             * check that by testing the bit of the buffer size in the keep
             * signal. */
            const int buffer_width_bit = buffer_size / 8 - 1;
            if (word.keep(buffer_width_bit, buffer_width_bit)) {
                auto out_buf = hls_ik::axi_data(out_data, 0xffffffff, false);
                out.write_nb(out_buf);
                state = word.last ? LAST : DATA;
                buffer = word.data(buffer_size - 1, 0);
            } else {
                auto out_buf = hls_ik::axi_data(
                        out_data,
                        last_word_keep >> (buffer_size / 8) |
                            mlx::last_word_keep_num_bytes_valid(buffer_size / 8),
                        true);
                out.write_nb(out_buf);
                state = IDLE;
            }
            break;
        }
        case LAST: {
            auto out_buf = hls_ik::axi_data((buffer, ap_uint<MLX_AXI4_WIDTH_BITS - buffer_size>(0)),
                last_word_keep << ((mlx::word::width - buffer_size) / 8), true);
            if (out.write_nb(out_buf))
                state = IDLE;
	    break;
	}
	case NO_HEADER:
no_header:
            if (data_in.empty() || out.full())
                break;

            hls_ik::axi_data word = data_in.read();
	    if (word.last)
		state = IDLE;
	    out.write_nb(word);
	    break;
        }
    }

protected:
    /** Empty packets do not expect data on data_in */
    bool empty;
    /** Reordering state:
        IDLE   waiting for header stream entry.
        HEADER reading the second part of the header
        DATA   reading from data stream, and combining with buffer.
        LAST   output the contents of the buffer after the last incoming
               data word.
    	NO_HEADER passing data stream as is to the output, when header push
	          was not enabled */
    enum { IDLE, HEADER, DATA, LAST, NO_HEADER } state;
    enum { buffer_size = header_length_bits % MLX_AXI4_WIDTH_BITS };
    /** Buffer for leftovers from the first header word. */
    ap_uint<buffer_size> buffer;
    ap_uint<32> last_word_keep;
};

