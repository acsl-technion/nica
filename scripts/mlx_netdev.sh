#!/bin/bash

# fpga_mst_dev=$(/usr/bin/ls /dev/mst/*fpga_i2c)
# cx_mst_dev=${fpga_mst_dev%%_fpga_i2c}

# bdf=$(sudo mst status | grep -Pzo "${cx_mst_dev}.*\n.*domain:bus:dev.fn=\K(.*)(?= addr)")
# netdev=$(/usr/bin/ls -d /sys/bus/pci/devices/$bdf/net/* | xargs -n1 basename)
# echo $netdev

candidates=$(/usr/bin/ls -d /sys/bus/pci/drivers/mlx5_core/*/net/* | xargs -n1 basename)
declare -a ether
ether=()
for netdev in $candidates ; do
    if /usr/sbin/ip link show $netdev | grep ether > /dev/null ; then
        ether+=($netdev)
    fi
done
declare -a has_ip
has_ip=()
for netdev in $ether ; do
    if /usr/sbin/ip addr show dev $netdev | grep inet > /dev/null ; then
        has_ip+=($netdev)
    fi
done
echo ${has_ip[@]}
