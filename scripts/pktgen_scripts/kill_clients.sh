#!/bin/bash

for pid in $(ps -ef | grep "/bin/bash /home/haggai/pktgen/pktgen_sample02_multiqueue.s" |grep taskset | awk '{print $2}'); do sudo kill -2 $pid; done
