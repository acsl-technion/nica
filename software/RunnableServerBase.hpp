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

#ifndef IKERNEL_RUNNABLESERVERBASE_HPP
#define IKERNEL_RUNNABLESERVERBASE_HPP

#include <boost/asio/io_service.hpp>
#include <boost/program_options/variables_map.hpp>

struct ikernel_attributes;

using boost::program_options::variables_map;

class RunnableServerBase {
public:
    RunnableServerBase(const unsigned char* ikernel_uuid, int argc, char** argv);
    virtual ~RunnableServerBase();
    virtual int run();
    virtual int parse_command_line_options();

    virtual void preflight() = 0;
    virtual void postflight() = 0;
    virtual void start_server(const int& thread_id) = 0;

protected:
    const unsigned char* ikernel_uuid;
    short port;
    int thread_num;
    int secs;
    uint32_t threshold;
    variables_map vm;
    int argc;
    char** argv;
    boost::asio::io_service io_service;
};


#endif //IKERNEL_RUNNABLESERVERBASE_HPP
