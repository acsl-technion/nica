/* * Copyright (c) 2016-2017 Haggai Eran, Gabi Malka, Lior Zeno, Maroun Tork
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 *  * Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation and/or
 * other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "hls_helper.h"

#pragma once

/* A FIFO that provides backpressure when it has X elements left. */
template <typename T, size_t stream_depth = 15, size_t index_width = 1 + hls_helpers::log2(stream_depth)>
class programmable_fifo
{
public:
    typedef ap_uint<index_width> index_t;
    programmable_fifo(index_t full_threshold = 490, index_t empty_threshold = 0) :
            _stream(),
            _pi(0), _ci(0), _full_threshold(full_threshold),
            _empty_threshold(empty_threshold),
            _empty_local_pi(0), _full_local_ci(0),
            _pi_updates(), _ci_updates(),
            _pi_update_flag(0), _ci_update_flag(0)
    { }

    programmable_fifo(index_t full_threshold, index_t empty_threshold, const std::string& name) :
            _stream(name.c_str()),
            _pi(0), _ci(0), _full_threshold(full_threshold),
            _empty_threshold(empty_threshold),
            _empty_local_pi(0), _full_local_ci(0),
            _pi_updates((name + "_pi").c_str()), _ci_updates((name + "_ci").c_str()),
            _pi_update_flag(0), _ci_update_flag(0)
    { }

    void write(const T& t) {
#pragma HLS inline
        ++_pi;
        _pi_update_flag = 1;
        _stream.write(t);
    }

    bool write_nb(const T& t) {
#pragma HLS inline
        if (full())
            return false;
        ++_pi;
        _pi_update_flag = 1;
        _stream.write_nb(t);
        return true;
    }

    T read() {
#pragma HLS inline
        ++_ci;
        _ci_update_flag = 1;
        return _stream.read();
    }

    bool read_nb(T& t) {
#pragma HLS inline
        if (empty())
            return false;
        ++_ci;
        _ci_update_flag = 1;
        _stream.read_nb(t);
        return true;
    }

    /* Empty progress must be called constantly from within the function that
     * calls empty(), to prevent deadlocks */
    void empty_progress() {
#pragma HLS inline
        if (!_pi_updates.empty())
            _pi_updates.read_nb(_empty_local_pi);
        if (_ci_update_flag) {
            if (_ci_updates.write_nb(_ci))
                _ci_update_flag = 0;
        }
    }

    ap_uint<index_width - 1> count(index_t pi, index_t ci)
    {
#pragma HLS inline
        return pi - ci;
    }

    bool empty() {
#pragma HLS inline
        return _empty_threshold ? count(_empty_local_pi, _ci) <= _empty_threshold :
               _stream.empty();
    }

    /* Full progress must be called constantly from within the function that
     * calls full(), to prevent deadlocks */
    void full_progress() {
#pragma HLS inline
        if (!_ci_updates.empty())
            _ci_updates.read_nb(_full_local_ci);
        if (_pi_update_flag) {
            if (_pi_updates.write_nb(_pi))
                _pi_update_flag = 0;
        }
    }

    bool full() {
#pragma HLS inline
        return count(_pi, _full_local_ci) >= _full_threshold;
    }

    bool internal_full() {
#pragma HLS inline
        return _stream.full();
    }

    // Exposed for data_pack pragmas in memcached
    hls::stream<T> _stream;

private:
    index_t _pi, _ci, _full_threshold, _empty_threshold;
    index_t _empty_local_pi, _full_local_ci;
    hls::stream<index_t> _pi_updates, _ci_updates;
    ap_uint<1> _pi_update_flag, _ci_update_flag;
};