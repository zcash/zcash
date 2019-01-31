/******************************************************************************
 * Copyright © 2014-2019 The SuperNET Developers.                             *
 *                                                                            *
 * See the AUTHORS, DEVELOPER-AGREEMENT and LICENSE files at                  *
 * the top-level directory of this distribution for the individual copyright  *
 * holder information and the developer policies on copyright and licensing.  *
 *                                                                            *
 * Unless otherwise agreed in a custom licensing agreement, no part of the    *
 * SuperNET software, including this file may be copied, modified, propagated *
 * or distributed except according to the terms contained in the LICENSE file *
 *                                                                            *
 * Removal or modification of this copyright notice is prohibited.            *
 *                                                                            *
 ******************************************************************************/
// derived from curve25519_donna

#ifndef dcnet_curve25519_h
#define dcnet_curve25519_h
#include <stdint.h>
#include <memory.h>
#include <string.h>

union _bits128 { uint8_t bytes[16]; uint16_t ushorts[8]; uint32_t uints[4]; uint64_t ulongs[2]; uint64_t txid; };
typedef union _bits128 bits128;
union _bits256 { uint8_t bytes[32]; uint16_t ushorts[16]; uint32_t uints[8]; uint64_t ulongs[4]; uint64_t txid; };
typedef union _bits256 bits256;

union _bits320 { uint8_t bytes[40]; uint16_t ushorts[20]; uint32_t uints[10]; uint64_t ulongs[5]; uint64_t txid; };
typedef union _bits320 bits320;

union _bits384 { bits256 sig; uint8_t bytes[48]; uint16_t ushorts[24]; uint32_t uints[12]; uint64_t ulongs[6]; uint64_t txid; };
typedef union _bits384 bits384;

struct sha256_vstate { uint64_t length; uint32_t state[8],curlen; uint8_t buf[64]; };
struct rmd160_vstate { uint64_t length; uint8_t buf[64]; uint32_t curlen, state[5]; };

struct acct777_sig { bits256 sigbits,pubkey; uint64_t signer64bits; uint32_t timestamp,allocsize; };

//#undef force_inline
//#define force_inline __attribute__((always_inline))


bits320 fmul(const bits320 in2,const bits320 in);
bits320 fexpand(bits256 basepoint);
bits256 fcontract(const bits320 input);
void cmult(bits320 *resultx,bits320 *resultz,bits256 secret,const bits320 q);
bits320 crecip(const bits320 z);
bits256 curve25519(bits256 mysecret,bits256 basepoint);
void OS_randombytes(unsigned char *x,long xlen);
bits256 rand256(int32_t privkeyflag);
bits256 curve25519_basepoint9();
bits256 curve25519_keypair(bits256 *pubkeyp);

void vcalc_sha256(char hashstr[(256 >> 3) * 2 + 1],uint8_t hash[256 >> 3],uint8_t *src,int32_t len);
void vcalc_sha256cat(uint8_t hash[256 >> 3],uint8_t *src,int32_t len,uint8_t *src2,int32_t len2);
void vupdate_sha256(uint8_t hash[256 >> 3],struct sha256_vstate *state,uint8_t *src,int32_t len);
bits256 curve25519_shared(bits256 privkey,bits256 otherpub);
int32_t iguana_rwnum(int32_t rwflag,uint8_t *serialized,int32_t len,void *endianedp);
int32_t iguana_rwbignum(int32_t rwflag,uint8_t *serialized,int32_t len,uint8_t *endianedp);

uint32_t calc_crc32(uint32_t crc,const void *buf,size_t size);
uint64_t conv_NXTpassword(unsigned char *mysecret,unsigned char *mypublic,uint8_t *pass,int32_t passlen);
bits128 calc_md5(char digeststr[33],void *buf,int32_t len);

bits256 acct777_msgprivkey(uint8_t *data,int32_t datalen);
bits256 acct777_msgpubkey(uint8_t *data,int32_t datalen);
void acct777_rwsig(int32_t rwflag,uint8_t *serialized,struct acct777_sig *sig);
int32_t acct777_sigcheck(struct acct777_sig *sig);

bits256 acct777_pubkey(bits256 privkey);
uint64_t acct777_nxt64bits(bits256 pubkey);
bits256 acct777_hashiter(bits256 privkey,bits256 pubkey,int32_t lockdays,uint8_t chainlen);
bits256 acct777_lockhash(bits256 pubkey,int32_t lockdays,uint8_t chainlen);
bits256 acct777_invoicehash(bits256 *invoicehash,uint16_t lockdays,uint8_t chainlen);
uint64_t acct777_sign(struct acct777_sig *sig,bits256 privkey,bits256 otherpubkey,uint32_t timestamp,uint8_t *serialized,int32_t datalen);
uint64_t acct777_validate(struct acct777_sig *sig,bits256 privkey,bits256 pubkey);
uint64_t acct777_signtx(struct acct777_sig *sig,bits256 privkey,uint32_t timestamp,uint8_t *data,int32_t datalen);
uint64_t acct777_swaptx(bits256 privkey,struct acct777_sig *sig,uint32_t timestamp,uint8_t *data,int32_t datalen);
void calc_hmac_sha256(uint8_t *mac,int32_t maclen,uint8_t *key,int32_t key_size,uint8_t *message,int32_t len);

#include "../includes/tweetnacl.h"
int32_t _SuperNET_cipher(uint8_t nonce[crypto_box_NONCEBYTES],uint8_t *cipher,uint8_t *message,int32_t len,bits256 destpub,bits256 srcpriv,uint8_t *buf);
uint8_t *_SuperNET_decipher(uint8_t nonce[crypto_box_NONCEBYTES],uint8_t *cipher,uint8_t *message,int32_t len,bits256 srcpub,bits256 mypriv);
void *SuperNET_deciphercalc(void **ptrp,int32_t *msglenp,bits256 privkey,bits256 srcpubkey,uint8_t *cipher,int32_t cipherlen,uint8_t *buf,int32_t bufsize);
uint8_t *SuperNET_ciphercalc(void **ptrp,int32_t *cipherlenp,bits256 *privkeyp,bits256 *destpubkeyp,uint8_t *data,int32_t datalen,uint8_t *space2,int32_t space2size);

#endif
