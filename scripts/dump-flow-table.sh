#!/bin/bash

declare -A regs=([source ip]=0x10 \
      [dest ip]=0x11 \
      [source port]=0x12 \
      [dest port]=0x13 \
      [dest port]=0x13 \
      [result_action]=0x18 \
      [valid]=0x20)

for reg in "${!regs[@]}" ; do
  printf "$reg,"
done

printf "entry\n"

#for entry in $(seq 1023) ; do
for entry in 149 150 956 957 ; do
  register.sh 0x418 w 0x5 $entry
  for reg in "${!regs[@]}" ; do
    register.sh 0x418 r ${regs[$reg]} | xargs echo -n
    printf ","
  done
  printf "$entry\n"
done
