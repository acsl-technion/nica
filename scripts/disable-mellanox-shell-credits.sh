#!/bin/bash

# Find FPGA Innova netdev
dev=/dev/mst/mt4117_pciconf0_fpga

echo Disabling Mellanox shell credits

# .cxp.prt_config.prt_config_buffers.rxb_config.fifo0_rxb_credits_enable
sudo mcra -a ~haggai/newton.adb $dev 0x990004.9 1
# .nwp.prt_config.prt_config_buffers.rxb_config.fifo0_rxb_credits_enable
sudo mcra -a ~haggai/newton.adb $dev 0x9b0004.9 1
