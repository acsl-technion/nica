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

#define CACHE_ENABLE_DEBUG_COMMANDS

#include <ntl/cache.hpp>
#include "gtest/gtest.h"

namespace ntl {

#define CACHE_SIZE 10

    class CacheTest : public testing::Test {
    protected:
        // SetUp() is run immediately before a test starts.
        virtual void SetUp() {
        }

        // TearDown() is invoked immediately after a test finishes.
        virtual void TearDown() {
            _cache = cache<int, int, CACHE_SIZE>();
        }

        cache<int, int, CACHE_SIZE> _cache;
    };

    TEST_F(CacheTest, insert) {
        for (int i = 0; i < CACHE_SIZE; ++i) {
            ASSERT_TRUE(_cache.insert(i, i));
        }

        ASSERT_FALSE(_cache.insert(CACHE_SIZE + 1, CACHE_SIZE + 1));
    }

    TEST_F(CacheTest, find) {
        ASSERT_TRUE(_cache.insert(0, 0));

        ASSERT_TRUE(_cache.find(0).valid());
        ASSERT_EQ(0, _cache.find(0).value());

        ASSERT_FALSE(_cache.find(1).valid());
    }

    // h(key) returns key % CACHE_SIZE
    // This test makes sure that the linear probing implementation
    // indeed moves a key in order to fill the hole and support correct
    // lookup after a key is removed from the cache
    TEST_F(CacheTest, erase) {
        ASSERT_TRUE(_cache.insert(0, 0));
        ASSERT_TRUE(_cache.insert(CACHE_SIZE, 0));
        ASSERT_TRUE(_cache.insert(2*CACHE_SIZE, 0));

        ASSERT_TRUE(_cache.erase(CACHE_SIZE));

        ASSERT_TRUE(_cache.find(0).valid());
        ASSERT_TRUE(_cache.find(2*CACHE_SIZE).valid());
    }
}

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
