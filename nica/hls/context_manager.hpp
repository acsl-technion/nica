/* * Copyright (c) 2016-2018 Haggai Eran, Gabi Malka, Lior Zeno, Maroun Tork
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

#pragma once

#include <tuple>

#include "gateway.hpp"

template <typename context_t, uint8_t log_size>
class base_context_manager {
public:
    typedef ap_uint<log_size> index_t;
    static const size_t size = 1 << log_size;

    base_context_manager() {}

    bool valid_index(uint32_t index)
    {
        return index < (1 << log_size);
    }

    int gateway_set(uint32_t index)
    {
#pragma HLS inline
#pragma HLS data_pack variable=updates
        if (!valid_index(index))
            return GW_FAIL;

        if (updates.full())
            return GW_BUSY;

        updates.write(std::make_tuple(index, gateway_context));
        return GW_DONE;
    }

    context_t& operator[](index_t index)
    {
#pragma HLS inline
        return contexts[index];
    }

    const context_t& operator[](index_t index) const
    {
#pragma HLS inline
        return contexts[index];
    }

    /* Return true when accessing the array for the gateway */
    bool update()
    {
#pragma HLS inline
#pragma HLS data_pack variable=updates
        std::tuple<index_t, context_t> update;
        if (updates.read_nb(update)) {
            index_t index;
            context_t context;

            std::tie(index, context) = update;
            contexts[index] = context;
            return true;
        }

        return false;
    }

    context_t gateway_context;
    context_t contexts[size];

private:

    hls::stream<std::tuple<index_t, context_t> > updates;
};


template <typename context_t, uint8_t log_size>
class context_manager {
public:
    typedef ap_uint<log_size> index_t;
    static const size_t size = 1 << log_size;

    context_manager() : gateway_state(IDLE) {}

    bool valid_index(uint32_t index)
    {
        return index < (1 << log_size);
    }

    int gateway_query(uint32_t index)
    {
#pragma HLS inline
        switch (gateway_state) {
        case IDLE:
            if (!valid_index(index))
                return GW_FAIL;

            if (queries.full())
                return GW_BUSY;

            queries.write(index);
            gateway_state = QUERY_SENT;
            return GW_BUSY;
        case QUERY_SENT:
            if (responses.empty())
                return GW_BUSY;
            gateway_context = responses.read();
            gateway_state = IDLE;
            return GW_DONE;
        default:
            return GW_FAIL;
        }
    }

    /* Read-modify-write operation from the gateway */
    template <typename F>
    int gateway_rmw(uint32_t index, F f)
    {
#pragma HLS inline
#pragma HLS data_pack variable=updates
        switch (gateway_state) {
        case IDLE:
            if (!valid_index(index))
                return GW_FAIL;

            if (queries.full())
                return GW_BUSY;

            queries.write(index);
            gateway_state = QUERY_SENT;
            return GW_BUSY;
        case QUERY_SENT:
            if (responses.empty())
                return GW_BUSY;

            gateway_context = responses.read();
            gateway_state = UPDATE;
            /* Fall through */
        case UPDATE:
            if (updates.full())
                return GW_BUSY;

            updates.write(std::make_tuple(index, f(gateway_context)));
            gateway_state = IDLE;
            return GW_DONE;

        default:
            return GW_FAIL;
        }
    }

    template <typename Type, Type context_t:: *field>
    int gateway_access_field(uint32_t index, int *value, bool read)
    {
#pragma HLS inline
        int ret;
        if (read) {
            ret = gateway_query(index);
            if (ret == GW_DONE)
                *value = gateway_context.*field;
            return ret;
        } else {
            return gateway_rmw(index, [value](context_t c) -> context_t {
                c.*field = *value;
                return c;
            });
        }
    }

    /* Return true when accessing the array for the gateway */
    bool update()
    {
#pragma HLS inline
#pragma HLS data_pack variable=updates
        std::tuple<index_t, context_t> update;
        if (updates.read_nb(update)) {
            index_t index;
            context_t context;

            std::tie(index, context) = update;
            contexts[index] = context;
            return true;
        }
        if (!queries.empty() && !responses.full()) {
            index_t index;
            queries.read_nb(index);
            responses.write_nb(contexts[index]);
            return true;
        }

        return false;
    }

    int gateway_set(uint32_t index)
    {
#pragma HLS inline
#pragma HLS data_pack variable=updates
        if (!valid_index(index))
            return GW_FAIL;

        if (updates.full())
            return GW_BUSY;

        updates.write(std::make_tuple(index, gateway_context));
        return GW_DONE;
    }

    context_t& operator[](index_t index)
    {
#pragma HLS inline
        return contexts[index];
    }

    const context_t& operator[](index_t index) const
    {
#pragma HLS inline
        return contexts[index];
    }

    context_t gateway_context;
    context_t contexts[size];

private:
    enum { IDLE, QUERY_SENT, UPDATE } gateway_state;

    hls::stream<index_t> queries;
    hls::stream<context_t> responses;
    hls::stream<std::tuple<index_t, context_t> > updates;
};
