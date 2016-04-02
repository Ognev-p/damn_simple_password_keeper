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

#include <stdio.h>
#include <algorithm>


enum RandomEntity { RE_UNKNOWN, RE_NAME, RE_PIN, RE_PASSWD, RE_BYTES };

typedef std::string (*RandomFn)(int);


static void help( const char *programName )
{
    printf( "Usage: %s [filename] <number> <nicknames/PINs/passwords/bytes> [length]\n"
            "\tProgram will output <number> of following entities:\n"
            "\t\tnicknames: random-generated words of [length(default = 2-5)] syllables\n"
            "\t\tPINs: PIN-codes of [length(default = 4)] digits\n"
            "\t\tpasswords: random string of [length(default = 12)] chars from 64 possible\n"
            "\t\tbytes: HEX presentation of [length(default = 16)] random bytes\n"
            "\tLength can be specified as a single decimal or a range, e.g. \"5-10\"\n\n",
            programName );
}


static RandomEntity identifyCommand( const char *cmd )
{
    std::string command( cmd );
    std::transform( command.begin(), command.end(), command.begin(), (int(*)(int))tolower );

    if( command.find( "name" ) != std::string::npos ) return RE_NAME;
    if( command.find( "pin" ) != std::string::npos )  return RE_PIN;
    if( command.find( "pass" ) != std::string::npos ) return RE_PASSWD;
    if( command.find( "byte" ) != std::string::npos ) return RE_BYTES;

    return RE_UNKNOWN;
}


int main( int argc, char **argv )
{
    if( 3 != argc && 4 != argc )
    {
        help( argv[0] );
        return 0;
    }

    int count;
    RandomEntity entity = identifyCommand( argv[2] );
    RandomFn randImpl = NULL;
    int minLength, maxLength;

    switch( entity )
    {
        case RE_NAME:
                minLength = 2;
                maxLength = 5;
            break;

        case RE_PIN:
                minLength = maxLength = 4;
                randImpl = &Randomizer::makePin;
            break;

        case RE_PASSWD:
                minLength = maxLength = 12;
                randImpl = &Randomizer::makePassword;
            break;

        case RE_BYTES:
                minLength = maxLength = 16;
                randImpl = &Randomizer::makeHexBlock;
            break;

        default:
                help( argv[0] );
                return 1;
    }

    sscanf( argv[1], "%d", &count );

    if( argc > 3 )
    {
        if( sscanf( argv[3], "%d-%d", &minLength, &maxLength ) == 1 )
            maxLength = minLength;
    }

    for( int i = 0; i < count; ++i )
        if( RE_NAME == entity )
        {
            puts( Randomizer::makeName( minLength, maxLength ).c_str() );
        }
        else if( NULL != randImpl )
        {
            int curLength = minLength;

            if( curLength < maxLength )
                curLength += Randomizer::makeNumber( maxLength - curLength + 1 );

            puts( randImpl( curLength ).c_str() );
        }

    return 0;
}
