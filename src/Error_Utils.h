/*
 * End to End LoRa® Network
 * 
 * AUTHORS: Serdar Pehlivan and Arman Yavuz
 * 
 * MIT License
 * 
 * Copyright (c) 2022 Serdar Pehlivan and Arman Yavuz
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

#ifndef EEL_ERROR_UTILS_H
#define EEL_ERROR_UTILS_H

#include "Arduino.h"

#define CRC_POLY (uint8_t)(0x9B)

const uint8_t crc_table[256] PROGMEM = {
								0,    37,    74,    111,    15,    42,    69,    96,    30,    59,    84,    113,    17,    52,    91,    126,
								60,    25,    118,    83,    51,    22,    121,    92,    34,    7,    104,    77,    45,    8,    103,    66,
								120,    93,    50,    23,    119,    82,    61,    24,    102,    67,    44,    9,    105,    76,    35,    6,
								68,    97,    14,    43,    75,    110,    1,    36,    90,    127,    16,    53,    85,    112,    31,    58,
								107,    78,    33,    4,    100,    65,    46,    11,    117,    80,    63,    26,    122,    95,    48,    21,
								87,    114,    29,    56,    88,    125,    18,    55,    73,    108,    3,    38,    70,    99,    12,    41,
								19,    54,    89,    124,    28,    57,    86,    115,    13,    40,    71,    98,    2,    39,    72,    109,
								47,    10,    101,    64,    32,    5,    106,    79,    49,    20,    123,    94,    62,    27,    116,    81,
								77,    104,    7,    34,    66,    103,    8,    45,    83,    118,    25,    60,    92,    121,    22,    51,
								113,    84,    59,    30,    126,    91,    52,    17,    111,    74,    37,    0,    96,    69,    42,    15,
								53,    16,    127,    90,    58,    31,    112,    85,    43,    14,    97,    68,    36,    1,    110,    75,
								9,    44,    67,    102,    6,    35,    76,    105,    23,    50,    93,    120,    24,    61,    82,    119,
								38,    3,    108,    73,    41,    12,    99,    70,    56,    29,    114,    87,    55,    18,    125,    88,
								26,    63,    80,    117,    21,    48,    95,    122,    4,    33,    78,    107,    11,    46,    65,    100,
								94,    123,    20,    49,    81,    116,    27,    62,    64,    101,    10,    47,    79,    106,    5,    32,
								98,    71,    40,    13,    109,    72,    39,    2,    124,    89,    54,    19,    115,    86,    57,    28,
								};

uint8_t CalculateCRC8(void* arr, uint8_t length);

uint8_t CheckCRC8(uint8_t original_crc, void* msg, uint8_t length);
#endif