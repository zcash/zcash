/*********************************************************************
* Filename:   sha256.h
* Author:     Brad Conte (brad AT bradconte.com)
* Copyright:
* Disclaimer: This code is presented "as is" without any guarantees.
* Details:    Defines the API for the corresponding SHA1 implementation.
*********************************************************************/

#ifndef SHA256H_H
#define SHA256H_H

/*************************** HEADER FILES ***************************/
#include <stddef.h>
#include <stdint.h>

/****************************** MACROS ******************************/
#define SHA256_BLOCK_SIZE 32            // SHA256 outputs a 32 byte digest

typedef struct {
	uint8_t data[64];
	uint32_t datalen;
	unsigned long long bitlen;
	uint32_t state[8];
} SHA256_CTX_mod;

/*********************** FUNCTION DECLARATIONS **********************/
void sha256_init(SHA256_CTX_mod *ctx);
void sha256_update(SHA256_CTX_mod *ctx, const uint8_t data[], size_t len);
void sha256_length_padding(SHA256_CTX_mod *ctx);
void sha256_final_no_padding(SHA256_CTX_mod *ctx, uint8_t hash[]);

#endif   // SHA256H_H
