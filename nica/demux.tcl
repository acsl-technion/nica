set basename [info script]
set basedir [file join [pwd] {*}[lrange [file split $basename] 0 end-1]]
source "$basedir/ikernel.tcl"

create_project demux demux_arb_top $basedir/hls {
    demux.cpp arbiter.cpp
} {tests/arbiter_tests.cpp udp.cpp}
exit
