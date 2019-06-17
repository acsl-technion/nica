#!/bin/bash
# [input]: secs b/i [baseline/ikernel]
if [[ "$2" == "b" ]]
then
  sudo env LD_LIBRARY_PATH=~/libvma/src/vma/.libs/ LD_PRELOAD=libvma.so VMA_NICA_EMULATION=1 ./echo_server -s $1 -I enp2s0
else
  sudo env VMA_NICA_ACCESS_MODE=0 VMA_RX_NO_CSUM=1 LD_PRELOAD=libvma.so LD_LIBRARY_PATH=~/libvma/src/vma/.libs/ ./echo_server -s $1 -I enp2s0
fi

