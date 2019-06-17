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
#include "RunnableServerBase.hpp"
#include "passthrough.hpp"
#include "echo.hpp"
#include <vma/vma_extra.h>

#include <future>

class echo_main : public RunnableServerBase {
public:
    echo_main(const unsigned char *ikernel_uuid, int argc, char **argv)
            : RunnableServerBase(ikernel_uuid, argc, argv), _uuid(ikernel_uuid) {}

protected:

    virtual void preflight() {
        counts = std::vector<std::future<int> >(thread_num);
    }

    virtual void postflight() {
        long total = 0;

        for (int i = 0; i < thread_num; ++i) {
            total += counts[i].get();
        }

	std::cout << "packets forwarded to server : " << total << std::endl;
        std::cout << "throughput : " << total / secs << std::endl;
    }

    virtual void start_server(const int &thread_id) {
        EchoUdpServer::args args = {
                .port = short(port + thread_id),
                .secs = secs,
                .interface = vm["interface"].as<std::string>(),
                .uuid = _uuid
        };
        std::promise<int> count;
        counts[thread_id] = count.get_future();
        EchoUdpServer s(io_service, args);
        s.do_receive();
        io_service.run();

        count.set_value(s.get_count());
    }

    std::vector<std::future<int> > counts;
    const unsigned char *_uuid;
};

int main(int argc, char **argv) {
    uuid_t passthrough = PASSTHROUGH_UUID, echo = ECHO_UUID, uuid;

    if (std::getenv("SIMULATION")) {
        uuid_copy(uuid, passthrough);
    } else {
	uuid_copy(uuid, echo);
    }

    return echo_main(uuid, argc, argv).run();
}
