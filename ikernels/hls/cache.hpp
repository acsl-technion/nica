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

#ifndef CACHE_HPP
#define CACHE_HPP

#include <boost/functional/hash.hpp>

#include <hls_stream.h>
#include <ap_int.h>

#include "gateway.hpp"
#include "maybe.hpp"
#include <hls_helper.h>

template <typename Tag, typename Value, unsigned Size, int max_hops = Size, bool insert_overrides = true>
class cache
{
public:
    typedef Tag tag_type;
    typedef std::tuple<Tag, Value> value_type;
    typedef uint64_t index_t;

    cache()
    {
#pragma HLS resource core=RAM_T2P_BRAM variable=tags
#pragma HLS resource core=RAM_T2P_BRAM variable=values
#pragma HLS resource core=RAM_T2P_BRAM variable=valid
        for (int i = 0; i < Size; ++i) {
            valid[i] = false;
            tags[i] = Tag();
            values[i] = Value();
        }
    }

    maybe<index_t> insert(const Tag& key, const Value& value)
    {
        maybe<index_t> index = lookup(h(key), key);

        if (!index.valid()) {
            return index;
        }

        if (valid[index.value()] && !insert_overrides)
                return maybe<index_t>();

        tags[index.value()] = key;
        values[index.value()] = value;
        valid[index.value()] = true;

        return index;
    }

    bool erase(const Tag& k)
    {
        maybe<index_t> index = lookup(h(k), k);

        if (!index.valid() || !valid[index.value()] || tags[index.value()] != k) {
            return false;
        }

        valid[index.value()] = false;

        // fill the hole if needed
        index_t hash = index.value();
        bool found = false;

        for (int i = 1; i < max_hops; ++i) {
            if (found) continue;

            index_t cur = (hash + i) % Size;

            if (!valid[cur]) continue;
            if (h(tags[cur]) <= index.value()) {
                tags[index.value()] = tags[cur];
                values[index.value()] = values[cur];
                valid[index.value()] = true;
                valid[cur] = false;
                found = true;
                continue;
            }
        }

        return true;
    }

    maybe<Value> find(const Tag& k, index_t& out_index) const
    {
#pragma HLS inline
        maybe<index_t> index = lookup(h(k), k);

        if (!index.valid() || !valid[index.value()] || tags[index.value()] != k) {
            return maybe<Value>();
        }

        Value value = values[index.value()];
        out_index = index.value();
        return maybe<Value>(value);
    }

    maybe<Value> find(const Tag& k) const
    {
#pragma HLS inline
        index_t index;

        return find(k, index);
    }

    /* For debugging */
    bool set_entry(index_t index, bool set_valid, const Tag& tag, const Value& value) {
        bool result = !valid[index];

        valid[index] = set_valid;
        tags[index] = tag;
        values[index] = value;

        return result;
    }

    const Tag& get_tag(index_t index) const { return tags[index % Size]; }
    const Value& get_value(index_t index) const { return values[index % Size]; }
    bool get_valid(index_t index) const { return valid[index % Size]; }

private:
    index_t h(const Tag& tag) const { return boost::hash<Tag>()(tag) % Size; }

    maybe<index_t> lookup(index_t hash, const Tag& tag) const {
        for (int i = 0; i < max_hops; ++i) {
            hash = (hash + 1) % Size;
            if (!valid[hash] || tags[hash] == tag)
                return maybe<index_t>(hash);
        }

        return maybe<index_t>();
    }

    Tag tags[Size];
    Value values[Size];
    bool valid[Size];
};

enum gateway_command_enum {
    HASH_INSERT,
    HASH_ERASE,
    HASH_READ,
    HASH_WRITE,
};

template <typename index_t, typename maybe_value_t>
struct gateway_command_template
{
    gateway_command_enum cmd;
    index_t index;
    maybe_value_t value;

};

template <typename index_t, typename maybe_value_t>
struct pack<gateway_command_template<index_t, maybe_value_t> > {
    static const int width = 2 + pack<index_t>::width + pack<maybe_value_t>::width;
    typedef gateway_command_template<index_t, maybe_value_t> gateway_command;

    static ap_uint<width> to_int(const gateway_command& e) {
        return (ap_uint<2>(e.cmd), pack<index_t>::to_int(e.index), pack<maybe_value_t>::to_int(e.value));
    }

    static gateway_command from_int(const ap_uint<width>& d) {
        gateway_command cmd = {
            gateway_command_enum(int(d(2 + index_t::width + pack<maybe_value_t>::width - 1, index_t::width + pack<maybe_value_t>::width))),
            d(index_t::width + pack<maybe_value_t>::width - 1, pack<maybe_value_t>::width),
            pack<maybe_value_t>::from_int(d(pack<maybe_value_t>::width - 1, 0)),
        };
        return cmd;
    }
};

template <typename Tag_type, typename Mapped_type, unsigned Size>
class hash_table_wrapper
{
public:
    hash_table_wrapper() :
        _hash_table(),
        gateway_commands("gateway_commands"),
        gateway_responses("gateway_responses"),
        gateway_command_sent(false)
    {}

    typedef ap_uint<hls_helpers::log2(Size)> index_t;
    typedef Tag_type tag_type;
    typedef Mapped_type mapped_type;
    typedef cache<tag_type, mapped_type, Size, 1, false> hash_table_t;
    typedef typename hash_table_t::value_type value_type;

    typedef hls::stream<value_type> value_stream;
    typedef hls::stream<tag_type> tag_stream;
    typedef hls::stream<maybe<std::tuple<uint32_t, mapped_type> > > lookup_result_stream;
    /* Debug access command: index in the hash table and read/write bit */
    typedef maybe<value_type> maybe_value_t;

    typedef gateway_command_template<index_t, maybe_value_t> gateway_command;
    typedef std::tuple<index_t, maybe_value_t> gateway_response;

    tag_stream lookups;
    lookup_result_stream results;

    void hash_table()
    {
    #pragma HLS pipeline enable_flush ii=3
        /* Gateway access are highest priority as they are more rare and lower
         * throughput */
        gateway_command cmd;
        if (gateway_commands.read_nb(cmd)) {
            gateway_response resp;

            switch (cmd.cmd) {
            case HASH_ERASE: {
                auto tag = std::get<0>(cmd.value.value());
                bool result = _hash_table.erase(tag);
                std::get<1>(resp) = make_maybe(result, cmd.value.value());
                break;
            }
            case HASH_INSERT: {
                auto value = cmd.value.value();
                auto result = _hash_table.insert(std::get<0>(value), std::get<1>(value));
                /* Return zero for failure, 1 + index otherwise */
                std::get<0>(resp) = result ? result.value() + 1 : 0;
                break;
            }
            case HASH_WRITE: {
                const bool valid = cmd.value.valid();
                const tag_type tag = std::get<0>(cmd.value.value());
                const mapped_type value = std::get<1>(cmd.value.value());
                _hash_table.set_entry(cmd.index, valid, tag, value);
                break;
            }
            case HASH_READ: {
                bool valid = _hash_table.get_valid(cmd.index);
                tag_type tag = _hash_table.get_tag(cmd.index);
                mapped_type value = _hash_table.get_value(cmd.index);
                std::get<1>(resp) = make_maybe(valid, make_tuple(tag, value));
                break;
            }
            }
            gateway_responses.write(resp);
            return;
        }

        if (!lookups.empty()) {
            tag_type tag;
            lookups.read_nb(tag);
            size_t index;
            auto result = _hash_table.find(tag, index);
            auto returned_results = make_maybe(result.valid(),
                make_tuple(result.valid() ? uint32_t(index + 1) : 0, result.value()));
            results.write(returned_results);
        }

    }

    /* Command from the gateway */
    int gateway_execute_command(const gateway_command& cmd, gateway_response& resp)
    {
    #pragma HLS inline
        if (!gateway_command_sent) {
            if (!gateway_commands.write_nb(cmd))
                return GW_BUSY;

            gateway_command_sent = true;
            return GW_BUSY;
        } else {
            if (!gateway_responses.read_nb(resp))
                return GW_BUSY;

            gateway_command_sent = false;
            return GW_DONE;
        }
    }

    /* Insert a new entry from the gateway. Returns GW_DONE when completed,
     * *result == 1 if successful. */
    int gateway_add_entry(const value_type& value, int *result)
    {
    #pragma HLS inline
        gateway_command cmd = {
            HASH_INSERT,
            0,
            value
        };
        gateway_response resp;

        int ret = gateway_execute_command(cmd, resp);
        if (ret == GW_DONE)
            *result = std::get<0>(resp);

        return ret;
    }

    /* Remove an entry from the gateway. Returns GW_DONE when completed,
     * *result == 1 if successful. */
    int gateway_delete_entry(const typename hash_table_t::tag_type& tag, int *result)
    {
    #pragma HLS inline
        gateway_command cmd = {
            HASH_ERASE,
            0,
            make_tuple(tag, mapped_type())
        };
        gateway_response resp;

        int ret = gateway_execute_command(cmd, resp);
        if (ret == GW_DONE)
            *result = std::get<1>(resp).valid();

        return ret;
    }

    /* For debug: access a given entry directly. */
    int gateway_debug_command(uint32_t address, bool write, maybe_value_t& entry)
    {
    #pragma HLS inline
        gateway_command cmd = {
            write ? HASH_WRITE : HASH_READ,
            address,
        };
        gateway_response resp;

        int ret = gateway_execute_command(cmd, resp);
        if (ret == GW_DONE) {
            if (!write)
                entry = std::get<1>(resp);
        }

        return ret;
    }

private:

    hash_table_t _hash_table;

    /* Gateway access definitions */
    pack_stream<gateway_command> gateway_commands;
    pack_stream<gateway_response> gateway_responses;
    bool gateway_command_sent;
};

#endif // CACHE_HPP
