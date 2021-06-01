#pragma once
#include <stdint.h>
#include <string.h>
#include <stdio.h>

#ifndef _BITS256
#define _BITS256
union _bits256 { uint8_t bytes[32]; uint16_t ushorts[16]; uint32_t uints[8]; uint64_t ulongs[4]; uint64_t txid; };
typedef union _bits256 bits256;
#endif

unsigned char _decode_hex(char *hex);

int32_t unhex(char c);

int32_t is_hexstr(char *str,int32_t n);

int32_t decode_hex(uint8_t *bytes,int32_t n,char *hex);
