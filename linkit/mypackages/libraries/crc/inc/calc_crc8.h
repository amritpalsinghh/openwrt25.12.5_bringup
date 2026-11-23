// calc_crc8.h -- (c)2001-2020 II-VI Incorporated

#ifndef CALC_CRC8_H
#define CALC_CRC8_H 1

#include <stddef.h>

extern const unsigned char crc8_msbfirst_table31[256]; // 1-wire CRC-8 bit-reversed

unsigned char calc_crc8_msbfirst ( unsigned char, const void *, size_t );
#ifdef __cplusplus
unsigned char calc_crc8_msbfirst ( unsigned char, const void *, size_t, unsigned char = 0x31 );
unsigned char calc_crc8_msbfirst ( unsigned char, const void *, size_t, const unsigned char [256] = crc8_msbfirst_table31 );
#endif
unsigned char calc_crc8_msbfirst_poly ( unsigned char, const void *, size_t, unsigned char /* = 0x31 */ );
unsigned char calc_crc8_msbfirst_table ( unsigned char, const void *, size_t, const unsigned char [256] /* = crc8_msbfirst_table31 */ );

extern const unsigned char crc8_lsbfirst_table8C[256]; // 1-wire CRC-8

unsigned char calc_crc8_lsbfirst ( unsigned char, const void *, size_t );
#ifdef __cplusplus
unsigned char calc_crc8_lsbfirst ( unsigned char, const void *, size_t, unsigned char = 0x8C );
unsigned char calc_crc8_lsbfirst ( unsigned char, const void *, size_t, const unsigned char [256] = crc8_lsbfirst_table8C );
#endif
unsigned char calc_crc8_lsbfirst_poly ( unsigned char, const void *, size_t, unsigned char /* = 0x8C */ );
unsigned char calc_crc8_lsbfirst_table ( unsigned char, const void *, size_t, const unsigned char [256] /* = crc8_lsbfirst_table8C */ );

#endif
