/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2016 Pavel Ognev
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef RANDOMIZER_H
#define RANDOMIZER_H

#include <stdint.h>
#include <string>


struct LitInfo;


class Randomizer
{
public:
    static uint32_t makeNumber( uint32_t modulo );

    static std::string makePin( int length );
    static std::string makePassword( int length );
    static std::string makeHexBlock( int bytes );
    static std::string makeName( int minSyllables, int maxSyllables );

private:
    Randomizer();

    bool getBits( uint32_t *dst, int count );
    const LitInfo *getLiteral( const LitInfo *stBegin, size_t stSizeBytes );

    static Randomizer *getInstance();

private:
    uint32_t pool[8];
    int poolSize;

};


#endif // RANDOMIZER_H
