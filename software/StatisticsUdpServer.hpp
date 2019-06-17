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

#ifndef STATISTICSUDPSERVER_HPP
#define STATISTICSUDPSERVER_HPP

#include "UdpServer.hpp"
#include <infiniband/verbs.h>

class ikernel;
class custom_ring;

class StatisticsUdpServer : public UdpServer {
public:
    struct args {
        short port;
        int secs;
        std::string interface;
    };

    StatisticsUdpServer(boost::asio::io_service& io_service, const args& args, const uint32_t threshold, bool use_custom_ring);
    ~StatisticsUdpServer();
    virtual void process(std::size_t length);
    void print_statistics();

    int get_dropped_count();    
    int get_host_count();

    virtual void do_receive();

private:
    int post_recv(int first_entry, int num_entries);

    uint32_t secs;
    uint32_t max;
    uint32_t min;
    uint32_t sum;
    uint32_t count;
    uint32_t threshold_count;
    uint32_t threshold;

    bool use_custom_ring;
    ikernel* ik;
    uint64_t* receive_buffer;
    ibv_mr *mr;
    custom_ring* cr;

    uint32_t consumer_index; // last read
    uint32_t producer_index; // last received
    uint32_t posted_receive_buffers; // of all times
    uint32_t outstanding_recv_wrs; // remaining
    uint32_t read_uint();
};

#endif //STATISTICSUDPSERVER_HPP
