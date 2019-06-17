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

#include "CmsUdpServer.hpp"
#include "cms-ikernel.hpp"
#include <nica.h>
#include <system_error>
#include <arpa/inet.h>

CmsUdpServer::CmsUdpServer(boost::asio::io_service &io_service, const args &args)
        : UdpServer(io_service, args.port, args.interface) {
    CountMinSketch::generateHashes(hashes);
    countMinSketch.setHashes(hashes);
    uuid_t uuid = CMS_UUID;
    ik = ik_create(args.interface.c_str(), uuid);
    if (!ik) {
        std::cerr << "Warning: couldn't create ikernel\n";
        _k = K;
    } else {
        write_hashes(ik);
        read_k_value(ik);
        int fd = socket_.native_handle();
        ik_attach(fd, ik, NULL, NULL);
    }
}

void CmsUdpServer::write_hashes(ikernel* ik) {
    for (int i = 0; i < DEPTH; ++i) {
        for (int j = 0; j < 2; ++j) {
            int hash = static_cast<int>(static_cast<float>(rand()) * static_cast<float>(LONG_PRIME)
                                        / static_cast<float>(RAND_MAX) + 1);
            int ret = ik_write(ik, 2 * i + j + HASHES_BASE, hash);
            if (ret) {
                std::cerr << "Warning: couldn't write ikernel hash in cell " << (2 * i + j) << std::endl;
            }
        }
    }
}

void CmsUdpServer::read_k_value(ikernel* ik) {
	int ret = ik_read(ik, READ_K_VALUE, (int *)&_k);
	if (ret) {
		std::cerr << "Warning: couldn't read k value from ikernel" << std::endl;
	}
}

void CmsUdpServer::process(std::size_t length) {
    assert(length >= 4);
    int new_data = read_int();
    countMinSketch.update(new_data, 1);

    // update topK
    // if the element is already in the topK
    // then we only need to increment the counter
    if (index.find(new_data) != index.end()) {
        fibHeap::handle_type handle = index[new_data];
        (*handle).payload.increment();
        topK.increase(handle);
    } else {
        unsigned int estimatedCount = countMinSketch.estimate(new_data);

        if (topK.size() >= _k) {
            if (topK.top().payload.frequency > estimatedCount) return;

            heap_data top = topK.top();
            index.erase(top.payload.value);
            topK.pop();
        }

        fibHeap::handle_type handle = topK.push(value_frequency_pair{.value=new_data,.frequency=estimatedCount});
        index[new_data] = handle;
        (*handle).handle = handle;
    }
}

int CmsUdpServer::read_int() {
    return ntohl(*reinterpret_cast<uint32_t*>(&data_[14]));
}

std::vector<int> CmsUdpServer::get_host_topK() {
    std::vector<int> topKValues;

    while (!topK.empty()) {
        topKValues.push_back(topK.top().payload.value);
        topKValues.push_back(topK.top().payload.frequency);
        topK.pop();
    }

    return topKValues;
}

std::vector<int> CmsUdpServer::get_topK() {
    // Detach ikernel before reading topK in order to avoid reading
    // partial updates.
    if (!ik) return std::vector<int>();

    ik_detach(socket_.native_handle(), ik);
    std::vector<int> topK;
    ik_write(ik,READ_TOP_K,0); // topK - read_req
    for (uint32_t i = 0; i < 2*_k; ++i) {
        int hw_cell = 0;
        int ret = ik_read(ik, TOPK_READ_NEXT_VALUE , &hw_cell);
        if (ret) {
            std::error_condition econd = std::system_category().default_error_condition(errno);
            std::cerr << "Warning: couldn't read cell from ikernel: " << econd.message() << "\n";
        } else {
            topK.push_back(hw_cell);
        }
    }

    return topK;
}

CmsUdpServer::~CmsUdpServer() {}
