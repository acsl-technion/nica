/*
 * Copyright (c) 2016 Mellanox Technologies Ltd. All rights reserved.
 * Copyright (c) 2015 System Fabric Works, Inc. All rights reserved.
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * OpenIB.org BSD license below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *	- Redistributions of source code must retain the above
 *	  copyright notice, this list of conditions and the following
 *	  disclaimer.
 *
 *	- Redistributions in binary form must reproduce the above
 *	  copyright notice, this list of conditions and the following
 *	  disclaimer in the documentation and/or other materials
 *	  provided with the distribution.
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

#ifndef RXE_HDR_H
#define RXE_HDR_H

#include "linux_types.h"

/*
 * IBA header types and methods
 *
 * Some of these are for reference and completeness only since
 * rxe does not currently support RD transport
 * most of this could be moved into IB core. ib_pack.h has
 * part of this but is incomplete
 *
 * Header specific routines to insert/extract values to/from headers
 * the routines that are named __hhh_(set_)fff() take a pointer to a
 * hhh header and get(set) the fff field. The routines named
 * hhh_(set_)fff take a packet info struct and find the
 * header and field based on the opcode in the packet.
 * Conversion to/from network byte order from cpu order is also done.
 */

#define RXE_ICRC_SIZE		(4)
#define RXE_MAX_HDR_LENGTH	(80)

/******************************************************************************
 * Base Transport Header
 ******************************************************************************/
struct rxe_bth {
	u8			opcode;
	u8			flags;
	__be16			pkey;
	__be32			qpn;
	__be32			apsn;
};

#define BTH_TVER		(0)
#define BTH_DEF_PKEY		(0xffff)

#define BTH_SE_MASK		(0x80)
#define BTH_MIG_MASK		(0x40)
#define BTH_PAD_MASK		(0x30)
#define BTH_TVER_MASK		(0x0f)
#define BTH_FECN_MASK		(0x80000000)
#define BTH_BECN_MASK		(0x40000000)
#define BTH_RESV6A_MASK		(0x3f000000)
#define BTH_QPN_MASK		(0x00ffffff)
#define BTH_ACK_MASK		(0x80000000)
#define BTH_RESV7_MASK		(0x7f000000)
#define BTH_PSN_MASK		(0x00ffffff)

static inline u8 __bth_opcode(void *arg)
{
	struct rxe_bth *bth = (struct rxe_bth *)arg;

	return bth->opcode;
}

static inline void __bth_set_opcode(void *arg, u8 opcode)
{
	struct rxe_bth *bth = (struct rxe_bth *)arg;

	bth->opcode = opcode;
}

static inline u8 __bth_se(void *arg)
{
	struct rxe_bth *bth = (struct rxe_bth *)arg;

	return 0 != (BTH_SE_MASK & bth->flags);
}

static inline void __bth_set_se(void *arg, int se)
{
	struct rxe_bth *bth = (struct rxe_bth *)arg;

	if (se)
		bth->flags |= BTH_SE_MASK;
	else
		bth->flags &= ~BTH_SE_MASK;
}

static inline u8 __bth_mig(void *arg)
{
	struct rxe_bth *bth = (struct rxe_bth *)arg;

	return 0 != (BTH_MIG_MASK & bth->flags);
}

static inline void __bth_set_mig(void *arg, u8 mig)
{
	struct rxe_bth *bth = (struct rxe_bth *)arg;

	if (mig)
		bth->flags |= BTH_MIG_MASK;
	else
		bth->flags &= ~BTH_MIG_MASK;
}

static inline u8 __bth_pad(void *arg)
{
	struct rxe_bth *bth = (struct rxe_bth *)arg;

	return (BTH_PAD_MASK & bth->flags) >> 4;
}

static inline void __bth_set_pad(void *arg, u8 pad)
{
	struct rxe_bth *bth = (struct rxe_bth *)arg;

	bth->flags = (BTH_PAD_MASK & (pad << 4)) |
			(~BTH_PAD_MASK & bth->flags);
}

static inline u8 __bth_tver(void *arg)
{
	struct rxe_bth *bth = (struct rxe_bth *)arg;

	return BTH_TVER_MASK & bth->flags;
}

static inline void __bth_set_tver(void *arg, u8 tver)
{
	struct rxe_bth *bth = (struct rxe_bth *)arg;

	bth->flags = (BTH_TVER_MASK & tver) |
			(~BTH_TVER_MASK & bth->flags);
}

static inline u16 __bth_pkey(void *arg)
{
	struct rxe_bth *bth = (struct rxe_bth *)arg;

	return be16_to_cpu(bth->pkey);
}

static inline void __bth_set_pkey(void *arg, u16 pkey)
{
	struct rxe_bth *bth = (struct rxe_bth *)arg;

	bth->pkey = cpu_to_be16(pkey);
}

static inline u32 __bth_qpn(void *arg)
{
	struct rxe_bth *bth = (struct rxe_bth *)arg;

	return BTH_QPN_MASK & be32_to_cpu(bth->qpn);
}

static inline void __bth_set_qpn(void *arg, u32 qpn)
{
	struct rxe_bth *bth = (struct rxe_bth *)arg;
	u32 resvqpn = be32_to_cpu(bth->qpn);

	bth->qpn = cpu_to_be32((BTH_QPN_MASK & qpn) |
			       (~BTH_QPN_MASK & resvqpn));
}

static inline int __bth_fecn(void *arg)
{
	struct rxe_bth *bth = (struct rxe_bth *)arg;

	return 0 != (cpu_to_be32(BTH_FECN_MASK) & bth->qpn);
}

static inline void __bth_set_fecn(void *arg, int fecn)
{
	struct rxe_bth *bth = (struct rxe_bth *)arg;

	if (fecn)
		bth->qpn |= cpu_to_be32(BTH_FECN_MASK);
	else
		bth->qpn &= ~cpu_to_be32(BTH_FECN_MASK);
}

static inline int __bth_becn(void *arg)
{
	struct rxe_bth *bth = (struct rxe_bth *)arg;

	return 0 != (cpu_to_be32(BTH_BECN_MASK) & bth->qpn);
}

static inline void __bth_set_becn(void *arg, int becn)
{
	struct rxe_bth *bth = (struct rxe_bth *)arg;

	if (becn)
		bth->qpn |= cpu_to_be32(BTH_BECN_MASK);
	else
		bth->qpn &= ~cpu_to_be32(BTH_BECN_MASK);
}

static inline u8 __bth_resv6a(void *arg)
{
	struct rxe_bth *bth = (struct rxe_bth *)arg;

	return (BTH_RESV6A_MASK & be32_to_cpu(bth->qpn)) >> 24;
}

static inline void __bth_set_resv6a(void *arg)
{
	struct rxe_bth *bth = (struct rxe_bth *)arg;

	bth->qpn = cpu_to_be32(~BTH_RESV6A_MASK);
}

static inline int __bth_ack(void *arg)
{
	struct rxe_bth *bth = (struct rxe_bth *)arg;

	return 0 != (cpu_to_be32(BTH_ACK_MASK) & bth->apsn);
}

static inline void __bth_set_ack(void *arg, int ack)
{
	struct rxe_bth *bth = (struct rxe_bth *)arg;

	if (ack)
		bth->apsn |= cpu_to_be32(BTH_ACK_MASK);
	else
		bth->apsn &= ~cpu_to_be32(BTH_ACK_MASK);
}

static inline void __bth_set_resv7(void *arg)
{
	struct rxe_bth *bth = (struct rxe_bth *)arg;

	bth->apsn &= ~cpu_to_be32(BTH_RESV7_MASK);
}

static inline u32 __bth_psn(void *arg)
{
	struct rxe_bth *bth = (struct rxe_bth *)arg;

	return BTH_PSN_MASK & be32_to_cpu(bth->apsn);
}

static inline void __bth_set_psn(void *arg, u32 psn)
{
	struct rxe_bth *bth = (struct rxe_bth *)arg;
	u32 apsn = be32_to_cpu(bth->apsn);

	bth->apsn = cpu_to_be32((BTH_PSN_MASK & psn) |
			(~BTH_PSN_MASK & apsn));
}

/******************************************************************************
 * Reliable Datagram Extended Transport Header
 ******************************************************************************/
struct rxe_rdeth {
	__be32			een;
};

#define RDETH_EEN_MASK		(0x00ffffff)

static inline u8 __rdeth_een(void *arg)
{
	struct rxe_rdeth *rdeth = (struct rxe_rdeth *)arg;

	return RDETH_EEN_MASK & be32_to_cpu(rdeth->een);
}

static inline void __rdeth_set_een(void *arg, u32 een)
{
	struct rxe_rdeth *rdeth = (struct rxe_rdeth *)arg;

	rdeth->een = cpu_to_be32(RDETH_EEN_MASK & een);
}

/******************************************************************************
 * Datagram Extended Transport Header
 ******************************************************************************/
struct rxe_deth {
	__be32			qkey;
	__be32			sqp;
};

#define GSI_QKEY		(0x80010000)
#define DETH_SQP_MASK		(0x00ffffff)

static inline u32 __deth_qkey(void *arg)
{
	struct rxe_deth *deth = (struct rxe_deth *)arg;

	return be32_to_cpu(deth->qkey);
}

static inline void __deth_set_qkey(void *arg, u32 qkey)
{
	struct rxe_deth *deth = (struct rxe_deth *)arg;

	deth->qkey = cpu_to_be32(qkey);
}

static inline u32 __deth_sqp(void *arg)
{
	struct rxe_deth *deth = (struct rxe_deth *)arg;

	return DETH_SQP_MASK & be32_to_cpu(deth->sqp);
}

static inline void __deth_set_sqp(void *arg, u32 sqp)
{
	struct rxe_deth *deth = (struct rxe_deth *)arg;

	deth->sqp = cpu_to_be32(DETH_SQP_MASK & sqp);
}

/******************************************************************************
 * RDMA Extended Transport Header
 ******************************************************************************/
struct rxe_reth {
	__be64			va;
	__be32			rkey;
	__be32			len;
};

static inline u64 __reth_va(void *arg)
{
	struct rxe_reth *reth = (struct rxe_reth *)arg;

	return be64_to_cpu(reth->va);
}

static inline void __reth_set_va(void *arg, u64 va)
{
	struct rxe_reth *reth = (struct rxe_reth *)arg;

	reth->va = cpu_to_be64(va);
}

static inline u32 __reth_rkey(void *arg)
{
	struct rxe_reth *reth = (struct rxe_reth *)arg;

	return be32_to_cpu(reth->rkey);
}

static inline void __reth_set_rkey(void *arg, u32 rkey)
{
	struct rxe_reth *reth = (struct rxe_reth *)arg;

	reth->rkey = cpu_to_be32(rkey);
}

static inline u32 __reth_len(void *arg)
{
	struct rxe_reth *reth = (struct rxe_reth *)arg;

	return be32_to_cpu(reth->len);
}

static inline void __reth_set_len(void *arg, u32 len)
{
	struct rxe_reth *reth = (struct rxe_reth *)arg;

	reth->len = cpu_to_be32(len);
}

/******************************************************************************
 * Atomic Extended Transport Header
 ******************************************************************************/
struct rxe_atmeth {
	__be64			va;
	__be32			rkey;
	__be64			swap_add;
	__be64			comp;
} __attribute__((__packed__));

static inline u64 __atmeth_va(void *arg)
{
	struct rxe_atmeth *atmeth = (struct rxe_atmeth *)arg;

	return be64_to_cpu(atmeth->va);
}

static inline void __atmeth_set_va(void *arg, u64 va)
{
	struct rxe_atmeth *atmeth = (struct rxe_atmeth *)arg;

	atmeth->va = cpu_to_be64(va);
}

static inline u32 __atmeth_rkey(void *arg)
{
	struct rxe_atmeth *atmeth = (struct rxe_atmeth *)arg;

	return be32_to_cpu(atmeth->rkey);
}

static inline void __atmeth_set_rkey(void *arg, u32 rkey)
{
	struct rxe_atmeth *atmeth = (struct rxe_atmeth *)arg;

	atmeth->rkey = cpu_to_be32(rkey);
}

static inline u64 __atmeth_swap_add(void *arg)
{
	struct rxe_atmeth *atmeth = (struct rxe_atmeth *)arg;

	return be64_to_cpu(atmeth->swap_add);
}

static inline void __atmeth_set_swap_add(void *arg, u64 swap_add)
{
	struct rxe_atmeth *atmeth = (struct rxe_atmeth *)arg;

	atmeth->swap_add = cpu_to_be64(swap_add);
}

static inline u64 __atmeth_comp(void *arg)
{
	struct rxe_atmeth *atmeth = (struct rxe_atmeth *)arg;

	return be64_to_cpu(atmeth->comp);
}

static inline void __atmeth_set_comp(void *arg, u64 comp)
{
	struct rxe_atmeth *atmeth = (struct rxe_atmeth *)arg;

	atmeth->comp = cpu_to_be64(comp);
}

/******************************************************************************
 * Ack Extended Transport Header
 ******************************************************************************/
struct rxe_aeth {
	__be32			smsn;
};

#define AETH_SYN_MASK		(0xff000000)
#define AETH_MSN_MASK		(0x00ffffff)

enum aeth_syndrome {
	AETH_TYPE_MASK		= 0xe0,
	AETH_ACK		= 0x00,
	AETH_RNR_NAK		= 0x20,
	AETH_RSVD		= 0x40,
	AETH_NAK		= 0x60,
	AETH_ACK_UNLIMITED	= 0x1f,
	AETH_NAK_PSN_SEQ_ERROR	= 0x60,
	AETH_NAK_INVALID_REQ	= 0x61,
	AETH_NAK_REM_ACC_ERR	= 0x62,
	AETH_NAK_REM_OP_ERR	= 0x63,
	AETH_NAK_INV_RD_REQ	= 0x64,
};

static inline u8 __aeth_syn(void *arg)
{
	struct rxe_aeth *aeth = (struct rxe_aeth *)arg;

	return (AETH_SYN_MASK & be32_to_cpu(aeth->smsn)) >> 24;
}

static inline void __aeth_set_syn(void *arg, u8 syn)
{
	struct rxe_aeth *aeth = (struct rxe_aeth *)arg;
	u32 smsn = be32_to_cpu(aeth->smsn);

	aeth->smsn = cpu_to_be32((AETH_SYN_MASK & (syn << 24)) |
			 (~AETH_SYN_MASK & smsn));
}

static inline u32 __aeth_msn(void *arg)
{
	struct rxe_aeth *aeth = (struct rxe_aeth *)arg;

	return AETH_MSN_MASK & be32_to_cpu(aeth->smsn);
}

static inline void __aeth_set_msn(void *arg, u32 msn)
{
	struct rxe_aeth *aeth = (struct rxe_aeth *)arg;
	u32 smsn = be32_to_cpu(aeth->smsn);

	aeth->smsn = cpu_to_be32((AETH_MSN_MASK & msn) |
			 (~AETH_MSN_MASK & smsn));
}

/******************************************************************************
 * Atomic Ack Extended Transport Header
 ******************************************************************************/
struct rxe_atmack {
	__be64			orig;
};

static inline u64 __atmack_orig(void *arg)
{
	struct rxe_atmack *atmack = (struct rxe_atmack *)arg;

	return be64_to_cpu(atmack->orig);
}

static inline void __atmack_set_orig(void *arg, u64 orig)
{
	struct rxe_atmack *atmack = (struct rxe_atmack *)arg;

	atmack->orig = cpu_to_be64(orig);
}

/******************************************************************************
 * Immediate Extended Transport Header
 ******************************************************************************/
struct rxe_immdt {
	__be32			imm;
};

static inline __be32 __immdt_imm(void *arg)
{
	struct rxe_immdt *immdt = (struct rxe_immdt *)arg;

	return immdt->imm;
}

static inline void __immdt_set_imm(void *arg, __be32 imm)
{
	struct rxe_immdt *immdt = (struct rxe_immdt *)arg;

	immdt->imm = imm;
}

/******************************************************************************
 * Invalidate Extended Transport Header
 ******************************************************************************/
struct rxe_ieth {
	__be32			rkey;
};

static inline u32 __ieth_rkey(void *arg)
{
	struct rxe_ieth *ieth = (struct rxe_ieth *)arg;

	return be32_to_cpu(ieth->rkey);
}

static inline void __ieth_set_rkey(void *arg, u32 rkey)
{
	struct rxe_ieth *ieth = (struct rxe_ieth *)arg;

	ieth->rkey = cpu_to_be32(rkey);
}

enum rxe_hdr_length {
	RXE_BTH_BYTES		= sizeof(struct rxe_bth),
	RXE_DETH_BYTES		= sizeof(struct rxe_deth),
	RXE_IMMDT_BYTES		= sizeof(struct rxe_immdt),
	RXE_RETH_BYTES		= sizeof(struct rxe_reth),
	RXE_AETH_BYTES		= sizeof(struct rxe_aeth),
	RXE_ATMACK_BYTES	= sizeof(struct rxe_atmack),
	RXE_ATMETH_BYTES	= sizeof(struct rxe_atmeth),
	RXE_IETH_BYTES		= sizeof(struct rxe_ieth),
	RXE_RDETH_BYTES		= sizeof(struct rxe_rdeth),
};

#endif /* RXE_HDR_H */
