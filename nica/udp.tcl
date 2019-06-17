set basename [info script]
set basedir [file join [pwd] {*}[lrange [file split $basename] 0 end-1]]
source "$basedir/ikernel.tcl"

create_project udp udp_top $basedir/hls {
    udp.cpp flow_table.cpp
} {}
exit
