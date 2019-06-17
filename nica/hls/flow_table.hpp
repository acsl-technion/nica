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

#ifndef FLOW_TABLE_HPP
#define FLOW_TABLE_HPP

enum flow_table_action {
    FT_PASSTHROUGH = 0,
    FT_DROP = 1,
    FT_IKERNEL = 2,
};

enum flow_table_fields {
    FT_FIELD_SRC_IP = 1 << 0,
    FT_FIELD_DST_IP = 1 << 1,
    FT_FIELD_SRC_PORT = 1 << 2,
    FT_FIELD_DST_PORT = 1 << 3,
    FT_FIELD_VM_ID = 1 << 4,
};

#define FLOW_TABLE_LOG_SIZE 10
#define FLOW_TABLE_SIZE (1 << FLOW_TABLE_LOG_SIZE)

#define FT_FIELDS 0
/* A read from this address causes the flow that was previously set through the
 * FT_KEY_* and FT_RESULT_* registers to be added to the flow table, returning
 * the flow ID or zero if the operation failed. */
#define FT_ADD_FLOW 0x1
/* A read from this address causes the flow that was set through FT_KEY_*
 * registers to be removed from the flow table. */
#define FT_DELETE_FLOW 0x2
#define FT_SET_ENTRY 0x4
#define FT_READ_ENTRY 0x5

#define FT_KEY_SADDR 0x10
#define FT_KEY_DADDR 0x11
#define FT_KEY_SPORT 0x12
#define FT_KEY_DPORT 0x13
#define FT_RESULT_ACTION 0x18
#define FT_RESULT_ENGINE 0x19
#define FT_RESULT_IKERNEL_ID 0x1a

/* Used with FT_SET_ENTRY and FT_READ_ENTRY to indicate valid/invalid entries */
#define FT_VALID 0x20

#endif
