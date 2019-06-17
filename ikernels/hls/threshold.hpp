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

#ifndef THRESHOLD_HPP
#define THRESHOLD_HPP

// 2b49fec3-d30c-464d-a6a1-171f9e4443f1
#define THRESHOLD_UUID { 0x2b, 0x49, 0xfe, 0xc3, 0xd3, 0x0c, 0x46, 0x4d, 0xa6, 0xa1, 0x17, 0x1f, 0x9e, 0x44, 0x43, 0xf1 }

#define THRESHOLD_MIN 0x10
#define THRESHOLD_MAX 0x14
#define THRESHOLD_COUNT 0x18
#define THRESHOLD_SUM_LO 0x1c
#define THRESHOLD_SUM_HI 0x20
#define THRESHOLD_VALUE 0x24
#define THRESHOLD_DROPPED 0x28
#define THRESHOLD_DROPPED_BACKPRESSURE 0x29
#define THRESHOLD_RING_ID 0x2c

#define THRESHOLD_FLAGS 0x30

#define THRESHOLD_RESET 0x10000

enum threshold_flags {
    THRESHOLD_FLAG_TCP = 1 << 0,
};

#define LOG_NUM_THRESHOLD_CONTEXTS 6
#define NUM_THRESHOLD_CONTEXTS (1 << LOG_NUM_THRESHOLD_CONTEXTS)

#endif // THRESHOLD_HPP
