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
# [input]: secs threshold_value b/i [baseline/ikernel]
#sudo env  VMA_RX_NO_CSUM=1 VMA_NICA_ACCESS_MODE=0  LD_PRELOAD=libvma.so LD_LIBRARY_PATH=~/libvma/src/vma/.libs/ ./threshold_server -s $1 -v $2 -I enp2s0
if [[ "$2" == "b" ]]
then
  sudo env LD_LIBRARY_PATH=~/libvma/src/vma/.libs/ LD_PRELOAD=libvma.so VMA_NICA_EMULATION=1 ./cms_server -s $1 -I enp2s0
else
  sudo env VMA_NICA_ACCESS_MODE=0 VMA_RX_NO_CSUM=1 LD_PRELOAD=libvma.so LD_LIBRARY_PATH=~/libvma/src/vma/.libs/ ./cms_server -s $1 -I enp2s0
fi
#sudo env LD_PRELOAD=libvma.so LD_LIBRARY_PATH=~/libvma/src/vma/.libs/ VMA_NICA_EMULATION=1 ./threshold_server -s $1 -v $2 -I enp2s0

