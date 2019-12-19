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

#ifndef CMS_HPP
#define CMS_HPP

/**
    Daniel Alabi
    Count-Min Sketch Implementation based on paper by
    Muthukrishnan and Cormode, 2004
    https://github.com/alabid/countminsketch
**/

#include <cmath>
#include <algorithm>

// define some constants
# define LONG_PRIME 32993
# define ERROR 0.01 // 0.01 < ERROR < 1
# define GAMMA 0.1   // probability for accuracy, 0 < gamma < 1
# define WIDTH 272
# define DEPTH 3

// The WIDTH and DEPTH parameters are hardcoded due to HLS limitations
//const unsigned int WIDTH = std::ceil(exp(1)/ERROR);
//const unsigned int DEPTH = std::ceil(log(1/GAMMA));

/** CountMinSketch class definition here **/
struct CountMinSketch {
    // aj, bj \in Z_p
    // both elements of fild Z_p used in generation of hash
    // function
    unsigned int aj, bj;

    // total count so far
    unsigned int total;

    // array of arrays of counters
    int C[DEPTH][WIDTH];

    // array of hash values for a particular item
    // contains two element arrays {aj,bj}
    int hashes[DEPTH][2];

    // constructor
    CountMinSketch();

    void setHashes(int hashes[DEPTH][2]);
    int getCell(const int row, const int col) const;

    // update item (int) by count c
    void update(int item, int c);
    // update item (string) by count c
    void update(const char *item, int c);

    // directly increase a specific counter
    void updateByAddress(int address, int value);

    // estimate count of item i and return count
    unsigned int estimate(int item);
    unsigned int estimate(const char *item);

    // return total count
    unsigned int totalcount();

    // generates a hash value for a string
    // same as djb2 hash function
    unsigned int hashstr(const char *str);

    static void generateHashes(int hashes[DEPTH][2]);

    // destructor
    ~CountMinSketch();
};

#endif //CMS_HPP
