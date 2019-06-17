#!/usr/bin/env python3

"""
nicactl - CLI to control NICA properties
"""

import argparse
import os
import glob
import sys
from nica import NicaHardware

# TODO control through NICA manager

def default_mst_device():
    '''Find the available MST device for the FPGA, or provide the default.'''
    options = glob.glob('/dev/mst/*_fpga_rdma')

    if options:
        return options[0]
    return '/dev/mst/mt4117_pciconf0_fpga_rdma'

MST_DEVICE = default_mst_device()

def define_parser():
    '''Parse command line arguments.'''
    parser = argparse.ArgumentParser(description='Control NICA.')
    parser.add_argument('-d', metavar='DEVICE', dest='device', default=MST_DEVICE,
                        help='MST device to use (default: {})'.format(MST_DEVICE))
    parser.add_argument('command', help='Subcommand to run')
    return parser

PARSER = define_parser()

def init_nica():
    '''Initialize a Netdev object for the expected one underlying card.'''
    args = PARSER.parse_args(sys.argv[1:2])
    if os.path.exists(args.device):
        # Setup custom ring
        nica = NicaHardware(args.device)

    return nica, args

# global NICA
NICA, ARGS = init_nica()

def print_help():
    '''Command line help'''
    PARSER.print_help()
    print('Commands:')
    print('\n'.join(COMMANDS.keys()))

def set_quantum():
    '''Set the quantum of a given TC to a number of flits.'''
    parser = argparse.ArgumentParser(description='Set quantum of a given TC')
    parser.add_argument('tc', type=int)
    parser.add_argument('quantum', type=int)
    args = parser.parse_args(sys.argv[2:])

    NICA.n2h_arbiter.set_quantum(args.tc, args.quantum)
    NICA.h2n_arbiter.set_quantum(args.tc, args.quantum)

def status():
    '''Set the quantum of a given TC to a number of flits.'''
    print('TC\tNet-to-Host Quantum\tHost-to-Net Quantum')
    for traffic_class in range(4):
        n2h_quantum = NICA.n2h_arbiter.get_quantum(traffic_class)
        h2n_quantum = NICA.h2n_arbiter.get_quantum(traffic_class)
        print('{}\t{}\t\t\t{}'.format(traffic_class, n2h_quantum, h2n_quantum))

COMMANDS = {
    'help': print_help,
    'set-quantum': set_quantum,
    'status': status,
}
def main():
    '''Main entrypoint'''
    if not hasattr(ARGS, 'command'):
        print('Unrecognized command')
        PARSER.print_help()
        sys.exit(1)
    COMMANDS[ARGS.command]()

if __name__ == '__main__':
    main()
