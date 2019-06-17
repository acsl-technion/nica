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

#ifndef UDP_H
#define UDP_H

#include <boost/preprocessor/repetition/enum_params.hpp>

#include <linux/if_ether.h>
#include <linux/ip.h>
#include <netinet/in.h>
#include <linux/udp.h>

#include <stdint.h>
#include <hls_stream.h>
#include <ap_int.h>

#include <mlx.h>
#include <hls_helper.h>
#include <link_with_reg.hpp>
#include <ntl/push_header.hpp>
#include <ikernel.hpp>

#include "flow_table_impl.hpp"

#ifndef NDEBUG
  #define DBG_DECL(decl...) decl
#else
  #define DBG_DECL(decl...)
  #undef assert
  #define assert(cond)
#endif

#ifndef NUM_IKERNELS
#error Expecting NUM_IKERNELS to be defined via external script
#endif

/* Iterate over all ikernel inputs. See:
 * http://www.boost.org/doc/libs/1_63_0/libs/preprocessor/doc/index.html */
#define DECL_IKERNEL_PARAMS() BOOST_PP_ENUM_PARAMS(NUM_IKERNELS, hls_ik::ports& ik)
#define IKERNEL_PARAMS() BOOST_PP_ENUM_PARAMS(NUM_IKERNELS, ik)

namespace udp {
	struct eth_header {
		ap_uint<48> dest;
		ap_uint<48> source;
		ap_uint<16> proto; /* host endianess */

		static const int width = 112;

		eth_header(ap_uint<width> d) :
            dest(d(111, 64)),
            source(d(63, 16)),
            proto(d(15, 0))
		{}

		operator ap_uint<width>()
		{
			return (dest, source, proto);
		}
	};

	struct ip_header {
		ap_uint<4> version;
		ap_uint<4> ihl;
		ap_uint<8> tos;
		ap_uint<16> tot_len;
		ap_uint<16> id;
		ap_uint<16> frag_off;
		ap_uint<8> ttl;
		ap_uint<8> protocol;
		ap_uint<16> check;
		ap_uint<32> saddr;
		ap_uint<32> daddr;

		static const int width = 160;

		ip_header(ap_uint<width> d) :
			version	(d(159, 156)),
			ihl		(d(155, 152)),
			tos		(d(151, 144)),
			tot_len	(d(143, 128)),
			id		(d(127, 112)),
			frag_off(d(111,  96)),
			ttl		(d( 95,  88)),
			protocol(d( 87,  80)),
			check	(d( 79,  64)),
			saddr	(d( 63,  32)),
			daddr	(d( 31,   0))
		{}

		operator ap_uint<width>() {
			return (version, ihl, tos, tot_len, id, frag_off, ttl, protocol,
				    check, saddr, daddr);
		}
	};

	struct udp_header {
		ap_uint<16> source;
		ap_uint<16> dest;
		ap_uint<16> length;
		ap_uint<16> checksum;

		static const int width = 64;
		udp_header(ap_uint<width> d) :
			source	(d(63, 48)),
			dest	(d(47, 32)),
			length	(d(31, 16)),
			checksum(d(15, 0))
		{}

		operator ap_uint<width>() {
			return (source, dest, length, checksum);
		}

        bool empty_packet() const { return length <= width / 8; }
	};

	struct checksum_t {
		ap_uint<16> ip_checksum;
		ap_uint<16> udp_checksum;
	};

    struct header_buffer;

	struct header_parser {
		eth_header eth;
		ip_header ip;
		udp_header udp;

		static const int width = eth_header::width + ip_header::width + udp_header::width;

		header_parser(ap_uint<width> d = 0) :
			eth(d(width - 1, 160 + 64)),
			ip(d(160 + 64 - 1, 64)),
			udp(d(63, 0))
		{}

		header_parser(const header_buffer& buf);

		operator ap_uint<width>() {
			return (ap_uint<eth_header::width>(eth),
				    ap_uint<ip_header::width>(ip),
					ap_uint<udp_header::width>(udp));
		}

        operator header_buffer();

		header_parser reply() const;

		checksum_t checksum_from_packet();
		ap_uint<16> udp_pseudoheader_checksum();
	};

    struct header_buffer {
        ap_uint<header_parser::width> hdr;
        static const int width = header_parser::width;
        mlx::pkt_id_t pkt_id;
        mlx::user_t user;
	bool drop; /* Mark the packet to be dropped */
        bool generated; /* Mark generated packets */
    };

#define HEADER_BUFFER(__name, __hdr, __pkt_id, __user, __generated) \
    header_buffer __name; \
    __name.hdr = __hdr; \
    __name.pkt_id = __pkt_id; \
    __name.user = __user; \
    __name.generated = __generated;

    typedef hls::stream<header_buffer> header_stream;

	typedef hls::stream<bool > bool_stream;

	#define HEADER_SIZE (sizeof(header_buffer))
	#define HEADER_SIZE_BITS (HEADER_SIZE * 8)

    struct config {
        bool enable;
        /** Gateway to access flow table */
        hls_ik::gateway_registers flow_table_gateway;
        /** Gateway to access the arbiter */
        hls_ik::gateway_registers arbiter_gateway;
    };

    struct config_h2n {
        config common;
    };

    struct config_n2h {
        config common;
        /** Gateway to access custom ring parameters */
        hls_ik::gateway_registers custom_ring_gateway;
    };

    typedef ap_uint<32> packet_counters;

    struct hds_stats {
        packet_counters passthrough_disabled;
        packet_counters passthrough_not_ipv4;
        packet_counters passthrough_bad_length;
        packet_counters passthrough_not_udp;
        packet_counters ft_action_passthrough,
                        ft_action_drop,
                        ft_action_ikernel;
    };

    class udp_dropper {
    public:
        udp_dropper(bool empty_packets_have_data) :
            empty_packets_have_data(empty_packets_have_data),
            data_buf_valid(false) {}
        void udp_dropper_step(bool_stream& pass,
                              header_stream& header_in, hls_ik::data_stream& data_in,
                              header_stream& header_out, hls_ik::data_stream& data_out);
	private:
		enum { IDLE, STREAM } state;
		bool drop;
        /** For a packet that should have no data according to its header word,
         * should we consume a word from the data stream? */
        const bool empty_packets_have_data;
        bool data_buf_valid;
        hls_ik::axi_data data_buf;
        DBG_DECL(mlx::pkt_id_t pkt_id);
	};

    class header_data_split
    {
    public:
        header_data_split();
        void step(mlx::stream& in, header_stream& header, hls_ik::data_stream& data);
        void split(header_stream& header, hls_ik::data_stream& data);

    private:
        enum { IDLE, READING_HEADER, STREAM, LAST } state;
        mlx::metadata meta;
        ap_uint<MLX_AXI4_WIDTH_BITS> buffer;
        mlx::extract_metadata extract_metadata;
    };

    /* Distinguish between UDP packets and non-UDP, and for UDP packets, splits
     * the data and the header. also matches the UDP flow */
    class steering
    {
    public:
        steering();
        void steer(header_stream& hdr_in, hls_ik::data_stream& data_in, bool_stream& pass_raw,
                   header_stream& hdr_out, hls_ik::data_stream& data_out,
                   result_stream& result_out,
                   config* config, hds_stats* s);
    private:
        void hdr_checks(const config& config);
        void checks_to_action(const config& config, result_stream& result_out);
        void update_stats_checks(hds_stats* s);
        void update_stats_actions(bool_stream& pass_raw, hds_stats* s);

        hds_stats stats;

        struct checks {
            bool disabled;
            bool not_ipv4;
            bool bad_length;
            bool not_udp;
            flow_table_result ft_result;
        };

        hls::stream<checks> checks_to_stats, checks_to_actions;
        header_stream hdr_dup_to_dropper, hdr_dup_to_checks, hdr_dup_to_flow_table;
        bool_stream matched;

        udp_dropper dropper;
        hls_helpers::duplicator<2, header_buffer> hdr_dup;
        result_stream ft_to_action, ft_results;
        flow_table ft;
    };

    class length_adjust
    {
	public:
		length_adjust();
		void adjust(header_stream& hdr_in, hls_ik::data_stream& data_in, hls_ik::data_stream& data_out);
	private:
        /** Read a packet header and retrieve the needed data lengths
         * from the IP header. */
        void find_length(header_stream& hdr_in);
        /** Cut the data stream according to lengths in the packets stream. */
        void cut_data(hls_ik::data_stream& data_in, hls_ik::data_stream& data_out);
		enum { IDLE, COUNT, CONSUME } state;
        bool data_buf_valid;
        hls_ik::axi_data data_buf;
        struct packet_metadata {
            ap_uint<11> word_count;
            ap_uint<16> tot_len;
            ap_uint<5> last_word_data;
            mlx::pkt_id_t pkt_id;
        };
        hls::stream<packet_metadata> packets;
        packet_metadata pkt;
    };

	class checksum {
	public:
        checksum() : state(IDLE) {}
		typedef hls::stream<checksum_t> stream;

		void checksum_step(header_stream& hdr_in, hls_ik::data_stream& data_in,
				  stream& checksum);

	private:
        /* For timing, split calculation into multiple aggregators. Only sum them in
         * an extra unit after every packet */
        static const int num_splits = 8;
        static const int ip_splits = 2;

        struct intermediate_checksum {
            ap_uint<32> ip_checksum[ip_splits];
            ap_uint<32> udp_checksum[num_splits];
        };

		void checksum_header_and_data(header_stream& hdr_in, hls_ik::data_stream& data_in);
		void finish_checksum(stream& checksum);

        /* First part of the IPv4 checksum */
        void ipv4_checksum(ip_header hdr);
        /* Pseudo header calculation */
        void udp_pseudoheader_checksum(const header_parser& hdr);

		enum { IDLE, DATA } state;
        DBG_DECL(mlx::pkt_id_t pkt_id);
		intermediate_checksum cur_checksum;
        hls::stream<intermediate_checksum> intermediate_stream;
	};

	class checksum_validate {
	public:
        typedef checksum_t first_argument_type;
        typedef header_buffer second_argument_type;
        typedef bool result_type;
		bool operator()(checksum_t sum, header_buffer buf);
	};
	typedef hls_helpers::binary_stream<checksum_validate> checksum_validator;

    struct udp_stats {
        hds_stats hds;
    };

	/* A basic UDP unit for parsing Ethernet, IP and UDP headers, detecting
	   flows and splitting the headers. */
	class udp {
	public:
        udp();
        void udp_step(mlx::stream& in,
                      BOOST_PP_ENUM_PARAMS(NUM_IKERNELS, header_stream& header_out),
                      BOOST_PP_ENUM_PARAMS(NUM_IKERNELS, result_stream& ft_results),
                      BOOST_PP_ENUM_PARAMS(NUM_IKERNELS, hls_ik::data_stream& data_out),
                      bool_stream& bool_pass_raw,
                      config* config, udp_stats* stats);
	private:
		header_data_split hds;
		steering steer;
		length_adjust length_adjuster;
		checksum checksum_calculator;
		checksum_validator validator;
		hls_helpers::duplicator<2, header_buffer> hdr_dup;
		udp_dropper udp_dropper_instance;

		/* Crossbar state */
		enum { IDLE, STREAM } state;
        flow_table_result current_steering_decision;
        void crossbar(header_stream& hdr_in, hls_ik::data_stream& data_in, result_stream& steer_results,
            BOOST_PP_ENUM_PARAMS(NUM_IKERNELS, header_stream& hdr_out),
            BOOST_PP_ENUM_PARAMS(NUM_IKERNELS, result_stream& ft_results),
            BOOST_PP_ENUM_PARAMS(NUM_IKERNELS, hls_ik::data_stream& data_out));

        hls_ik::data_stream data_split_to_steer, data_steer_to_length, data_length_to_crossbar;
        header_stream header_split_to_steer, header_steer_to_dup, header_dup_to_length,
            header_dup_to_crossbar;
        result_stream steer_results;
	};

        typedef hls_ik::metadata udp_builder_metadata;

    typedef hls::stream<ap_uint<udp_builder_metadata::width> > udp_builder_metadata_stream;

    /* Take a header stream and generate two-beat packets with the header only */
    class header_to_mlx
    {
    public:
        void hdr_to_mlx(udp_builder_metadata_stream& in, hls_ik::data_stream& out,
                        bool_stream& empty_packet,
                        mlx::metadata_stream& metadata_out,
			bool_stream& enable_stream);
    protected:
	static header_parser metadata_to_header(const hls_ik::metadata& m);
        enum { buffer_size = header_parser::width - MLX_AXI4_WIDTH_BITS };
        /** Buffer for leftovers from the first header word. */
        ap_uint<buffer_size> buffer;

        /** Reordering state:
         *  IDLE   waiting for header stream entry.
         *  SECOND sending the second output word
         */
        enum { IDLE, SECOND } state;
    };

    /* Pad Ethernet packets to a minimum of 60 bytes */
    class ethernet_padding {
    public:
        ethernet_padding();
        void pad(mlx::stream& in, mlx::stream& out);

    protected:
        ap_uint<256> pad_one(ap_uint<256> data, ap_uint<32> keep);

        ap_uint<6> beat_count;
    };

    /* Build the AXI4 Stream of UDP packets back from the split header and data
       fifos */
    class udp_builder
    {
    public:
        udp_builder();
        /** Builds packets from split header and data streams, and re-calculates
         * the checksum. */
        void builder_step(udp_builder_metadata_stream& header_in, hls_ik::data_stream& data_in,
                          mlx::stream& out);
    private:
        void out_word(mlx::stream& out, mlx::stream& generated_out, mlx::axi4s word);
        bool full_output(mlx::stream& out, mlx::stream& generated_out);

        mlx::stream raw_reorder_to_reg;
        mlx::metadata_stream mlx_metadata;
        hls_ik::data_stream data_hdr_to_reorder, data_reorder_to_join;
        bool_stream empty_packet, enable_stream;
        header_to_mlx hdr2mlx;
        ntl::push_header<header_parser::width> merger;
        mlx::join_packet_metadata join_pkt_metadata;
        link_with_reg<mlx::axi4s, false> link;
    };
}

#endif // UDP_H
