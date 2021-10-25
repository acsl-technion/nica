#!/bin/bash

dir=$(dirname $0)
dev=${1:-$($dir/mlx_netdev.sh)}
ip -4 a show dev $dev | grep inet | awk '{print $2}' | cut -f1  -d'/'
