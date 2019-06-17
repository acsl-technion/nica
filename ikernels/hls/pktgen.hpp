//
// Copyright (c) 2016-2018 Haggai Eran, Gabi Malka, Lior Zeno, Maroun Tork
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

#ifndef PKTGEN_HPP
#define PKTGEN_HPP

// 2f8e8996-1b5e-4c02-908c-0f2878b0d4e4
#define PKTGEN_UUID { 0x2f, 0x8e, 0x89, 0x96, 0x1b, 0x5e, 0x4c, 0x02, 0x90, 0x8c, 0x0f, 0x28, 0x78, 0xb0, 0xd4, 0xe4 }

/* Number of packets to send on each burst */
#define PKTGEN_BURST_SIZE 0x10
/* Current packet (R/O) (goes down from burst size to zero) */
#define PKTGEN_CUR_PACKET 0x11

#define PKTGEN_SCHEDULER 0x20

#include <ikernel.hpp>
#include <gateway.hpp>
#include <context_manager.hpp>
#include <scheduler.hpp>

DECLARE_TOP_FUNCTION(pktgen_top);

struct pktgen_context {
    pktgen_context() : cur_packet(0), burst_size(0), metadata()
    {}

    /** Number of the current packet in its burst */
    uint32_t cur_packet;
    /** Size of each burst in packets */
    uint32_t burst_size;
    /** Length of data in the data array (in elements) */
    uint32_t data_length;
    /** Packet metadata */
    hls_ik::metadata metadata;
};

class pktgen_contexts : public context_manager<pktgen_context, LOG_NUM_IKERNELS>
{
public:
    int rpc(int address, int *value, hls_ik::ikernel_id_t ikernel_id, bool read);
};

class pktgen : public hls_ik::ikernel, public hls_ik::virt_gateway_impl<pktgen> {
public:
    pktgen();

    virtual void step(hls_ik::ports& ports, hls_ik::tc_ikernel_data_counts& tc);

    int reg_write(int address, int value, hls_ik::ikernel_id_t ikernel_id);
    int reg_read(int address, int* value, hls_ik::ikernel_id_t ikernel_id);

private:
    scheduler<LOG_NUM_IKERNELS> sched;

    void data_plane(hls_ik::pipeline_ports& p, hls_ik::tc_pipeline_data_counts& tc);
    void sched_wrapper();
    void check_quantum_complete(hls_ik::pipeline_ports& p, hls_ik::tc_pipeline_data_counts& tc);

    enum { IDLE, INPUT_PACKET, DUPLICATE } state;
    /** The metadata for newly incoming packet */
    hls_ik::metadata metadata;
    /** The context of the current transmitting ikernel */
    pktgen_context context;
    /** Context updates from data plane */
    hls::stream<pktgen_context> context_updates;
    /** Transmit commands from the scheduler unit */
    hls::stream<std::tuple<pktgen_context, uint32_t> > transmit_commands;
    /** The offset into the data array we are currently accessing */
    size_t data_offset;
    /** Quota for the data plane to use before context switch */
    uint32_t quota;
    /** Size of the data array (in elements) */
    static const int data_size = 2048 / 32;
    /** An array that holds the packet payload to duplicate */
    hls_ik::axi_data data[1 << LOG_NUM_IKERNELS][data_size];
    /** Context manager class to access per ikernel context */
    pktgen_contexts contexts;

    /* Scheduler state */
    enum { FIND_NEXT_IKERNEL, TRANSMIT_COMMAND } scheduler_state;
    /** The ikernel we are currently processing */
    hls_ik::ikernel_id_t sched_ikernel_id;
    /** Current quota from the scheduler to the data plane */
    uint32_t quota_from_sched;
};
#endif
