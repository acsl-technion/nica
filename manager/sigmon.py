#!/usr/bin/env python3

'''
Signal monitoring control module - enable and access the monitoring logic. See
sigmon_top.v for more details on the interface.
'''

import operator
from functools import reduce
import struct
from enum import IntEnum
from nica import NICA

def nica_event(index):
    '''Calculate a NICA event index.'''
    return 0xb0 + index

class Events(IntEnum):
    '''Enumeration of all the events.'''

    NO_EVENT = 0x00
    EVENT_TRUE = 0x01
    EVENT_FALSE = 0x02

    NWP2SBU_EOP = 0x10
    NWP2SBU_SOP = 0x11
    NWP2SBU_RDY = 0x12
    NWP2SBU_VLD = 0x13
    NWP2SBU_LAST = 0x14
    NWP2SBU_EOP_TEMP = 0x15
    # 0x16 reserved
    NWP2SBU_LOSSLESS_CREDITS = 0x17
    NWP2SBU_LOSSLESS_CREDITS_ON = 0x18
    NWP2SBU_LOSSLESS_CREDITS_OFF = 0x19

    SBU2NWP_EOP = 0x20
    SBU2NWP_SOP = 0x21
    SBU2NWP_RDY = 0x22
    SBU2NWP_VLD = 0x23
    SBU2NWP_LAST = 0x24
    SBU2NWPFIFO_EOP = 0x25
    SBU2NWPFIFO_SOP = 0x26

    CXP2SBU_EOP = 0x30
    CXP2SBU_SOP = 0x31
    CXP2SBU_RDY = 0x32
    CXP2SBU_VLD = 0x33
    CXP2SBU_LAST = 0x34
    CXP2SBU_EOP_TEMP = 0x35
    # 0x36 reserved
    CXP2SBU_LOSSLESS_CREDITS = 0x37
    CXP2SBU_LOSSLESS_CREDITS_ON = 0x38
    CXP2SBU_LOSSLESS_CREDITS_OFF = 0x39

    SBU2CXP_EOP = 0x40
    SBU2CXP_SOP = 0x41
    SBU2CXP_RDY = 0x42
    SBU2CXP_VLD = 0x43
    SBU2CXP_LAST = 0x44
    SBU2CXPFIFO_EOP = 0x45
    SBU2CXPFIFO_SOP = 0x46

    DRAM_IK_WADDR = 0x70
    DRAM_WADDR = 0x71
    DRAM_WADDR_RDY = 0x72
    DRAM_WADDR_VLD = 0x73

    DRAM_WDATA = 0x74
    DRAM_WDATA_RDY = 0x75
    DRAM_WDATA_VLD = 0x76
    DRAM_WDATA_LAST = 0x77

    DRAM_WDONE = 0x78
    DRAM_WDONE_RDY = 0x79
    DRAM_WDONE_VLD = 0x7a

    DRAM_IK_RADDR = 0x7b
    DRAM_RADDR = 0x7c
    DRAM_RADDR_RDY = 0x7d
    DRAM_RADDR_VLD = 0x7e

    DRAM_RDATA = 0x80
    DRAM_RDATA_RDY = 0x81
    DRAM_RDATA_VLD = 0x82
    DRAM_RDATA_LAST = 0x83

    @staticmethod
    def local_event(index):
        '''Calculate a local event index.'''
        return 0xa0 + index

    NICA_N2H_ARB_PORT_0 = nica_event(0)
    NICA_N2H_ARB_PORT_1 = nica_event(1)
    NICA_N2H_ARB_PORT_2 = nica_event(2)
    NICA_N2H_ARB_EVICTED = nica_event(3)

    NICA_H2N_ARB_PORT_0 = nica_event(4)
    NICA_H2N_ARB_PORT_1 = nica_event(5)
    NICA_H2N_ARB_PORT_2 = nica_event(6)
    NICA_H2N_ARB_EVICTED = nica_event(7)

    TIMESTAMP_24TOGGLE = 0xf7
    TIMESTAMP_HIGH = 0xf8
    DRAM_TEST_ENABLE = 0xfc
    SIGMON_ENABLED = 0xff

class CLBEvents(IntEnum):
    '''Configurable logic block events.'''

    OUT = 0x0
    OUT_ON = 0x1
    OUT_OFF = 0x2
    START = 0x3
    MID = 0x4
    END = 0x5

    @staticmethod
    def identifier(clb_index, clb_event):
        '''CLB event index calculator.'''
        return 0x50 + clb_index * 8 + clb_event

class Event(object):
    '''Parsed events including metadata.'''
    def __init__(self, value):
        if value is None:
            self.event_id = None
            self.clb_index = None
            return

        try:
            self.event_id = Events(value)
            self.clb_index = None
            return
        except ValueError:
            pass

        first = CLBEvents.identifier(0, CLBEvents.OUT)
        if first and value <= CLBEvents.identifier(3, CLBEvents.END):
            self.event_id = CLBEvents(value & 0x7)
            self.clb_index = (value - first) >> 3
        else:
            raise ValueError()

    def __str__(self):
        ret = str(self.event_id)
        if self.clb_index is not None:
            ret += '(%d)' % self.clb_index
        return ret

class ConfigurableLogicBlock(object):
    '''Tracks a combination of signals. See sigmon_logic_block.v.'''

    SIGMON_CTRL10 = 0x8030
    SIGMON_CTRL22 = 0x8070

    def __init__(self, _nica, index):
        self.nica = _nica
        self.index = index
        self.base = self.SIGMON_CTRL10 + self.index * 0x10

    def signal_selector_addr(self):
        '''Signal selection ctrl register address.'''
        return self.base

    def start_interval_limit_addr(self):
        '''Start interval limit ctrl register address.'''
        return self.base + 0x4

    def mid_interval_limit_addr(self):
        '''Mid interval limit ctrl register address.'''
        return self.base + 0x8

    def status(self):
        '''Status register address.'''
        return (self.nica.axi_read(0x802c) >> (self.index * 8)) & 0xff

    @staticmethod
    def logic_equation_function(negate, is_or):
        '''Return the logic equation byte of the given negate[4] and is_or[3] lists.'''
        ret = 0
        for j in range(min(4, len(negate))):
            print(j)
            neg = negate[j]
            ret = ret | (neg << (6 - j * 2))
            print('0x%02x' % ret)
        for j in range(3):
            print(j)
            if len(is_or) > j:
                func = is_or[j]
            else:
                func = 1
            ret = ret | (func << (5 - j * 2))
            print('0x%02x' % ret)
        return ret

    def select_signals(self, signals, negate, is_or):
        '''Configure a given set of signals and a boolean function of this CLB.'''
        assert len(signals) <= 4
        events = reduce(operator.or_, (signal << j * 8 for (j, signal) in enumerate(signals)))
        print('CLB %d: writing 0x%08x to 0x%08x' % (self.index, events,
                                                    self.signal_selector_addr()))
        self.nica.axi_write(self.signal_selector_addr(), events)

        equation = self.logic_equation_function(negate, is_or)
        ctrl22 = NICA.axi_read(self.nica, self.SIGMON_CTRL22) or 0
        ctrl22 = ctrl22 | (equation << (self.index * 8))
        print('CLB %d: writing 0x%08x to 0x%08x' % (self.index, ctrl22, self.SIGMON_CTRL22))
        self.nica.axi_write(self.SIGMON_CTRL22, ctrl22)

    def set_intervals(self, start=None, mid=None):
        '''Set the start and mid intervals.'''
        if start is not None:
            self.nica.axi_write(self.start_interval_limit_addr(), start)
        if mid is not None:
            self.nica.axi_write(self.mid_interval_limit_addr(), mid)

class Sigmon(object):
    '''Signal monitoring control object.'''

    sigmon_base = 0x8000

    sigmon_ctrl1 = 0x8000
    sigmon_status = 0x8004
    sigmon_fifo_data = 0x8008
    sigmon_ctrl2 = 0x8010
    sigmon_ctrl3 = 0x8014
    sigmon_ctrl4 = 0x8018
    sigmon_ctrl5 = 0x801c
    sigmon_ctrl6 = 0x8020

    CTRL1_SOURCE_SHIFT = 16
    CTRL1_SIGMON_ENABLE = 0x80000000

    def __init__(self, _nica):
        self.nica = _nica
        self.clbs = [ConfigurableLogicBlock(_nica, i) for i in range(4)]

    def has_sigmon(self):
        '''Check whether image has signal monitoring enabled.'''
        return self.nica.axi_read(self.sigmon_status) != 0xdeadf00d

    def enable_sigmon(self, trigger, window=2, delay=None):
        '''Enable signal monitoring with a given trigger source.'''
        window = window & 0xfff
        cmd = self.CTRL1_SIGMON_ENABLE | (trigger << self.CTRL1_SOURCE_SHIFT | window)
        #print(hex(self.sigmon_ctrl1), hex(cmd))
        self.nica.axi_write(self.sigmon_ctrl1, cmd, delay=delay)
        self.nica.axi_write(self.sigmon_ctrl6, 0xff, delay=delay)

    def disable(self, delay):
        '''Disable signal monitoring.'''
        cmd = 0
        #print(hex(self.sigmon_ctrl1), hex(cmd))
        self.nica.axi_write(self.sigmon_ctrl1, cmd, delay=delay)

    def configure_events(self, events, delay=None):
        '''Configure a list of events to record.'''
        num_events = len(events)
        if num_events > 16:
            raise Exception("Too many events")
        if num_events < 16:
            events = events + (16 - num_events) * [0]
        quads = [events[i:i+4] for i in range(0, len(events), 4)]
        for i, quad in enumerate(quads):
            events = [event << ((3 - j) * 8) for (j, event) in enumerate(quad)]
            word = reduce(operator.or_, events, 0)
            #print(hex(self.sigmon_ctrl2 + i * 4), hex(word))
            self.nica.axi_write(self.sigmon_ctrl2 + i * 4, word, delay=delay)

    def read_status(self, delay):
        '''Read the status register.'''
        result = self.nica.axi_read(self.sigmon_status, delay=delay)
        print("Read status register: %x" % (result))
        num_elements_lo, num_elements_hi = (struct.unpack('HH', struct.pack('I', result)))
        num_elements_hi &= 0x3 # two extra bits are here
        return (num_elements_hi << 16) | num_elements_lo

    def read_fifo(self, delay):
        '''Read the event FIFO.'''
        result = self.nica.axi_read(self.sigmon_fifo_data, delay=delay)
        if result:
            event_id = result >> 24
            timestamp = result & ((1 << 24) - 1)
            return (event_id, timestamp)
        else:
            return None, None

def main():
    '''Main CLI entrypoint.'''
    from nica import NicaHardware
    import sys

    path = '/dev/mst/mt4117_pciconf0_fpga_i2c'
    nica = NicaHardware(path)
    print('Mellanox image version: %d' % nica.axi_read(0x900000))
    print('Build number: %d' % (nica.axi_read(0x900024) >> 16))

    sigmon = Sigmon(nica)

    if not sigmon.has_sigmon():
        print('No signal monitoring core detected.')
        sys.exit(1)

    cmd = sys.argv[1]

    if cmd == 'enable':
        sigmon.clbs[0].select_signals([Events.DRAM_WADDR_VLD], [0], [])
        sigmon.clbs[1].select_signals([Events.DRAM_WDATA_VLD], [0], [])
        sigmon.clbs[2].select_signals([Events.DRAM_WDONE_VLD], [0], [])
        #sigmon.clbs[3].select_signals([Events.DRAM_WDATA_VLD], [0], [])
        for i in range(3):
            sigmon.clbs[i].set_intervals(start=0, mid=0)
        events = [[CLBEvents.identifier(i, CLBEvents.OUT_ON), CLBEvents.identifier(i, CLBEvents.OUT_OFF)]
                  for i in range(4)]
        events = sum(events, [])
        sigmon.configure_events(events)
        sigmon.enable_sigmon(Events.SIGMON_ENABLED, delay=10)

    elif cmd == 'disable':
        sigmon.disable(10)

    elif cmd == 'log':
        #sigmon.disable(10)
        num_elements = sigmon.read_status(10)
        print("%d entries:" % num_elements)
        results = []
        for _ in range(num_elements):
            event_id, timestamp = sigmon.read_fifo(10)
            try:
                event = Event(event_id)
                results.append((timestamp, event_id, event))
            except ValueError:
                print("Invalid event: %d" % event_id)
                #break

        results.sort(key=lambda tup: tup[0])
        for timestamp, identifier, event in results:
            sys.stdout.write("%8d: 0x%02x %s" % (timestamp, identifier, event))
            sys.stdout.write('\n')

if __name__ == '__main__':
    main()
