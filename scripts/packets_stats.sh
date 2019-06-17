#!/bin/bash
#
# Copyright (c) 2016-2017 Haggai Eran, Gabi Malka, Lior Zeno, Maroun Tork
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without modification,
# are permitted provided that the following conditions are met:
#
#  * Redistributions of source code must retain the above copyright notice, this
# list of conditions and the following disclaimer.
#  * Redistributions in binary form must reproduce the above copyright notice,
# this list of conditions and the following disclaimer in the documentation and/or
# other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ``AS IS''
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
# ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
# ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
let passthrough=$(sudo mlx_fpga -d /dev/mst/mt4117_pciconf0_fpga r 0xc8)
let drop=$(sudo mlx_fpga -d /dev/mst/mt4117_pciconf0_fpga r 0xd4)
let to_ikernel=$(sudo mlx_fpga -d /dev/mst/mt4117_pciconf0_fpga r 0xe0)
echo --------flow table n2h---------
echo n2h_flow_table_passthrough: $passthrough
echo n2h_flow_table_drop: $drop
echo n2h_flow_table_to_ikernel: $to_ikernel
echo 
let passthrough=$(sudo mlx_fpga -d /dev/mst/mt4117_pciconf0_fpga r 0x2c0)
let drop=$(sudo mlx_fpga -d /dev/mst/mt4117_pciconf0_fpga r 0x2cc)
let to_ikernel=$(sudo mlx_fpga -d /dev/mst/mt4117_pciconf0_fpga r 0x2d8)
echo --------flow table h2n---------
echo h2n_flow_table_passthrough: $passthrough
echo h2n_flow_table_drop: $drop
echo h2n_flow_table_to_ikernel: $to_ikernel
echo
let passthrough=$(sudo mlx_fpga -d /dev/mst/mt4117_pciconf0_fpga r 0x26c)
let drop=$(sudo mlx_fpga -d /dev/mst/mt4117_pciconf0_fpga r 0x278)
let generated=$(sudo mlx_fpga -d /dev/mst/mt4117_pciconf0_fpga r 0x284)
echo --------ikernel n2h---------
echo n2h_ik0_passthrough: $passthrough
echo n2h_ik0_drop: $drop
echo n2h_ik0_generated: $generated
echo 
let passthrough=$(sudo mlx_fpga -d /dev/mst/mt4117_pciconf0_fpga r 0x4bc)
let drop=$(sudo mlx_fpga -d /dev/mst/mt4117_pciconf0_fpga r 0x4c8)
let generated=$(sudo mlx_fpga -d /dev/mst/mt4117_pciconf0_fpga r 0x4d4)
echo --------ikernel h2n---------
echo h2n_ik0_passthrough: $passthrough
echo h2n_ik0_drop: $drop
echo h2n_ik0_generated: $generated
echo
for ik in $(seq 0 5) ; do
        echo ikernel $ik
	let memcached_get_requests=$(. register.sh 0x1014 r 0x10 $ik)
	let memcached_get_requests_hits=$(. register.sh 0x1014 r 0x11 $ik)
	let memcached_get_requests_misses=$(. register.sh 0x1014 r 0x12 $ik)
	let memcached_set_requests=$(. register.sh 0x1014 r 0x13 $ik)
	let memcached_n2h_unknown=$(. register.sh 0x1014 r 0x14 $ik)
	let memcached_get_responses=$(. register.sh 0x1014 r 0x15 $ik)
	let memcached_h2n_unknown=$(. register.sh 0x1014 r 0x16 $ik)
	let memcached_dropped_backpressure=$(. register.sh 0x1014 r 0x17 $ik)
	let memcached_ring_id=$(. register.sh 0x1014 r 0x18 $ik)
	echo MEMCACHED_STATS_GET_REQUESTS: $memcached_get_requests
	echo MEMCACHED_STATS_GET_REQUESTS_HITS: $memcached_get_requests_hits
	echo MEMCACHED_STATS_GET_REQUESTS_MISSES: $memcached_get_requests_misses
	echo MEMCACHED_STATS_GET_RESPONSES: $memcached_get_responses
	echo
	echo MEMCACHED_STATS_SET_REQUESTS: $memcached_set_requests
	echo
	echo MEMCACHED_STATS_N2H_UNKNOWN: $memcached_n2h_unknown
	echo MEMCACHED_STATS_H2N_UNKNOWN: $memcached_h2n_unknown
	echo MEMCACHED_DROPPED_BACKPRESSURE $memcached_dropped_backpressure
	echo
	echo MEMCACHED_RING_ID $memcached_ring_id
	echo
done
