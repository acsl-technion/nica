#!/bin/bash

cmd="sudo mlx_fpga -d /dev/mst/mt4117_pciconf0_fpga"
read="$cmd read"
write="$cmd write"

capture_next=0x60
capture_ack=0x68

capture_ap_vld=0x160
capture_start=0x134

echo Hold on...
echo $write $capture_next 0
$write $capture_next 0
echo $write $capture_ack 1
$write $capture_ack 1
#sleep 1
echo $write $capture_ack 0
$write $capture_ack 0
echo $write $capture_next 1
$write $capture_next 1

echo Capturing...
#sleep 3

let addr=$[$capture_start]
while [[ $addr -lt $[$capture_ap_vld] ]] ; do
  echo $read $addr
  $read $addr
  let addr=$[addr + 4]
done
