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

#ifndef IKERNEL_CMSUDPSERVER_HPP
#define IKERNEL_CMSUDPSERVER_HPP

#include "UdpServer.hpp"
#include "cms.hpp"
#include <boost/heap/fibonacci_heap.hpp>
#include <map>

#define K 256

class ikernel;
struct heap_data;

using fibHeap = boost::heap::fibonacci_heap<heap_data>;

struct value_frequency_pair {
    int value;
    unsigned int frequency;

    void increment() {
        frequency += 1;
    }
};

struct heap_data {
    value_frequency_pair payload;
    fibHeap::handle_type handle;

    heap_data(value_frequency_pair pair) : payload(pair), handle() {}

    bool operator<(heap_data const & rhs) const {
        return payload.frequency > rhs.payload.frequency;
    }
};


class CmsUdpServer : public UdpServer {
public:
    struct args {
        short port;
        std::string interface;
    };

    CmsUdpServer(boost::asio::io_service& io_service, const args& args);
    virtual void process(std::size_t length);

    std::vector<int> get_topK();
    std::vector<int> get_host_topK();

    virtual ~CmsUdpServer();
private:
    ikernel* ik;
    CountMinSketch countMinSketch;
    int hashes[DEPTH][2];
    uint32_t _k;
    fibHeap topK;
    std::map<int, fibHeap::handle_type> index;

    static void write_hashes(ikernel* ik);
    void read_k_value(ikernel* ik);
    int read_int();
};


#endif //IKERNEL_CMSUDPSERVER_HPP
