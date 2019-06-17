#!/bin/bash

dir=$(/usr/bin/dirname $0)
dev=${1:-$($dir/mlx_netdev.sh)}
/usr/sbin/ip -4 a show dev $dev | /usr/bin/grep inet | /usr/bin/awk '{print $2}' | /usr/bin/cut -f1  -d'/'
