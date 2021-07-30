#include <string.h>
#include "hex.h"

/***
 * turn a char into its hex value
 * A '5' becomes a 5, 'B' (or 'b') becomes 11.
 * @param c the input
 * @returns the value
 */
int32_t unhex(char c)
{
    if ( c >= '0' && c <= '9' )
        return(c - '0');
    else if ( c >= 'a' && c <= 'f' )
        return(c - 'a' + 10);
    else if ( c >= 'A' && c <= 'F' )
        return(c - 'A' + 10);
    return(-1);
}

/***
 * Check n characters of a string to see if it can be
 * interpreted as a hex string
 * @param str the input
 * @param n the size of input to check, 0 to check until null terminator found
 * @returns 0 on error, othewise a positive number
 */
int32_t is_hexstr(const char *str,int32_t n)
{
    int32_t i;
    if ( str == 0 || str[0] == 0 )
        return(0);
    for (i=0; str[i]!=0; i++)
    {
        if ( n > 0 && i >= n )
            break;
        if ( unhex(str[i]) < 0 )
            break;
    }
    if ( n == 0 )
        return(i);
    return(i == n);
}

/***
 * Decode a 2 character hex string into its value
 * @param hex the string (i.e. 'ff')
 * @returns the value (i.e. 255)
 */
unsigned char _decode_hex(const char *hex) 
{ 
    return (unhex(hex[0])<<4) | unhex(hex[1]); 
}

/***
 * Turn a hex string into bytes
 * NOTE: If there is 1 extra character in a null-terminated str, treat the first char as a full byte
 * 
 * @param bytes where to store the output (will be cleared if hex has invalid chars)
 * @param n number of bytes to process
 * @param hex the input (will ignore CR/LF)
 * @returns the number of bytes processed
 */
int32_t decode_hex(uint8_t *bytes, int32_t n,const char *str)
{
    uint8_t extra = 0;
    // check validity of input
    if ( is_hexstr(str,n) <= 0 )
    {
        memset(bytes,0,n); // give no results
        return 0;
    }
    if (str[n*2+1] == 0 && str[n*2] != 0)
    {
        // special case: odd number of char, then null terminator
        // treat first char as a whole byte
        bytes[0] = unhex(str[0]);
        extra = 1;
        bytes++;
        str++;
    }
    if ( n > 0 )
    {
        for (int i=0; i<n; i++)
            bytes[i] = _decode_hex(&str[i*2]);
    }
    return n + extra;
}

/***
 * Convert a byte into its stringified representation
 * eg: 1 becomes '1', 11 becomes 'b'
 * @param c the byte
 * @returns the character
 */
char hexbyte(int32_t c)
{
    c &= 0xf;
    if ( c < 10 )
        return('0'+c);
    else if ( c < 16 )
        return('a'+c-10);
    else return(0);
}

/****
 * Convert a binary array of bytes into a hex string
 * @param hexbytes the result
 * @param message the array of bytes
 * @param len the length of message
 * @returns the length of hexbytes (including null terminator)
 */
int32_t init_hexbytes_noT(char *hexbytes, unsigned char *message, long len)
{
    if ( len <= 0 ) // parameter validation
    {
        hexbytes[0] = 0;
        return(1);
    }

    for (int i=0; i<len; i++)
    {
        hexbytes[i*2] = hexbyte((message[i]>>4) & 0xf);
        hexbytes[i*2 + 1] = hexbyte(message[i] & 0xf);
    }
    hexbytes[len*2] = 0;
    return((int32_t)len*2+1);
}

/***
 * Convert a bits256 into a hex character string
 * @param hexstr the results
 * @param x the input
 * @returns a pointer to hexstr
 */
char *bits256_str(char hexstr[65],bits256 x)
{
    init_hexbytes_noT(hexstr,x.bytes,sizeof(x));
    return(hexstr);
}
