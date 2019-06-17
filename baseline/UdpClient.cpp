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

#include "UdpClient.hpp"
#include <system_error>

UdpClient::~UdpClient() {}
UdpClient::UdpClient(const std::string& local_ip) : _io_service(), _socket(_io_service), _resolver(_io_service) {
    _socket.open(udp::v4());
    _socket.set_option(udp::socket::reuse_address(true));
    if (!local_ip.empty()) {
        udp::endpoint local (boost::asio::ip::address::from_string(local_ip), 0);
        _socket.bind(local);
    }
    _socket.bind(udp::endpoint());
}

void UdpClient::send_to(void *data, size_t size_in_bytes, const std::string &hostname, const std::string &port) {
    send_to(data, size_in_bytes, get_endpoint(hostname, port));
}
udp::endpoint UdpClient::get_endpoint(const std::string &hostname, const std::string &port) {
    return *_resolver.resolve({udp::v4(), hostname, port});
}
void UdpClient::send_to(void *data, size_t size_in_bytes, const udp::endpoint &endpoint) {
     _socket.send_to(boost::asio::buffer(data, size_in_bytes), endpoint);
}
udp::socket& UdpClient::get_socket() { return _socket; }
