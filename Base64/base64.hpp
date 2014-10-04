/* base64.hpp - base64 encoder/decoder implementing section 6.8 of RFC2045

  Copyright (C) 2002 Ryan Petrie (ryanpetrie@netscape.net)
  and released under the zlib license:

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.

  --------------------------------------------------------------------------

  base64::encoder and base64::decoder are intended to be identical in usage
  as the STL's std::copy and similar algorithms.  They require as parameters
  an input iterator range and an output iterator, and just as std::copy, the
  iterators can be bare pointers, container iterators, or even adaptors such
  as ostream_iterator.

  Examples:

    // encode/decode with in-place memory buffers
    char source[size], dest[size*2];
    base64::encode(source, source+size, dest);
    base64::decode(dest, dest+size*2, source);

    // encode from memory to a file
    char source[size];
    ofstream file("output.txt");
    base64::encode((char*)source, source+size, ostream_iterator<char>(file));

    // decode from file to a standard container
    ifstream file("input.txt");
    vector<char> dest;
    base64::decode(istream_iterator<char>(file), istream_iterator<char>(),
                   back_inserter(dest)
                  );
*/

#ifndef _BASE64_HPP
#define _BASE64_HPP

#include <algorithm>


namespace base64
{
    typedef unsigned uint32;
    typedef unsigned char uint8;

    template <class InputIterator, class OutputIterator>
    void encode(const InputIterator& begin,
                const InputIterator& end,
                OutputIterator out)
    {
        InputIterator it = begin;
        int lineSize = 0;

        int bytes;

        const char _to_table[64] = {
            'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H',
            'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
            'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
            'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f',
            'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',
            'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
            'w', 'x', 'y', 'z', '0', '1', '2', '3',
            '4', '5', '6', '7', '8', '9', '+', '/'
        };
        const char* to_table = _to_table;


        const char* to_table_end =
            _to_table + sizeof(_to_table);

        const char _from_table[128] = {
            -1, -1, -1, -1, -1, -1, -1, -1, // 0
            -1, -1, -1, -1, -1, -1, -1, -1, // 8
            -1, -1, -1, -1, -1, -1, -1, -1, // 16
            -1, -1, -1, -1, -1, -1, -1, -1, // 24
            -1, -1, -1, -1, -1, -1, -1, -1, // 32
            -1, -1, -1, 62, -1, -1, -1, 63, // 40
            52, 53, 54, 55, 56, 57, 58, 59, // 48
            60, 61, -1, -1, -1, 0, -1, -1, // 56
            -1, 0, 1, 2, 3, 4, 5, 6, // 64
            7, 8, 9, 10, 11, 12, 13, 14, // 72
            15, 16, 17, 18, 19, 20, 21, 22, // 80
            23, 24, 25, -1, -1, -1, -1, -1, // 88
            -1, 26, 27, 28, 29, 30, 31, 32, // 96
            33, 34, 35, 36, 37, 38, 39, 40, // 104
            41, 42, 43, 44, 45, 46, 47, 48, // 112
            49, 50, 51, -1, -1, -1, -1, -1  // 120
        };
        const char* from_table = _from_table;

        do {
            uint32 input = 0;

            // get the next three bytes into "in" (and count how many we actually get)
            bytes = 0;
            for(; (bytes < 3) && (it != end); ++bytes, ++it) {
                input <<= 8;
                input += static_cast<uint8>(*it);
            }

            // convert to base64
            int bits = bytes*8;
            while (bits > 0) {
                bits -= 6;
                const uint8 index = ((bits < 0) ? input << -bits : input >> bits) & 0x3F;
                *out = to_table[index];
                ++out;
                ++lineSize;
            }

            //if (lineSize >= 76) // ensure proper line length
            //{
            //  *out = 13;
            //  ++out;
            //  *out = 10;
            //  ++out;
            //  lineSize = 0;
            //}

        } while (bytes == 3);


        // add pad characters if necessary
        if (bytes > 0)
            for(int i=bytes; i < 3; ++i) {
                *out = '=';
                ++out;
            }
    }


    template <class InputIterator, class OutputIterator>
    void decode(const InputIterator& begin,
                const InputIterator& end,
                OutputIterator out)
    {
        InputIterator it = begin;
        int chars;

        const char _to_table[64] = {
            'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H',
            'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
            'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
            'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f',
            'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',
            'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
            'w', 'x', 'y', 'z', '0', '1', '2', '3',
            '4', '5', '6', '7', '8', '9', '+', '/'
        };
        const char* to_table = _to_table;


        const char* to_table_end =
            _to_table + sizeof(_to_table);

        const char _from_table[128] = {
            -1, -1, -1, -1, -1, -1, -1, -1, // 0
            -1, -1, -1, -1, -1, -1, -1, -1, // 8
            -1, -1, -1, -1, -1, -1, -1, -1, // 16
            -1, -1, -1, -1, -1, -1, -1, -1, // 24
            -1, -1, -1, -1, -1, -1, -1, -1, // 32
            -1, -1, -1, 62, -1, -1, -1, 63, // 40
            52, 53, 54, 55, 56, 57, 58, 59, // 48
            60, 61, -1, -1, -1, 0, -1, -1, // 56
            -1, 0, 1, 2, 3, 4, 5, 6, // 64
            7, 8, 9, 10, 11, 12, 13, 14, // 72
            15, 16, 17, 18, 19, 20, 21, 22, // 80
            23, 24, 25, -1, -1, -1, -1, -1, // 88
            -1, 26, 27, 28, 29, 30, 31, 32, // 96
            33, 34, 35, 36, 37, 38, 39, 40, // 104
            41, 42, 43, 44, 45, 46, 47, 48, // 112
            49, 50, 51, -1, -1, -1, -1, -1  // 120
        };
        const char* from_table = _from_table;

        do {
            uint8 input[4] = {0, 0, 0, 0};

            // get four characters
            chars=0;
            while((chars<4) && (it != end)) {
                uint8 c = static_cast<char>(*it);
                if (c == '=') break; // pad character marks the end of the stream
                ++it;

                if (std::find(to_table, to_table_end, c) != to_table_end) {
                    input[chars] = from_table[c];
                    chars++;
                }
            }

            // output the binary data
            if (chars >= 2) {
                *out = static_cast<uint8>((input[0] << 2) + (input[1] >> 4));
                ++out;
                if (chars >= 3) {
                    *out = static_cast<uint8>((input[1] << 4) + (input[2] >> 2));
                    ++out;
                    if (chars >= 4) {
                        *out = static_cast<uint8>((input[2] << 6) + input[3]);
                        ++out;
                    }
                }
            }
        } while (chars == 4);

    }

} // end namespace

#endif