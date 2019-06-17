/* * Copyright (c) 2016-2017 Haggai Eran, Gabi Malka, Lior Zeno, Maroun Tork
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 *  * Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation and/or
 * other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <string>
#include <pcap/pcap.h>
#include "mlx.h"

#include <limits>

#include "nica-top.hpp"

namespace udp_tb {
    class pkt_id_verifier {
    public:
        pkt_id_verifier();

        void inc(mlx::pkt_id_t id);
        void dec(mlx::pkt_id_t id);

        bool verify() const;
    protected:
        int count[8];
    };

    class testbench
    {
    public:
        testbench();
        void run();
        virtual void top() = 0;
        virtual int num_extra_clocks() { return 3000; }
        static int read_pcap(const std::string& filename, mlx::stream& stream,
                             int range_start = 0, int range_end = std::numeric_limits<int>::max(),
                             pkt_id_verifier* verifier = NULL,
                             hls::stream<mlx::user_t>* user_values = NULL);
        typedef std::function<void()> callback_t;
        static int read_pcap_callback(const std::string& filename, mlx::stream& stream,
                             int range_start, int range_end,
                             pkt_id_verifier* verifier,
                             hls::stream<mlx::user_t>* user_values,
		             callback_t callback);
        static int write_pcap(FILE* file, mlx::stream& stream,
                              bool expected_lossy = true,
                              pkt_id_verifier* verifier = NULL,
                              hls::stream<mlx::user_t>* user_values = NULL);

        /** Helper function to get filename from temporary file. */
        static std::string filename(FILE* file);

        void TearDown();

        mlx::stream cxp2sbu;
        mlx::stream sbu2cxp;
        mlx::stream nwp2sbu;
        mlx::stream sbu2nwp;

        trace_event events[NUM_TRACE_EVENTS];

        toe_app_ports toe;

        unsigned count;
    protected:
        /** Compare a filtered pcap file against a different filtered pcap file. */
        bool compare_output(const std::string& pcap1, const std::string& filter1,
                            const std::string& pcap2, const std::string& filter2);
        /** Compare a filtered pcap file against a pre-formatted text file. */
        bool compare_output(const std::string& pcap1, const std::string& filter1,
                            const std::string& output_txt_file);
    };
}

/* Ignore lossy and drop bits */
#define MLX_TUSER_MAGIC_MASK (MLX_TUSER_PRESERVE)
#define MLX_TUSER_MAGIC (0x1bc << 3)
