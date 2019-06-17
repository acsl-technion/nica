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

#include "EchoUdpServer.hpp"
#include <arpa/inet.h>
#include <nica.h>

EchoUdpServer::EchoUdpServer(boost::asio::io_service &io_service, const args &args)
        : UdpServer(io_service, args.port, args.interface), secs(args.secs),
          count(0), client(args.local_ip)
{
    ik = ik_create(args.interface.c_str(), args.uuid);
    if (!ik) {
        std::cerr << "Warning: couldn't create ikernel\n";
    } else {
        int fd = socket_.native_handle();
        ik_attach(fd, ik, NULL, NULL);
    }
}

void EchoUdpServer::process(std::size_t length) {
    assert(length >= 4);

    int new_data = read_int();
    new_data = htonl(new_data);

    client.send_to(&new_data, sizeof(new_data), sender_endpoint_);
    ++count;
}

int EchoUdpServer::read_int() {
   return ntohl(*reinterpret_cast<uint32_t*>(data_));
}

int EchoUdpServer::get_count() {
    return count;
}

void EchoUdpServer::print_statistics() {
    std::cout << double(count) / secs << std::endl;
}
