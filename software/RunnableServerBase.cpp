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
#include <chrono>
#include <thread>
#include <iostream>
#include <boost/program_options/options_description.hpp>
#include <boost/program_options/parsers.hpp>

#include <nica.h>

using boost::program_options::options_description;
using boost::program_options::value;
using boost::program_options::store;

RunnableServerBase::RunnableServerBase(const unsigned char *ikernel_uuid, int argc, char **argv)
        : ikernel_uuid(ikernel_uuid),
          argc(argc),
          argv(argv) {}

int RunnableServerBase::parse_command_line_options() {
    options_description desc("options");
    desc.add_options()
            ("help,h", "print help message")
            ("port,p", value<short>()->default_value(1111), "UDP port")
            ("threads,t", value<int>()->default_value(1), "number of threads")
            ("seconds,s", value<int>()->default_value(5), "running time in seconds")
            ("threshold,v", value<uint32_t>()->default_value(0), "threshold value")
	    ("interface,I", value<std::string>()->default_value(""), "interface name")
	    ("use_custom_ring,c", "enable custom ring");

    store(parse_command_line(argc, argv, desc), vm);

    if (vm.count("help")) {
        std::cerr << desc << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

int RunnableServerBase::run() {
    try {
        if (parse_command_line_options() == EXIT_FAILURE) {
            return EXIT_FAILURE;
        }

        port = vm["port"].as<short>();
        thread_num = vm["threads"].as<int>();
        secs = vm["seconds"].as<int>();
	threshold = vm["threshold"].as<uint32_t>();
        preflight();

        std::vector<std::thread> threads(thread_num);
        for (int i = 0; i < thread_num; ++i) {
            threads[i] = std::thread([this, i] { this->start_server(i); });
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(secs * 1000));
        io_service.stop();

        for (int i = 0; i < thread_num; ++i) {
            threads[i].join();
        }
        postflight();
    }
    catch (std::exception &e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

RunnableServerBase::~RunnableServerBase() {}
