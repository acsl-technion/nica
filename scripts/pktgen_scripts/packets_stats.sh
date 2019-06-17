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
let passthrough=$(sudo mlx_fpga -d /dev/mst/mt4117_pciconf0_fpga r 0xa8)
let drop=$(sudo mlx_fpga -d /dev/mst/mt4117_pciconf0_fpga r 0xb4)
let generated=$(sudo mlx_fpga -d /dev/mst/mt4117_pciconf0_fpga r 0xc0)
echo --------flow table n2h---------
echo n2h_flow_table_passthrough: $passthrough
echo n2h_flow_table_drop: $drop
echo n2h_flow_table_generated: $generated
echo 
let passthrough=$(sudo mlx_fpga -d /dev/mst/mt4117_pciconf0_fpga r 0x298)
let drop=$(sudo mlx_fpga -d /dev/mst/mt4117_pciconf0_fpga r 0x2a4)
let generated=$(sudo mlx_fpga -d /dev/mst/mt4117_pciconf0_fpga r 0x2b0)
echo --------flow table h2n---------
echo h2n_flow_table_passthrough: $passthrough
echo h2n_flow_table_drop: $drop
echo h2n_flow_table_generated: $generated
echo
let passthrough=$(sudo mlx_fpga -d /dev/mst/mt4117_pciconf0_fpga r 0x244)
let drop=$(sudo mlx_fpga -d /dev/mst/mt4117_pciconf0_fpga r 0x250)
let generated=$(sudo mlx_fpga -d /dev/mst/mt4117_pciconf0_fpga r 0x25c)
echo --------flow table n2h---------
echo n2h_ik0_passthrough: $passthrough
echo n2h_ik0_drop: $drop
echo n2h_ik0_generated: $generated
echo 
let passthrough=$(sudo mlx_fpga -d /dev/mst/mt4117_pciconf0_fpga r 0x484)
let drop=$(sudo mlx_fpga -d /dev/mst/mt4117_pciconf0_fpga r 0x490)
let generated=$(sudo mlx_fpga -d /dev/mst/mt4117_pciconf0_fpga r 0x49c)
echo --------flow table h2n---------
echo h2n_ik0_passthrough: $passthrough
echo h2n_ik0_drop: $drop
echo h2n_ik0_generated: $generated

