#!/bin/bash

counter=$1
dev=$2

start=$(ethtool -S $dev | grep $counter | cut -d: -f2)
sleep 1
end=$(ethtool -S $dev | grep $counter | cut -d: -f2)

echo $[$end - $start]
