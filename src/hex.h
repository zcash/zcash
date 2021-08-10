/***
 * Routines for char to hex conversions
 */
#pragma once
#include <stdint.h>
#include "bits256.h"

#ifdef __cplusplus
extern "C"
{
#endif

/***
 * turn a char into its hex value
 * A '5' becomes a 5, 'B' (or 'b') becomes 11.
 * @param c the input
 * @returns the value
 */
int32_t unhex(char c);

/***
 * Check n characters of a string to see if it can be
 * interpreted as a hex string
 * @param str the input
 * @param n the size of input to check, 0 to check until null terminator found
 * @returns 0 on error, othewise a positive number
 */
int32_t is_hexstr(const char *str,int32_t n);

/***
 * Decode a 2 character hex string into its value
 * @param hex the string (i.e. 'ff')
 * @returns the value (i.e. 255)
 */
unsigned char _decode_hex(const char *hex) ;

/***
 * Turn a hex string into bytes
 * @param bytes where to store the output
 * @param n number of bytes to process
 * @param hex the input (will ignore CR/LF)
 * @returns the number of bytes processed (not an indicator of success)
 */
int32_t decode_hex(uint8_t *bytes, int32_t n,const char *in);

/****
 * Convert a binary array of bytes into a hex string
 * @param hexbytes the result
 * @param message the array of bytes
 * @param len the length of message
 * @returns the length of hexbytes (including null terminator)
 */
int32_t init_hexbytes_noT(char *hexbytes, unsigned char *message, long len);

/***
 * Convert a bits256 into a hex character string
 * @param hexstr the results
 * @param x the input
 * @returns a pointer to hexstr
 */
char *bits256_str(char hexstr[65], bits256 x);

#ifdef __cplusplus
}
#endif
