#!/bin/bash

sizes=(64 128 256 512)

for packet_size in "${sizes[@]}"; do
	./run_variable_tokens.sh 'host_'$packet_size'.log' 'ikernel_'$packet_size'.log' $packet_size
done
