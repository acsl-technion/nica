#!/bin/bash

dir=$(dirname $0)
. $dir/fpga.inc.sh

regs=(0x9b0000 0x9b1000 0x990000 0x991000)
values=(0x20 0x1 0x20 0x1)
set=$1
let i=0
for reg in ${regs[@]} ; do
    cur=$($read_cmd $reg)
    if [[ -z "$set" ]] ; then
        echo $cur
    elif [[ "$set" == "set" ]] ; then
        cur=$[$cur | ${values[$i]}]
    elif [[ "$set" == "clear" ]] ; then
        cur=$[$cur & ~${values[$i]}]
    fi
    echo $write_cmd $reg $cur
    $write_cmd $reg $cur
    let i=$[i+1]
done
	
