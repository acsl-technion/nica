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

#include "StatisticsUdpServer.hpp"
#include "RunnableServerBase.hpp"
#include "threshold.hpp"
#include <vma/vma_extra.h>
#include <future>

class threshold_main : public RunnableServerBase {
public:
    threshold_main(const unsigned char *uuid, int argc, char **argv)
            : RunnableServerBase(uuid, argc, argv) {}

protected:

    virtual void preflight() {
        dropped_counts = std::vector<std::future<int> >(thread_num);
        host_counts = std::vector<std::future<int> >(thread_num);

    }

    virtual void postflight() {
        long total = 0, host_total = 0;

        for (int i = 0; i < thread_num; ++i) {
            total += dropped_counts[i].get();// + host_counts[i].get();
            host_total += host_counts[i].get();		
        }
	total += host_total;
	double ratio = total > 0 ? (((double)(host_total))/total)*100 : 100;
        std::cout << "packets to host: " << ratio << " %" << std::endl;
        std::cout << "throughput: " << total / secs << std::endl;
    }

    virtual void start_server(const int &thread_id) {
        StatisticsUdpServer::args args = {
                .port = short(port + thread_id),
                .secs = secs,
                .interface = vm["interface"].as<std::string>(),
        };
        std::promise<int> dropped_count,host_count;
        dropped_counts[thread_id] = dropped_count.get_future();
        host_counts[thread_id] = host_count.get_future();
        StatisticsUdpServer s(io_service, args, threshold, vm.count("use_custom_ring"));
        s.do_receive();
	io_service.run();
        dropped_count.set_value(s.get_dropped_count());

        host_count.set_value(s.get_host_count());
    }

    std::vector<std::future<int> > dropped_counts,host_counts;
};

int main(int argc, char **argv) {
    uuid_t uuid = THRESHOLD_UUID;

    return threshold_main(uuid, argc, argv).run();
}
