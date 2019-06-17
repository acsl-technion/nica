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

#ifndef HLS_HELPER_H
#define HLS_HELPER_H

#include <hls_stream.h>
#include <ap_int.h>
#include <type_traits>

#define PRAGMA_SUB(x) _Pragma(#x)
/* Use DO_PRAGMA to be able to have C preprocessor expansion in a pragma */
#define DO_PRAGMA(x) PRAGMA_SUB(x)

namespace hls_helpers {

    template <typename T>
    bool any_full(hls::stream<T>& out1, hls::stream<T>& out2)
    {
        return out1.full() || out2.full();
    }

    template <typename T>
    void dup(hls::stream<T>& in, hls::stream<T>& out1, hls::stream<T>& out2)
    {
    #pragma HLS pipeline II=1 enable_flush
        if (!in.empty() && !any_full(out1, out2)) {
            T word;
            in.read(word);
            out1.write(word);
            out2.write(word);
        }
    }

template <unsigned num_streams, typename data>
class duplicator
{
public:
    typedef hls::stream<data> stream;
    void dup_array(stream& in, stream out[num_streams])
    {
#pragma HLS pipeline II=1 enable_flush
        if (!in.empty() && !any_full(out)) {
            data word;
            in.read(word);
            for (unsigned i = 0; i < num_streams; ++i)
                out[i].write(word);
        }
    }

    void dup2(stream& in, stream& out1, stream& out2)
    {
#pragma HLS pipeline II=1 enable_flush
        if (!in.empty() && !any_full(out1, out2)) {
            data word;
            in.read(word);
            out1.write(word);
            out2.write(word);
        }
    }

    void dup3(stream& in, stream& out1, stream& out2, stream& out3)
    {
#pragma HLS pipeline II=1 enable_flush
        if (!in.empty() && !any_full(out1, out2, out3)) {
            data word;
            in.read(word);
            out1.write(word);
            out2.write(word);
            out3.write(word);
        }
    }

    void dup4(stream& in, stream& out1, stream& out2, stream& out3, stream& out4)
    {
#pragma HLS inline
        dup2(in, intermediate1, intermediate2);
        dup2(intermediate1, out1, out2);
        dup2(intermediate2, out3, out4);
    }
private:

    bool any_full(stream out[num_streams])
    {
    	for (int i = 0; i < num_streams; ++i)
    		if (out[i].full())
    			return true;
    	return false;
    }

    bool any_full(stream& out1, stream& out2)
    {
        return out1.full() || out2.full();
    }

    bool any_full(stream& out1, stream& out2, stream& out3)
    {
    	return out1.full() || out2.full() || out3.full();
    }

    stream intermediate1, intermediate2;
};

template <typename Functor>
class unary_stream_type
{
public:
    constexpr unary_stream_type(Functor f) : f(f) {}

    template <typename X>
    void step(hls::stream<X>& in, hls::stream<decltype(f(std::declval<X>()))>& out)
    {
#pragma HLS pipeline enable_flush ii=1
        if (!in.empty() && !out.full()) {
            out.write(f(in.read()));
        }
    }
private:
    Functor f;
};

template <typename Functor>
unary_stream_type<Functor> unary_stream(Functor f) { return unary_stream_type<Functor>(f); }

template <typename Functor>
class binary_stream
{
public:
    typedef typename Functor::first_argument_type first_argument_type;
    typedef typename Functor::second_argument_type second_argument_type;
    typedef typename Functor::result_type result_type;
    void step(hls::stream<first_argument_type>& in1, hls::stream<second_argument_type>& in2, hls::stream<result_type>& out)
    {
#pragma HLS pipeline enable_flush
        if (!in1.empty() && !in2.empty() && !out.full()) {
            out.write(f(in1.read(), in2.read()));
        }
    }
private:
    Functor f;
};

typedef unary_stream_type<std::logical_not<bool> > not_stream;

static inline ap_uint<16> swap16(const ap_uint<16> d)
{
	return (d(7, 0), d(15, 8));
}

/* AXI Stream output cannot check for fullness as it generates code that is not
 * allowed in the standard. */
template <typename T1, typename T2>
static inline void link_axi_stream(hls::stream<T1>& in, hls::stream<T2>& out)
{
#pragma HLS pipeline enable_flush ii=1
    T1 flit;
    if (in.read_nb(flit))
        out.write(T2(flit));
}

template <typename T1, typename T2>
static inline void link_fifo(hls::stream<T1>& in, hls::stream<T2>& out)
{
#pragma HLS pipeline enable_flush ii=1
    if (in.empty() || out.full())
        return;

    T1 flit;
    in.read_nb(flit);
    out.write_nb(T2(flit));
}

template <typename T1, typename T2>
static inline void link_axi_to_fifo(hls::stream<T1>& in, hls::stream<T2>& out)
{
#pragma HLS pipeline enable_flush ii=1
    if (in.empty() || out.full())
        return;

    T1 flit = in.read();
    out.write_nb(T2(flit));
}

template <typename T>
void consume(hls::stream<T>& in)
{
    if (!in.empty())
        in.read();
}

template <typename T>
void consume(hls::stream<T>& in, bool enabled)
{
    T tmp;
    if (enabled && !in.empty())
/* In simulation build we read a FIFO, but with synthesis it is an AXI4-Stream.
 * This causes an error when trying to use read_nb about an interface mismatch:
 *
 *   ERROR: [XFORM 203-801] Interface read on 'in.V.V' has incompatible types.
 *   Possible cause(s): data pack is only applied on source(port) or
 *   destination(variable).
 *   ERROR: [HLS 200-70] Failed building synthesis data model.
 */
#ifdef SIMULATION_BUILD
        in.read_nb(tmp);
#else
        in.read(tmp);
#endif
}

template <unsigned Width>
unsigned char get_byte(const ap_uint<Width>& vec, const int i) {
#pragma HLS inline
    const int bottom = (Width - 1) - ((i + 1) * 8 - 1), top = (Width - 1) - (i * 8);

    return vec(top, bottom);
}

template <unsigned Width>
void write_byte(ap_uint<Width>& vec, const int i, const unsigned char val) {
#pragma HLS inline
    const int bottom = (Width - 1) - ((i + 1) * 8 - 1), top = (Width - 1) - (i * 8);

    vec(top, bottom) = val;
}

template <unsigned Size, typename T>
void memcpy(T *dest, const T *src) {
#pragma HLS inline
    for (int i = 0; i < Size; ++i) {
#pragma HLS unroll
         dest[i] = src[i];
    }
}

template <unsigned Size, int Width, int Width2>
void memcpy(ap_uint<Width>& dest, ap_uint<Width2>& src) {
#pragma HLS inline
    for (int i = 0; i < Size; ++i) {
#pragma HLS unroll
        write_byte<Width>(dest, i, get_byte<Width2>(src, i));
    }
}

template <unsigned Size, int Width>
void ap_uint_to_char(char *dst, ap_uint<Width>& src) {
#pragma HLS inline
    for (int i = 0; i < Size; ++i) {
#pragma HLS unroll
        dst[i] = get_byte<Width>(src, i);
    }
}

template <unsigned Size, typename T>
void memset(T *dest, T value) {
#pragma HLS inline
    for (int i = 0; i < Size; ++i) {
#pragma HLS unroll
        dest[i] = value;
    }
}

inline int bytes_to_int(const unsigned char* bytes, const uint32_t offset) {
#pragma HLS inline
    return (bytes[4 * offset]) << 24 |
           (bytes[4 * offset + 1] & 0xFF) << 16 |
           (bytes[4 * offset + 2] & 0xFF) << 8 |
           (bytes[4 * offset + 3] & 0xFF);
}

template <unsigned Width>
inline void int_to_bytes(const int src, ap_uint<Width>& dest, const uint32_t offset) {
#pragma HLS inline
    write_byte<Width>(dest, 4 * offset, src >> 24);
    write_byte<Width>(dest, 4 * offset + 1, src >> 16);
    write_byte<Width>(dest, 4 * offset + 2, src >> 8);
    write_byte<Width>(dest, 4 * offset + 3, src);
}

/* Zero out bytes in the data stream that have their keep bit cleared */
template <typename axi>
// TODO instead of 256 get the width from axi::data::width
ap_uint<256> mask_last_word(axi word)
{
    const size_t bits = 256;
    const size_t bytes = bits / 8;
    ap_uint<8> ret[bytes];
#pragma HLS array_partition variable=ret complete
    ap_uint<bits> result;

    for (unsigned i = 0; i < sizeof(ret); ++i)
        ret[i] = word.data((i+1) * 8 - 1, i * 8);

    if (word.last) {
        for (unsigned i = 0; i < sizeof(ret); ++i)
            if (!word.keep(bytes - 1 - i, bytes - 1 - i))
                ret[i] = 0;
    }

    for (unsigned i = 0; i < sizeof(ret); ++i)
        result((i+1) * 8 - 1, i * 8) = ret[i];

    return result;
}

} // namespace

#endif // HLS_HELPER_H
