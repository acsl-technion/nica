#!/bin/bash -e

# Start tools driver
modprobe -v mlx_accel_tools

# Re-load fpga
mlx_fpga -d /dev/mst/mt4117_pciconf0_fpga load --user

# Unload drivers
modprobe -vr mlx_accel_tools

sudo mlxfwreset --yes -d /dev/mst/mt4117_pciconf0 --level 3 reset

# Start driver
modprobe -v mlx5_ib

# delay
sleep 1s

# Start tools driver
modprobe -v mlx_accel_tools

# Start tools
mst start --with_fpga
