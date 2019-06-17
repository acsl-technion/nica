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

#include <ntl/constexpr.hpp>

#include <cassert>

/** Holds a value of either one of two types (Left or Right).
 * Left and Right must implement conversion functions to ap_uint and expose a
 * static width field with their required number of bits
 */
template <typename Left, typename Right>
class either : public boost::equality_comparable<either<Left, Right> >
{
public:
    enum { width = 1 + ntl::max(Left::width, Right::width), };

    bool is_left() const { return !data(width - 1, width - 1); }

    template <typename T>
    T get() const
    {
        const int cast_width = T::width;
        const ap_uint<cast_width> cast_data = data(cast_width - 1, 0);
        return T(cast_data);
    }

    either(const Left& x) :
        data((ap_uint<width - Left::width>(0),
              static_cast<ap_uint<Left::width> >(x)))
    {
        assert(is_left());
    }

    either(const Right& x) :
        data((ap_uint<width - Right::width>(0),
              static_cast<ap_uint<Right::width> >(x)))
    {
        data(width - 1, width - 1) = 1;
        assert(!is_left());
    }

    either(const ap_uint<width> d = ap_uint<width>(0)) : data(d) {}

    bool operator ==(const either<Left, Right>& o) const {
        if (is_left() != o.is_left()) return false;
        if (is_left()) return get<Left>() == o.get<Left>();
        else           return get<Right>() == o.get<Right>();
    }

    operator ap_uint<width>() const {
        return data;
    }

private:
    ap_uint<width> data;
};
