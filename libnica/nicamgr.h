/* * Copyright (c) 2016-2018 Haggai Eran, Gabi Malka, Lior Zeno, Maroun Tork
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

#pragma once

/* NICA manager protocol definitions */

#define NICA_MANAGER_PATH "/var/run/nica-manager.socket"

/* For IFNAMSIZ */
#include <net/if.h>

struct nicamgr_header {
	uint16_t opcode;
	uint16_t length;
	uint16_t flags;
	uint16_t status;
};

enum nicamgr_opcode {
	NICA_IK_CREATE = 1,
	NICA_IK_DESTROY,
	NICA_IK_RPC,
	NICA_IK_ATTACH,
	NICA_IK_DETACH,
	NICA_CR_CREATE,
	NICA_CR_DESTROY,
	NICA_CR_UPDATE_CREDITS,
	NICA_IK_CREATE_ATTRS,
};

enum {
	/* Request have this bit set, responses have it cleared. */
	NICA_MANAGER_FLAG_REQUEST = 1 << 0,
};

struct nica_req_ik_create {
	char netdev[IFNAMSIZ];
	uuid_t uuid;
};

struct nica_resp_ik_create {
	uint32_t ik;
};

struct nica_req_ik_create_attrs {
	uint32_t size;
	char netdev[IFNAMSIZ];
	uuid_t uuid;
	uint32_t log_dram_size;
};

struct nica_resp_ik_create_attrs {
	uint32_t size;
	uint32_t ik;
};

struct nica_req_ik_destroy {
	uint32_t ik;
};

struct nica_resp_ik_destroy {
	uint32_t reserved;
};

struct nica_req_ik_attach {
	uint32_t ik;
	/* Send the socket file descriptor over SCM_RIGHTS */
};

struct nica_resp_ik_attach {
	uint32_t h2n_flow_id;
	uint32_t n2h_flow_id;
};

struct nica_req_ik_detach {
	uint32_t ik;
	/* Send the socket file descriptor over SCM_RIGHTS */
};

struct nica_resp_ik_detach {
	uint32_t reserved;
};

struct nica_req_ik_rpc {
	uint32_t ik;
	int address;
	int value;
	int write;
};

struct nica_resp_ik_rpc {
	int value;
};

struct nica_req_cr_create {
	uint32_t ik;
	uint32_t qp_num;
};

struct nica_resp_cr_create {
	uint32_t cr;
};

struct nica_req_cr_destroy {
	uint32_t cr;
};

struct nica_resp_cr_destroy {
	uint32_t reserved;
};

struct nica_req_cr_update_credits {
	uint32_t cr;
	uint32_t credits;
};

struct nica_resp_cr_update_credits {
	uint32_t reserved;
};
