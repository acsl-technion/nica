#!/bin/bash

# Find FPGA Innova netdev
dev=/dev/mst/mt4117_pciconf0_fpga_i2c

echo Disabling Mellanox shell credits

function set_credits_enable() {
    cur=$(sudo mlx_fpga -d $dev r $1)
    sudo mlx_fpga -d $dev w $1 $[$cur | (1 << 9)]
}

# .cxp.prt_config.prt_config_buffers.rxb_config.fifo0_rxb_credits_enable
set_credits_enable 0x990004
# .nwp.prt_config.prt_config_buffers.rxb_config.fifo0_rxb_credits_enable
set_credits_enable 0x9b0004
