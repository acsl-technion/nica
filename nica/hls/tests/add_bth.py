#!/usr/bin/env python

# Copyright (c) 2016-2017 Haggai Eran, Gabi Malka, Lior Zeno, Maroun Tork
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without modification,
# are permitted provided that the following conditions are met:
#
#  * Redistributions of source code must retain the above copyright notice, this
# list of conditions and the following disclaimer.
#  * Redistributions in binary form must reproduce the above copyright notice,
# this list of conditions and the following disclaimer in the documentation and/or
# other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ``AS IS''
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
# ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
# ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


from scapy.all import *
import sys
from optparse import OptionParser
from roce import *

parser = OptionParser()
parser.add_option("-r", "--reverse", action="store_true", dest="reverse",
                  default=False, help="Reverse destination and source addresses")
parser.add_option("", "--dest-port", dest="dport",
                  help="Filter UDP destination port")
parser.add_option("", "--dqpn", dest="dqpn", default=0x1,
                  help="Destination QP")
options, args = parser.parse_args()
infile, outfile = args

bth_len = len(BTH())
icrc_len = len(ICRC(icrc=1))

pkts = rdpcap(infile)
out = []
psn = 0
def print_pkt(p):
    i = 0
    cur = p.getlayer(i)
    while cur:
        print type(cur)
        for f in cur.fields_desc:
            val = cur.getfieldval(f.name)
            print "%s = %s (%s)" % (f.name, val, type(val)) 
        i += 1
        cur = p.getlayer(i)

for p in pkts:
    if p.haslayer(IP) and p.haslayer(UDP):
        if options.dport is None or options.dport == p[UDP].dport:
            #print_pkt(p)
            #print 'IP: %d, UDP: %d, payload: %d' % (p[IP].len, p[UDP].len, \
            #    len(p[UDP].payload))
            p[Ether].src = '00:00:00:00:00:00'
            p[Ether].dst = 'ff:ff:ff:ff:ff:ff'
            p[IP].src = '127.0.0.1'
            p[IP].dst = '127.0.0.1'
            p[IP].flags = 0
            p[UDP].sport = 0xf00d
            p[UDP].dport = 4791
            data = bytes(p[UDP].payload)[14:18]
            data += '\x00' * (4 - len(data))
            payload_len = len(data)
            p[UDP].payload = BTH(opcode=0x24, dqpn=int(options.dqpn), psn=psn, pkey=0xffff,
                                 padcount=((- payload_len) % 4))/data
            psn += 1
            #p[BTH].payload = data / icrc(p)
            p[BTH].payload = data / ICRC(icrc=0)

            p[IP].len = payload_len + 20 + 8 + bth_len + icrc_len
            p[UDP].len = payload_len + 8 + bth_len + icrc_len
            p[UDP].chksum = 0
            p[IP].chksum = 0
            #print 'IP: %d, UDP: %d, payload: %d' % (p[IP].len, p[UDP].len, \
            #    len(p[UDP].payload))
            #p[UDP].payload.show2()
            #print_pkt(p)

    if len(p) < 60:
        p = Ether(str(p) + '\0' * (60 - len(p)))
    out.append(p)

wrpcap(outfile, out)
