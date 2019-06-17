#!/bin/bash
#[input]: packet_size burst_size [tokens nwp_sx_gen_credits_init cxp_rx_passthrough_credits_init]

if [ $# -lt 2 ]; then
	echo "Usage: ./ipktgen.sh packet_size burst_size [tokens [nwp_sx_gen_credits_init cxp_rx_passthrough_credits_init] ]"
	exit 1
fi

packet_size=$1
burst_size=$2
tokens=0
gen_credits_init=""
passthrough_credits_init=""

if [ $# -gt 2 ]; then
	tokens=$3
	if [ $# -eq 4 ]; then
		echo "you have to update credits in cxp and nwp simultaneously"
		exit 1
	fi
	if [ $# -eq 5 ]; then
		gen_credits_init=$4
		passthrough_credits_init=$5	
		num1=`echo $[0x40]`; num2=`echo $[0xc0]`
		echo $num1 $num2
		let exp_total=($num1 + $num2)
		let input_total=($gen_credits_init + $passthrough_credits_init)
		if [ $exp_total -ne $input_total ]; then
			echo "input_total is $input_total not equal to $exp_total"
			exit 1
		fi
	fi
fi


# -1) update credits in lossless fifo (cxp_rx and nwp_sx)
echo passthrugh_credits_init = $passthrough_credits_init
sudo mcra -a ~haggai/newton.adb /dev/mst/mt4117_pciconf0_fpga 0x990004.9 1
sudo mcra -a ~haggai/newton.adb /dev/mst/mt4117_pciconf0_fpga 0x990004.0 $passthrough_credits_init
sudo mcra -a ~haggai/newton.adb /dev/mst/mt4117_pciconf0_fpga 0x990004.9 0
echo gen_credits_init = $gen_credits_init
sudo mcra -a ~haggai/newton.adb /dev/mst/mt4117_pciconf0_fpga 0x9b1010.9 1
sudo mcra -a ~haggai/newton.adb /dev/mst/mt4117_pciconf0_fpga 0x9b1010.0 $gen_credits_init
sudo mcra -a ~haggai/newton.adb /dev/mst/mt4117_pciconf0_fpga 0x9b1010.9 0

# 0) enable nica pipeline
echo enable nica pipeline
sudo mlx_fpga -d /dev/mst/mt4117_pciconf0_fpga w 0x410 1
sudo mlx_fpga -d /dev/mst/mt4117_pciconf0_fpga w 0x10 1

# 1) init burst_size in pktgen ikernel
echo burst_size = $burst_size
./register.sh 0x1014 w 0x10 $burst_size

# 2) attatch the ikernel manually
echo ik_attach
./register.sh 0x418 w 0x18 2
./register.sh 0x418 w 0 0

# 3) change the number of tokens
if [ `echo $[$tokens]` -ne 0 ];then
	./register.sh 0x458 w 0x21 $tokens
fi
echo tokens=`./register.sh 0x458 r 0x21`

# 4) send packets
dd if=/dev/zero of=$packet_size"_file" bs=$packet_size count=1
file $packet_size"_file"
echo run_client...
echo $packet_size"_file"
cat $packet_size"_file" > /dev/udp/192.168.0.3/1111
