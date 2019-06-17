package require Tcl 8.5

set nica_basename [info script]
set nica_basedir [file join [pwd] {*}[lrange [file split $nica_basename] 0 end-1]]

proc get_env {var defvalue} {
    if {[info exists ::env($var)]} {
        return $::env($var)
    } else {
        return $defvalue
    }
}

proc create_project {name top dir files tb_files} {
    set simulation_build [info exists ::env(SIMULATION_BUILD)]

    if {$simulation_build} {
        open_project -reset "$name-sim"
    } else {
        open_project -reset "$name"
    }

    set num_ikernels [get_env "NUM_IKERNELS" 1]
    set num_tc [get_env "NUM_TC" 8]
    set memcached_cache_size [get_env "MEMCACHED_CACHE_SIZE" 4096]
    set memcached_key_size [get_env "MEMCACHED_KEY_SIZE" 10]
    set memcached_value_size [get_env "MEMCACHED_VALUE_SIZE" 10]

    global env nica_basedir
    set gtest_root $env(GTEST_ROOT)
    set_top $top
    set uuid_cflags [exec pkg-config --cflags uuid]
    set uuid_ldflags [exec pkg-config --libs uuid]
    set cflags "-std=gnu++0x $uuid_cflags \
                -I$nica_basedir/hls \
                -I$nica_basedir/../ntl \
                -I$nica_basedir/../ikernels/hls \
                -I$nica_basedir/../ikernels/hls/tests \
                -I$gtest_root/include \
                -Wno-gnu-designator -DNUM_IKERNELS=$num_ikernels \
                -DNUM_TC=$num_tc"
    if {$simulation_build} {
        set cflags "$cflags -DSIMULATION_BUILD=1"
    }
    puts $memcached_cache_size
    if {$memcached_cache_size ne ""} {
        set cflags "$cflags -DMEMCACHED_CACHE_SIZE=$memcached_cache_size"
    }
    puts $memcached_key_size
    if {$memcached_key_size ne ""} {
        set cflags "$cflags -DMEMCACHED_KEY_SIZE=$memcached_key_size"
    }
    puts $memcached_value_size
    if {$memcached_value_size ne ""} {
        set cflags "$cflags -DMEMCACHED_VALUE_SIZE=$memcached_value_size"
    }

    set ldflags "-lpcap -lssl -lcrypto $uuid_ldflags -L$gtest_root -lgtest -L$nica_basedir/../build/nica/ -lnica-csim"

    foreach f $files {
        set f [file join $dir $f]
        add_files $f -cflags $cflags
    }

    foreach f $tb_files {
        if {![file exists $f]} {
            set f [file join $dir $f]
        }
        add_files -tb $f -cflags $cflags
    }

    open_solution "40Gbps"
    set_part {xcku060-ffva1156-2-i}
    create_clock -period "5.185ns"
    config_rtl -prefix ${name}_
    config_rtl -disable_start_propagation
    config_interface -m_axi_addr64
    if {[string match "*2018.2" $::ap_root]} {
        # Our pattern of using a class to hold state breaks dataflow strict mode
        config_dataflow -strict_mode off
    }
    #if {[llength $tb_files] > 0} {
    #    csim_design -ldflags $ldflags
    #}
    csynth_design
    if {$simulation_build && [llength $tb_files] > 0} {
        cosim_design -O -ldflags $ldflags -trace_level none
    } else {
        export_design -format ip_catalog
    }
}
