//
// Copyright (c) 2016-2017 Haggai Eran, Gabi Malka, Lior Zeno, Maroun Tork
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

#include "pack.hpp"

template <typename Value>
class maybe {
public:
    maybe(bool valid, const Value& v) : _valid(valid), _value(v) {}
    maybe(const Value& v) : _valid(true), _value(v) {}
    maybe() : _valid(false) {}

    bool valid() const { return _valid; }
    const Value& value() const { return _value; }

    operator bool() const { return valid(); }

    void reset() { _valid = false; }
private:
    bool _valid;
    Value _value;
};

template <typename Value>
maybe<Value> make_maybe(bool valid, const Value& v)
{
    return maybe<Value>(valid, v);
}

template <typename Value>
maybe<Value> make_maybe(const Value& v)
{
    return maybe<Value>(true, v);
}

template <typename T>
struct pack<maybe<T> > {
    static const int width = 1 + pack<T>::width;
    static ap_uint<width> to_int(const maybe<T>& e) {
        return (ap_uint<1>(e.valid()), pack<T>::to_int(e.value()));
    }

    static maybe<T> from_int(const ap_uint<width>& d) {
        bool valid = d(width - 1, width - 1);
        T value = pack<T>::from_int(d(width - 2, 0));
        return make_maybe(valid, value);
    }
};
