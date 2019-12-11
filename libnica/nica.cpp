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


#include <sys/socket.h>
#include "nica.h"
#include "nicamgr.h"
#include <cerrno>
#include <string>
#include <mutex>
#include <iostream>

#include <boost/thread/shared_mutex.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>

using std::string;
extern "C" {
#include <infiniband/driver.h>
}
#include <boost/lexical_cast.hpp>

#include <boost/asio.hpp>

namespace fs = boost::filesystem;

ibv_gid fpga_gid = {
    raw: { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0x0a, 0x00, 0x00, 0x01 }
};


struct ikernel {
	uuid_t uuid;
	uint32_t handle;
        string netdev;

	ikernel(const uuid_t id, uint32_t handle, const string& netdev) : handle(handle), netdev(netdev) {
		uuid_copy(uuid, id);
	}	
};

using boost::asio::local::stream_protocol;

struct global_state {
	global_state() :
		sock(io_service),
		sock_mutex()
	{
		sock.connect(stream_protocol::endpoint(NICA_MANAGER_PATH));
	}

	typedef std::function<int(stream_protocol::socket&)> callback_t;

	template <typename Request, typename Response>
	int call(nicamgr_opcode opcode, const Request& req, Response& resp, callback_t callback = callback_t())
	{
		using boost::asio::write;
		using boost::asio::read;
		using boost::asio::buffer;

		std::lock_guard<std::mutex> lock(sock_mutex);

		nicamgr_header hdr = {
			opcode,
			sizeof(req),
			NICA_MANAGER_FLAG_REQUEST,
			0
		};
		auto hdr_buf = buffer(&hdr, sizeof(hdr));
		write(sock, hdr_buf);
		write(sock, buffer(&req, sizeof(req)));
		if (callback) {
			int ret = callback(sock);
			if (ret)
				return ret;
		}

		read(sock, hdr_buf);
		assert(opcode == hdr.opcode);
		assert(!(hdr.flags & NICA_MANAGER_FLAG_REQUEST));
		if (hdr.status) {
			char buf[hdr.length];
			read(sock, buffer(buf, sizeof(buf)));
			errno = hdr.status;
			return -1;
		}
		assert(sizeof(resp) == hdr.length);

		read(sock, buffer(&resp, sizeof(resp)));
		return 0;
	}

	boost::asio::io_service io_service;
	stream_protocol::socket sock;
	std::mutex sock_mutex;
} *_g_state = NULL;

boost::shared_mutex g_state_mutex;
global_state& g_state()
{
	{
		boost::shared_lock<boost::shared_mutex> lock(g_state_mutex);
		if (_g_state)
			return *_g_state;
	}

	std::lock_guard<boost::shared_mutex> lock(g_state_mutex);
	if (!_g_state)
		_g_state = new global_state();

	return *_g_state;
}

#ifdef __cplusplus
extern "C" {
#endif


ikernel* ik_create_attrs(const char* netdev, const uuid_t uuid, struct ik_create_attrs *attrs)
{
	nica_req_ik_create_attrs req;
	nica_resp_ik_create_attrs resp;

	strncpy(req.netdev, netdev, sizeof(req.netdev));
	uuid_copy(req.uuid, uuid);

	if (attrs->size >= offsetof(struct ik_create_attrs, log_dram_size) + sizeof(attrs->log_dram_size))
		req.log_dram_size = attrs->log_dram_size;

	int ret = g_state().call(NICA_IK_CREATE_ATTRS, req, resp);
	if (ret) {
		return NULL;
	}

	return new ikernel(uuid, resp.ik, netdev);
}

int ik_destroy(ikernel* ik)
{
	nica_req_ik_destroy req = { ik->handle };
	nica_resp_ik_destroy resp;

	int ret = g_state().call(NICA_IK_DESTROY, req, resp);
	if (ret)
		return ret;
	delete ik;
	return 0;
}

static int send_fd(stream_protocol::socket& sock, int fd)
{
	/* From: https://linux.die.net/man/3/cmsg */
	struct msghdr msg;
	struct cmsghdr *cmsg;
	int myfds[1] = { fd }; /* Contains the file descriptors to pass. */
	char buf[CMSG_SPACE(sizeof myfds)];  /* ancillary data buffer */
	int *fdptr;

	memset(&msg, 0, sizeof(msg));
	/* For SOCK_STREAM, a minimum of one byte message is needed */
	char msg_buf[1] = {};
	struct iovec sg = { &msg_buf, sizeof(msg_buf) };
	msg.msg_iov = &sg;
	msg.msg_iovlen = 1;

	msg.msg_control = buf;
	msg.msg_controllen = sizeof buf;
	cmsg = CMSG_FIRSTHDR(&msg);
	cmsg->cmsg_level = SOL_SOCKET;
	cmsg->cmsg_type = SCM_RIGHTS;
	cmsg->cmsg_len = CMSG_LEN(sizeof(myfds));
	/* Initialize the payload: */
	fdptr = (int *) CMSG_DATA(cmsg);
	memcpy(fdptr, myfds, sizeof(myfds));
	/* Sum of the length of all control messages in the buffer: */
	msg.msg_controllen = cmsg->cmsg_len;

	if (sendmsg(sock.native_handle(), &msg, 0) < 0)
		return -1;
	return 0;
}

int ik_attach(int socket, ikernel* ik, uint32_t *h2n_flow_id, uint32_t *n2h_flow_id)
{
	nica_req_ik_attach req = { ik->handle };
	nica_resp_ik_attach resp;

	int ret = g_state().call(NICA_IK_ATTACH, req, resp, [&] (stream_protocol::socket& sock) {
		using boost::asio::buffer;

		nicamgr_header hdr;
		read(sock, buffer(&hdr, sizeof(hdr)));
		assert(NICA_IK_ATTACH == hdr.opcode);
		assert(4 == hdr.length); // empty struct with 4-byte reserved field
		uint32_t reserved;
		read(sock, buffer(&reserved, sizeof(reserved)));
		if (hdr.status)
			return int(hdr.status);
		return send_fd(sock, socket);
	});

	if (ret < 0)
		return ret;

	if (h2n_flow_id)
		*h2n_flow_id = resp.h2n_flow_id;
	if (n2h_flow_id)
		*n2h_flow_id = resp.n2h_flow_id;
	return 0;
}

int ik_detach(int socket, ikernel* ik)
{
	nica_req_ik_attach req = { ik->handle };
	nica_resp_ik_attach resp;

	return g_state().call(NICA_IK_DETACH, req, resp, [&] (stream_protocol::socket& sock) {
		return send_fd(sock, socket);
	});
}

static int ik_rpc(ikernel* ik, int address, int *value, bool write)
{
	nica_req_ik_rpc req = {
		ik->handle,
		address,
		*value,
		write
	};
	nica_resp_ik_rpc resp;

	int ret = g_state().call(NICA_IK_RPC, req, resp);
	if (!ret && !write)
		*value = resp.value;
	return ret;
}

int ik_write(ikernel* ik, int address, int value)
{
	return ik_rpc(ik, address, &value, true);
}

int ik_read(ikernel* ik, int address, int* value)
{
	return ik_rpc(ik, address, value, false);
}

#if 0
int ik_axi_read(ikernel* ik, int address, int* value) {
	if (!api) {
		errno = EOPNOTSUPP;
		return -1;
	}
	register_access cmd = register_access();
	cmd.handle = ik->handle;
	cmd.address = address;
	int ret = api->ik_axi_read(&cmd);
	if (!ret)
		*value = cmd.value;
	return ret;
}
#endif

int get_gid_index(ibv_context* dev)
{
        for (int i = 0; i < 0xffff; ++i) {
                ibv_gid gid;

                if (ibv_query_gid(dev, 1, i, &gid)) {
                        printf("ibv_query_gid failed for gid %d", i);
                        exit(1);
                }

                /* Check for IPv4 */
                if (gid.global.subnet_prefix != 0ull ||
                    (gid.global.interface_id & 0xffffffff) != 0xffff0000ull)
                        continue;

                char gid_type_str[7];
                int len = ibv_read_sysfs_file("/sys/class/infiniband/mlx5_0/ports/1/gid_attrs/types",
                        boost::lexical_cast<string>(i).c_str(), gid_type_str, sizeof(gid_type_str));
                if (len < 0) {
                        printf("cannot read gid type for gid %d", i);
                        return -1;
                }

                if (strncasecmp(gid_type_str, "RoCE v2", len) != 0)
                        continue;

                /* TODO check also the netdev matches */
         	return i;
        }
        return -1;
}

struct ibv_context *ibv_open_device_by_name(const std::string& device_name)
{
    int num_devices = 0;
    struct ibv_device **devices_list = ibv_get_device_list(&num_devices);
    if(!devices_list){
            printf("ERROR: ibv_get_device_list() failed\n");
            exit(1);
    }
    for (int i = 0; i < num_devices; ++i) {
        string cur_name = ibv_get_device_name(devices_list[i]);
            if (device_name == cur_name)
                return ibv_open_device(devices_list[i]);
    }

    printf("ERROR: device named '%s' not found\n", device_name.c_str());
    return NULL;
}

struct custom_ring {
        uint16_t credits;
        uint16_t msn; //last message sequence number
        uint16_t msn_diff;

        uint32_t handle;

        ibv_context* context;
        ibv_pd* pd;
        ibv_qp* qp;
        ibv_cq* cq;

	custom_ring(const std::string& device_name, unsigned int max_cr_size) : credits(0), msn(0), msn_diff(0) {
		context = ibv_open_device_by_name(device_name);
		if(!context) {
			printf("ERROR: ibv_open_device failed\n");
                        exit(1);
                }
		pd = ibv_alloc_pd(context);
	        if(!pd){
        	        printf("ERROR: ibv_alloc_pd() failed\n");
	                exit(1);
        	}
	        cq = ibv_create_cq(context,max_cr_size,NULL,NULL,0);
        	if(!cq){
                	printf("ERROR: ibv_create_cq() failed\n");
	                exit(1);
        	}

	        struct ibv_qp_init_attr qp_init_attr;
        	memset(&qp_init_attr, 0, sizeof(struct ibv_qp_init_attr));
		qp_init_attr.send_cq = cq;
        	qp_init_attr.recv_cq = cq;
	        qp_init_attr.qp_type = IBV_QPT_UC;
        	qp_init_attr.cap.max_send_wr = 0;
	        qp_init_attr.cap.max_recv_wr = max_cr_size;
        	qp_init_attr.cap.max_send_sge = 0;
	        qp_init_attr.cap.max_recv_sge = 1;
		qp = ibv_create_qp(pd, &qp_init_attr);
	        if(!qp) {
        	        printf("ERROR: ibv_create_qp() failed\n");
                	exit(1);
	        }
		struct ibv_port_attr port_attr;
	        int ret = ibv_query_port(context, PORT_NUM, &port_attr);
        	if(ret){
	                printf("ERROR: ibv_query_port() failed\n");
        	        exit(1);
        	}
		
		// QP state: RESET -> INIT
		struct ibv_qp_attr qp_attr;
        	memset(&qp_attr, 0, sizeof(struct ibv_qp_attr));
	        qp_attr.qp_state = IBV_QPS_INIT;
        	qp_attr.pkey_index = 0;
	        qp_attr.port_num = PORT_NUM;
        	qp_attr.qp_access_flags = 0;
	        ret = ibv_modify_qp(qp, &qp_attr, IBV_QP_STATE | IBV_QP_PKEY_INDEX | IBV_QP_PORT | IBV_QP_ACCESS_FLAGS);
        	if(ret){
	            printf("ERROR: ibv_modify_qp() to INIT failed\n");
        	    exit(1);
        	}
	        // QP state: INIT -> RTR
		memset(&qp_attr, 0, sizeof(struct ibv_qp_attr));
	        qp_attr.qp_state = IBV_QPS_RTR;
        	qp_attr.path_mtu = IBV_MTU_1024;
	        qp_attr.dest_qp_num = 0;
        	qp_attr.rq_psn = 0;
	        qp_attr.ah_attr.sl = 0;
        	qp_attr.ah_attr.src_path_bits = 0;
	        qp_attr.ah_attr.port_num = PORT_NUM;
        	qp_attr.ah_attr.is_global = 1;
	        qp_attr.ah_attr.grh.dgid = fpga_gid;
        	qp_attr.ah_attr.grh.sgid_index = get_gid_index(context);
	        qp_attr.ah_attr.grh.flow_label = 0;
        	qp_attr.ah_attr.grh.hop_limit = 1;
	        qp_attr.ah_attr.grh.traffic_class = 0;
        	ret = ibv_modify_qp(qp, &qp_attr, IBV_QP_STATE | IBV_QP_AV | IBV_QP_PATH_MTU| IBV_QP_DEST_QPN | IBV_QP_RQ_PSN);
	        if (ret) {
        	        printf("ERROR: ibv_modify_qp() to RTR failed ret = %d\n",ret);
                	exit(1);
	        }

        	// QP state: RTR -> RTS
		memset(&qp_attr, 0, sizeof(struct ibv_qp_attr));
	        qp_attr.qp_state = IBV_QPS_RTS;
        	qp_attr.sq_psn = 0;
	        ret = ibv_modify_qp(qp, &qp_attr, IBV_QP_STATE | IBV_QP_SQ_PSN);
        	if (ret) {
                	printf("ERROR: ibv_modify_qp() to RTS failed\n");
	                exit(1);
        	}
	}

        ~custom_ring(){
	        ibv_destroy_qp(qp);
        	ibv_destroy_cq(cq);
	        ibv_dealloc_pd(pd);
        	ibv_close_device(context);
	}
	
};

string ib_device_from_netdev(const string& netdev)
{
    fs::path dir = "/sys/class/net/" + netdev + "/device/infiniband";
    fs::directory_iterator end;
    for (fs::directory_iterator dir_itr(dir); dir_itr != end; ++dir_itr) {
        return dir_itr->path().filename().string();
    }

    printf("Could not find IB device of netdev '%s'\n", netdev.c_str());
    abort();
}
	        
custom_ring* custom_ring_create(ikernel* ik, unsigned int max_cr_size) 
{
	if(max_cr_size > MAX_CR_SIZE) {
		printf("ERROR: custom_ring_create() failed: max_cr_size=%d > MAX_CR_SIZE=%d\n",max_cr_size,MAX_CR_SIZE);
        	exit(1);	
	}
        string device_name = ib_device_from_netdev(ik->netdev);
	custom_ring* ret_cr = new custom_ring(device_name, max_cr_size);
	if(!ret_cr){
		return NULL;
	}

	nica_req_cr_create req = {
		ik->handle,
		ret_cr->qp->qp_num,
	};
	nica_resp_cr_create resp;

	int ret = g_state().call(NICA_CR_CREATE, req, resp);
	if (ret)
		return NULL;

	ret_cr->handle = resp.cr;
	
	return ret_cr;
}


int custom_ring_destroy(custom_ring* cr)
{
	nica_req_cr_destroy req = {
		cr->handle,
	};
	nica_resp_cr_create resp;

	return g_state().call(NICA_CR_DESTROY, req, resp);
}

int custom_ring_handle(custom_ring* cr)
{
	return cr->handle;
}

ibv_mr *custom_ring_reg_mr(custom_ring* cr, void *addr, size_t length, enum ibv_access_flags access) {
	return ibv_reg_mr(cr->pd,addr,length,access);
}

int custom_ring_post_recv_attr(custom_ring* cr, ibv_recv_wr* recv_wr, ibv_recv_wr** bad_wr, int num_of_entries, bool update_credits)
{
        int ret = ibv_post_recv(cr->qp, recv_wr, bad_wr);
        if (ret) {
            printf("ERROR: custom_ring_post_recv() failed ret=%d\n",ret);
            exit(1);
        }

        cr->credits += num_of_entries;
        if(update_credits){
		cr->credits += cr->msn_diff;
		cr->msn_diff = 0;
		nica_req_cr_update_credits req = {
			cr->handle,
                        cr->credits,
                };
		nica_resp_cr_update_credits resp;
                ret = g_state().call(NICA_CR_UPDATE_CREDITS, req, resp);
	}
        return ret;
}

int custom_ring_post_recv(custom_ring* cr, ibv_recv_wr* recv_wr, ibv_recv_wr** bad_wr){
        int num_of_entries = 0;
        for (ibv_recv_wr* num_recv_wr = recv_wr ; num_recv_wr != NULL; num_recv_wr = num_recv_wr->next)
                num_of_entries++;
        return custom_ring_post_recv_attr(cr, recv_wr, bad_wr, num_of_entries, true);
}

int custom_ring_poll_cq(custom_ring* cr, int num_entries, struct ibv_wc* wc)
{
	if (!cr) {
		std::cout << "cr is NULL" << std::endl;
		exit(1);
	}
	int ret = ibv_poll_cq(cr->cq, num_entries, wc);
	if (ret > 0) {
//		int wc_msn = wc[ret-1].imm_data;
		// msn in hardware should start from 1
		
//		cr->msn_diff += wc_msn - cr->msn - ret;
//		cr->msn = wc_msn;
	}
	return ret;
}

#ifdef __cplusplus
}
#endif
