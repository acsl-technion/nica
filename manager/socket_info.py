import os
import socket
from ctypes import *

libc = CDLL('libc.so.6', use_errno=True)
libc.getsockname.argtypes = [c_int, c_void_p, POINTER(c_int32)]
libc.getsockopt.argtypes = [c_int, c_int, c_int, c_void_p, POINTER(c_int32)]

def get_sock_family(fd):
    family = c_ushort(0)
    length = c_int32(sizeof(family))
    ret = libc.getsockname(fd, byref(family), length)
    if ret:
        errno = get_errno()
        raise OSError(errno, os.strerror(errno))

    return socket.AddressFamily(family.value)

def get_sock_type(fd):
    socket_type = c_int()
    length = c_int32(sizeof(socket_type))
    ret = libc.getsockopt(fd, socket.SOL_SOCKET, socket.SO_TYPE, byref(socket_type), length)
    if ret:
        errno = get_errno()
        raise OSError(errno, os.strerror(errno))

    return socket.SocketKind(socket_type.value)

import struct
from enum import IntEnum

# https://stackoverflow.com/a/18189190/46192
def get_tcp_info(s):
    fmt = "B"*7+"I"*21
    x = struct.unpack(fmt, s.getsockopt(socket.IPPROTO_TCP, socket.TCP_INFO, 92))
    return x

def get_tcp_state(s):
    return TCPStates(get_tcp_info(s)[0])

class TCPStates(IntEnum):
    TCP_ESTABLISHED = 1
    TCP_SYN_SENT = 2
    TCP_SYN_RECV = 3
    TCP_FIN_WAIT1 = 4
    TCP_FIN_WAIT2 = 5
    TCP_TIME_WAIT = 6
    TCP_CLOSE = 7
    TCP_CLOSE_WAIT = 8
    TCP_LAST_ACK = 9
    TCP_LISTEN = 10
    TCP_CLOSING = 11
    TCP_NEW_SYN_RECV = 12
