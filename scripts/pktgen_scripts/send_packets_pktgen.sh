#!/bin/bash
#[input]: unicast/broadcast cores 

if [ $# -lt 3 ]; then
        echo "Usage: ./get_throughput.sh password [broadcast/unicast] packet_size"
        exit 1
fi

password=$1 ;shift
cores=12
mode=$1;shift
packet_size=$1
mac_addr="ff:ff:ff:ff:ff:ff"

if [ $mode == unicast ] ; then
	mac_addr="7c:fe:90:5f:9d:1e"
fi

echo $password | sudo -S /home/haggai/pktgen/pktgen_sample02_multiqueue.sh -i enp2s0 -d 192.168.0.3 -m $mac_addr -s 18 -t $cores -c 30 -p 1
