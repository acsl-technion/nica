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

#ifndef MLX_H
#define MLX_H

#include <tuple>
#include <ap_int.h>
#include <hls_stream.h>
#include "hls_helper.h"
#include "axi_data.hpp"

#define MLX_AXI4_WIDTH_BITS 256
#define MLX_AXI4_WIDTH_BYTES (MLX_AXI4_WIDTH_BITS / 8)

#define MAX_PACKET_SIZE 1520
#define MAX_PACKET_WORDS 48 // MAX_PACKET_SIZE / MLX_AXI4_WIDTH_BYTES
#define FIFO_PACKETS 15 // A single SRL
#define FIFO_WORDS 511 // Utilize a BRAM

/* The number of packets to hold while waiting for the flow table results */
#define FIFO_FLOW_TABLE_PACKETS 5

namespace mlx {
    typedef ap_uint<MLX_AXI4_WIDTH_BITS> word;
    typedef ap_uint<12> user_t;
    typedef ap_uint<3> pkt_id_t;
    struct axi4s {
        word data;
        ap_uint<MLX_AXI4_WIDTH_BYTES> keep;
        ap_uint<1> last;
        /**
         * bit 0 - drop
         * bit 2 - lossy
         */
        user_t user;
        /** Must be 0 for generated packets, and must be preserved for other packets. */
        pkt_id_t id;

        axi4s(const word& data = word(0),
              const ap_uint<MLX_AXI4_WIDTH_BYTES>& keep = 0xffffffff,
              const ap_uint<1>& last = ap_uint<1>(0),
              const user_t& user = user_t(0),
              const pkt_id_t& id = pkt_id_t(0)) :
            data(data), keep(keep), last(last), user(user), id(id)
        {}

        axi4s(const hls_ik::axi_data& flit, 
              const user_t& user,
              const pkt_id_t& id) :
            data(flit.data), keep(flit.keep), last(flit.last), user(user), id(id)
        {}

        operator hls_ik::axi_data() { return hls_ik::axi_data(data, keep, last); }
    };

    enum {
        USER_DROP = 1,
        USER_LOSSY = 4,
    };

#define MLX_TUSER_PRESERVE (~(mlx::USER_DROP | mlx::USER_LOSSY))

    static inline ap_uint<MLX_AXI4_WIDTH_BYTES> last_word_keep_num_bytes_padding(ap_uint<5> padding)
	{
    	return 0xffffffff ^ ((1 << padding) - 1);
	}

    /* 0 means all are valid */
    static inline ap_uint<MLX_AXI4_WIDTH_BYTES> last_word_keep_num_bytes_valid(ap_uint<5> num_valid)
	{
		ap_uint<MLX_AXI4_WIDTH_BYTES> tmp = (1 << num_valid) - 1;
		/* Reverse the bits */
		return num_valid ? tmp(0, MLX_AXI4_WIDTH_BYTES - 1) : 0xffffffff;
	}

    static inline axi4s last_word(ap_uint<MLX_AXI4_WIDTH_BITS> data, ap_uint<5> padding)
    {
        axi4s word;
        word.data = data;
        word.keep = last_word_keep_num_bytes_padding(padding);
        word.last = 1;
        word.user = 0;
        word.id = 0;

        return word;
    }

    typedef hls::stream<axi4s> stream;

    class dropper
    {
    public:
        void step(stream& in, hls::stream<bool>& pass_stream, stream& out);
    private:
        bool drop;
        enum { IDLE, STREAM } state;
    };

    /* Per-packet metadata that does not need to go to the ikernel */
    struct metadata : public boost::equality_comparable<metadata> {
        /**
         * bit 0 - drop
         * bit 2 - lossy
         */
        ap_uint<12> user;
        /** Must be 0 for generated packets, and must be preserved for other packets. */
        ap_uint<3> id;

        metadata(ap_uint<12> user, ap_uint<3> id) : user(user), id(id) {}

        static const int width = 12 + 3;

        metadata(ap_uint<15> d = 0) :
            user(d(14, 3)),
            id(d(2, 0))
        {}

        operator ap_uint<width>() const {
            return (user, id);
        }

	void set_drop(bool drop)
        {
            if (drop)
                user |= USER_DROP;
            else
                user &= ~USER_DROP;
	}

	bool get_drop() const { return user & USER_DROP; }

        bool operator ==(const metadata& o) const {
            return user == o.user && id == o.id;
        }
    };

    std::ostream& operator<<(::std::ostream& out, const metadata& m);

    typedef hls::stream<metadata> metadata_stream;

    std::tuple<metadata, hls_ik::axi_data> split_metadata(const axi4s& in);
    axi4s join_metadata(const std::tuple<metadata, hls_ik::axi_data>& in);

    class join_packet_metadata
    {
    public:
        join_packet_metadata() : state(IDLE) {}
        void operator()(metadata_stream& meta_in, hls_ik::data_stream& data_in, stream& out);

    private:
        enum { IDLE, STREAM } state;
        metadata m;
    };
}

#endif // MLX_H
