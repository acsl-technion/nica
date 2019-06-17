#!/bin/bash

for pid in $(ps -ef | grep "sockperf" | awk '{print $2}'); do sudo kill -2 $pid; done

