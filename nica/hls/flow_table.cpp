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

#include "flow_table_impl.hpp"
#include "udp.h"

using ntl::maybe;
using ntl::make_maybe;

using udp::header_stream;
using namespace hls_ik;

using std::make_tuple;

flow::flow(ap_uint<16> source_port, ap_uint<16> dest_port, ap_uint<32> saddr, ap_uint<32> daddr, vm_id_t vm_id)
    : source_port(source_port), dest_port(dest_port), saddr(saddr), daddr(daddr), vm_id(vm_id)
{
}

flow flow::from_header(const udp::header_parser& hdr, vm_id_t vm_id)
{
    return flow(hdr.udp.source, hdr.udp.dest, hdr.ip.saddr, hdr.ip.daddr, vm_id);
}

flow flow::mask(int fields)
{
    return flow(
        (fields & FT_FIELD_SRC_PORT) ? 0xffff : 0,
        (fields & FT_FIELD_DST_PORT) ? 0xffff : 0,
        (fields & FT_FIELD_SRC_IP) ? 0xffffffff : 0,
        (fields & FT_FIELD_DST_IP) ? 0xffffffff : 0,
        (fields & FT_FIELD_VM_ID) ? 0xffffffff : 0);
}

flow flow::operator &(const flow& mask) const
{
    return flow(
        source_port & mask.source_port,
        dest_port & mask.dest_port,
        saddr & mask.saddr,
        daddr & mask.daddr);
}

std::size_t hash_value(flow f)
{
#pragma HLS pipeline enable_flush ii=2
    std::size_t seed = 0;

    boost::hash_combine(seed, f.source_port.to_short());
    boost::hash_combine(seed, f.dest_port.to_short());
    boost::hash_combine(seed, f.saddr.to_int());
    boost::hash_combine(seed, f.daddr.to_int());
    boost::hash_combine(seed, f.vm_id.to_int());

    return seed;
}

::std::ostream& operator<<(::std::ostream& out, const flow& v)
{
    return out << "flow(saddr=" << v.saddr << ", sport=" << v.source_port
               << ", daddr=" << v.daddr << ", dport=" << v.dest_port << ", vm_id=" << v.vm_id << ")";
}


::std::ostream& operator<<(::std::ostream& out, const flow_table_value& v)
{
    return out << "flow_table_value(action=" << v.action << ", engine=" << v.engine_id
               << ", ikernel_id=" << v.ikernel_id << ")";
}

void flow_table::ft_step(header_stream& header, result_stream& result,
                         gateway_registers& g)
{
#pragma HLS inline
    ft_wrapper(header, result, g);
    hash_flow_table.hash_table();
}

void flow_table::ft_wrapper(header_stream& header, result_stream& result,
                            gateway_registers& g)
{
#pragma HLS pipeline enable_flush ii=3
#pragma HLS inline region
    gateway.gateway(g, [=](ap_uint<31> addr, int& data) -> int {
#pragma HLS inline
        if (addr & GW_WRITE)
            return reg_write(addr & ~GW_WRITE, data);
        else
            return reg_read(addr & ~GW_WRITE, &data);
    });

    
    if (!header.empty() && !hash_flow_table.lookups.full()) {
        auto packet_flow_info = flow::from_header(header.read()) &
                                flow::mask(fields);
        hash_flow_table.lookups.write(packet_flow_info);
    }

    if (!hash_flow_table.results.empty() && !result.full()) {
        auto cur_results = hash_flow_table.results.read();
        if (cur_results.valid()) {
            flow_table_result res;
            std::tie(res.flow_id, res.v) = cur_results.value();
            result.write(res);
        } else {
            result.write(flow_table_result(0, flow_table_value(FT_PASSTHROUGH)));
        }
    }
}

int flow_table::reg_write(int address, int value)
{
#pragma HLS inline

    maybe<std::tuple<flow, flow_table_value> > entry;

    switch (address) {
    case FT_FIELDS:
        fields = value;
        return 0;
    case FT_KEY_SADDR:
        gateway_flow.saddr = value;
        break;
    case FT_KEY_DADDR:
        gateway_flow.daddr = value;
        break;
    case FT_KEY_SPORT:
        gateway_flow.source_port = value;
        break;
    case FT_KEY_DPORT:
        gateway_flow.dest_port = value;
        break;
    case FT_RESULT_ACTION:
        gateway_result.action = flow_table_action(value);
        break;
    case FT_RESULT_ENGINE:
        gateway_result.engine_id = value;
        break;
    case FT_RESULT_IKERNEL_ID:
        gateway_result.ikernel_id = value;
        break;
    case FT_VALID:
        gateway_valid = value;
        break;
    case FT_SET_ENTRY:
        entry = make_maybe(gateway_valid, make_tuple(gateway_flow, gateway_result));
        return hash_flow_table.gateway_debug_command(value, true, entry);
    case FT_READ_ENTRY: {
	int ret = hash_flow_table.gateway_debug_command(value, false, entry);
        if (ret == GW_DONE) {
            gateway_valid = entry.valid();
            if (entry.valid())
                std::tie(gateway_flow, gateway_result) = entry.value();
        }
        return ret;
    }
    default:
        return GW_FAIL;
    }

    return GW_DONE;
}

int flow_table::reg_read(int address, int* value)
{
#pragma HLS inline
    switch (address) {
    case FT_FIELDS:
        *value = fields;
        return 0;
    case FT_KEY_SADDR:
        *value = gateway_flow.saddr;
        break;
    case FT_KEY_DADDR:
        *value = gateway_flow.daddr;
        break;
    case FT_KEY_SPORT:
        *value = gateway_flow.source_port;
        break;
    case FT_KEY_DPORT:
        *value = gateway_flow.dest_port;
        break;
    case FT_RESULT_ACTION:
        *value = gateway_result.action;
        break;
    case FT_RESULT_ENGINE:
        *value = gateway_result.engine_id;
        break;
    case FT_RESULT_IKERNEL_ID:
        *value = gateway_result.ikernel_id;
        break;
    case FT_VALID:
        *value = gateway_valid;
        break;
    case FT_ADD_FLOW:
        return hash_flow_table.gateway_add_entry(make_tuple(gateway_flow, gateway_result), value);
    case FT_DELETE_FLOW:
        return hash_flow_table.gateway_delete_entry(gateway_flow, value);
    default:
        goto err;
    }

    return 0;

err:
    *value = -1;
    return GW_FAIL;
}

void flow_table::reset()
{
    fields = 0;

    // TODO reset table
}

void flow_table::gateway_update()
{
}

/* Just for testing synthesis results faster */
void flow_table_top(header_stream& header, result_stream& result,
                    gateway_registers& g)
{
#pragma HLS dataflow
    GATEWAY_OFFSET(g, 0x18, 0x28, 0x20);
    static flow_table ft;

    ft.ft_step(header, result, g);
}

