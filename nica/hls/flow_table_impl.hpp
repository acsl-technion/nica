/* * Copyright (c) 2016-2018 Haggai Eran, Gabi Malka, Lior Zeno, Maroun Tork
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

#include <ap_int.h>
#include <hls_stream.h>
#include <iostream>
#include "gateway.hpp"

#include "flow_table.hpp"
#include "ikernel.hpp"
#include <ntl/cache.hpp>

namespace udp {
    struct header_parser;
    struct header_buffer;
    typedef hls::stream<header_buffer> header_stream;
}

struct flow {
    ap_uint<16> source_port;
    ap_uint<16> dest_port;
    ap_uint<32> saddr;
    ap_uint<32> daddr;
    hls_ik::vm_id_t vm_id;

    flow(ap_uint<16> source_port = 0, ap_uint<16> dest_port = 0,
         ap_uint<32> saddr = 0, ap_uint<32> daddr = 0, hls_ik::vm_id_t vm_id = 0);
    static flow from_header(const udp::header_parser& hdr, hls_ik::vm_id_t vm_id = 0);

    static flow mask(int fields);
    flow operator& (const flow& mask) const;

    bool operator== (const flow& other) const
    {
        return source_port == other.source_port && dest_port == other.dest_port &&
               saddr == other.saddr && daddr == other.daddr && vm_id == other.vm_id;
    }

    bool operator!= (const flow& other) const
    {
        return source_port != other.source_port || dest_port != other.dest_port ||
               saddr != other.saddr || daddr != other.daddr || vm_id != other.vm_id;
    }

    flow& operator&= (const flow& other)
    {
        *this = *this & other;
        return *this;
    }
};

namespace ntl {
    template <>
    struct pack<flow> {
        static const int width = 16 * 2 + 32 * 2 + hls_ik::vm_id_t::width;

        static ap_uint<width> to_int(const flow& e) {
            return (e.source_port, e.dest_port, e.saddr, e.daddr, e.vm_id);
        }

        static flow from_int(const ap_uint<width>& d) {
            auto e = flow(
                d(hls_ik::vm_id_t::width + 96 - 1, hls_ik::vm_id_t::width + 80),
                d(hls_ik::vm_id_t::width + 80 - 1, hls_ik::vm_id_t::width + 64),
                d(hls_ik::vm_id_t::width + 64 - 1, hls_ik::vm_id_t::width + 32),
                d(hls_ik::vm_id_t::width + 32 - 1, hls_ik::vm_id_t::width),
                d(hls_ik::vm_id_t::width      - 1, 0)
            );
            return e;
        }
    };
}

std::size_t hash_value(flow f);
::std::ostream& operator<<(::std::ostream& out, const flow& v);

struct flow_table_value : public boost::equality_comparable<flow_table_value> {
    flow_table_action action;
    hls_ik::engine_id_t engine_id;
    hls_ik::ikernel_id_t ikernel_id;

    explicit flow_table_value(flow_table_action action = FT_PASSTHROUGH,
        hls_ik::engine_id_t engine = 0, hls_ik::ikernel_id_t ikernel_id = 0) :
        action(action), engine_id(engine), ikernel_id(ikernel_id)
    {}

    bool operator== (const flow_table_value& o) const
    {
        return action == o.action && engine_id == o.engine_id && ikernel_id == o.ikernel_id;
    }
};

namespace ntl {
    template <>
    struct pack<flow_table_value> {
        static const int width = 2 + pack<hls_ik::engine_id_t>::width + pack<hls_ik::ikernel_id_t>::width;

        typedef flow_table_value type;

        static ap_uint<width> to_int(const type& e) {
            return (ap_uint<2>(e.action),
                    e.engine_id,
                    e.ikernel_id);
        }

        static type from_int(const ap_uint<width>& d) {
            auto e = flow_table_value(
                flow_table_action(int(d(width - 1, width - 2))),
                d(width - 3, width - 2 - pack<hls_ik::engine_id_t>::width),
                d(pack<hls_ik::ikernel_id_t>::width - 1, 0)
            );
            return e;
        }
    };
}

::std::ostream& operator<<(::std::ostream& out, const flow_table_value& v);

struct flow_table_result {
    flow_table_value v;
    hls_ik::flow_id_t flow_id;

    explicit flow_table_result(hls_ik::flow_id_t flow_id = 0, const flow_table_value& v = flow_table_value()) :
        v(v), flow_id(flow_id)
    {}
};

typedef hls::stream<flow_table_result> result_stream;

typedef ntl::hash_table_wrapper<flow, flow_table_value, FLOW_TABLE_SIZE> hash_flow_table_t;

class flow_table {
public:
    flow_table() { reset(); }
    void ft_step(udp::header_stream& header, result_stream& result,
                 hls_ik::gateway_registers& gateway);

    int reg_write(int address, int value);
    int reg_read(int address, int* value);
    void gateway_update();
    void reset();

private:
    void ft_wrapper(udp::header_stream& header, result_stream& result,
                    hls_ik::gateway_registers& gateway);

    bool reset_done;
    hash_flow_table_t hash_flow_table;
    flow gateway_flow;
    flow_table_value gateway_result;
    bool gateway_valid;
    int fields;

    ntl::gateway_impl<int> gateway;
};

