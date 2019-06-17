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

#pragma once

#include <ap_int.h>

template <typename T>
struct pack {
    static const int width = T::width;

    static ap_uint<width> to_int(const T& e) {
        return e;
    }

    static T from_int(const ap_uint<width>& d) {
        return T(d);
    }
};

template <typename T1, typename T2>
struct pack<std::tuple<T1, T2> > {
    static const int width = pack<T1>::width + pack<T2>::width;

    typedef std::tuple<T1, T2> type;

    static ap_uint<width> to_int(const type& e) {
        return (pack<T1>::to_int(std::get<0>(e)),
                pack<T2>::to_int(std::get<1>(e)));
    }

    static type from_int(const ap_uint<width>& d) {
        T1 t1 = pack<T1>::from_int(d(width - 1, pack<T2>::width));
        T2 t2 = pack<T2>::from_int(d(pack<T2>::width - 1, 0));
        return make_tuple(t1, t2);
    }
};

template <typename T>
ap_uint<pack<T>::width> pack_to_int(const T& t)
{
    return pack<T>::to_int(t);
}

template <typename T>
T unpack(const ap_uint<pack<T>::width>& d)
{
    return pack<T>::from_int(d);
}

template <typename T>
class pack_stream
{
public:
    typedef T type;
    static const int width = pack<type>::width;
    typedef ap_uint<width> raw_t;

    pack_stream() {}
    pack_stream(const char *name) : raw_s(name) {}

    bool full() const
    {
#pragma HLS inline
        return raw_s.full();
    }

    bool empty() const
    {
#pragma HLS inline
        return raw_s.empty();
    }

    void write(const type& value)
    {
#pragma HLS inline
        raw_s.write(pack_to_int(value));
    }

    bool write_nb(const type& value)
    {
#pragma HLS inline
        return raw_s.write_nb(pack_to_int(value));
    }

    T read()
    {
#pragma HLS inline
        return unpack<type>(raw_s.read());
    }

    bool read_nb(type& value)
    {
#pragma HLS inline
        raw_t raw;
        bool ret = raw_s.read_nb(raw);
        if (ret)
            value = unpack<type>(raw);
        return ret;
    }
private:
    hls::stream<raw_t> raw_s;
};
