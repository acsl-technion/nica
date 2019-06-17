#!/bin/bash

#GDB="gdb --args"
#[input] : [time_to_run_in_seconds[default-inf] packet_size]

LIBVMA=/home/maroun/libvma/src/vma/.libs/libvma.so

cores=6
time_to_run=1000000
packet_size=64
headers_size=46

if [ $# -eq 2 ]; then
        time_to_run=$1
	packet_size=$2
fi

payload_size=$[$packet_size-$headers_size]

for i in `seq 1 1 $cores`; do
    echo running server number $i
    let j=($i-1)
    let k=(1111+$j)

sudo \
        taskset -c $j \
        $GDB env \
#                LD_PRELOAD=$LIBVMA \
                VMA_MTU=200 \
                VMA_RX_POLL=200000 \
                VMA_RX_POLL_INIT=200000 \
                VMA_RX_UDP_POLL_OS_RATIO=0 \
                VMA_RX_BUFS=30000 \
                VMA_THREAD_MODE=0 \
                VMA_TX_BUFS=200000 \
                VMA_TX_WRE=16000 \
/home/maroun/sockperf/sockperf tp --mc-tx-if=192.168.0.5 -i 192.168.0.8 -m $payload_size -t $time_to_run --load-vma --daemonize --nonblocked --dontwarmup --mps max

#/home/maroun/sockperf/sockperf tp --mc-tx-if=192.168.0.5 -i 224.4.4.4 -m $payload_size -t $time_to_run --load-vma --daemonize --nonblocked --dontwarmup --mps max
done

