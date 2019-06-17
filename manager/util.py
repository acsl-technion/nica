'''Utility functions for NICA manager.'''

import socket
import struct

def mac_to_str(mac):
    '''Convert a MAC bytes object to a string representation.'''
    return ':'.join('%02x' % b for b in mac)

def str_to_mac(mac):
    '''Convert a MAC string representation into a bytes object.'''
    return bytes.fromhex(mac.replace(':', ''))

def inet_ntoa(ip_addr):
    '''Convert an IP address from a number to a string representation.'''
    return socket.inet_ntoa(struct.pack('!L', ip_addr))
