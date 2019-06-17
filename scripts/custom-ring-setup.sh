#!/bin/bash

basedir="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

# Find FPGA Innova netdev
dev=$($basedir/mlx_netdev.sh)

echo Setting up custom ring related settings for \"$dev\"

ip=10.0.0.1
fpga_mac=00:00:00:00:00:01

# image 640 / MLNX OFED 3.3 requires enabling PFC on priority 0
mlnx_qos -i $dev --pfc=1,0,0,0,0,0,0,0
# image 2768 / MLNX OFED 4.0 requires enabling global pauses
ethtool -A $dev rx on tx on

# Find local ConnectX MAC
local_mac=$(ip link show $dev | sed -n s'# *link/ether \(.*\) brd.*#\1#p')
local_ip=$(/sbin/ip -o -4 addr list $dev | awk '{print $4}' | cut -d/ -f1)

# Set a static route
sudo ip route replace $ip/32 dev $dev
sudo ip neigh replace to $ip dev $dev lladdr $fpga_mac

# Disable ICRC and RoCE IP header checksum checks in ConnectX hardware
# .rxt.checks.rxt_checks_packet_checks_wrapper.g_check_mask.packet_checks_action0.bits_ng.bad_icrc
sudo mcra /dev/mst/mt4117_pciconf0 0x5363c.12:1 0
# .rxt.checks.rxt_checks_packet_checks_wrapper.g_check_mask.packet_checks_action1.bits_ng.bad_icrc
sudo mcra /dev/mst/mt4117_pciconf0 0x5367c.12:1 0
# .rxt.checks.rxt_checks_packet_checks_wrapper.g_check_mask.packet_checks_action0.bits_ng.bad_roce_l3_header_corrupted
sudo mcra /dev/mst/mt4117_pciconf0 0x53628.3:1 0
# .rxt.checks.rxt_checks_packet_checks_wrapper.g_check_mask.packet_checks_action1.bits_ng.bad_roce_l3_header_corrupted
sudo mcra /dev/mst/mt4117_pciconf0 0x53668.3:1 0
