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

. fpga.inc.sh

gpio_datain=$(sudo mcra /dev/mst/mt4117_pciconf0 0xf2004)
fpga_done=$[$gpio_datain & (1 << 6)]
image_select=$[$gpio_datain & (1 << 9)]
force_load=$[$gpio_datain & (1 << 8)]

if [ "$fpga_done" -eq 0 ] ; then
	echo "FPGA not loaded"
else
	echo "FPGA loaded"
fi

if [ "$image_select" -eq 0 ] ; then
	echo "Image: user"
else
	echo "Image: factory"
fi

if [ "$force_load" -eq 0 ] ; then
	echo "Not force loading"
else
	echo "Force load"
fi

mlnx_image_number=$[$($read_cmd 0x900000)]
build_number=$[$($read_cmd 0x900024) >> 16]

echo "Mellanox image #${mlnx_image_number}"
echo "Build number #${build_number}"
