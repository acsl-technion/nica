#!/bin/bash
#input: file_name packet_size

if [ $# -ne 3 ]; then
	echo "Usag: ./... file_name packet_size password"
	exit 1
fi

file=$1
packet_size=$2
password=$3

if [ -f $file ]; then 
	rm -f $file
fi

MAX_TOKENS=512
burst_size=$[300<<19]
nwp_sx_generate_credits=0xc0
cxp_rx_passthrough_credits=0x40

sudo mlx_fpga -d /dev/mst/mt4117_pciconf0_fpga load
sudo modprobe -vr mlx_accel_tools
sudo modprobe -v mlx_accel_tools && sudo mst restart --with_fpga
sleep 5s

if [ -f temp ];then
	rm -f temp
fi

tokens=0
echo $tokens >> temp
for i in `seq 1 5`; do
	./ipktgen.sh $packet_size $burst_size $tokens $nwp_sx_generate_credits $cxp_rx_passthrough_credits
        sleep 1s
	echo $password | sudo -S sshpass -p $password ssh -tt maroun@gpu-os07.ef.technion.ac.il "~/pktgen/get_stats.sh unicast bits" >> temp
        sleep 7s
        sudo mlx_fpga -d /dev/mst/mt4117_pciconf0_fpga load
        sudo modprobe -vr mlx_accel_tools
        sudo modprobe -v mlx_accel_tools && sudo mst restart --with_fpga
        sleep 5s
done

less temp | grep -v $password | cut -d" " -f1 | tr '\n' ' ' >> $file
echo "" >> $file


for ((tokens = 1; $tokens < $MAX_TOKENS ; tokens=$tokens*2)); do
	rm -f temp
	tokens_hexa=`echo "obase=16; $tokens" | bc`
	echo $tokens >> temp
	for i in `seq 1 5`; do
		./ipktgen.sh $packet_size $burst_size $tokens_hexa $nwp_sx_generate_credits $cxp_rx_passthrough_credits 
		sleep 1s
		echo $password | sudo -S sshpass -p $password ssh -tt maroun@gpu-os07.ef.technion.ac.il "~/pktgen/get_stats.sh unicast bits" >> temp
		sleep 7s
		sudo mlx_fpga -d /dev/mst/mt4117_pciconf0_fpga load
		sudo modprobe -vr mlx_accel_tools
		sudo modprobe -v mlx_accel_tools && sudo mst restart --with_fpga
		sleep 5s
	done
	less temp | grep -v $password | cut -d" " -f1 | tr '\n' ' ' >> $file
	echo "" >> $file
done
