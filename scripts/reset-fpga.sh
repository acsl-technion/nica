#!/bin/bash -e

# Start tools driver
modprobe -v mlx_accel_tools

# Re-load fpga
mlx_fpga -d /dev/mst/mt4117_pciconf0_fpga_i2c load

# Unload drivers
modprobe -vr mlx_accel_tools

# Start driver
modprobe -v mlx5_ib

# delay
sleep 1s

# Start tools driver
modprobe -v mlx_accel_tools

# Start tools
mst start --with_fpga
