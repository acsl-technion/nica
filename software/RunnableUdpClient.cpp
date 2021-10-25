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

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <boost/asio.hpp>
#include <boost/program_options/options_description.hpp>
#include <boost/program_options/variables_map.hpp>
#include <boost/program_options/parsers.hpp>
#include <arpa/inet.h>
#include "UdpClient.hpp"
#include <pktgen.hpp>
#include <nica.h>
#include <unistd.h>

namespace po = boost::program_options;

int main(int argc, char* argv[]) {
    try
    {
        po::options_description desc("options");
        desc.add_options()
            ("help,h", "print help message")
            ("host,H", po::value<std::string>()->default_value("localhost"), "server host name")
            ("port,p", po::value<std::string>()->default_value("1111"), "UDP port")
            ("count,c", po::value<int>()->default_value(1000), "packet count")
            ("offload,o", "offload to hardware")
            ("interface,I", po::value<std::string>()->default_value(""), "interface name")
            ("local-ip,l", po::value<std::string>()->default_value(""), "local IP address")
            ("range,r", po::value<int>()->default_value(1U), "payload range [0,range), default [0,inf)")
	    ("payload-size,s", po::value<int>()->default_value(4), "payload size in bytes, default 4 Bytes")
            ("burst-size,b", po::value<int>()->default_value(4000), "burst_size for pktgen ikernel, default 4000");
        ;

        po::variables_map vm;
        po::store(po::parse_command_line(argc, argv, desc), vm);

        if (vm.count("help"))
        {
            std::cerr << desc << "\n";
            return 1;
        }

        std::string hostname = vm["host"].as<std::string>(),
                    port = vm["port"].as<std::string>(),
                    interface = vm["interface"].as<std::string>(),
                    local_ip = vm["local-ip"].as<std::string>();
	
	int payload_size = vm["payload-size"].as<int>();
	int burst_size = vm["burst-size"].as<int>();
        int count = vm["count"].as<int>();
        int range = vm["range"].as<int>();
        UdpClient client (local_ip);
        udp::endpoint endpoint = client.get_endpoint(hostname, port);

        bool offload = vm.count("offload");
        ikernel* ik = NULL;
        if (offload) {
            uuid_t uuid = PKTGEN_UUID;
            ik = ik_create(interface.c_str(), uuid);
            if (!ik) {
                    std::cerr << "Couldn't create ikernel\n";
                    return 1;
            }
            ik_write(ik, PKTGEN_BURST_SIZE, burst_size - 1);
	    count = 1;
            ik_attach(client.get_socket().native_handle(), ik, NULL, NULL);
        }

        for (int i = 0; i < count; ++i) {
            uint32_t request = std::rand() % range;
            uint32_t* buf = (uint32_t*)malloc(payload_size);
            *buf = htonl(request);
            client.send_to(buf, payload_size, endpoint);
	    free(buf);
        }
        if (offload) {
            while (true) {
                int cur = 0;
                int ret = ik_read(ik, PKTGEN_CUR_PACKET, &cur);
                if (ret) {
                    std::cerr << "Error reading current packet.";
                    return ret;
                }
                if (!cur)
                    break;

                sleep(1);
            }
        }
    }
    catch (std::exception& e)
    {
        std::cerr << "Exception: " << e.what() << "\n";
    }

    return 0;
}
