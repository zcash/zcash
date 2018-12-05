#ifndef SPX_HARAKA_H
#define SPX_HARAKA_H

/* load constants */
void load_constants_port();

/* Tweak constants with seed */
void tweak_constants(const unsigned char *pk_seed, const unsigned char *sk_seed, 
	                 unsigned long long seed_length);

/* Haraka Sponge */
void haraka_S(unsigned char *out, unsigned long long outlen,
              const unsigned char *in, unsigned long long inlen);

/* Applies the 512-bit Haraka permutation to in. */
void haraka512_perm(unsigned char *out, const unsigned char *in);

/* Implementation of Haraka-512 */
void haraka512_port(unsigned char *out, const unsigned char *in);

/* Applies the 512-bit Haraka permutation to in, using zero key. */
void haraka512_perm_zero(unsigned char *out, const unsigned char *in);

/* Implementation of Haraka-512, using zero key */
void haraka512_port_zero(unsigned char *out, const unsigned char *in);

/* Implementation of Haraka-256 */
void haraka256_port(unsigned char *out, const unsigned char *in);

/* Implementation of Haraka-256 using sk.seed constants */
void haraka256_sk(unsigned char *out, const unsigned char *in);

#endif
