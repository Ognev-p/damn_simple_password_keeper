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

#include "Randomizer.h"

#include <openssl/rand.h>

// Let's exclude letters looking similar to digits and add some symbols...
static const char passwordCharSet[64+1] = "ACDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnpqrstuvwxyz0123456789#*?:+=_";

static const char hexCharSet[16+1] = "0123456789abcdef";

/*
 * +----------------------------------------------+
 * |            Name generation scheme            |
 * +----------------------------------------------+
 *
 * The following rules are implemented:
 * - name consist of random syllables and a random word ending
 * - syllable structure is: <consonant> [consonant] <vowel>
 * - second consonant are added with probability 1/4
 * - first consonant and vowel are duplicated in 1/16 cases if allowed
 * - first letter in a word cannot be duplicated (in terms of single syllable)
 * - second consonant cannot be added after duplicated first one
 * - first consonant can be absent in a first syllable in 1/4 cases
 * - syllable count distribution is normal, not uniform
 * - vowels and consonant have a specific distribution, natural for English
 * - word ending has its own distribution and is not counted as a syllable
 *
 */

struct LitInfo
{
    char value[3];
    char canDup;     // Can be duplicated, as "ee" or "rr" but not "uu"
    uint32_t weight; // Sum of literal weights must be 2**24
};


static const LitInfo vowelSet[] = {
    { "e", 1, 5040273 },
    { "a", 0, 3406646 },
    { "o", 1, 3221018 },
    { "i", 0, 3063451 },
    { "u", 0, 1159547 },
    { "y", 0, 886281 }
};

static const LitInfo consonantSet[] = {
    { "n",  1, 1965342 },
    { "r",  1, 1703266 },
    { "t",  0, 1674560 },
    { "s",  1, 1466326 },
    { "d",  1, 1221783 },
    { "l",  1, 1125424 },
    { "" ,  0, 1048588 },
    { "th", 0, 899191 },
    { "c",  1, 766989 },
    { "m",  1, 738749 },
    { "f",  1, 651700 },
    { "w",  0, 592582 },
    { "g",  1, 573031 },
    { "p",  0, 514533 },
    { "b",  0, 421277 },
    { "v",  0, 313281 },
    { "sh", 0, 310333 },
    { "h",  0, 263783 },
    { "ch", 0, 201716 },
    { "k",  0, 195044 },
    { "x",  0, 48877 },
    { "qu", 0, 31809 },
    { "j",  0, 29171 },
    { "z",  0, 19861 }
};

static const LitInfo wordEndSet[] = {
    { "" ,  0, 4194304 },
    { "t",  0, 1331525 },
    { "s",  0, 1249585 },
    { "r",  0, 1167645 },
    { "ck", 0, 1085706 },
    { "y",  0, 1029371 },
    { "k",  0, 1003765 },
    { "x",  0, 921825 },
    { "n",  0, 839885 },
    { "th", 0, 757945 },
    { "v",  0, 676005 },
    { "sh", 0, 594065 },
    { "p",  0, 512125 },
    { "b",  0, 430185 },
    { "l",  0, 348245 },
    { "z",  0, 266305 },
    { "ty", 0, 221238 },
    { "cy", 0, 147492 },
};


Randomizer::Randomizer()
: poolSize( 0 )
{
}


uint32_t Randomizer::makeNumber( uint32_t modulo )
{
    Randomizer *randomizer = getInstance();
    uint64_t t;

    if( !randomizer->getBits( ((uint32_t*)&t), 32 ) ||
        !randomizer->getBits( ((uint32_t*)&t) + 1, 32 ) )
    {
        return modulo;
    }

    return (t % modulo);
}


std::string Randomizer::makePin( int length )
{
    std::string res;
    res.reserve( length + 3 );

    for( int i = 0; i < length; i += 4 )
    {
        uint32_t t = makeNumber( 10000 );
        for( int j = 0; j < 4; ++j )
        {
            res.push_back( '0' + (t % 10) );
            t /= 10;
        }
    }

    res.resize( length );
    return res;
}


std::string Randomizer::makePassword( int length )
{
    Randomizer *randomizer = getInstance();
    std::string res;
    uint32_t t;

    res.reserve( length );
    for( int i = 0; i < length; ++i )
    {
        if( !randomizer->getBits( &t, 6 ) )
            return res;

        res.push_back( passwordCharSet[t] );
    }

    return res;
}


std::string Randomizer::makeHexBlock( int bytes )
{
    Randomizer *randomizer = getInstance();
    std::string res;
    uint32_t t;

    res.reserve( bytes * 2 );
    for( int i = 0; i < bytes; ++i )
    {
        if( !randomizer->getBits( &t, 8 ) )
            return res;

        res.push_back( hexCharSet[t & 15] );
        res.push_back( hexCharSet[t >> 4] );
    }

    return res;
}


std::string Randomizer::makeName( int minSyllables, int maxSyllables )
{
    Randomizer *randomizer = getInstance();
    std::string res;
    const LitInfo *literal;
    uint32_t t;

    // Randomize actual syllable count with norm distribution
    int syllableCount = minSyllables;
    for( int i = 0; i < maxSyllables - minSyllables; ++i )
    {
        if( !randomizer->getBits( &t, 1 ) )
            return res;

        syllableCount += t;
    }

    // Generate syllables
    for( int i = 0; i < syllableCount; ++i )
    {
        literal = randomizer->getLiteral( consonantSet, sizeof(consonantSet) );
        if( NULL == literal || !randomizer->getBits( &t, 4 ) )
            return res;

        if( 0 != i || t >= 4 )
            res += literal->value;

        if( t == 0 && literal->canDup && 0 != i )
        {
            // Consonant duplication
            res += literal->value;
        }
        else if( t >= 12 )
        {
            // Additional consonant
            literal = randomizer->getLiteral( consonantSet, sizeof(consonantSet) );
            if( NULL == literal || !randomizer->getBits( &t, 4 ) )
                return res;
        }

        literal = randomizer->getLiteral( vowelSet, sizeof(vowelSet) );
        if( NULL == literal || !randomizer->getBits( &t, 4 ) )
            return res;

        res += literal->value;
        if( t == 0 && literal->canDup && res.size() > 1 )
        {
            // Vowel duplication
            res += literal->value;
        }
    }

    // Add some word end
    literal = randomizer->getLiteral( wordEndSet, sizeof(wordEndSet) );
    if( NULL != literal )
        res += literal->value;

    return res;
}


bool Randomizer::getBits( uint32_t *dst, int count )
{
    // Black magic is living in this function. Don't touch anything!!!
    uint32_t res = 0;

    if( count > poolSize )
    {
        res = pool[0] & ~( ((uint32_t)-1) << poolSize );
        count -= poolSize;
        poolSize = 0;

        if( !RAND_bytes( (uint8_t*)pool, sizeof(pool) ) )
            return false;

        poolSize = sizeof(pool) * 8;
    }

    while( count > 0 )
    {
        const int limbNo = (poolSize - 1) / 32;
        const int limbRem = poolSize - limbNo * 32;

        if( limbRem <= count )
        {
            res = (res << count) | pool[limbNo];
            count -= limbRem;
            poolSize -= limbRem;
        }
        else
        {
            res = (res << count) | (pool[limbNo] >> (limbRem - count));
            pool[limbNo] &= ~( ((uint32_t)-1) << (limbRem - count) );

            poolSize -= count;
            count = 0;
        }
    }

    *dst = res;
    return true;
}


const LitInfo *Randomizer::getLiteral( const LitInfo *stBegin, size_t stSizeBytes )
{
    uint32_t t;

    if( !getBits( &t, 24 ) )
        return NULL;

    for( const LitInfo *i = stBegin;
         stSizeBytes >= sizeof(*i);
         stSizeBytes -= sizeof(*i), ++i )
    {
        if( i->weight > t )
            return i;

        t -= i->weight;
    }

    return NULL;
}


Randomizer *Randomizer::getInstance()
{
    static Randomizer *singleton = new Randomizer();
    return singleton;
}
