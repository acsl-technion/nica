#!/bin/bash

# Find FPGA Innova netdev
devs=$(ls /dev/mst/mt4117_pciconf*_fpga_i2c)

echo Disabling Mellanox shell credits

function set_credits_enable() {
    local dev="$1"
    local reg="$2"
    cur=$(sudo mlx_fpga -d $dev r $reg | grep -v 'Unsupported SBU ID')
    sudo mlx_fpga -d $dev w $reg $[$cur | (1 << 15)] | grep -v 'Unsupported SBU ID'
}

for dev in $devs ; do
    # .cxp.prt_config.prt_config_buffers.rxb_config.fifo0_rxb_credits_enable
    set_credits_enable $dev 0x990004
    # .nwp.prt_config.prt_config_buffers.rxb_config.fifo0_rxb_credits_enable
    set_credits_enable $dev 0x9b0004
done
