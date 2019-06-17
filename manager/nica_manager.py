#!/usr/bin/env python3

"""
NICA manager - a trusted process that controls access to NICA hardware.
"""

import asyncio
import os
from struct import Struct
from uuid import UUID
import socket
import array
import errno
import glob
import argparse
import logging

from abc import ABC, abstractmethod
from enum import IntEnum

import netifaces
from idpool import IDPool
from util import mac_to_str, str_to_mac, inet_ntoa

from nica import NicaHardware, FlowTable, inet_aton

FPGA_MAC = '00:00:00:00:00:01'
FPGA_IP = '10.0.0.1'

# TODO use command line or environment variable
logging.basicConfig(level=logging.INFO)

def exception(errno_code):
    '''Return an OS exception matching the given errno value.'''
    return OSError(errno_code, os.strerror(errno_code))

class Netdev(ABC):
    """Per netdev NICA objects"""
    def __init__(self, ifname):
        super().__init__()

        logging.info('Binding NICA to netdev "{}"'.format(ifname))
        self.ifname = ifname

        ipaddresses = netifaces.ifaddresses(self.ifname)[netifaces.AF_INET]
        if len(ipaddresses) > 1 or not ipaddresses:
            logging.warning('only the first IP address will be used.')
        self.ip_addr = ipaddresses[0]['addr']

        self.ikernel_ids = IDPool(max_id=1024) # TODO get from HW
        self.custom_ring_ids = None
        self.ikernels = {}
        self.custom_rings = {}
        self.flows = {}
        self.uuids = []

    def initialize(self):
        '''Initialization that must occur after all constructors (RPC link is made).'''
        mac = netifaces.ifaddresses(self.ifname)[netifaces.AF_LINK][0]['addr']
        logging.info('Configuring custom ring to MAC {}, IP {}'.format(mac, self.ip_addr))
        self.configure_custom_ring(mac, self.ip_addr)

        self.custom_ring_ids = IDPool(min_id=0, max_id=self.num_rings())

        self.uuids = self.get_uuids()
        logging.info('Supported UUIDs:\n{}'.format('\n'.join(str(uuid) for uuid in self.uuids)))

    @abstractmethod
    def configure_custom_ring(self, mac, ip_addr):
        '''Configure the custom ring for this interface's network address.'''
        pass

    @abstractmethod
    def num_rings(self):
        '''Return the maximum number of custom rings available.'''
        pass

    @abstractmethod
    def get_uuids(self):
        '''Return a list of UUIDs support by the hardware.'''
        pass

    @abstractmethod
    def shutdown(self):
        '''Shutdown the hardware and release all resources.'''
        for ikernel in self.ikernels.values():
            ikernel.destroy()

    def ik_create(self, uuid):
        '''Allocate an ikernel instance and register it.'''
        try:
            ikernel = Ikernel(self)
            self.allocate_ikernel(uuid, ikernel)
            self.ikernels[ikernel.ikernel_id] = ikernel
            return ikernel

        except IndexError:
            logging.error('Unknown UUID requested')
            raise exception(errno.ENOENT)

    def ik_destroy(self, ikernel_id):
        '''Destroy a given ikernel and deregister it.'''
        try:
            ikernel = self.ikernels[ikernel_id]
            ikernel.destroy()
            del self.ikernels[ikernel_id]
            # TODO reset ikernel context
            return (0, 0)
        except KeyError:
            logging.error('Unknown ikernel handle {}'.format(ikernel_id))
            return (errno.ENOENT, )

    @abstractmethod
    def allocate_ikernel(self, uuid, ikernel):
        '''Allocate an ikernel instance.'''
        pass

    @abstractmethod
    def deallocate_ikernel(self, ikernel_id):
        '''Deallocate an ikernel instance.'''
        self.ikernel_ids.release_id(ikernel_id)

    def bind_local(self, flow):
        '''Change an INADDR_ANY flow to a local IP flow.'''
        if flow[0] == '0.0.0.0':
            return (self.ip_addr, flow[1])
        return flow

    def get_ikernel(self, ikernel_id):
        '''Try getting an ikernel of the given ID.'''
        try:
            return self.ikernels[ikernel_id]
        except KeyError:
            logging.error('Unknown ikernel handle {}'.format(ikernel_id))
            raise exception(errno.ENOENT)

    def ik_attach(self, ikernel_id, flow):
        '''Associate a flow with a given ikernel.'''
        flow = self.bind_local(flow)
        return self.get_ikernel(ikernel_id).attach(flow)

    def ik_detach(self, ikernel_id, flow):
        '''Detach a socket's flow from a given ikernel.'''
        flow = self.bind_local(flow)
        self.get_ikernel(ikernel_id).detach(flow)

    @abstractmethod
    def attach(self, flow, ikernel):
        '''Register a flow attachment'''
        pass

    @abstractmethod
    def detach(self, flow, ikernel):
        '''Delete a flow attachment'''
        pass

    def ik_rpc(self, ikernel_id, address, value, write):
        '''Invoke register read or register write RPC call to the underlying ikernel hardware.'''
        ikernel = self.get_ikernel(ikernel_id)
        return self.invoke_ikernel_rpc(ikernel, address, value, int(write))

    @abstractmethod
    def invoke_ikernel_rpc(self, ikernel, address, value, write):
        '''Invoke register access or RPC call.'''
        pass

    @abstractmethod
    def cr_create(self, ikernel_id, qpn, mac=None, dst_ip=None):
        '''Allocate and program a new custom ring.'''
        pass

    @abstractmethod
    def cr_destroy(self, ring_id):
        '''Deallocate a custom ring.'''
        pass

    @abstractmethod
    def update_credits(self, ring_id, msn_max):
        '''Update the number of messages to allow the ikernel to send on a given ring.'''
        pass

class NetdevHardware(Netdev):
    '''Control NICA hardware directly.'''
    def __init__(self, ifname, mstfile):
        # super().__init__ can call our methods, which use nica, so init it first.
        self.nica = NicaHardware(mstfile)

        super().__init__(ifname)

        self.nica.enable()
        self.nica.n2h_flow_table.set_flow_table_mask(
            daddr=True, dport=True, saddr=False, sport=False)
        self.nica.h2n_flow_table.set_flow_table_mask(
            saddr=True, sport=True, daddr=False, dport=False)

        self.custom_ring_mac = None
        self.custom_ring_ip = None

    def configure_custom_ring(self, mac, ip_addr):
        self.custom_ring_mac = mac
        self.custom_ring_ip = ip_addr

        self.nica.custom_ring.set_mac(source=True, mac=FPGA_MAC)
        self.nica.custom_ring.set_ip(source=True, ip_addr=FPGA_IP)

    def num_rings(self):
        return self.nica.custom_ring.num_rings()

    def get_uuids(self):
        return [self.nica.get_uuid(i) for i in range(1)]

    def shutdown(self):
        super().shutdown()

        self.nica.disable()

    def allocate_ikernel(self, uuid, ikernel):
        try:
            ikernel.ikernel_index = self.uuids.index(uuid)
            ikernel.ikernel_id = self.ikernel_ids.get_id()
            return ikernel
        except ValueError:
            logging.error('Unknown UUID requested')
            raise exception(errno.ENOENT)

    def deallocate_ikernel(self, ikernel_id):
        super(NetdevHardware, self).deallocate_ikernel(ikernel_id)

    def attach(self, flow, ikernel):
        logging.info('Adding flow {}'.format(flow))
        if flow in self.flows.keys():
            logging.warning('flow already taken')
            raise exception(errno.EADDRINUSE)

        h2n_flow_id = self.nica.h2n_flow_table.set_flow(
            flow[0], flow[1], 0, socket.INADDR_ANY,
            action=FlowTable.FT_IKERNEL, ikernel=ikernel.ikernel_index, ikernel_id=ikernel.ikernel_id)
        if h2n_flow_id == 0xffffffff or h2n_flow_id == 0:
            logging.error("h2n FT_ADD_FLOW returned {}".format(h2n_flow_id))
            raise exception(errno.EINVAL)
        n2h_flow_id = self.nica.n2h_flow_table.set_flow(
            0, socket.INADDR_ANY, flow[0], flow[1],
            action=FlowTable.FT_IKERNEL, ikernel=ikernel.ikernel_index, ikernel_id=ikernel.ikernel_id)
        if n2h_flow_id == 0xffffffff or n2h_flow_id == 0:
            # TODO clean h2n flow
            logging.error("n2h FT_ADD_FLOW returned {}".format(n2h_flow_id))
            raise exception(errno.EINVAL)

        self.flows[flow] = (ikernel, h2n_flow_id, n2h_flow_id)
        return h2n_flow_id, n2h_flow_id

    def detach(self, flow, ikernel):
        '''Delete a flow attachment'''
        logging.info('Removing flow {}'.format(flow))
        if flow not in self.flows.keys():
            logging.warning('flow missing')
            raise exception(errno.ENOENT)

        success = True
        ret = self.nica.h2n_flow_table.del_flow(flow[0], flow[1], socket.INADDR_ANY, 0)
        if ret == 0xffffffff:
            logging.error("h2n FT_DELETE_FLOW returned -1")
            success = False
        ret = self.nica.n2h_flow_table.del_flow(socket.INADDR_ANY, 0, flow[0], flow[1])
        if ret == 0xffffffff:
            logging.error("n2h FT_DELETE_FLOW returned -1")
            success = False

        del self.flows[flow]

        if not success:
            raise exception(errno.ENOENT)

    def invoke_ikernel_rpc(self, ikernel, address, value, write):
        nica_ikernel = self.nica.ikernels[ikernel.ikernel_index]
        if write:
            nica_ikernel.write(address, value, ikernel_id=ikernel.ikernel_id)
        else:
            value = nica_ikernel.read(address, ikernel_id=ikernel.ikernel_id)
        return value

    def cr_create(self, ikernel_id, qpn, mac=None, dst_ip=None):
        ring_id = self.custom_ring_ids.get_id()
        logging.info('Allocating custom ring ID {}'.format(ring_id))
        if not mac:
            mac = self.custom_ring_mac
        if not dst_ip:
            dst_ip = self.custom_ring_ip
        self.nica.custom_ring.set_custom_ring(ring_id, mac=mac, dst_ip=dst_ip, qpn=qpn)

        self.nica.update_credits(ring_id, max_msn=0, reset=True)
        return ring_id

    def cr_destroy(self, ring_id):
        self.nica.custom_ring.set_custom_ring(ring_id, qpn=0)
        self.custom_ring_ids.release_id(ring_id)

    def update_credits(self, ring_id, msn_max):
        self.nica.update_credits(ring_id, msn_max)

EMPTY_STRUCT = Struct('I') # one 32-bit reserved field to overcome C++ no empty-struct policy
EMPTY_TUPLE = (0, )

class RPC(object):
    '''Some helper RPC functions.'''

    def __init__(self):
        self.transport = None
        self.buf = bytes()

    hdr = Struct('HHHH')

    @staticmethod
    def hdr_to_str(hdr):
        '''String representation of an RPC header.'''
        return '(opcode={}, length={}, flags={}, ' \
               'status={})'.format(*hdr)

    def send_msg(self, opcode, strct, msg, status=0, flags=0):
        '''Send an RPC message (request or response) struct.'''
        send_hdr = (opcode,
                    strct.size,
                    flags,
                    status)
        logging.debug('Sending header {}'.format(self.hdr_to_str(send_hdr)))
        self.transport.write(self.hdr.pack(*send_hdr))
        logging.debug('Sending data ({} bytes)'.format(strct.size))
        packed = strct.pack(*msg)
        self.transport.write(packed)

    def invoke(self, opcode, struct_req, req, struct_resp=EMPTY_STRUCT):
        '''Send an RPC and return the response (synchronous).'''
        self.send_msg(opcode, struct_req, req, flags=1)
        self.buf += self.transport.read(self.hdr.size)
        hdr = self.unpack_chunk(self.hdr)
        if hdr is None:
            raise exception(errno.EPIPE)

        logging.debug('Received header {}'.format(self.hdr_to_str(hdr)))
        resp_opcode, length, _, status = hdr
        if length != struct_resp.size:
            logging.debug('Error: got invalid response size {}, expected {}'.format(
                length, struct_resp.size))
            raise exception(errno.EINVAL)
        if resp_opcode != opcode:
            logging.error('Error: got unexpected opcode {}, expected {}'.format(resp_opcode, opcode))
            raise exception(errno.EINVAL)

        if status:
            logging.warning('Error: status={}'.format(status))
            raise exception(status)
        self.buf += self.transport.read(length)
        return self.unpack_chunk(struct_resp)

    def unpack_chunk(self, strct):
        '''Try to read a message from the received buffer.'''
        if len(self.buf) >= strct.size:
            ret = strct.unpack(self.buf[:strct.size])
            self.buf = self.buf[strct.size:]
            return ret
        return None

def rpc_class(cls):
    '''Decorator to collect opcodes.'''
    opcode_handlers = {}
    for value in cls.__dict__.values():
        if hasattr(value, 'opcode'):
            opcode_handlers[value.opcode] = value
    attributesdict = cls.__dict__.copy()
    attributesdict.update(opcode_handlers=opcode_handlers)
    return type(cls.__name__, cls.__bases__, attributesdict)

class HypervisorOpcodes(IntEnum):
    '''Opcodes for RPC between VM and hypervisor.'''
    CONFIG_CUSTOM_RING = 1
    NUM_RINGS = 2
    GET_UUIDS = 3
    ALLOCATE_IKERNEL = 4
    DEALLOCATE_IKERNEL = 5
    ATTACH = 6
    DETACH = 7
    CR_CREATE = 8
    CR_DESTROY = 9
    UPDATE_CREDITS = 10
    RPC = 11

@rpc_class
class NetdevParavirt(Netdev, RPC):
    '''Control NICA through a para-virtual interface to the hypervisor (virtio-serial).'''

    def __init__(self, netdev, chardev):
        super(NetdevParavirt, self).__init__(netdev)
        self.transport = open(chardev, 'r+b', buffering=0)

    def configure_custom_ring(self, mac, ip_addr):
        self.invoke(HypervisorOpcodes.CONFIG_CUSTOM_RING,
                    Struct('6sI'), (str_to_mac(mac), inet_aton(ip_addr)))

    def num_rings(self):
        return self.invoke(HypervisorOpcodes.NUM_RINGS,
                           Struct(''), (),
                           Struct('I'))

    def get_uuids(self):
        uuid_bytes, = self.invoke(HypervisorOpcodes.GET_UUIDS,
                                  Struct(''), (),
                                  Struct('16s'))
        return [UUID(bytes=uuid_bytes)]

    def shutdown(self):
        super(NetdevParavirt, self).shutdown()

    def allocate_ikernel(self, uuid, ikernel):
        ikernel.ikernel_id, = self.invoke(HypervisorOpcodes.ALLOCATE_IKERNEL,
                                          Struct('16s'), (uuid.bytes,),
                                          Struct('I'))

    def deallocate_ikernel(self, ikernel_id):
        self.invoke(HypervisorOpcodes.DEALLOCATE_IKERNEL,
                    Struct('I'), (ikernel_id,))

        super(NetdevParavirt, self).deallocate_ikernel(ikernel_id)

    def attach(self, flow, ikernel):
        ip_addr = inet_aton(flow[0])
        return self.invoke(HypervisorOpcodes.ATTACH,
                           Struct('IHI'), (ip_addr, flow[1], ikernel.ikernel_id),
                           Struct('II'))

    def detach(self, flow, ikernel):
        ip_addr = inet_aton(flow[0])
        self.invoke(HypervisorOpcodes.DETACH,
                    Struct('IHI'), (ip_addr, flow[1], ikernel.ikernel_id))

    def invoke_ikernel_rpc(self, ikernel, address, value, write):
        return self.invoke(HypervisorOpcodes.RPC,
                           Struct('IIII'), (ikernel.ikernel_id, address, value, write),
                           Struct('I'))[0]

    def cr_create(self, ikernel_id, qpn, mac=None, dst_ip=None):
        return self.invoke(HypervisorOpcodes.CR_CREATE,
                           Struct('II'), (ikernel_id, qpn,),
                           Struct('I'))[0]

    def cr_destroy(self, ring_id):
        return self.invoke(HypervisorOpcodes.CR_DESTROY,
                           Struct('I'), (ring_id,))

    def update_credits(self, ring_id, msn_max):
        self.invoke(HypervisorOpcodes.UPDATE_CREDITS,
                    Struct('II'), (ring_id, msn_max))

def default_mst_device():
    '''Find the available MST device for the FPGA, or provide the default.'''
    options = glob.glob('/dev/mst/*_fpga_rdma')

    if options:
        return options[0]
    return '/dev/mst/mt4117_pciconf0_fpga_rdma'

MST_DEVICE = default_mst_device()
VIRTIO_DEVICE = '/dev/virtio-ports/nica'

def parse_args():
    '''Parse command line arguments.'''
    parser = argparse.ArgumentParser(description='Manages NICA.')
    parser.add_argument('-d', metavar='DEVICE', dest='device', default=MST_DEVICE,
                        help='MST device to use (default: {})'.format(MST_DEVICE))
    parser.add_argument('-v', metavar='DEVICE', dest='vdevice', default=VIRTIO_DEVICE,
                        help='virtio-serial device to use (default: {})'.format(VIRTIO_DEVICE))
    return parser.parse_args()

def init_nica():
    '''Initialize a Netdev object for the expected one underlying card.'''
    args = parse_args()
    # TODO support multiple mlx netdevs
    mlx_netdevs = [dev for path in glob.glob('/sys/bus/pci/drivers/mlx5_core/*/net')
                   for dev in os.listdir(path)]
    if os.path.exists(args.device):
        # Setup custom ring
        # TODO embed the script here
        cur_dir = os.path.dirname(__file__)
        os.system(os.path.join(cur_dir, '../scripts/custom-ring-setup.sh'))
        nica = NetdevHardware(mlx_netdevs[0], args.device)
    else:
        logging.info('No hardware device found. Attempt using paravirt.')
        nica = NetdevParavirt(mlx_netdevs[0], args.vdevice)

    nica.initialize()
    return nica

# global NICA - TODO support multiple netdevs
NICA = init_nica()

class Ikernel(object):
    '''Designate an ikernel instance allocated to the client.'''
    def __init__(self, netdev):
        self.netdev = netdev
        self.ikernel_index = None # To be overridden
        self.ikernel_id = None # To be overridden
        self.flows = set()
        self.custom_rings = set()

    def attach(self, flow):
        '''Attach a flow (IP, port) to this ikernel instance.'''
        flow_ids = self.netdev.attach(flow, self)
        self.flows.add(flow)

        return flow_ids

    def detach(self, flow):
        '''Detach a flow (IP, port) from this ikernel instance.'''
        if flow not in self.flows:
            logging.warning('flow missing')
            raise exception(errno.ENOENT)
        self.netdev.detach(ikernel=self, flow=flow)
        self.flows.remove(flow)

    def destroy(self):
        '''Destroy an ikernel instance, releasing all of its resources.'''
        while self.flows:
            flow = next(iter(self.flows))
            self.detach(flow)

        while self.custom_rings:
            ring_id = next(iter(self.custom_rings))
            self.cr_destroy(ring_id)

        self.netdev.deallocate_ikernel(self.ikernel_id)

    def cr_create(self, qpn, mac=None, dst_ip=None):
        '''Allocate and program a new custom ring.'''
        ring_id = self.netdev.cr_create(ikernel_id=self.ikernel_id, mac=mac, dst_ip=dst_ip, qpn=qpn)
        self.custom_rings.add(ring_id)
        return ring_id

    def cr_destroy(self, ring_id):
        '''Deallocate a custom ring.'''
        self.netdev.cr_destroy(ring_id)
        self.custom_rings.remove(ring_id)


def rpc(opcode, req, resp=EMPTY_STRUCT):
    '''A decorator that handles RPC boilerplate. Receives a unique opcode
        number, request struct and response struct.'''
    def rpc_decorator(func):
        '''Curried decorator function.'''
        def func_wrapper(self):
            '''Function wrapper that adds boilerplate code to receive the RPC
            message and send the response.'''
            unpacked = self.unpack_chunk(req)
            if unpacked is None:
                return False
            logging.debug('{}{}'.format(func.__name__, unpacked))

            try:
                resp_tuple = func(self, *unpacked)
            except OSError as exc:
                logging.warning('Exception: {}'.format(exc))
                resp_tuple = (int(exc.errno),)

            if resp_tuple[0]: # err
                self.send_msg(self.cur_hdr[0], EMPTY_STRUCT, EMPTY_TUPLE, resp_tuple[0])
            else:
                self.send_msg(self.cur_hdr[0], resp, resp_tuple[1:])

            self.cur_handler = self.__class__.header_data_received
            return True
        func_wrapper.__name__ = func.__name__
        func_wrapper.opcode = opcode
        return func_wrapper
    return rpc_decorator

class NICAManagerProtocolBase(asyncio.Protocol, RPC):
    '''asyncio protocol class that handles UNIX socket RPC calls from the libnica clients or VMs.'''

    opcode_handlers = {} # to be overwritten by the decorator @rpc_class

    def __init__(self):
        super().__init__()
        self.transport = None
        self.socket = None
        self.cur_handler = self.__class__.header_data_received
        self.cur_hdr = None
        self.ikernels = set()
        self.custom_ring_ikernels = {}

    def connection_made(self, transport):
        peername = transport.get_extra_info('peername')
        logging.info('Connection with {}'.format(peername))
        self.transport = transport
        self.socket = transport.get_extra_info('socket')

    def connection_lost(self, exc):
        logging.info('Connection lost.')
        for ikernel in self.ikernels:
            ikernel.destroy()

    def data_received(self, data):
        logging.debug('Data received: {} bytes'.format(len(data)))
        self.buf += data

        while self.cur_handler(self):
            pass

    def header_data_received(self):
        '''Handle received data for an RPC header.'''
        self.cur_hdr = self.unpack_chunk(self.hdr)
        if self.cur_hdr:
            logging.debug('Got header (opcode={}, length={}, flags={}, status={})'.format(*self.cur_hdr))
            try:
                self.cur_handler = self.opcode_handlers[self.cur_hdr[0]]
            except KeyError:
                logging.error('Unknown opcode')
                self.send_msg(self.cur_hdr[0], EMPTY_STRUCT, EMPTY_TUPLE, errno.ENOSYS)
                # TODO consume the rest of the message (using length in the header)
            return True
        return False

    def check_ring_id(self, ring_id):
        '''Check that the given ring ID is valid for this connection.'''
        if ring_id not in self.custom_ring_ikernels.keys():
            logging.warning('ring_id {} not found.\n'.format(ring_id))
            raise exception(errno.ENOENT)

@rpc_class
class NICAManagerProtocol(NICAManagerProtocolBase):
    '''asyncio protocol class that handles UNIX socket RPC calls from the libnica clients.'''

    @rpc(1, Struct('16s16s'), Struct('I'))
    def ik_create(self, netdev, uuid):
        '''Create a new ikernel instance.'''
        netdev = netdev.split(b'\x00', 1)[0].decode()
        uuid = UUID(bytes=uuid)

        if netdev != NICA.ifname:
            logging.warning('Expecting netdev "{}", not "{}".'.format(NICA.ifname, netdev))
            return (errno.ENODEV, )

        ikernel = NICA.ik_create(uuid)
        self.ikernels.add(ikernel)
        ikernel_id = ikernel.ikernel_id
        logging.info('Got ikernel ID {}'.format(ikernel_id))
        return (0, ikernel_id)

    @rpc(2, Struct('I'))
    def ik_destroy(self, ikernel_handle):
        '''Release an ikernel instance.'''
        try:
            ikernel = NICA.ikernels[ikernel_handle]
            ikernel.destroy()
            del NICA.ikernels[ikernel_handle]
            self.ikernels.remove(ikernel)
            # TODO reset ikernel context
            return (0, 0)
        except KeyError:
            logging.warning('Unknown ikernel handle {}'.format(ikernel_handle))
            return (errno.ENOENT, )

    @rpc(3, Struct('IIII'), Struct('I'))
    def ik_rpc(self, ikernel_handle, address, value, write):
        '''Invoke register read or register write RPC call to the underlying ikernel hardware.'''
        write = bool(write)
        value = NICA.ik_rpc(ikernel_handle, address, value, write)
        return (0, value)

    def receive_fd(self):
        '''Receive a file descriptor over UNIX domain socket using recvmsg.'''
        # A message to signal we are ready for the fd msg
        self.send_msg(self.cur_hdr[0], EMPTY_STRUCT, EMPTY_TUPLE)

        fds = array.array("i")   # Array of ints
        maxfds = 1
        msglen = 1
        while True:
            try:
                _, ancdata, _, _ = self.socket.recvmsg(msglen,
                                                       socket.CMSG_LEN(maxfds * fds.itemsize))
                break
            except BlockingIOError:
                pass
        for cmsg_level, cmsg_type, cmsg_data in ancdata:
            if cmsg_level == socket.SOL_SOCKET and cmsg_type == socket.SCM_RIGHTS:
                # Append data, ignoring any truncated integers at the end.
                fds.fromstring(cmsg_data[:len(cmsg_data) - (len(cmsg_data) % fds.itemsize)])
        fds_list = list(fds)
        if len(fds_list) != 1:
            logging.error('Expecting a single file descriptor')
            raise exception(errno.EINVAL)

        return fds_list[0]

    @staticmethod
    def flow_from_fd(sock_fd):
        '''Convert a given socket file descriptor to a sockname (IP address, port) tuple.'''
        sock = socket.fromfd(sock_fd, socket.AF_INET, socket.SOCK_DGRAM)
        os.close(sock_fd)
        flow = sock.getsockname()
        sock.close()
        return flow

    @rpc(4, Struct('I'), Struct('II'))
    def ik_attach(self, ikernel_handle):
        '''Associate a flow with a given ikernel.'''
        sock_fd = self.receive_fd()
        flow = self.flow_from_fd(sock_fd)

        h2n_flow_id, n2h_flow_id = NICA.ik_attach(ikernel_handle, flow)
        return (0, h2n_flow_id, n2h_flow_id)

    @rpc(5, Struct('I'), Struct('I'))
    def ik_detach(self, ikernel_handle):
        '''Detach a socket's flow from a given ikernel.'''
        try:
            sock_fd = self.receive_fd()
            flow = self.flow_from_fd(sock_fd)
        except OSError as exc:
            return (exc.errno,)

        NICA.ik_detach(ikernel_handle, flow)
        return (0, 0)

    @rpc(6, Struct('II'), Struct('I'))
    def cr_create(self, ikernel_handle, qp_num):
        '''Create and initialize a custom ring in the hardware.'''
        # TODO pass uverbs fd to verify the QP number
        try:
            ikernel = NICA.ikernels[ikernel_handle]
        except KeyError:
            logging.warning('Unknown ikernel handle {}'.format(ikernel_handle))
            return (errno.ENOENT, )

        ring_id = ikernel.cr_create(qp_num)
        self.custom_ring_ikernels[ring_id] = ikernel

        return (0, ring_id)

    @rpc(7, Struct('I'))
    def cr_destroy(self, ring_id):
        '''Deallocate a custom ring.'''
        try:
            ikernel = self.custom_ring_ikernels[ring_id]
        except KeyError:
            logging.warning('Unknown custom ring ID {}'.format(ring_id))
            return (errno.ENOENT, )

        ikernel.cr_destroy(ring_id)
        del self.custom_ring_ikernels[ring_id]

        return (0, 0)

    @rpc(8, Struct('II'))
    def cr_update_credits(self, ring_id, max_msn):
        '''Update the number of messages to allow the ikernel to send on a given ring.'''
        try:
            ikernel = self.custom_ring_ikernels[ring_id]
        except KeyError:
            logging.warning('Unknown custom ring ID {}'.format(ring_id))
            return (errno.ENOENT, )

        ikernel.netdev.update_credits(ring_id, max_msn)
        return 0, 0

@rpc_class
class NICAManagerHypervisorProtocol(NICAManagerProtocolBase):
    '''asyncio protocol class that handles UNIX socket RPC calls from a VM's NICA manager
    over a UNIX domain socket to the QEMU's virtio-serial socket.'''

    def __init__(self):
        super(NICAManagerHypervisorProtocol, self).__init__()

        # TODO use to configure custom ring with per-ring MAC/IP
        self.custom_ring_mac = bytes(6)
        self.custom_ring_ip = 0

    @rpc(HypervisorOpcodes.CONFIG_CUSTOM_RING, Struct('6sI'))
    def configure_custom_ring(self, mac, ip_addr):
        '''Configure the custom ring for this VM's MAC and IP.'''
        self.custom_ring_mac = mac_to_str(mac)
        self.custom_ring_ip = inet_ntoa(ip_addr)

        logging.info('Got custom ring MAC={} and IP={}.'.format(self.custom_ring_mac, self.custom_ring_ip))

        return (0, 0)

    @rpc(HypervisorOpcodes.NUM_RINGS, Struct(''))
    def num_rings(self):
        '''Return the maximum number of custom rings available.'''
        num = NICA.num_rings()
        logging.debug('num rings: {}'.format(num))
        return (0, num)

    @rpc(HypervisorOpcodes.GET_UUIDS, Struct(''), Struct('16s'))
    def get_uuids(self):
        '''Return a list of UUIDs support by the hardware.'''
        uuids = NICA.get_uuids()
        return (0, uuids[0].bytes)

    @rpc(HypervisorOpcodes.ALLOCATE_IKERNEL, Struct('16s'), Struct('I'))
    def allocate_ikernel(self, uuid_bytes):
        '''Allocate an ikernel instance.'''
        uuid = UUID(bytes=uuid_bytes)
        ikernel = NICA.ik_create(uuid)
        ikernel_id = ikernel.ikernel_id
        self.ikernels.add(ikernel_id)
        logging.info('Got ikernel ID {}'.format(ikernel_id))
        return (0, ikernel_id)

    @rpc(HypervisorOpcodes.DEALLOCATE_IKERNEL, Struct('I'))
    def deallocate_ikernel(self, ikernel_id):
        '''Deallocate an ikernel instance.'''
        if ikernel_id not in self.ikernels:
            return (errno.ENOENT, )
        NICA.ik_destroy(ikernel_id)
        self.ikernels.remove(ikernel_id)
        return (0, 0)

    @staticmethod
    def flow_from_binary(flow_ip, flow_port):
        '''Convert IP address to string and return a flow tuple.'''
        return (inet_ntoa(flow_ip), flow_port)

    @rpc(HypervisorOpcodes.ATTACH, Struct('IHI'), Struct('II'))
    def attach(self, flow_ip, flow_port, ikernel_id):
        '''Attach a flow (IP, port) to the given ikernel.'''
        if ikernel_id not in self.ikernels:
            logging.warning('ikernel {} not found.\n'.format(ikernel_id))
            raise exception(errno.ENOENT)

        if inet_ntoa(flow_ip) != self.custom_ring_ip:
            logging.warning('Asked for wrong IP address in attach {}'.format(flow_ip))
            raise exception(errno.EPERM)

        h2n_flow_id, n2h_flow_id = NICA.ik_attach(ikernel_id,
                                                  self.flow_from_binary(flow_ip, flow_port))
        return (0, h2n_flow_id, n2h_flow_id)

    @rpc(HypervisorOpcodes.DETACH, Struct('IHI'))
    def detach(self, flow_ip, flow_port, ikernel_id):
        '''Detach a flow from a given ikernel.'''
        if ikernel_id not in self.ikernels:
            logging.warning('ikernel {} not found.\n'.format(ikernel_id))
            raise exception(errno.ENOENT)

        flow = self.flow_from_binary(flow_ip, flow_port)
        NICA.ik_detach(ikernel_id, flow)
        return (0, 0)

    @rpc(HypervisorOpcodes.RPC, Struct('IIII'), Struct('I'))
    def invoke_ikernel_rpc(self, ikernel_id, address, value, write):
        '''Invoke register access or RPC call.'''
        if ikernel_id not in self.ikernels:
            logging.warning('ikernel {} not found.\n'.format(ikernel_id))
            raise exception(errno.ENOENT)

        ret = NICA.ik_rpc(ikernel_id, address, value, write)
        return (0, ret)

    @rpc(HypervisorOpcodes.CR_CREATE, Struct('II'), Struct('I'))
    def cr_create(self, ikernel_id, qp_num):
        '''Create and initialize a custom ring in the hardware.'''
        if ikernel_id not in self.ikernels:
            logging.warning('ikernel {} not found.\n'.format(ikernel_id))
            raise exception(errno.ENOENT)

        ikernel = NICA.get_ikernel(ikernel_id)
        ring_id = NICA.cr_create(ikernel_id=ikernel_id, qpn=qp_num, mac=self.custom_ring_mac, dst_ip=self.custom_ring_ip)
        self.custom_ring_ikernels[ring_id] = ikernel
        return (0, ring_id)

    @rpc(HypervisorOpcodes.CR_DESTROY, Struct('I'))
    def cr_destroy(self, ring_id):
        '''Deallocate a custom ring.'''
        self.check_ring_id(ring_id)
        NICA.cr_destroy(ring_id)
        del self.custom_ring_ikernels[ring_id]
        return (0, 0)

    @rpc(HypervisorOpcodes.UPDATE_CREDITS, Struct('II'))
    def update_credits(self, ring_id, msn_max):
        '''Update the given ring's credits.'''
        self.check_ring_id(ring_id)
        NICA.update_credits(ring_id, msn_max)
        return (0, 0)

NICA_SOCKET_PATH = '/var/run/nica-manager.socket'

def main():
    '''Main entrypoint'''
    loop = asyncio.get_event_loop()
    if os.path.exists(NICA_SOCKET_PATH):
        logging.warning('Deleting old socket at "{}".'.format(NICA_SOCKET_PATH))
        os.unlink(NICA_SOCKET_PATH)
    coru = loop.create_unix_server(NICAManagerProtocol, NICA_SOCKET_PATH)

    # TODO add VMs dynamically
    channels = glob.glob('/var/lib/libvirt/qemu/channel/target/*/nica')
    for chan in channels:
        try:
            loop.run_until_complete(loop.create_unix_connection(NICAManagerHypervisorProtocol, chan))
        except:
            logging.warning("Couldn't connect to {}".format(chan))

    server = loop.run_until_complete(coru)

    os.chmod(NICA_SOCKET_PATH, int('0777', base=8))

    logging.info('Serving on {}'.format(server.sockets[0].getsockname()))
    try:
        loop.run_forever()
    except KeyboardInterrupt:
        pass
    finally:
        NICA.shutdown()
    server.close()
    loop.run_until_complete(server.wait_closed())
    loop.close()
    os.unlink(NICA_SOCKET_PATH)

if __name__ == '__main__':
    main()
