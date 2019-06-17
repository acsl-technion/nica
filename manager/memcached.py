class Memcached(object):
    def __init__(self, nica):
        self.gw = nica.ikernel0

    MEMCACHED_REG_CACHE_SIZE = 0
    MEMCACHED_STATS_GET_REQUESTS = 0x10
    MEMCACHED_STATS_GET_REQUESTS_HITS = 0x11
    MEMCACHED_STATS_GET_REQUESTS_MISSES = 0x12
    MEMCACHED_STATS_SET_REQUESTS = 0x13
    MEMCACHED_STATS_N2H_UNKNOWN = 0x14
    MEMCACHED_STATS_GET_RESPONSE = 0x15
    MEMCACHED_STATS_H2N_UNKNOWN = 0x16
    MEMCACHED_DROPPED_BACKPRESSURE = 0x17
    MEMCACHED_RING_ID = 0x18
    MEMCACHED_STATS_GET_REQUESTS_DROPPED_HITS = 0x19
    MEMCACHED_STATS_DROPPED_TC_BACKPRESSURE = 0x20

    def set_ring_id(self, ikernel_id, ring_id, delay=None):
        self.gw.write(self.MEMCACHED_RING_ID, ring_id, ikernel_id=ikernel_id,
                      delay=delay)

    def set_cache_size(self, ikernel_id, log_dram_size, delay=None):
        self.gw.write(self.MEMCACHED_REG_CACHE_SIZE, log_dram_size,
                      ikernel_id=ikernel_id, delay=delay)
