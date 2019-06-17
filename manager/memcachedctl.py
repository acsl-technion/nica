#!/usr/bin/env python3

"""
memcachedctl - CLI to access the memcached ikernel
"""

import argparse
import os
import sys
from nica import NicaHardware, default_mst_device
from memcached import Memcached

# TODO control through NICA manager

MST_DEVICE = default_mst_device()

def define_parser():
    '''Parse command line arguments.'''
    parser = argparse.ArgumentParser(description='Control NICA.')
    parser.add_argument('-d', metavar='DEVICE', dest='device', default=MST_DEVICE,
                        help='MST device to use (default: {})'.format(MST_DEVICE))
    parser.add_argument('-i,--ikernel', metavar='IKERNEL_ID',
                        dest='ikernel_id', type=int, nargs='+',
                        help='ikernel IDs to access')
    parser.add_argument('command', help='Subcommand to run')
    return parser

PARSER = define_parser()

def init_nica():
    '''Initialize a Netdev object for the expected one underlying card.'''
    args = PARSER.parse_args()
    if os.path.exists(args.device):
        nica = NicaHardware(args.device)

    return nica, args

# global NICA
NICA, ARGS = init_nica()
MEMCACHED = Memcached(NICA)

def print_help():
    '''Command line help'''
    PARSER.print_help()
    print('Commands:')
    print('\n'.join(COMMANDS.keys()))

COUNTERS = [
    ('get req.', Memcached.MEMCACHED_STATS_GET_REQUESTS),
    ('get req. hits', Memcached.MEMCACHED_STATS_GET_REQUESTS_HITS),
    ('dropped tc backpressure', Memcached.MEMCACHED_STATS_DROPPED_TC_BACKPRESSURE),
    ('get req. dropped hits', Memcached.MEMCACHED_STATS_GET_REQUESTS_DROPPED_HITS),
    ('get req. misses', Memcached.MEMCACHED_STATS_GET_REQUESTS_MISSES),
    ('dropped cr backpressure', Memcached.MEMCACHED_DROPPED_BACKPRESSURE),
    ('set req.', Memcached.MEMCACHED_STATS_SET_REQUESTS),
    ('unknown req.', Memcached.MEMCACHED_STATS_N2H_UNKNOWN),
    ('get resp.', Memcached.MEMCACHED_STATS_GET_RESPONSE),
    ('unknown resp.', Memcached.MEMCACHED_STATS_H2N_UNKNOWN),
]

def reset():
    '''Reset all counters of a given ikernel.'''
    for ikernel_id in ARGS.ikernel_id:
        for _, reg in COUNTERS:
            MEMCACHED.gw.write(reg, 0, ikernel_id=ikernel_id)
        print('Reset counters for ikernel: {}'.format(ikernel_id))

def status():
    '''Print the current counters of a given ikernel.'''
    for ikernel_id in ARGS.ikernel_id:
        print('ikernel ID: {}\n'.format(ikernel_id))
        namewidth = max(len(name) for name, _ in COUNTERS) + 2
        formatting_title = '{{:<{}}}{{:<{}}}'.format(namewidth, namewidth)
        print(formatting_title.format('Name', 'Value'))
        print('-' * (namewidth * 2))
        for name, reg in COUNTERS:
            count = MEMCACHED.gw.read(reg, ikernel_id=ikernel_id)
            print(formatting_title.format(name, count))
        print('-' * (namewidth * 2) + '\n')

COMMANDS = {
    'help': print_help,
    'reset': reset,
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
