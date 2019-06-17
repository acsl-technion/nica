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

tarball=${1:-newton_ku060_2_40g_v640.tar}

if [ "$#" -lt 1 ] ; then
  echo "Missing argument: tarball"
  echo "Using: $tarball as a default"
fi

if [ ! -f "$tarball" ] ; then
  echo "File not found: $tarball"
  exit 1
fi

rm -rf user

dirs=(user/{examples/exp_hls/{vlog,xdc},mlx,project,scripts,tb/exp_vlog})
tar xvf $tarball ${dirs[@]}
patch -p1 -d user < ../scripts/mellanox-shell-scripts.patch

ln -snf examples/exp_hls user/sources

ln -snf ../../../../nica/nica/40Gbps/impl/ip/hdl/verilog user/examples/exp_hls/vlog/nica
for ikernel in cms echo memcached passthrough pktgen threshold ; do
  ln -snf ../../../../ikernels/$ikernel/40Gbps/impl/ip/hdl/verilog user/examples/exp_hls/vlog/$ikernel
done

ln -snf ../../../../nica/xci user/examples/exp_hls/xci
cd user/examples/exp_hls/vlog
cp -sf ../../../../../nica/verilog/* .
cd -
