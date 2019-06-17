set basename [info script]
set basedir [file join [pwd] {*}[lrange [file split $basename] 0 end-1]]
source "$basedir/ikernel.tcl"

create_project flow_table flow_table_top $basedir/hls {
    flow_table.cpp udp.cpp ikernel.cpp
} {tests/flow_table_tests.cpp}
exit
