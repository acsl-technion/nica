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

#include "RunnableServerBase.hpp"
#include "CmsUdpServer.hpp"
#include "cms-ikernel.hpp"
#include <vma/vma_extra.h>

#include <sstream>
#include <future>

class cms_main : public RunnableServerBase {
public:
    cms_main(const unsigned char *uuid, int argc, char **argv)
            : RunnableServerBase(uuid, argc, argv) {}

protected:

    virtual void preflight() {
        topKs = std::vector<std::future<std::vector<int>>>(thread_num);
    }

    std::string vec_to_str(const std::vector<int>& vec) {
        std::ostringstream sstream;

        if (vec.size() > 0) {
            std::copy(&vec[0], &vec[vec.size() - 1], std::ostream_iterator<int>(sstream, " "));
            sstream << vec[vec.size() - 1];

        }

        return sstream.str();
    }

    virtual void postflight() {
        if (ikernel_top_k.size() > 0) {
            std::cout << "iKernel topK: " << vec_to_str(ikernel_top_k) << std::endl;
        } else {
            std::cout << "iKernel topK is empty" << std::endl;
        }

        std::cout << "Host topK per thread: " << std::endl;

        for (int i = 0; i < thread_num; ++i) {
            std::vector<int> host_top_k = topKs[i].get();
            std::cout << "Thread " << i << ": " << vec_to_str(host_top_k) << std::endl;
        }
    }

    virtual void start_server(const int &thread_id) {
        CmsUdpServer::args args = {
                .port = short(port + thread_id),
                .interface = vm["interface"].as<std::string>(),
        };
        std::promise<std::vector<int>> topK;
        topKs[thread_id] = topK.get_future();
        CmsUdpServer s(io_service, args);
        s.do_receive();
        io_service.run();

        topK.set_value(s.get_host_topK());

        if (thread_id == 0) {
            ikernel_top_k = s.get_topK();
        }
    }

    std::vector<std::future<std::vector<int>>> topKs;
    std::vector<int> ikernel_top_k;

};

int main(int argc, char **argv) {
    uuid_t uuid = CMS_UUID;

    return cms_main(uuid, argc, argv).run();
}
