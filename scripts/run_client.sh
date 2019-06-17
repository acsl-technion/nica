#!/bin/bash
#input: number_of_packets payload_size [offlad  burst_size] 

basedir="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

if [ $# -lt 2 ];then
	echo "Usage: ./run_client.sh number_of_packets payload_size [offlad  burst_size]"
	exit 1
fi

number_of_packets=$1
payload_size=$2

offload=FALSE
if [ $# -gt 3 ] ; then
	offload=TRUE
	burst_size=$4
fi


libvma_dir=$basedir/../../../libvma/src/vma/.libs
client=$basedir/../build/baseline/client
# Find an mlx5_core netdev
dev=$($basedir/mlx_netdev.sh)
# Find local IP address
local_ip=$($basedir/mlx_ip.sh $dev)
remote_ip=192.168.0.8
cmd="LD_PRELOAD=libvma.so LD_LIBRARY_PATH=$libvma_dir $client -c $number_of_packets -I $dev -H $remote_ip -p 1111 -l $local_ip -s $payload_size"

if [ $offload == TRUE ] ; then
	sudo env VMA_RX_NO_CSUM=1 VMA_NICA_ACCESS_MODE=0 $cmd --offload -b $burst_size
else 
	sudo env $cmd
fi
