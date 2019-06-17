#!/bin/bash
#[input]: packet_size burst_size [tokens nwp_sx_gen_credits_init cxp_rx_passthrough_credits_init]

register=../register.sh

if [ $# -lt 2 ]; then
	echo "Usage: ./ipktgen.sh packet_size burst_size [tokens [nwp_sx_gen_credits_init cxp_rx_passthrough_credits_init] ]"
	exit 1
fi

packet_size=$1
burst_size=$2
tokens=`$register 0x458 r 0x21`
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
		if [ $input_total -gt $exp_total ]; then
			echo "input_total is $input_total not equal to $exp_total"
			exit 1
		fi
	fi
fi


# update credits in lossless fifo (cxp_rx and nwp_sx)
echo passthrugh_credits_init = $passthrough_credits_init
sudo mcra -a ~haggai/newton.adb /dev/mst/mt4117_pciconf0_fpga 0x990004.9 1
sudo mcra -a ~haggai/newton.adb /dev/mst/mt4117_pciconf0_fpga 0x990004.0 $passthrough_credits_init
sudo mcra -a ~haggai/newton.adb /dev/mst/mt4117_pciconf0_fpga 0x990004.9 0
echo gen_credits_init = $gen_credits_init
sudo mcra -a ~haggai/newton.adb /dev/mst/mt4117_pciconf0_fpga 0x9b1010.9 1
sudo mcra -a ~haggai/newton.adb /dev/mst/mt4117_pciconf0_fpga 0x9b1010.0 $gen_credits_init
sudo mcra -a ~haggai/newton.adb /dev/mst/mt4117_pciconf0_fpga 0x9b1010.9 0

# change the number of tokens
echo tokens=$tokens
$register 0x458 w 0x21 $tokens

echo run_client...
../run_client.sh $burst_size $packet_size offload $burst_size
