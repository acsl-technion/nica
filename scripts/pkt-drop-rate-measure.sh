#!/bin/bash

function measure()
{
	start=$(cat /sys/class/net/enp2s0/statistics/rx_dropped)
	sleep 5
	end=$(cat /sys/class/net/enp2s0/statistics/rx_dropped)
	echo "scale=2 ; ($end - $start) / 5" | bc
}

for i in {1..3} ; do
	measure
done
