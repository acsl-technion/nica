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

#pragma once

#include <ntl/gateway.hpp>

#include "ikernel-types.hpp"

#define VIRT_GATEWAY_OFFSET(gateway, offset_cmd, offset_data, offset_done, offset_ikernel_id) \
    GATEWAY_OFFSET(gateway.common, offset_cmd, offset_data, offset_done) \
    DO_PRAGMA_SYN(HLS interface s_axilite port=gateway.ikernel_id offset=offset_ikernel_id)

using ntl::GW_FAIL;
using ntl::GW_DONE;
using ntl::GW_BUSY;

namespace hls_ik {
    enum {
        GW_WRITE = (1U << 30),
    };

    struct gateway_registers : public ntl::gateway_registers<int> {
    };

    struct virt_gateway_registers {
        virt_gateway_registers() : common(), ikernel_id(0) {}

        gateway_registers common;

	/** Write: ikernel ID to associate the command with. */
	ikernel_id_t ikernel_id;
    };
}
