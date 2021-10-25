//
// Copyright (c) 2016-2018 Haggai Eran, Gabi Malka, Lior Zeno, Maroun Tork
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without modification,
// are permitted provided that the following conditions are met:
//
//  * Redistributions of source code must retain the above copyright notice, this
// list of conditions and the following disclaimer.
//  * Redistributions in binary form must reproduce the above copyright notice,
// this list of conditions and the following disclaimer in the documentation and/or
// other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ``AS IS''
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
// ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
// LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
// ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//

#ifndef MEMCACHEDCACHE_HPP
#define MEMCACHEDCACHE_HPP

#include <ntl/maybe.hpp>
#include <boost/functional/hash.hpp>

template <unsigned Size, unsigned Bits>
struct byte_array {
    char data[Size];

    byte_array(const ap_uint<Bits>& d = 0)
    {
        for (unsigned int i = 0; i < Size; ++i)
            data[i] = d(8 * i + 7, 8 * i);
    }

    bool operator==(const byte_array<Size, Bits>& rhs) const {
         bool equal = true;

         for (unsigned int i = 0; i < Size; ++i) {
#pragma HLS unroll
              if (data[i] != rhs.data[i]) {
                   equal = false;
                   break;
              }
         }

         return equal;
    }

    operator ap_uint<Bits>() const
    {
        ap_uint<Bits> ret;
        for (unsigned int i = 0; i < Size; ++i)
            ret(8 * i + 7, 8 * i) = data[i];
        return ret;
    }
};

template <unsigned Size>
struct memcached_key : byte_array<Size, 128>
{
    memcached_key() {}
    memcached_key(const ap_uint<128>& d) : byte_array<Size, 128>(d) {}
};

template <unsigned Size>
struct memcached_value : byte_array<Size, 128>
{
    memcached_value() {}
    memcached_value(const ap_uint<128>& d) : byte_array<Size, 128>(d) {}
};

template <unsigned KeySize, unsigned ValueSize>
struct entry
{
    static_assert(KeySize <= 16 && ValueSize <= 16, "Key / Value too large");

    ap_uint<1> valid;
    memcached_key<KeySize> key;
    memcached_value<ValueSize> value;

    entry(const ap_uint<512>& d) :
        valid(d(511, 511)),
        key(d(255, 128)),
        value(d(127, 0))
    {}

    entry(const memcached_key<KeySize>& key, const memcached_value<ValueSize>& value, bool valid) :
        valid(valid),
        key(key),
        value(value)
    {}

    operator ap_uint<512>() const
    {
        ap_uint<128> key = this->key,
                     value = this->value;
        return (valid, ap_uint<255>(0), key, value);
    }
};

#define MEMCACHED_DEFAULT_CACHE_SIZE (DDR_SIZE >> 6)

template <unsigned KeySize, unsigned ValueSize>
class memcached_cache
{
public:
    typedef entry<KeySize, ValueSize> entry_t;
    typedef uint64_t index_t;

    memcached_cache() {
        // TODO initialize DRAM with valid bit = 0
#pragma HLS stream variable=outstanding_reads depth=510
    }

    typedef hls_ik::memory_t memory;

    void insert(memory& m, const memcached_key<KeySize>& key, const memcached_value<ValueSize>& value, index_t log_size, int ikernel_id)
    {
#pragma HLS inline
        index_t index = h(key, log_size, ikernel_id);

        m.write(index, entry_t(key, value, true));
    }

    void erase(memory& m, const memcached_key<KeySize>& k, index_t log_size, int ikernel_id)
    {
#pragma HLS inline
        index_t index = h(k, log_size, ikernel_id);

        // TODO reading the tag to check whether the existing value is actually
        // the one we want to erase causes HLS to detect a dependency and fail
        // to optimize.
        m.write(index, entry_t(memcached_key<KeySize>(), memcached_value<ValueSize>(), false));
    }

    bool can_post_find(memory& m)
    {
#pragma HLS inline
        return !outstanding_reads.full();
    }

    void find(memory& m, const memcached_key<KeySize>& k, index_t log_size, int ikernel_id)
    {
#pragma HLS inline
        if (!can_post_find(m))
            return;

        index_t index = h(k, log_size, ikernel_id);
        m.post_read(index);
        outstanding_reads.write(k);
    }

    bool has_find_result(memory& m)
    {
        return !outstanding_reads.empty() && m.has_read_response();
    }

    ntl::maybe<memcached_value<ValueSize> > get_find_result(memory& m)
    {
#pragma HLS inline
        entry_t ent = m.get_read_response();
        memcached_key<KeySize> k;
        outstanding_reads.read_nb(k);

        return ntl::maybe<memcached_value<ValueSize> >(ent.valid && ent.key == k, ent.value);
    }

private:
    index_t h(const memcached_key<KeySize>& tag, index_t log_size, int ikernel_id) const {
#pragma HLS inline
        ap_uint<31 - 6> hash = h_b(h_a(tag), tag) & ((1 << log_size) - 1);
        ap_uint<10> ik = ikernel_id;
        return (ik, hash);
    }

    index_t h_a(const memcached_key<KeySize>& tag) const {
#pragma HLS pipeline enable_flush ii=3
	    index_t seed = 5381;
	    for (unsigned int i = 0; i < KeySize / 2; ++i) {
#pragma HLS unroll
		    seed = ((seed << 5) + seed) + tag.data[i];
	    }

	    return seed;
    }

    index_t h_b(index_t seed, const memcached_key<KeySize>& tag) const {
#pragma HLS pipeline enable_flush ii=3
	    for (unsigned int i = KeySize / 2; i < KeySize; ++i) {
#pragma HLS unroll
		    seed = ((seed << 5) + seed) + tag.data[i];
	    }

	    return seed;
    }

    hls::stream<memcached_key<KeySize> > outstanding_reads;
};

#endif // MEMCACHEDCACHE_HPP
