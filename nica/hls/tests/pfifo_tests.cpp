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

#include "gtest/gtest.h"
#include "programmable_fifo.hpp"

#define THRESHOLD 10

class pfifo_tests : public ::testing::Test
{
public:
    pfifo_tests() :
        pfifo(THRESHOLD)
    {}

protected:
    programmable_fifo<int> pfifo;
};

TEST_F(pfifo_tests, read_write)
{
    ASSERT_TRUE(pfifo.empty());

    for (int i = 0; i < THRESHOLD; ++i) {
        pfifo.full_progress();
        ASSERT_FALSE(pfifo.full());
        pfifo.write(i);
    }
    ASSERT_TRUE(pfifo.full());
    for (int i = 0; i < THRESHOLD; ++i) {
        ASSERT_FALSE(pfifo.empty());
        ASSERT_EQ(pfifo.read(), i);
        pfifo.empty_progress();
        pfifo.full_progress();
        ASSERT_FALSE(pfifo.full());
    }

    ASSERT_TRUE(pfifo.empty());
}

TEST_F(pfifo_tests, back_pressure)
{
    ASSERT_TRUE(pfifo.empty());

    for (int i = 0; i < 15; ++i) {
        ASSERT_EQ(pfifo.full(), i >= THRESHOLD) << i;
        pfifo.write(i);
        pfifo.empty_progress();
        pfifo.full_progress();
    }
    for (int i = 0; i < 15; ++i) {
        ASSERT_FALSE(pfifo.empty());
        ASSERT_EQ(pfifo.read(), i);
        pfifo.empty_progress();
        pfifo.full_progress();
    }

    ASSERT_FALSE(pfifo.full());
    ASSERT_TRUE(pfifo.empty());
}

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
