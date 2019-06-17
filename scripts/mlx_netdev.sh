#!/bin/bash

# fpga_mst_dev=$(/usr/bin/ls /dev/mst/*fpga_i2c)
# cx_mst_dev=${fpga_mst_dev%%_fpga_i2c}

# bdf=$(sudo mst status | /usr/bin/grep -Pzo "${cx_mst_dev}.*\n.*domain:bus:dev.fn=\K(.*)(?= addr)")
# netdev=$(/usr/bin/ls -d /sys/bus/pci/devices/$bdf/net/* | /usr/bin/xargs -n1 /usr/bin/basename)
# echo $netdev

candidates=$(/usr/bin/ls -d /sys/bus/pci/drivers/mlx5_core/*/net/* | /usr/bin/xargs -n1 /usr/bin/basename)
declare -a ether
ether=()
for netdev in $candidates ; do
    if /usr/sbin/ip link show $netdev | /usr/bin/grep ether > /dev/null ; then
        ether+=($netdev)
    fi
done
declare -a has_ip
has_ip=()
for netdev in $ether ; do
    if /usr/sbin/ip addr show dev $netdev | /usr/bin/grep inet > /dev/null ; then
        has_ip+=($netdev)
    fi
done
echo ${has_ip[@]}
