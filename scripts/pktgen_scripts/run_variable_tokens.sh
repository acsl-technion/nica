#!/bin/bash
#input: file_name packet_size

if [ $# -ne 4 ]; then
	echo "Usag: ./... password ikernel_file_name host_file_name packet_size"
	exit 1
fi

password=$1;shift
ikernel_file=$1
host_file=$2
packet_size=$3

if [ -f $ikernel_file ]; then 
	rm -f $ikernel_file
fi
if [ -f $host_file ]; then
        rm -f $host_file
fi

MAX_DELTA=100 #92
burst_size=$[300<<22]
#nwp_sx_generate_credits=0xc0
#cxp_rx_passthrough_credits=0x40
nwp_sx_generate_credits=""
cxp_rx_passthrough_credits=""


sudo mlx_fpga -d /dev/mst/mt4117_pciconf0_fpga load
sudo modprobe -vr mlx_accel_tools
sudo modprobe -v mlx_accel_tools && sudo mst restart --with_fpga
sleep 5s

if [ -f temp_host ];then
	rm -f temp_host
fi
if [ -f temp_ikernel ];then
	rm -f temp_ikernel
fi

for ((delta = 0; $delta <= $MAX_DELTA ; delta=$delta+10)); do
	rm -f temp_ikernel
	rm -f temp_host
	host_tokens=$[$MAX_DELTA-$delta]
	ikernel_tokens=$delta
	echo $host_tokens >> temp_host
	echo $ikernel_tokens >> temp_ikernel
	for i in `seq 1 5`; do
		./ipktgen.sh $packet_size $burst_size $ikernel_tokens $host_tokens $nwp_sx_generate_credits $cxp_rx_passthrough_credits
	        ./run_sockperf_client.sh 20s $packet_size &
        	sleep 2s
        	echo $password | sudo -S sshpass -p $password ssh -tt maroun@gpu-os07.ef.technion.ac.il "~/pktgen/get_stats.sh unicast bits" >> temp_ikernel
	        echo $password | sudo -S sshpass -p $password ssh -tt maroun@gpu-os07.ef.technion.ac.il "~/pktgen/get_stats.sh broadcast bits" >> temp_host
        	sleep 7s
		./kill_sockperf_client.sh
		sudo mlx_fpga -d /dev/mst/mt4117_pciconf0_fpga load
        	sudo modprobe -vr mlx_accel_tools
	        sudo modprobe -v mlx_accel_tools && sudo mst restart --with_fpga
        	sleep 5s
	done
	less temp_ikernel | grep -v $password | cut -d" " -f1 | tr '\n' ' ' >> $ikernel_file
	echo "" >> $ikernel_file
	less temp_host | grep -v $password | cut -d" " -f1 | tr '\n' ' ' >> $host_file
	echo "" >> $host_file
	if [ "$delta" -eq 80 ] ; then
		delta=82 #to get the max number of tokens in the next iteration
	fi
done


#change from tokens to MAX Gbps
awk '{ temp=$1/92*40;$1=""; {printf "%.1f",temp};print $0}' $host_file > host_temp && cat host_temp > $host_file
rm -rf host_temp
awk '{ temp=$1/92*40;$1=""; {printf "%.1f",temp};print $0}' $ikernel_file > ikernel_temp && cat ikernel_temp > $ikernel_file
rm -rf ikernel_temp

python plot_bar.py -n 'Pktgen ikernel' -ib $host_file -ii $ikernel_file -x 'Max BandWidth for ikernel [Gbps]' -y 'Bandwidth [Gbps]' -o 'pktgen_bandwidth_'$packet_size
