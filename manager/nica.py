#!/usr/bin/env python3

'''
NICA control module - used both for the NICA manager and for simulation input generation.
'''

import struct
import os
import socket
import fcntl
import logging
import glob
from ipaddress import ip_address
from uuid import UUID
from abc import ABC, abstractmethod
from time import clock

TIMEOUT=5 # seconds

class Gateway(object):
    '''Wrap a hardware RPC gateway.'''
    def __init__(self, nica, base, done_delay=200, cmd_delay=25):
        self.nica = nica
        self.cmd = base
        self.data_i = base + 0x8
        self.data_o = base + 0x10
        self.done = base + 0x18
        self.ikernel_id = base + 0x20
        self.done_delay = done_delay
        self.cmd_delay = cmd_delay

    cmd_write = 1 << 30
    cmd_go = 1 << 31

    def write(self, address, value, ikernel_id=None, delay=None):
        '''Write a value to the gateway.'''
        if ikernel_id is not None:
            self.nica.axi_write(self.ikernel_id, ikernel_id, delay=delay)
        self.nica.axi_write(self.data_i, value, delay=10)
        self.nica.axi_write(self.cmd, address | self.cmd_write | self.cmd_go, delay=self.cmd_delay)
        start = clock()
        while clock() - start <= TIMEOUT:
            ret = self.nica.axi_read(self.done, delay=self.done_delay)
            if ret is None or ret:
                break
        if ret == 0:
            raise TimeoutError()
        self.nica.axi_write(self.cmd, 0, delay=self.cmd_delay)
        start = clock()
        while self.nica.axi_read(self.done, delay=self.done_delay):
            if clock() - start > TIMEOUT:
                raise TimeoutError()

    def read(self, address, ikernel_id=None, delay=None):
        '''Read a value from the gateway.'''
        if ikernel_id is not None:
            self.nica.axi_write(self.ikernel_id, ikernel_id, delay=delay)
        self.nica.axi_write(self.cmd, address | self.cmd_go, delay=10)
        start = clock()
        while clock() - start <= TIMEOUT:
            ret = self.nica.axi_read(self.done, delay=self.done_delay)
            if ret is None or ret:
                break
        if ret == 0:
            raise TimeoutError()
        ret = self.nica.axi_read(self.data_o, delay=self.cmd_delay)
        self.nica.axi_write(self.cmd, 0, delay=self.cmd_delay)
        start = clock()
        while self.nica.axi_read(self.done, delay=self.done_delay):
            if clock() - start > TIMEOUT:
                raise TimeoutError()
        return ret

class Arbiter(Gateway):
    '''Control the NICA packet scheduler.'''

    ARBITER_NUM_TC = 0
    ARBITER_SCHEDULER = 0x10
    ARBITER_SCHEDULER_STRIDE = 0x2

    SCHEDULER_DRR_QUANTUM = 0
    SCHEDULER_DRR_DEFICIT = 1

    def __init__(self, nica, base, done_delay=100, cmd_delay=25):
        super(Arbiter, self).__init__(nica, base, done_delay, cmd_delay)

    def quantum_address(self, traffic_class):
        '''Calculate gateway offset of the quantum of a given TC'''
        return self.ARBITER_SCHEDULER + traffic_class * self.ARBITER_SCHEDULER_STRIDE + \
               self.SCHEDULER_DRR_QUANTUM

    def set_quantum(self, traffic_class, quantum, delay=None):
        """Speed will be quantum / (sum of all quanta)"""
        self.write(self.quantum_address(traffic_class), quantum, delay=delay)

    def get_quantum(self, traffic_class, delay=None):
        '''Return the current quantum'''
        return self.read(self.quantum_address(traffic_class), delay=delay)

class MMU(object):
    BASE = 0x9000

    def __init__(self, nica):
        self.nica = nica

    def set_mapping(self, ikernel_id, ddr_address, delay=None):
        assert isinstance(ikernel_id, int)
        assert isinstance(ddr_address, int)
        # Page aligned
        assert ddr_address & ((1 << 12) - 1) == 0

        addr = ddr_address >> 12
        logging.debug('set_mapping(AXI-lite address 0x%x, base=0x%x)',
                      self.BASE + 4 * ikernel_id, addr)
        self.nica.axi_write(self.BASE + 4 * ikernel_id, addr, delay=delay)

class NICA(ABC):
    '''NICA's main control object. This is an abstract class derived by the simulation version and
    the hardware version.'''
    def __init__(self):
        self.n2h_flow_table = FlowTable(self, 0x18, 250)
        self.h2n_flow_table = FlowTable(self, 0x418, 250)
        self.ikernel0 = Gateway(self, 0x1014)
        self.ikernels = [self.ikernel0]
        self.n2h_arbiter = Arbiter(self, 0x58)
        self.h2n_arbiter = Arbiter(self, 0x458)
        self.custom_ring = CustomRing(self, 0x78)
        self.mmu = MMU(self)
        self.axi_cache = {}

    @abstractmethod
    def axi_read(self, address, delay=None):
        '''Read a value from AXI4-Lite.'''
        return self.axi_cache.get(address, None)

    @abstractmethod
    def axi_write(self, address, value, delay=None):
        '''Write a value to AXI4-Lite.'''
        self.axi_cache[address] = value

    def get_uuid(self, ikernel=0):
        '''Read the UUID of a given ikernel index.'''
        assert ikernel == 0

        uuid = bytes()
        for i in range(4):
            uuid += self.axi_read(0x1000 + i * 4).to_bytes(4, byteorder='little')
        return UUID(bytes=uuid)

    def enable_all_flows(self, remote_port=None, local_port=None):
        """Enable NICA for all flows"""
        self.enable()

        if remote_port or local_port:
            self.n2h_flow_table.set_flow_table_mask(daddr=False, saddr=False,
                                                    dport=local_port, sport=remote_port, delay=10)
            self.h2n_flow_table.set_flow_table_mask(daddr=False, saddr=False,
                                                    dport=remote_port, sport=local_port, delay=10)

        remote_port = remote_port or 0
        local_port = local_port or 0
        self.n2h_flow_table.set_flow(daddr=socket.INADDR_ANY, dport=local_port,
                                     saddr=socket.INADDR_ANY, sport=remote_port,
                                     action=FlowTable.FT_IKERNEL, delay=10)
        self.h2n_flow_table.set_flow(saddr=socket.INADDR_ANY, dport=remote_port,
                                     daddr=socket.INADDR_ANY, sport=local_port,
                                     action=FlowTable.FT_IKERNEL, delay=10)

    def enable(self):
        '''Enable NICA's global switch.'''
        self.axi_write(0x010, 1, delay=10)
        self.axi_write(0x410, 1, delay=10)

    def disable(self):
        '''Disable NICA's global switch.'''
        self.axi_write(0x010, 0, delay=10)
        self.axi_write(0x410, 0, delay=10)

    def update_credits(self, ring, max_msn, reset=False, delay=None):
        '''Update the given ring's credits.'''
        cmd = ring | max_msn << 7 | reset << 23
        self.axi_write(0x1050, cmd, delay=delay)

class NicaSimulation(NICA):
    '''Implement NICA for generating simulation inputs and expected outputs.'''
    def __init__(self):
        super(NicaSimulation, self).__init__()
        self.total_delay = 0

    def axi_write(self, address, value, delay=None):
        '''Write a value to AXI4-Lite.'''
        print("%d: 0 %x %x\n" % (delay, address, value))
        self.total_delay += delay
        return super(NicaSimulation, self).axi_write(address, value, delay=delay)

    def axi_read(self, address, delay=None):
        '''Read a value from AXI4-Lite.'''
        print("%d: 1 %x\n" % (delay, address))
        self.total_delay += delay
        return super(NicaSimulation, self).axi_read(address, delay=delay)

def ioctl_ioc(ioctl_dir, ioctl_type, ioctl_nr, size):
    '''Calculate ioctl number.'''
    return ioctl_dir << 30 | \
        size << 16 | \
        ioctl_type << 8 | \
        ioctl_nr

def ioctl_iow(ioctl_type, ioctl_nr, size):
    '''Calculate an ioctl number for write.'''
    return ioctl_ioc(1, ioctl_type, ioctl_nr, size)

def inet_aton(ip_addr):
    '''Convert an IP address string to an int.'''
    return int.from_bytes(ip_address(ip_addr).packed, byteorder='big', signed=False)

class CustomRing(Gateway):
    '''Control the custom ring hardware interface.'''
    CR_DST_MAC_LO = 0
    CR_DST_MAC_HI = 1
    CR_SRC_MAC_LO = 2
    CR_SRC_MAC_HI = 3
    CR_DST_IP = 4
    CR_SRC_IP = 5
    CR_DST_UDP = 6
    CR_SRC_UDP = 7

    CR_NUM_CONTEXTS = 0xa

    CR_DST_QPN = 0x10
    CR_PSN = 0x11
    CR_WRITE_CONTEXT = 0x1e

    def __init__(self, nica, base, done_delay=250, cmd_delay=25):
        super(CustomRing, self).__init__(nica, base, done_delay, cmd_delay)

    def num_rings(self, delay=None):
        '''Number of custom ring contexts available in hardware.'''
        return self.read(self.CR_NUM_CONTEXTS, delay=delay)

    def set_mac(self, source, mac, delay=None):
        '''Set the MAC addresses used by the custom ring when sending packets to the host.'''
        if source:
            addr_lo, addr_hi = self.CR_SRC_MAC_LO, self.CR_SRC_MAC_HI
        else:
            addr_lo, addr_hi = self.CR_DST_MAC_LO, self.CR_DST_MAC_HI

        mac = mac.split(':')
        words = (mac[2:], mac[:2])
        mac_lo, mac_hi = (int(''.join(x), 16) for x in words)
        self.write(addr_lo, mac_lo, delay=delay)
        self.write(addr_hi, mac_hi, delay=delay)

    def set_ip(self, source, ip_addr, delay=None):
        '''Set the IP addresses used by the custom ring for sending to the host.'''
        if source:
            addr = self.CR_SRC_IP
        else:
            addr = self.CR_DST_IP

        self.write(addr, inet_aton(ip_addr), delay=delay)

    def set_udp_port(self, source, port, delay=None):
        '''Set RoCE UDP port for the custom ring.'''
        self.write(self.CR_SRC_UDP if source else self.CR_DST_UDP, port, delay=delay)

    def set_custom_ring(self, ring, mac='00:00:00:00:00:00', dst_ip='0.0.0.0', qpn=0, psn=0,
                        delay=None):
        '''Set a given custom ring's context (QPN and initial PSN).'''
        self.set_mac(False, mac, delay=delay)
        self.set_ip(False, dst_ip, delay=10)
        self.write(self.CR_DST_QPN, qpn, delay=10)
        self.write(self.CR_PSN, psn, delay=10)
        self.write(self.CR_WRITE_CONTEXT, ring, delay=10)

class FlowTable(Gateway):
    '''Control the flow table hardware interface.'''
    # actions
    FT_PASSTHROUGH = 0
    FT_IKERNEL = 2

    # registers
    FT_FIELDS = 0x0
    FT_ADD_FLOW = 1
    FT_DELETE_FLOW = 2
    FT_KEY_SADDR = 0x10
    FT_KEY_DADDR = 0x11
    FT_KEY_SPORT = 0x12
    FT_KEY_DPORT = 0x13
    FT_RESULT_ACTION = 0x18
    FT_RESULT_IKERNEL = 0x19
    FT_RESULT_IKERNEL_ID = 0x1a

    def set_flow_table_mask(self, daddr=False, dport=False, saddr=False,
                            sport=False, delay=None):
        '''Set the header fields that are included as part of the flow match.'''
        mask = 0
        if saddr:
            mask |= 1
        if daddr:
            mask |= 2
        if sport:
            mask |= 4
        if dport:
            mask |= 8
        if mask:
            self.write(self.FT_FIELDS, mask, delay=delay)

    def enter_flow_in_gateway(self, saddr, sport, daddr, dport, delay=None):
        '''Enter a flow in the flow table gateway to be added or deleted.'''
        self.write(self.FT_KEY_SPORT, sport, delay=delay)
        self.write(self.FT_KEY_DPORT, dport, delay=10)
        self.write(self.FT_KEY_SADDR, inet_aton(saddr), delay=10)
        self.write(self.FT_KEY_DADDR, inet_aton(daddr), delay=10)

    def set_flow(self, saddr, sport, daddr, dport, action=FT_PASSTHROUGH,
                 ikernel=0, ikernel_id=0, delay=None):
        '''Add a flow with the associated action to the table.'''
        self.enter_flow_in_gateway(saddr, sport, daddr, dport, delay=delay)

        self.write(self.FT_RESULT_ACTION, action, delay=10)
        self.write(self.FT_RESULT_IKERNEL, ikernel, delay=10)
        self.write(self.FT_RESULT_IKERNEL_ID, ikernel_id, delay=10)

        return self.read(self.FT_ADD_FLOW, delay=10)

    def del_flow(self, saddr, sport, daddr, dport, delay=None):
        '''Remove the provided flow from the table.'''
        self.enter_flow_in_gateway(saddr, sport, daddr, dport, delay=delay)

        return self.read(self.FT_DELETE_FLOW, delay=10)

def swap32(i):
    '''Swap big-endian to little-endian or vice versa.'''
    return struct.unpack("<I", struct.pack(">I", i))[0]

class NicaHardware(NICA):
    '''Implement NICA with actual hardware access.'''
    MLX_ACCEL_ACCESS_TYPE_I2C = 0
    MLX_ACCEL_ACCESS_TYPE_RDMA = 1
    IOCTL_ACCESS_TYPE = ioctl_iow(ord('m'), 0x80, 4)

    def __init__(self, path):
        super().__init__()
        self.fpga_fd = os.open(path, os.O_RDWR)
        fcntl.ioctl(self.fpga_fd, self.IOCTL_ACCESS_TYPE, self.MLX_ACCEL_ACCESS_TYPE_RDMA)

        self.int_struct = struct.Struct('<I') # little endian
        try:
            self.shell_version = self.axi_read(0x900000)
        except OSError:
            logging.warning('Unable to read AXI lite. Reverting to I2C interface. This can reduce performance.')
            fcntl.ioctl(self.fpga_fd, self.IOCTL_ACCESS_TYPE, self.MLX_ACCEL_ACCESS_TYPE_I2C)
            self.shell_version = self.axi_read(0x900000)

        if self.shell_version >= 0x10000 or self.shell_version == 0:
            # SimX is big endian but doesn't export shell version */
            self.int_struct = struct.Struct('>I')
            self.shell_version = swap32(self.shell_version)
        print('Shell version: {}'.format(self.shell_version))

    def axi_write(self, address, value, delay=None):
        os.pwrite(self.fpga_fd, self.int_struct.pack(value), address)
        return super(NicaHardware, self).axi_write(address, value, delay=delay)

    def axi_read(self, address, delay=None):
        return self.int_struct.unpack(os.pread(self.fpga_fd, 4, address))[0]

def default_mst_device():
    '''Find the available MST device for the FPGA, or provide the default.'''
    options = glob.glob('/dev/mst/*_fpga_rdma')

    if options:
        return options[0]
    return '/dev/mst/mt4117_pciconf0_fpga_rdma'
