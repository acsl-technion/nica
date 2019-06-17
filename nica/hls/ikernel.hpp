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

#ifndef IKERNEL_HPP
#define IKERNEL_HPP

#include <hls_stream.h>
#include <ap_int.h>
#include <uuid.h>
#include <boost/operators.hpp>

#include "gateway.hpp"
#include "ikernel-types.hpp"
#include "hls_macros.hpp"
#include "axi_data.hpp"
#include <either.hpp>

#include <cassert>

namespace hls_ik {

struct ikernel_id {
    uuid_t uuid;
};

inline bool operator==(const ikernel_id& a, const ikernel_id& b)
{
    return uuid_compare(a.uuid, b.uuid) == 0;
}

/* Packet headers metadata */
struct packet_metadata : public boost::equality_comparable<packet_metadata> {
    /* Ethernet destination MAC address */
    ap_uint<48> eth_dst;
    /* Ethernet source MAC address */
    ap_uint<48> eth_src;
    /* Destination IP address */
    ap_uint<32> ip_dst;
    /* Source IP address */
    ap_uint<32> ip_src;
    /* Destination UDP port */
    ap_uint<16> udp_dst;
    /* Source UDP port */
    ap_uint<16> udp_src;

    bool operator ==(const packet_metadata& o) const {
        return eth_dst  == o.eth_dst  &&
               eth_src  == o.eth_src  &&
               ip_dst   == o.ip_dst   &&
               ip_src   == o.ip_src   &&
               udp_dst  == o.udp_dst  &&
               udp_src  == o.udp_src;
    }

    static const int width =
        48 +
        48 +
        32 +
        32 +
        16 +
        16;

    packet_metadata(const ap_uint<width> d = 0) :
        eth_dst(d(191, 144)),
        eth_src(d(143, 96)),
        ip_dst(d(95, 64)),
        ip_src(d(63, 32)),
        udp_dst(d(31, 16)),
        udp_src(d(15, 0))
    {}

    operator ap_uint<width>() const {
        return (eth_dst, eth_src, ip_dst, ip_src, udp_dst,
                udp_src);
    }

    packet_metadata reply() const {
        packet_metadata m = *this;

        m.eth_dst = eth_src;
        m.eth_src = eth_dst;
        m.ip_dst = ip_src;
        m.ip_src = ip_dst;
        m.udp_dst = udp_src;
        m.udp_src = udp_dst;

        return m;
    }
};

/* Metadata accompanying custom-ring data packets. */
struct custom_ring_metadata : public
			      boost::equality_comparable<custom_ring_metadata> {
    /* End of message bit. Can be used to create large messages that are
     * comprised of multiple packets. At the moment must be set to 1. */
    ap_uint<1> end_of_message;

    bool operator ==(const custom_ring_metadata& o) const {
        return end_of_message == o.end_of_message;
    }

    static const int width = 1;

    custom_ring_metadata(const ap_uint<width> d = 0) :
        end_of_message(d(0, 0))
    {}

    operator ap_uint<width>() const {
        return end_of_message;
    }
};

// TODO maybe unite this with the ring_id != 0 convention
typedef ap_uint<1> pkt_type_t;
#define PKT_TYPE_UDP (0)
#define PKT_TYPE_RAW (1)

/* Metadata accompanying each packet coming in or going out of an ikernel */
struct metadata : public boost::equality_comparable<metadata>
{
    metadata(const metadata& o) :
        pkt_type(o.pkt_type),
        flow_id(o.flow_id),
        ikernel_id(o.ikernel_id),
        var(o.var),
        ring_id(o.ring_id),
        ip_identification(o.ip_identification),
        length(o.length)
    {
        verify();
    }

    ~metadata() {}

    /* From flow table */
    pkt_type_t pkt_type;
    flow_id_t flow_id;
    ikernel_id_t ikernel_id;

    /* A union of packet metadata (describing UDP/IP packet headers) and custom
     * ring metadata (containing the end-of-message bit). */
    typedef either<packet_metadata, custom_ring_metadata> either_t;
    either_t var;

    /* Common fields to both custom ring and network stack. */

    /* The ID of the custom ring to be used for the associated data packet. A
     * ring_id of zero means passing an ordinary UDP packet to the network stack.
     */
    ring_id_t ring_id;
    /* The value to be set in the IP header identification field. */
    ap_uint<16> ip_identification;
    /* The associated data packet length. */
    ap_uint<16> length;

    bool operator ==(const metadata& o) const {
        return ring_id == o.ring_id && var == o.var &&
            length == o.length &&
            ikernel_id == o.ikernel_id &&
            flow_id == o.flow_id &&
            pkt_type == o.pkt_type;
    }

    metadata& operator=(const metadata& o) {
        ring_id = o.ring_id;
        ip_identification = o.ip_identification;
        length = o.length;
        var = o.var;
        flow_id = o.flow_id;
        ikernel_id = o.ikernel_id;
        pkt_type = o.pkt_type;

        return *this;
    }

    static const int width = either_t::width + ring_id_t::width + 16 + 16 + ikernel_id_t::width + flow_id_t::width + pkt_type_t::width;

    metadata(const ap_uint<width> d = 0) :
        pkt_type(d(pkt_type_t::width + flow_id_t::width + ikernel_id_t::width + either_t::width + 31 + ring_id_t::width, flow_id_t::width + ikernel_id_t::width + either_t::width + 32 + ring_id_t::width)),
        flow_id(d(flow_id_t::width + ikernel_id_t::width + either_t::width + 31 + ring_id_t::width, ikernel_id_t::width + either_t::width + 32 + ring_id_t::width)),
        ikernel_id(d(ikernel_id_t::width + either_t::width + 31 + ring_id_t::width, either_t::width + 32 + ring_id_t::width)),
        var(d(either_t::width + 31 + ring_id_t::width, 32 + ring_id_t::width)),
        ring_id(d(32 + ring_id_t::width - 1, 32)),
        ip_identification(d(31, 16)),
        length(d(15, 0))
    {}

    operator ap_uint<width>() const {
        return (pkt_type, flow_id, ikernel_id, static_cast<ap_uint<either_t::width> >(var), ring_id,
                ip_identification, length);
    }

    packet_metadata get_packet_metadata() const {
        return var.get<packet_metadata>();
    }

    custom_ring_metadata get_custom_ring_metadata() const {
        return var.get<custom_ring_metadata>();
    }

    void set_packet_metadata(const packet_metadata& x) {
        var = x;
    }

    void set_custom_ring_metadata(const custom_ring_metadata& x) {
        var = x;
    }

    metadata reply(const short length) const {
        metadata m = *this;

        if (ring_id == 0)
            m.var = var.get<packet_metadata>().reply();
        m.length = length;

        return m;
    }

    void verify()
    {
        if (ring_id == 0)
            assert(var.is_left());
        else
            assert(!var.is_left());
        assert(*this == metadata(ap_uint<width>(*this)));
    }

    bool empty_packet() const { return length == 0; }
};


std::ostream& operator<<(::std::ostream& out, const metadata& m);

typedef hls::stream<ap_uint<metadata::width> > metadata_stream;

typedef ap_uint<10> tc_ports_data_counts[NUM_TC];

struct pipeline_ports {
    metadata_stream metadata_input;
    data_stream data_input;

    metadata_stream metadata_output;
    data_stream data_output;

    tc_ports_data_counts tc_meta_counts;
    tc_ports_data_counts tc_data_counts;
};

typedef ap_uint<16> msn_t;

struct credit_update_registers : public boost::equality_comparable<credit_update_registers>
{
    credit_update_registers() : ring_id(0), max_msn(0), reset(0) {}
    hls_ik::ring_id_t ring_id;
    msn_t max_msn;
    ap_uint<1> reset;
};

static inline bool operator==(const credit_update_registers& l, const credit_update_registers& r)
{
    return l.ring_id == r.ring_id && l.max_msn == r.max_msn && l.reset == r.reset;
}

#if __SYNTHESIS__ && !SIMULATION_BUILD
/** Log size of memory in bytes. */
#  define DDR_WIDTH 31 /* 2 GB */
#else
/** Log size of memory in bytes. For testing we limit the memory
 * consumption. */
#  define DDR_WIDTH 7 /* 2 entries */
#endif
#define DDR_SIZE (1ULL << DDR_WIDTH)
/* Interface width also includes the ikernel ID */
#define DDR_INTERFACE_WIDTH (10 + DDR_WIDTH)

template <size_t _interface_width>
class memory {
public:
    enum {
        interface_width = _interface_width,
        array_size = 1ull << (interface_width - 6),
    };
    ap_uint<512> mem[array_size];
};

typedef memory<DDR_INTERFACE_WIDTH> memory_t;

#define IKERNEL_NUM_EVENTS 8
typedef ap_uint<1> trace_event;

struct ports {
    pipeline_ports host, net;
    credit_update_registers host_credit_regs;

    memory_t mem;

    /* Custom ikernel events to be traced by the NICA tracing infrastructure */
    trace_event events[IKERNEL_NUM_EVENTS];

    ports() {}
};

struct ikernel_ring_context
{
    msn_t msn;
    msn_t max_msn;

    bool can_transmit() const
    {
        return msn != max_msn;
    }

    void inc_msn() { ++msn; }
};

class ikernel {
public:
    ikernel() {}
    virtual ~ikernel() {}

    virtual void step(ports& ports) = 0;

    /* For simulation: accessor functions to AXI4-Lite registers. */
    virtual int reg_write(int address, int value, ikernel_id_t ikernel_id)
    {
        switch (address) {
        case 0:
            break;
        default:
            return GW_FAIL;
        }

        return GW_DONE;
    }

    virtual int reg_read(int address, int* value, ikernel_id_t ikernel_id)
    {
        switch (address) {
        case 0:
            *value = 1;
            break;
        default:
            *value = 0;
            return GW_FAIL;
        }

        return GW_DONE;
    }

    /** Update all gateway related registers in here.
     *
     * Otherwise HLS is not happy (dataflow won't work). */
    virtual void gateway_update() {}

    /* Called from the top function to queue credit updates */
    void host_credits_update(credit_update_registers& regs);
protected:
    /* Helper functions to be used by an ikernel to implement flow control for
     * the custom ring */

    /* Check whether a specific ikernel, ring, and direction (HOST / NET) is
     * allowed to transmit a data packet of a given length. */
    bool can_transmit(pipeline_ports& p, ikernel_id_t id, ring_id_t ring, pkt_len_t len, direction_t dir);

    /* Call before transmitting a new message through the given custom ring ID
     * and direction. The number of calls to this function should match the number
     * of times the end-of-message bit was set in a metadata flit. */
    void new_message(ring_id_t ring, direction_t dir);

    /* Call from the same function that calls can_transmit() in order to
     * implement the AXI-Lite interface for credit updates.
     *
     * Returns true if the per-ring or per-ikernel array was accessed. In that
     * case the new_message() method shouldn't be called on the same invocation
     */
    bool update();
private:
    ikernel_ring_context host_credits[1 << CUSTOM_RINGS_LOG_NUM]; // TODO magic number
    hls::stream<credit_update_registers> credit_updates;
    credit_update_registers last_host_credit_regs;
};

void pass_packets(pipeline_ports& p);

void init(hls_ik::ports& p);

static inline void link_pipeline_sim(hls_ik::pipeline_ports& in, hls_ik::pipeline_ports& out)
{
#pragma HLS inline
    hls_helpers::link_axi_to_fifo(in.metadata_input, out.metadata_input);
    hls_helpers::link_axi_to_fifo(in.data_input, out.data_input);
    hls_helpers::link_axi_stream(out.metadata_output, in.metadata_output);
    hls_helpers::link_axi_stream(out.data_output, in.data_output);

    for (int i = 0; i < NUM_TC; ++i) {
         out.tc_meta_counts[i] = in.tc_meta_counts[i];
         out.tc_data_counts[i] = in.tc_data_counts[i];
    }
}

static inline void link_ports_sim(hls_ik::ports& in, hls_ik::ports& out)
{
#pragma HLS inline
    link_pipeline_sim(in.host, out.host);
    link_pipeline_sim(in.net, out.net);
    out.host_credit_regs = in.host_credit_regs;
}

} // namespace

#define DECLARE_TOP_FUNCTION(__name) void __name(\
    hls_ik::ports& ik, \
    hls_ik::ikernel_id& uuid, \
    hls_ik::virt_gateway_registers& gateway)

/* RTL/C co-simulation doesn't work well with certain definitions, so make them
 * conditional */
#ifdef SIMULATION_BUILD
#  define _PragmaSyn(x)
#  define DO_PRAGMA_SYN(x)
#  define IF_SIM(x, y) x
#else
#  define _PragmaSyn(x) _Pragma(x)
#  define DO_PRAGMA_SYN(x) DO_PRAGMA(x)
#  define IF_SIM(x, y) y
#endif

#define TC_COUNTS_PRAGMAS(__tc_counts) \
    DO_PRAGMA(HLS interface ap_none port=__tc_counts) \
    DO_PRAGMA(HLS array_reshape variable=__tc_counts complete)


#define IKERNEL_PIPELINE_PORTS_PRAGMAS(__pipeline) \
    DO_PRAGMA(HLS interface axis port=__pipeline.metadata_input) \
    DO_PRAGMA(HLS interface axis port=__pipeline.metadata_output) \
    DO_PRAGMA(HLS interface axis port=__pipeline.data_input) \
    DO_PRAGMA(HLS interface axis port=__pipeline.data_output) \
    TC_COUNTS_PRAGMAS(__pipeline.tc_data_counts) \
    TC_COUNTS_PRAGMAS(__pipeline.tc_meta_counts)

#define IKERNEL_CREDIT_REGS_PRAGMAS(__credit_regs, __offset) \
    DO_PRAGMA_SYN(HLS data_pack variable=__credit_regs) \
    DO_PRAGMA_SYN(HLS interface s_axilite port=__credit_regs offset=__offset)

#define IKERNEL_PORTS_PRAGMAS(__ports) \
    IKERNEL_PIPELINE_PORTS_PRAGMAS(__ports.net) \
    IKERNEL_PIPELINE_PORTS_PRAGMAS(__ports.host) \
    IKERNEL_CREDIT_REGS_PRAGMAS(__ports.host_credit_regs, 0x1050) \
    DO_PRAGMA_SYN(HLS interface m_axi port=__ports.mem.mem latency=33 depth=40 \
        num_write_outstanding=40 num_read_outstanding=40) \
    DO_PRAGMA(HLS array_partition variable=__ports.events complete) \
    DO_PRAGMA(HLS interface ap_none port=__ports.events)

#define INSTANCE(__class) __class
#define DEFINE_TOP_FUNCTION(__name, __class, __uuid) \
static __class INSTANCE(__class); \
DECLARE_TOP_FUNCTION(__name) \
{ \
    _Pragma("HLS dataflow") \
    _Pragma("HLS ARRAY_RESHAPE variable=uuid.uuid complete dim=1") \
    IKERNEL_PORTS_PRAGMAS(ik) \
    /* TODO multiple kernels will need different AXI4-Lite offsets */ \
    _PragmaSyn("HLS interface s_axilite port=uuid offset=0x1000") \
    VIRT_GATEWAY_OFFSET(gateway, 0x1014, 0x101c, 0x102c, 0x1034) \
    _PragmaSyn("HLS interface ap_ctrl_none port=return") \
\
    static hls_ik::ports ports_buf; \
    IF_SIM(link_ports_sim(ik, ports_buf);,) \
    DO_PRAGMA_SIM(HLS stream variable=ports_buf.host.metadata_input depth=256); \
    DO_PRAGMA_SIM(HLS stream variable=ports_buf.host.data_input depth=256); \
    DO_PRAGMA_SIM(HLS stream variable=ports_buf.net.metadata_input depth=256); \
    DO_PRAGMA_SIM(HLS stream variable=ports_buf.net.data_input depth=256); \
    using namespace hls_ik; \
    static const ikernel_id __constant_uuid = { __uuid }; \
    INSTANCE(__class).host_credits_update(IF_SIM(ports_buf, ik).host_credit_regs); \
    INSTANCE(__class).step(IF_SIM(ports_buf, ik)); \
    INSTANCE(__class).gateway(&INSTANCE(__class), gateway); \
    output_uuid: uuid = __constant_uuid; \
}

#endif // IKERNEL_HPP
