set basename [info script]
set basedir [file join [pwd] {*}[lrange [file split $basename] 0 end-1]]
source "$basedir/ikernel.tcl"

create_project custom_ring custom_ring_top $basedir/hls {
    custom_rx_ring.cpp
} {}
exit
