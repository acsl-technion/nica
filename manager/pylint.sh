#!/bin/bash

pylint=$1
shift

$pylint "$@"
ret="$?"

if [[ "$ret" -lt 3 ]] ; then
    exit 1
else
    exit 0
fi
