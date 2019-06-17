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

parser = OptionParser()
parser.add_option("-r", "--reverse", action="store_true", dest="reverse",
                  default=False, help="Reverse destination and source addresses")
parser.add_option("", "--dest-port", dest="dport",
                  help="Filter UDP destination port")
options, args = parser.parse_args()
infile, outfile = args

pkts = rdpcap(infile)
out = []
for p in pkts:
    if p.haslayer(IP) and p.haslayer(UDP):
        if options.dport is None or options.dport == p[UDP].dport:
            p[IP].chksum = 0
            p[IP].flags = 0
            p[UDP].chksum = 0
            if options.reverse:
                p[UDP].sport, p[UDP].dport = p[UDP].dport, p[UDP].sport
                p[IP].src, p[IP].dst = p[IP].dst, p[IP].src
                p[Ether].src, p[Ether].dst = p[Ether].dst, p[Ether].src
    if len(p) < 60:
        p = Ether(str(p) + '\0' * (60 - len(p)))
    out.append(p)

wrpcap(outfile, out)
