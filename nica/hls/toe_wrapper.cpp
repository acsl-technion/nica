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

#include "toe_wrapper.hpp"
#include "toe_wrapper-impl.hpp"

void link(toe_app_ports& internal, toe_app_ports& external)
{
    using hls_helpers::link_axi_to_fifo;
    using hls_helpers::link_axi_stream;

    link_axi_to_fifo(external.s_axis_listen_port_status, internal.s_axis_listen_port_status);
    link_axi_stream(internal.m_axis_listen_port, external.m_axis_listen_port);
    link_axi_to_fifo(external.s_axis_notifications, internal.s_axis_notifications);
    link_axi_stream(internal.m_axis_rx_data_req, external.m_axis_rx_data_req);
    link_axi_to_fifo(external.s_axis_rx_data_rsp_metadata, internal.s_axis_rx_data_rsp_metadata);
    // link_axi_to_fifo(external.s_axis_open_status, internal.s_axis_open_status);
    // link_axi_stream(internal.m_axis_open_connection, external.m_axis_open_connection);
    // link_axi_stream(internal.m_axis_close_connection, external.m_axis_close_connection);

    link_axi_stream(internal.meta_toe_to_ik, external.meta_toe_to_ik);
}

toe_control::toe_control()
{}

void toe_control::toe_c(hls_ik::gateway_registers& r)
{
#pragma HLS inline
    toe_control::gateway(this, r);
    event_acceptor();
}

template <typename Type, Type toe_control:: *field>
int toe_control::read_write(int *value, bool read)
{
    if (read)
        *value = this->*field;
    else
        this->*field = *value;

    return 0;
}

int toe_control::rpc(int address, int *value, bool read)
{
#pragma HLS inline
    switch (address) {
    case TOE_PORT:
        return read_write<ap_uint<16>, &toe_control::port>(value, read);
    case TOE_IP:
        return read_write<ap_uint<32>, &toe_control::ip>(value, read);
    case TOE_CLOSED:
        return read_write<bool, &toe_control::closed>(value, read);
    case TOE_LISTEN_PORT:
        if (!listen_req_sent) {
            if (p.m_axis_listen_port.write_nb(port))
                listen_req_sent = true;
        } else {
            bool status;
            if (p.s_axis_listen_port_status.read_nb(status)) {
                if (read)
                    *value = status;
                listen_req_sent = false;
                return GW_DONE;
            }
        }
        return GW_BUSY;
    case TOE_READ_NOTIFICATION:
        if (!read)
            return GW_DONE;

        if (notifications.empty()) {
            *value = 0;
        } else {
            appNotification n;

            notifications.read_nb(n);
            ip = n.ipAddress;
            port = n.dstPort;
            closed = n.closed;

            *value = 1;
        }
        return GW_DONE;
    case TOE_CONNECT:
    default:
        if (read)
            *value = -1;
        return GW_FAIL;
    }
}

void toe_control::gateway_update()
{
#pragma HLS inline
}

void toe_control::event_acceptor()
{
#pragma HLS pipeline II=3

#pragma HLS stream variable=&notifications depth=16

    appNotification notification;
    ap_uint<16> id;

    p.s_axis_rx_data_rsp_metadata.read_nb(id);

    if (p.s_axis_notifications.empty() || p.m_axis_rx_data_req.full() ||
        p.meta_toe_to_ik.full() || notifications.full())
        return;

    p.s_axis_notifications.read_nb(notification);
    if (notification.length == 0) {
        notifications.write_nb(notification);
        return;
    }

    p.m_axis_rx_data_req.write_nb(appReadRequest{
        notification.sessionID,
        notification.length});

    hls_ik::metadata m;
    // TODO: at the moment, use UDP metadata for simplicity, but switch to TCP
    // specific in the future.
    m.pkt_type = PKT_TYPE_UDP;
    m.flow_id = notification.sessionID; // TODO incompatible sizes
    m.ikernel_id = 0; // TODO virtualization support
    m.length = notification.length;

    hls_ik::packet_metadata pkt_metadata;
    pkt_metadata.ip_src = notification.ipAddress;
    pkt_metadata.udp_dst = notification.dstPort;
    m.set_packet_metadata(pkt_metadata);
    p.meta_toe_to_ik.write_nb(m);
}
