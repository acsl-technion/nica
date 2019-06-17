#!/usr/bin/python
__author__ = 'Thomas Dixon'
__license__ = 'MIT'

import copy, struct, sys

def new(m=None):
    return sha256(m)

class sha256(object):
    _k = (0x428a2f98L, 0x71374491L, 0xb5c0fbcfL, 0xe9b5dba5L,
          0x3956c25bL, 0x59f111f1L, 0x923f82a4L, 0xab1c5ed5L,
          0xd807aa98L, 0x12835b01L, 0x243185beL, 0x550c7dc3L,
          0x72be5d74L, 0x80deb1feL, 0x9bdc06a7L, 0xc19bf174L,
          0xe49b69c1L, 0xefbe4786L, 0x0fc19dc6L, 0x240ca1ccL,
          0x2de92c6fL, 0x4a7484aaL, 0x5cb0a9dcL, 0x76f988daL,
          0x983e5152L, 0xa831c66dL, 0xb00327c8L, 0xbf597fc7L,
          0xc6e00bf3L, 0xd5a79147L, 0x06ca6351L, 0x14292967L,
          0x27b70a85L, 0x2e1b2138L, 0x4d2c6dfcL, 0x53380d13L,
          0x650a7354L, 0x766a0abbL, 0x81c2c92eL, 0x92722c85L,
          0xa2bfe8a1L, 0xa81a664bL, 0xc24b8b70L, 0xc76c51a3L,
          0xd192e819L, 0xd6990624L, 0xf40e3585L, 0x106aa070L,
          0x19a4c116L, 0x1e376c08L, 0x2748774cL, 0x34b0bcb5L,
          0x391c0cb3L, 0x4ed8aa4aL, 0x5b9cca4fL, 0x682e6ff3L,
          0x748f82eeL, 0x78a5636fL, 0x84c87814L, 0x8cc70208L,
          0x90befffaL, 0xa4506cebL, 0xbef9a3f7L, 0xc67178f2L)
    _h = (0x6a09e667L, 0xbb67ae85L, 0x3c6ef372L, 0xa54ff53aL,
          0x510e527fL, 0x9b05688cL, 0x1f83d9abL, 0x5be0cd19L)
    _output_size = 8
    
    blocksize = 1
    block_size = 64
    digest_size = 32
    
    def __init__(self, m=None):        
        self._buffer = ''
        self._counter = 0
        
        if m is not None:
            if type(m) is not str:
                raise TypeError, '%s() argument 1 must be string, not %s' % (self.__class__.__name__, type(m).__name__)
            self.update(m)
        
    def _rotr(self, x, y):
        return ((x >> y) | (x << (32-y))) & 0xFFFFFFFFL
                    
    def _sha256_process(self, c):
        w = [0]*64
        w[0:16] = struct.unpack('!16L', c)
        
        for i in range(16, 64):
            s0 = self._rotr(w[i-15], 7) ^ self._rotr(w[i-15], 18) ^ (w[i-15] >> 3)
            s1 = self._rotr(w[i-2], 17) ^ self._rotr(w[i-2], 19) ^ (w[i-2] >> 10)
            w[i] = (w[i-16] + s0 + w[i-7] + s1) & 0xFFFFFFFFL
        
        a,b,c,d,e,f,g,h = self._h
        
        for i in range(64):
            s0 = self._rotr(a, 2) ^ self._rotr(a, 13) ^ self._rotr(a, 22)
            maj = (a & b) ^ (a & c) ^ (b & c)
            t2 = s0 + maj
            s1 = self._rotr(e, 6) ^ self._rotr(e, 11) ^ self._rotr(e, 25)
            ch = (e & f) ^ ((~e) & g)
            t1 = h + s1 + ch + self._k[i] + w[i]
            
            h = g
            g = f
            f = e
            e = (d + t1) & 0xFFFFFFFFL
            d = c
            c = b
            b = a
            a = (t1 + t2) & 0xFFFFFFFFL
            
        self._h = [(x+y) & 0xFFFFFFFFL for x,y in zip(self._h, [a,b,c,d,e,f,g,h])]
        
    def update(self, m):
        if not m:
            return
        if type(m) is not str:
            raise TypeError, '%s() argument 1 must be string, not %s' % (sys._getframe().f_code.co_name, type(m).__name__)
        
        self._buffer += m
        self._counter += len(m)
        
        while len(self._buffer) >= 64:
            for i in range(64):
                print ord(self._buffer[:64][i]),
            print
            self._sha256_process(self._buffer[:64])
            self._buffer = self._buffer[64:]
            
    def digest(self):
        mdi = self._counter & 0x3F
        length = struct.pack('!Q', self._counter<<3)
        
        if mdi < 56:
            padlen = 55-mdi
        else:
            padlen = 119-mdi
        
        r = self.copy()
        r.update('\x80'+('\x00'*padlen)+length)
        print 'Length = {}'.format(self._counter << 3)
        return ''.join([struct.pack('!L', i) for i in r._h[:self._output_size]])
        
    def hexdigest(self):
        return self.digest().encode('hex')
        
    def copy(self):
        return copy.deepcopy(self)

h = sha256()
h.update('ESUDSB6666666666666666666666666666666666666666666666666666666666eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJkYXRhIjoiSGVsbG8iLCJpYXQiOjE1MjAxOTA4NzN9')
print h.hexdigest()
