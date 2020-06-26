#!/usr/bin/env python3
import sys
import numpy as np
import csv
import click
from collections import defaultdict, deque


def analyze(accesses, rules):
    cache_states = defaultdict(lambda: defaultdict(lambda: 'I'))
    costs = defaultdict(lambda: np.array([0, 0, 0, 0]))
    for tag, core, access, addr in accesses:
        line_addr = addr &~0x3F
        rules(tag, core, access, line_addr, cache_states[core][line_addr], cache_states, costs)
    return costs

class LRUCache:
    def __init__(self, N, lst=None):
        self.N = N
        self.lst = deque(lst if lst is not None else [], N)
    def __len__(self):
        return len(self.lst)
    def __iter__(self):
        return iter(self.lst)
    def __contains__(self, item):
        return item in self.lst
    def mark_used(self, item):
        if item in self.lst:
            self.lst = deque(filter(lambda x: x != item, self.lst), self.N)
            self.lst.append(item)
            return True
        else:
            return False
    def insert(self, item):
        if item in self.lst:
            self.mark_used(item)
            return True
        else:
            self.lst.append(item)
            return False

# cycles
L1_HIT = 1
REMOTE_L1_HIT = 30
L2_FORWARD = 50
BUS_FORWARD = 45

# entries
WRITE_COALESCING_BUFFER = 32

# hops
L1_TO_L1_HOPS = 1
L1_TO_L2_HOPS = 1

# flits
WORD_SIZE = 1
HEADER_SIZE = 1
ADDRESS_SIZE = 1
LINE_SIZE = 8
ACK_ADDRESS_SIZE = 1

coalescing_buffer = LRUCache(WRITE_COALESCING_BUFFER)
def mode_Spandex_consumer_owned(tag, core, access, line_addr, cache_state, cache_states, costs):
    if access == 'store':
        if coalescing_buffer.insert(line_addr):
            costs[core] += [1, 0, WORD_SIZE, 0]
        else:
            costs[core] += [REMOTE_L1_HIT, 0, L1_TO_L1_HOPS*(HEADER_SIZE+ADDRESS_SIZE) + L1_TO_L1_HOPS*(HEADER_SIZE+ACK_ADDRESS_SIZE+WORD_SIZE), 0]
    elif access == 'load':
        costs[core] += [0, L1_HIT, 0, 0]
    else:
        raise RuntimeError('fUnknown access {access}')


def mode_Spandex_producer_owned(tag, core, access, line_addr, cache_state, cache_states, costs):
    cost, new_cache_state = {
        ('store', 'I'): ([L1_HIT, 0, 0, 0], 'I'),
        ('load', 'I'): ([0, REMOTE_L1_HIT, 0, L1_TO_L1_HOPS*(HEADER_SIZE+ADDRESS_SIZE) + L1_TO_L1_HOPS*(HEADER_SIZE+ACK_ADDRESS_SIZE+LINE_SIZE)], 'V'),
        ('load', 'V'): ([0, L1_HIT, 0, 0], 'V'),
    }[access, cache_state]
    costs[core] += cost
    cache_states[core][line_addr] = new_cache_state


def mode_Spandex(tag, core, access, line_addr, cache_state, cache_states, costs):
    if access == 'store':
        if cache_state == 'S' or cache_state == 'I':
            costs[core] += [L2_FORWARD, 0, 2*L1_TO_L2_HOPS*(HEADER_SIZE+ADDRESS_SIZE) + L1_TO_L1_HOPS*(HEADER_SIZE+ACK_ADDRESS_SIZE), 0]
            for other_core in cache_states:
                cache_states[other_core][line_addr] = 'S'
            cache_states[core][line_addr] = 'O'
        elif cache_state == 'O':
            costs[core] += [L1_HIT, 0, 0, 0]
        else:
            raise RuntimeError('fUnknown state {cache_state}')
    elif access == 'load':
        if cache_state == 'I':
            costs[core] += [0, L2_FORWARD, 0, 2*L1_TO_L2_HOPS*(HEADER_SIZE+ADDRESS_SIZE) + L1_TO_L1_HOPS*(HEADER_SIZE+ACK_ADDRESS_SIZE+LINE_SIZE)]
            for other_core in cache_states:
                cache_states[other_core][line_addr] = 'S'
        elif cache_state == 'S':
            costs[core] += [0, L1_HIT, 0, 0]
        else:
            raise RuntimeError('fUnknown state {cache_state}')
    else:
        raise RuntimeError('fUnknown access {access}')


def mode_MESI(tag, core, access, line_addr, cache_state, cache_states, costs):
    if access == 'store':
        if cache_state == 'S' or cache_state == 'I':
            costs[core] += [BUS_FORWARD, 0, 2*L1_TO_L2_HOPS*(HEADER_SIZE+ADDRESS_SIZE) + L1_TO_L1_HOPS*(HEADER_SIZE+ACK_ADDRESS_SIZE), 0]
            for other_core in cache_states:
                cache_states[other_core][line_addr] = 'S'
            cache_states[core][line_addr] = 'M'
        elif cache_state == 'M':
            costs[core] += [L1_HIT, 0, 0, 0]
        else:
            raise RuntimeError('fUnknown state {cache_state}')
    elif access == 'load':
        if cache_state == 'I':
            costs[core] += [0, BUS_FORWARD, 0, 2*L1_TO_L2_HOPS*(HEADER_SIZE+ADDRESS_SIZE) + L1_TO_L1_HOPS*(HEADER_SIZE+ACK_ADDRESS_SIZE+LINE_SIZE)]
            for other_core in cache_states:
                cache_states[other_core][line_addr] = 'S'
        elif cache_state == 'S':
            costs[core] += [0, L1_HIT, 0, 0]
        else:
            raise RuntimeError('fUnknown state {cache_state}')
    else:
        raise RuntimeError('fUnknown access {access}')

modes = {
    (mode_name[5:], mode_fn)
    for mode_name, mode_fn in sorted(globals().items())
    if mode_name.startswith('mode_')
}


@click.command()
@click.option('--name', default='""')
def main(name):
    import dask.bag
    import itertools

    accesses = [
        (tag, core, access, int(zero_ex_addr[2:], 16))
        for tag, core, access, zero_ex_addr in (
                line.strip().split(' ')
                for line in sys.stdin
                if line.startswith('s_')
        )
    ]

    writer = csv.writer(sys.stdout)

    if not name:
        writer.writerow(['program', 'mode', 'core', 'store', 'load', 'traffic', '(header)'])

    mode_names_costs = list(
        dask.bag.from_sequence(modes, npartitions=len(modes))
        .map(lambda mode: (mode[0], analyze(accesses, mode[1])))
    )

    for mode_name, costs in sorted(mode_names_costs):
        for core, cost_kinds in sorted(costs.items()):
            writer.writerow([name, mode_name, core] + list(cost_kinds))


if __name__ == '__main__':
    main()
