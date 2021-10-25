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
#include <arpa/inet.h>
#include <nica.h>
#include <threshold.hpp>
#include <system_error>

#include <boost/lexical_cast.hpp>

extern "C" {
	#include <infiniband/driver.h>
}

#define NUM_RECEIVE_WR 1024

using std::string;
using std::cerr;

void perror(const string& msg)
{
	std::error_condition econd = std::system_category().default_error_condition(errno);
	std::cout << "ERROR: " << msg << " " << econd.message() << "\n";
}

int StatisticsUdpServer::post_recv(int producer_index, int num_entries)
{
    ibv_sge sg[num_entries];
    ibv_recv_wr wr[num_entries];

    memset(wr, 0, sizeof(wr));

    for (int i = 0; i < num_entries; ++i) {
        size_t wr_id = posted_receive_buffers++;
        sg[i].addr = (uintptr_t)&(receive_buffer[wr_id % NUM_RECEIVE_WR]);
        sg[i].length =  sizeof(*receive_buffer);
        sg[i].lkey = mr->lkey;

        wr[i].wr_id = uint64_t(wr_id);
        wr[i].sg_list = &sg[i];
        wr[i].num_sge = 1;
        if (i != num_entries - 1)
            wr[i].next = &wr[i + 1];
    }
    ibv_recv_wr *bad_wr;
    int ret = custom_ring_post_recv(cr, wr, &bad_wr);
    if (ret) {
            perror("ibv_post_recv");
            return -1;
    }

    outstanding_recv_wrs += num_entries;

    return 0;
}

StatisticsUdpServer::StatisticsUdpServer(boost::asio::io_service &io_service, const StatisticsUdpServer::args& args, const uint32_t threshold, bool use_custom_ring)
        : UdpServer(io_service, args.port, args.interface), secs(args.secs), max(0), min(0), sum(0), count(0), threshold_count(0), threshold(threshold),
	use_custom_ring(use_custom_ring), ik(), receive_buffer(), mr(), cr(), consumer_index(0), producer_index(0), posted_receive_buffers(0), outstanding_recv_wrs(0)
{

    uuid_t uuid = THRESHOLD_UUID;
    ik = ik_create(args.interface.c_str(), uuid);
    if (!ik) {
        std::cerr << "Warning: couldn't create ikernel\n";
    } else {
        int ret = ik_write(ik, THRESHOLD_VALUE, threshold);
        if (ret) {
            std::cerr << "Warning: couldn't write ikernel threshold\n";
        }
        int fd = socket_.native_handle();
        uint32_t n2h_flow_id, h2n_flow_id;
        ret = ik_attach(fd, ik, &h2n_flow_id, &n2h_flow_id);
	if (ret < 0) {
                perror("ik_attach");
		return;
        }
	std::cout << "Got n2h flow ID: " << n2h_flow_id << "\n";
	std::cout << "Got h2n flow ID: " << h2n_flow_id << "\n";

	if (use_custom_ring) {
		cr = custom_ring_create(ik, NUM_RECEIVE_WR);
		receive_buffer = new uint64_t[NUM_RECEIVE_WR];
		for (int i = 0; i  < NUM_RECEIVE_WR; ++i) {
	                receive_buffer[i] = 0;
		}
                mr = custom_ring_reg_mr(cr, receive_buffer, sizeof(*receive_buffer) * NUM_RECEIVE_WR, IBV_ACCESS_LOCAL_WRITE);
		if (!mr) {
			perror("ibv_reg_mr");
			return;
		}
                if (post_recv(0, NUM_RECEIVE_WR))
                    return;
		std::cout << "Got custom ring handle " << custom_ring_handle(cr) << "\n";

		ret = ik_write(ik, THRESHOLD_RING_ID, custom_ring_handle(cr));
	} else {
		ret = ik_write(ik, THRESHOLD_RING_ID, 0);
	}
	if (ret) {
		perror("ik_write for threshold ring id");
		return;
	}

        ret = ik_write(ik, THRESHOLD_COUNT, 0);
        ret = ret || ik_write(ik, THRESHOLD_DROPPED, 0);
        ret = ret || ik_write(ik, THRESHOLD_MIN, 0);
        ret = ret || ik_write(ik, THRESHOLD_MAX, 0);
        ret = ret || ik_write(ik, THRESHOLD_SUM_LO, 0);
        ret = ret || ik_write(ik, THRESHOLD_DROPPED_BACKPRESSURE, 0);
        if (ret) {
            perror("ik_write");
            return;
        }
    }
}

StatisticsUdpServer::~StatisticsUdpServer()
{
	if (mr)
        	ibv_dereg_mr(mr);
	if (receive_buffer)
        	delete [] receive_buffer;
	if (cr)
		custom_ring_destroy(cr);
	if(ik)
		ik_destroy(ik);
}

void StatisticsUdpServer::process(std::size_t length) {
   assert(length >= 4);

   uint32_t new_data = read_uint();
   ++count;
   sum += new_data;
   max = std::max(max, new_data);
   min = std::min(min, new_data);

   if (new_data >= threshold) {
      // alert
       ++threshold_count;
   }
}

uint32_t StatisticsUdpServer::read_uint() {
    if (use_custom_ring) {
        return receive_buffer[(consumer_index++) % NUM_RECEIVE_WR];
    } else {
        return ntohl(*reinterpret_cast<uint32_t*>(data_+14));   
    }
}


int StatisticsUdpServer::get_host_count() {
	return count;
}

int StatisticsUdpServer::get_dropped_count() {
    int hw_count = 0;
    int ret = ik_read(ik, THRESHOLD_COUNT, &hw_count);
    if (ret) {
        std::error_condition econd = std::system_category().default_error_condition(errno);
        std::cerr << "Warning: couldn't read count from ikernel: " << econd.message() << "\n";
    } else {
        std::cerr << "hw counter value: " << hw_count << "\n";
    }
 
    int hw_threshold_value = 0;
    ret = ik_read(ik, THRESHOLD_VALUE, &hw_threshold_value);
    if (ret) {
        std::error_condition econd = std::system_category().default_error_condition(errno);
        std::cerr << "Warning: couldn't read threshold value from ikernel: " << econd.message() << "\n";
    } else {
        std::cerr << "threshold value: " << hw_threshold_value << "\n";
    }
   
    int hw_dropped = 0;
    ret = ik_read(ik, THRESHOLD_DROPPED, &hw_dropped);
    if (ret) {
        std::error_condition econd = std::system_category().default_error_condition(errno);
        std::cerr << "Warning: couldn't read dropped value from ikernel: " << econd.message() << "\n";
    } else {
        std::cerr << "dropped packets: " << hw_dropped << "\n";
    }

    int hw_min = 0;
    ret = ik_read(ik, THRESHOLD_MIN, &hw_min);
    if (ret) {
        std::error_condition econd = std::system_category().default_error_condition(errno);
        std::cerr << "Warning: couldn't read min value from ikernel: " << econd.message() << "\n";
    } else {
        std::cerr << "hw min: " << hw_min << "\n";
    }

    int hw_max = 0;
    ret = ik_read(ik, THRESHOLD_MAX, &hw_max);
    if (ret) {
        std::error_condition econd = std::system_category().default_error_condition(errno);
        std::cerr << "Warning: couldn't read max value from ikernel: " << econd.message() << "\n";
    } else {
        std::cerr << "hw max: " << hw_max << "\n";
    }
   
    int hw_sum_lo = 0;
    ret = ik_read(ik, THRESHOLD_SUM_LO, &hw_sum_lo);
    if (ret) {
        std::error_condition econd = std::system_category().default_error_condition(errno);
        std::cerr << "Warning: couldn't read sum_lo value from ikernel: " << econd.message() << "\n";
    } else {
        std::cerr << "hw sum_lo: " << hw_sum_lo << "\n";
    }

    int hw_dropped_backpressure = 0;
    ret = ik_read(ik, THRESHOLD_DROPPED_BACKPRESSURE, &hw_dropped_backpressure);
    if (ret) {
        std::error_condition econd = std::system_category().default_error_condition(errno);
        std::cerr << "Warning: couldn't read value from ikernel: " << econd.message() << "\n";
    } else {
        std::cerr << "hw dropped backpressure: " << hw_dropped_backpressure << "\n";
    }

    std::cerr << "host threshold_count " << threshold_count << "\n";
    std::cerr << "host count " << count << "\n";

    return hw_dropped;//count + hw_count;
}

void StatisticsUdpServer::print_statistics() {
//   std::cout << "Count: " << count
//             << ", Sum: " << sum
//             << ", Max: " << max
//             << ", Min: " << min
//             << ", Threshold count: " << threshold_count
//             << std::endl;
    std::cout << double(count) / secs << std::endl;
}

void StatisticsUdpServer::do_receive()
{
    if (!use_custom_ring)
        return UdpServer::do_receive();

    ibv_wc wc[16];
    int ret = custom_ring_poll_cq(cr, 16, wc);
    if (ret < 0)
        return perror("ibv_poll_cq");

    outstanding_recv_wrs -= ret;

    for (int i = 0; i < ret; ++i) {
        if (wc[i].status != IBV_WC_SUCCESS) {
            std::cerr << "error completion: " << wc[i].status << "\n";
            abort();
        }
        producer_index = wc[i].wr_id;
        std::cout << "got completion (wr_id = " << wc[i].wr_id << ") value " << std::hex << (uint32_t)receive_buffer[producer_index % NUM_RECEIVE_WR] << " length " << wc[i].byte_len << "\n";
    }
    while (producer_index != consumer_index)
        process(4);

    if (outstanding_recv_wrs < NUM_RECEIVE_WR / 2)
        post_recv(producer_index, NUM_RECEIVE_WR / 2);

    // polling
    io_service.post([this]() { do_receive(); });
}
