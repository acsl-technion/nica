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

#include "gateway.hpp"
#include "ikernel.hpp"

#include <hls_stream.h>
#include <toe/toe.hpp>

struct toe_app_ports {
    // Connected to ToE
    hls::stream<ap_uint<16> > m_axis_listen_port;
    hls::stream<bool> s_axis_listen_port_status;
    hls::stream<appNotification> s_axis_notifications;
    hls::stream<appReadRequest> m_axis_rx_data_req;
    hls::stream<ap_uint<16> > s_axis_rx_data_rsp_metadata;
    // hls::stream<ipTuple> m_axis_open_connection;
    // hls::stream<openStatus> s_axis_open_status;
    // hls::stream<ap_uint<16> > m_axis_close_connection;

    // Connected to AFU
    hls_ik::metadata_stream meta_toe_to_ik;
};

#define TOE_PORTS_PRAGMAS(toe) \
    DO_PRAGMA(HLS interface axis port=&toe.m_axis_listen_port) \
    DO_PRAGMA(HLS interface axis port=&toe.s_axis_listen_port_status) \
    DO_PRAGMA(HLS interface axis port=&toe.s_axis_notifications) \
    DO_PRAGMA(HLS interface axis port=&toe.m_axis_rx_data_req) \
    DO_PRAGMA(HLS interface axis port=&toe.s_axis_rx_data_rsp_metadata) \
    \
    DO_PRAGMA(HLS interface axis port=&toe.meta_toe_to_ik) \
    /* \
    DO_PRAGMA(HLS interface axis port=&toe.m_axis_open_connection) \
    DO_PRAGMA(HLS interface axis port=&toe.s_axis_open_status) \
    DO_PRAGMA(HLS interface axis port=&toe.m_axis_close_connection) \
    */ \
    \
    DO_PRAGMA(HLS data_pack variable=&toe.s_axis_notifications) \
    DO_PRAGMA(HLS data_pack variable=&toe.m_axis_rx_data_req) \
    /*
    DO_PRAGMA(HLS data_pack variable=&toe.m_axis_open_connection) \
    DO_PRAGMA(HLS data_pack variable=&toe.s_axis_open_status) \
    DO_PRAGMA(HLS data_pack variable=&toe.m_axis_close_connection) */

void link(toe_app_ports& internal, toe_app_ports& external);

/* Build RoCE UC send packets from ikernel outputs */
class toe_control : public hls_ik::gateway_impl<toe_control>
{
public:
    toe_control();

    /** Builds packets from split header and data streams, and re-calculates
     * the checksum. */
    void toe_c(hls_ik::gateway_registers& r);

    int rpc(int address, int *value, bool read);

    template <typename Type, Type toe_control:: *field>
    int read_write(int *value, bool read);

    int reg_read(int address, int* value)
    {
#pragma HLS inline
        return rpc(address, value, true);
    }

    int reg_write(int address, int value)
    {
#pragma HLS inline
        return rpc(address, &value, false);
    }

    void gateway_update();

    toe_app_ports p;
private:
    void event_acceptor();

    hls::stream<appNotification> notifications;

    ap_uint<32> ip;
    ap_uint<16> port;
    bool closed;

    bool listen_req_sent;

    int close_counter;
};
