/*
 * Copyright (c) 2016 Haggai Eran. All rights reserved.
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * BSD license below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef NICA_H
#define NICA_H

#include <uuid.h>
#include <infiniband/verbs.h>
#include <arpa/inet.h>

#ifdef __cplusplus
extern "C" {
#endif


#define MAX_NUM_OF_CUSTOM_RINGS 1
#define MAX_CR_SIZE (32*1024)
#define PORT_NUM 1

struct ikernel;

struct ik_create_attrs {
	uint32_t size;
	/* Log2 of the amount of DRAM needed for the ikernel. 0 designate no
	 * DRAM. */
	uint32_t log_dram_size;
};

ikernel* ik_create_attrs(const char* netdev, const uuid_t uuid, struct ik_create_attrs *attrs);

static inline ikernel* ik_create(const char* netdev, const uuid_t uuid)
{
	struct ik_create_attrs cmd = { sizeof(cmd), 0 };
	return ik_create_attrs(netdev, uuid, &cmd);
}

int ik_destroy(ikernel* ik);
/* Attach a given socket to the ikernel. Returns 0 for success, -1 for error.
 * h2n_flow_id and n2h_flow_id contain the flow ID for the host-to-net flow table
 * and net-to-host flow table respectively. */
int ik_attach(int socket, ikernel* ik, uint32_t *h2n_flow_id, uint32_t *n2h_flow_id);
int ik_detach(int socket, ikernel* ik);

/* Accessor functions for the ikernel register space */
int ik_write(ikernel* ik, int address, int value);
int ik_read(ikernel* ik, int address, int* value);

int ik_axi_read(ikernel* ik, int address, int* value);


struct custom_ring;

/* custom ring */

/* Create a new custom ring that is associated with a given ikernel. 
 * max_cr_size is the max number of recv_wr to be posted
 * max_cr_size should be less than 32K wrs */
custom_ring* custom_ring_create(ikernel* ik, unsigned int max_cr_size);

/* Destroy a given custom ring and free its resources. */
int custom_ring_destroy(custom_ring* cr);

/* Query the custom ring for its handle. The return ring_id can be passed in
 * the ikernel's output metadata stream to pass data packets to this specific
 * ring. */
int custom_ring_handle(custom_ring* cr);

/* Register a memory region for a given custom ring. See ibv_reg_mr(3) for more
 * details. */
ibv_mr *custom_ring_reg_mr(custom_ring* cr, void *addr, size_t length, enum ibv_access_flags access);

/* Post a receive buffer for the custom ring to contain future received
 * messages. For more details see ibv_post_recv(3). */
int custom_ring_post_recv(custom_ring* cr, ibv_recv_wr* recv_wr, ibv_recv_wr** bad_wr);

/* Post a receive buffer for the custom ring to contain future received messages.
 * args: 
 * 	num_of_entries: number of recv_wrs to be posted
 * 	update_credits: set true to update the credits in ikernel. 
 * 			Otherwise, it will only post recv_wrs. 
 * For more details see ibv_post_recv(3). */
int custom_ring_post_recv_attr(custom_ring* cr, ibv_recv_wr* recv_wr, ibv_recv_wr** bad_wr, int num_of_entries, bool update_credits);

/* Poll the custom ring for newly received messages. For more details see
 * ibv_poll_cq(3). */
int custom_ring_poll_cq(custom_ring* cr, int num_entries, struct ibv_wc* wc);


#ifdef __cplusplus
}
#endif

#endif // NICA_H
