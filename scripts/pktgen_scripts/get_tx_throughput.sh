
#!/bin/bash
#input multicast/unicast packets/bits

if [ $# -ne 2 ]; then
        echo "Usage: ./get_throughput.sh [multicast/unicast] [packets/bytes]"
        exit 1
fi

mode=$1
pkts_bits=$2
escape_char=\'

if [ $mode == unicast ] ; then
        if [ $pkts_bits == packets ] ; then
		watch -n0 'result=`start=$(ethtool -S enp2s0|grep tx_vport_unicast_packets|cut -d: -f2); sleep 1 ; end=$(ethtool -S enp2s0|grep tx_vport_unicast_packets|cut -d: -f2); echo $[$end - $start]/1000000 | bc -l`; echo $result Mpps'
        else
                watch -n0 'result=`start=$(ethtool -S enp2s0|grep tx_vport_unicast_bytes|cut -d: -f2); sleep 1 ; end=$(ethtool -S enp2s0|grep tx_vport_unicast_bytes|cut -d: -f2); echo $[8*($end - $start)]/1024/1024/1024 | bc -l`; echo $result Gbps'
        fi
else
        if [ $pkts_bits == packets ] ; then
                watch -n0 'result=`start=$(ethtool -S enp2s0|grep tx_vport_multicast_packets|cut -d: -f2); sleep 1 ; end=$(ethtool -S enp2s0|grep tx_vport_multicast_packets|cut -d: -f2); echo $[$end - $start]/1000000 | bc -l`; echo $result Mpps'
        else
                watch -n0 'result=`start=$(ethtool -S enp2s0|grep tx_vport_multicast_bytes|cut -d: -f2); sleep 1 ; end=$(ethtool -S enp2s0|grep tx_vport_multicast_bytes|cut -d: -f2); echo $[8*($end - $start)]/1024/1024/1024 | bc -l`; echo $result Gbps'
        fi
fi

