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

#include "emu.hpp"
#include "nica-top.hpp"
#include "threshold-impl.hpp"
#include "passthrough-impl.hpp"
#include "pktgen.hpp"
#include "memcached-ik.hpp"

#include <boost/preprocessor/iteration/local.hpp>

#include <mutex>

namespace emulation {

    template <typename T>
    static void var_access(T& lhs, uint32_t* rhs, bool read)
    {
        if (read)
            *rhs = lhs;
        else
            lhs = *rhs;
    }

    struct gateway_wrapper {
        hls_ik::gateway_registers& gateway;

        gateway_wrapper(hls_ik::gateway_registers& r) : gateway(r) {}

        void reg_access(uint32_t address, uint32_t* value, bool read)
        {
            switch (address) {
            case 0x0: {
                uint32_t cmd = gateway.cmd.addr | (gateway.cmd.write << 30) | (gateway.cmd.go << 31);
                var_access(cmd, value, read);
                gateway.cmd.addr = cmd & 0x3fffffff;
                gateway.cmd.write = cmd >> 30;
                gateway.cmd.go = cmd >> 31;
                break;
            }
            case 0x8: /* data_i */
            case 0x10: /* data_o */
                var_access(gateway.data, value, read);
                break;
            case 0x18:
                var_access(gateway.done, value, read);
                break;
            case 0x14: // data_o valid
            case 0x1c: // done valid
                if (read) *value = 1;
                break;
            default:
                std::cerr << "Unknown address in gateway: " << address << '\n';
                break;
            }
        }
    };

    struct virt_gateway_wrapper : public gateway_wrapper {
        hls_ik::virt_gateway_registers& gateway;

        virt_gateway_wrapper(hls_ik::virt_gateway_registers& r) : gateway_wrapper(r.common), gateway(r) {}

        void reg_access(uint32_t address, uint32_t* value, bool read)
        {
            switch (address) {
            case 0x20:
                var_access(gateway.ikernel_id, value, read);
                break;
            default:
                gateway_wrapper::reg_access(address, value, read);
                break;
            }
        }
    };

    struct ikernel_wrapper {
        hls_ik::ports ports;
        hls_ik::tc_ikernel_data_counts tc;
        hls_ik::ikernel_id id;
        virt_gateway_wrapper gateway;
        hls_ik::virt_gateway_registers gateway_regs;
        using ikernel_top_func = std::function<void(hls_ik::ports&,
            hls_ik::ikernel_id&, hls_ik::virt_gateway_registers&,
            hls_ik::tc_ikernel_data_counts&)>;
        ikernel_top_func func;

        ikernel_wrapper() :
            gateway(gateway_regs)
        {}

        void init(size_t i)
        {
            std::string ikernel_env = std::string("IKERNEL") + std::to_string(i);
            char *ikernel_name = std::getenv(ikernel_env.c_str());
            std::string ikernel_str = ikernel_name ? ikernel_name : "threshold";
            if (ikernel_str == "threshold")
                func = threshold_top;
            else if (ikernel_str == "passthrough")
                func = passthrough_top;
            else if (ikernel_str == "pktgen")
                func = pktgen_top;
            else if (ikernel_str == "memcached")
                func = memcached_top;
            else
                throw std::exception();

            hls_ik::init(tc);
        }

        void step() 
        {
            func(ports, id, gateway.gateway, tc);
        }

        void reg_access(uint32_t address, uint32_t* value, bool read)
        {
            if (address >= 0x14 && address <= 0x38)
                return gateway.reg_access(address - 0x14, value, read);

            switch (address) {
            case 0x0:
            case 0x4:
            case 0x8:
            case 0xc:
                var_access(*(uint32_t *)(id.uuid + address), value, read);
                break;
            case 0x10: // uuid valid
                if (read) *value = 1;
                break;
            case 0x50: {
                uint32_t msn_ring = ports.host_credit_regs.reset;
                msn_ring <<= ports.host_credit_regs.max_msn.width;
                msn_ring |= ports.host_credit_regs.max_msn;
                msn_ring <<= ports.host_credit_regs.ring_id.width;
                msn_ring |= ports.host_credit_regs.ring_id;

                var_access(msn_ring, value, read);

                ports.host_credit_regs.ring_id = msn_ring;
                msn_ring >>= ports.host_credit_regs.ring_id.width;
                ports.host_credit_regs.max_msn = msn_ring;
                msn_ring >>= ports.host_credit_regs.max_msn.width;
                ports.host_credit_regs.reset = msn_ring;
                break;
            }
            default:
                std::cerr << "Unknown address in ikernel: " << address << '\n';
                break;
            }
        }
    };

    static nica_config cfg;
    static nica_stats stats;
    static trace_event events[NUM_TRACE_EVENTS];
    static mlx::stream prt_nw2sbu("prt_nw2sbu"),
                       sbu2prt_nw("sbu2prt_nw"),
                       prt_cx2sbu("prt_cx2sbu"),
                       sbu2prt_cx("sbu2prt_cx");
    static std::mutex emulation_interface_mutex;
    static size_t num_ikernels = 0;
    static gateway_wrapper n2h_flow_table_gateway(cfg.n2h.common.flow_table_gateway),
                           h2n_flow_table_gateway(cfg.h2n.common.flow_table_gateway),
                           n2h_custom_ring_gateway(cfg.n2h.custom_ring_gateway);
    static tc_ports h2n_tc, n2h_tc;

    static std::vector<ikernel_wrapper> init_ikernels()
    {
        const char *num_ikernels_str = std::getenv("NUM_IKERNELS") ?: "1";
        num_ikernels = std::stoi(num_ikernels_str);
        if (num_ikernels > NUM_IKERNELS)
            throw std::exception();

        std::vector<ikernel_wrapper> ikernels(num_ikernels);
        for (size_t i = 0; i < num_ikernels; ++i)
            ikernels[i].init(i);
        return ikernels;
    }

    static std::vector<ikernel_wrapper> ikernels = init_ikernels();

    void step()
    {
        std::lock_guard<std::mutex> lock(emulation_interface_mutex);

        nica(prt_nw2sbu, sbu2prt_nw, prt_cx2sbu, sbu2prt_cx, &cfg, &stats, events
#define BOOST_PP_LOCAL_MACRO(n) \
            , ikernels[n].ports
#define BOOST_PP_LOCAL_LIMITS (0, NUM_IKERNELS - 1)
%:include BOOST_PP_LOCAL_ITERATE()
            ,
            h2n_tc, h2n_tc, n2h_tc, n2h_tc
        );
        for (auto& ik : ikernels)
            ik.step();
    }

    static void reg_access(uint32_t address, uint32_t* value, bool read)
    {
        std::lock_guard<std::mutex> lock(emulation_interface_mutex);

        if (address >= 0x1000 && address < 0x1000 * (num_ikernels + 1)) {
            auto& ik = ikernels[(address / 0x1000) - 1];
            ik.reg_access(address - (address / 0x1000) * 0x1000, value, read);
            return;
        } else if (address >= 0x18 && address <= 0x34) {
            return n2h_flow_table_gateway.reg_access(address - 0x18, value, read);
        } else if (address >= 0x418 && address <= 0x434) {
            return h2n_flow_table_gateway.reg_access(address - 0x418, value, read);
        } else if (address >= 0x78 && address <= 0x94) {
            return n2h_custom_ring_gateway.reg_access(address - 0x78, value, read);
        }

        switch (address) {
        case 0x10:
            var_access(cfg.n2h.common.enable, value, read);
            break;
        case 0x410:
            var_access(cfg.h2n.common.enable, value, read);
            break;
	case 0x800:
	    var_access(stats.flow_table_size, value, read);
            break;
        default:
            std::cerr << "Unknown address: " << address << '\n';
            break;
        }
    }

    void reg_read(uint32_t address, uint32_t* value)
    {
        reg_access(address, value, true);
    }

    void reg_write(uint32_t address, uint32_t value)
    {
        reg_access(address, &value, false);
    }

    void send_packet(const packet* pkt)
    {
        std::lock_guard<std::mutex> lock(emulation_interface_mutex);

        for (size_t i = 0; i < pkt->len; i += 32) {
            hls_ik::axi_data flit;
            const uint8_t cur_len = std::min(pkt->len - i, 32ul);

            flit.set_data(pkt->data + i, cur_len);
            flit.last = i + cur_len == pkt->len;

            mlx::axi4s mlx_flit(flit, 0, 1);

            if (pkt->dir == Net)
                prt_nw2sbu.write(mlx_flit);
            else
                prt_cx2sbu.write(mlx_flit);
        }
    }

    struct packet_buffer {
        char data[2048]; // TODO
        packet pkt;
        mlx::stream& out;

        packet_buffer(interface dir, mlx::stream& out) : 
            pkt(), out(out)
        {
            pkt.data = data;
            pkt.dir = dir;
        }

        packet* get_packet()
        {
            std::lock_guard<std::mutex> lock(emulation_interface_mutex);
            packet *ret_pkt = NULL;

            while (!out.empty()) {
                hls_ik::axi_data flit = out.read();

                if (pkt.len < sizeof(data) - 32)
                    pkt.len += flit.get_data(data + pkt.len);
                if (flit.last) {
                    ret_pkt = new packet(pkt);

                    pkt.len = 0;
                    break;
                }
            }

            return ret_pkt;
        }
    };

    static packet_buffer net_pkt_buffer(Net, sbu2prt_nw),
                         host_pkt_buffer(Host, sbu2prt_cx);

    static uint32_t ntohl(uint32_t const net) {
        uint8_t data[4] = {};
        memcpy(&data, &net, sizeof(data));

        return ((uint32_t) data[3] << 0)
               | ((uint32_t) data[2] << 8)
               | ((uint32_t) data[1] << 16)
               | ((uint32_t) data[0] << 24);
    }

    static uint16_t htons(uint16_t n) {
        return ((((unsigned short)(n) & 0xFF)) << 8) | (((unsigned short)(n) & 0xFF00) >> 8);
    }

    static uint16_t ip_checksum(void* vdata,size_t length) {
        // Cast the data pointer to one that can be indexed.
        char* data=(char*)vdata;

        // Initialise the accumulator.
        uint64_t acc=0xffff;

        // Handle any partial block at the start of the data.
        unsigned int offset=((uintptr_t)data)&3;
        if (offset) {
            size_t count=4-offset;
            if (count>length) count=length;
            uint32_t word=0;
            memcpy(offset+(char*)&word,data,count);
            acc+=ntohl(word);
            data+=count;
            length-=count;
        }

        // Handle any complete 32-bit blocks.
        char* data_end=data+(length&~3);
        while (data!=data_end) {
            uint32_t word;
            memcpy(&word,data,4);
            acc+=ntohl(word);
            data+=4;
        }
        length&=3;

        // Handle any partial block at the end of the data.
        if (length) {
            uint32_t word=0;
            memcpy(&word,data,length);
            acc+=ntohl(word);
        }

        // Handle deferred carries.
        acc=(acc&0xffffffff)+(acc>>32);
        while (acc>>16) {
            acc=(acc&0xffff)+(acc>>16);
        }

        // If the data began at an odd byte address
        // then reverse the byte order to compensate.
        if (offset&1) {
            acc=((acc&0xff00)>>8)|((acc&0x00ff)<<8);
        }

        // Return the checksum in network byte order.
        return htons(~acc);
    }

    packet* get_packet()
    {
        packet* ret = net_pkt_buffer.get_packet();
        if (!ret) {
            ret = host_pkt_buffer.get_packet();
        }

        if (!ret) return ret;

        if (ret->len < 26) return ret;
        if ((*(uint16_t*)(ret->data+12)) != 8) return ret;

        std::memset(ret->data + 24, 0, 2);
        uint16_t csum = ip_checksum(ret->data + 14, 20);
        std::memcpy(ret->data + 24, (void*)&csum, sizeof(uint16_t));
        return ret;
    }

}

