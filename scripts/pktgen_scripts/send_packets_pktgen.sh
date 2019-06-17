#!/bin/bash
#[input]: unicast/broadcast cores 

if [ $# -lt 2 ]; then
        echo "Usage: ./get_throughput.sh password [broadcast/unicast]"
        exit 1
fi

password=$1
cores=12
if [ $# -eq 2 ] ; then
	cores=$2
fi

mode=$1
mac_addr="ff:ff:ff:ff:ff:ff"

if [ $mode == unicast ] ; then
	mac_addr="7c:fe:90:5f:9d:1e"
fi

echo $password | sudo -S /home/haggai/pktgen/pktgen_sample02_multiqueue.sh -i enp2s0 -d 192.168.0.3 -m $mac_addr -s 18 -t $cores -c 30 -p 1
