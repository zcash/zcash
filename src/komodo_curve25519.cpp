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
#include "komodo_curve25519.h"
#include "komodo_utils.h" // for vcalc_sha256

void store_limb(uint8_t *out,uint64_t in)
{
    int32_t i;
    for (i=0; i<8; i++,in>>=8)
        out[i] = (in & 0xff);
}

// Take a little-endian, 32-byte number and expand it into polynomial form
bits320 fexpand(bits256 basepoint)
{
    bits320 out;
    out.ulongs[0] = load_limb(basepoint.bytes) & 0x7ffffffffffffLL;
    out.ulongs[1] = (load_limb(basepoint.bytes+6) >> 3) & 0x7ffffffffffffLL;
    out.ulongs[2] = (load_limb(basepoint.bytes+12) >> 6) & 0x7ffffffffffffLL;
    out.ulongs[3] = (load_limb(basepoint.bytes+19) >> 1) & 0x7ffffffffffffLL;
    out.ulongs[4] = (load_limb(basepoint.bytes+24) >> 12) & 0x7ffffffffffffLL;
    return(out);
}

#if __amd64__
// Multiply two numbers: output = in2 * in
// output must be distinct to both inputs. The inputs are reduced coefficient form, the output is not.
// Assumes that in[i] < 2**55 and likewise for in2. On return, output[i] < 2**52
bits320 fmul(const bits320 in2,const bits320 in)
{
    uint128_t t[5]; uint64_t r0,r1,r2,r3,r4,s0,s1,s2,s3,s4,c; bits320 out;
    r0 = in.ulongs[0], r1 = in.ulongs[1], r2 = in.ulongs[2], r3 = in.ulongs[3], r4 = in.ulongs[4];
    s0 = in2.ulongs[0], s1 = in2.ulongs[1], s2 = in2.ulongs[2], s3 = in2.ulongs[3], s4 = in2.ulongs[4];
    t[0]  =  ((uint128_t) r0) * s0;
    t[1]  =  ((uint128_t) r0) * s1 + ((uint128_t) r1) * s0;
    t[2]  =  ((uint128_t) r0) * s2 + ((uint128_t) r2) * s0 + ((uint128_t) r1) * s1;
    t[3]  =  ((uint128_t) r0) * s3 + ((uint128_t) r3) * s0 + ((uint128_t) r1) * s2 + ((uint128_t) r2) * s1;
    t[4]  =  ((uint128_t) r0) * s4 + ((uint128_t) r4) * s0 + ((uint128_t) r3) * s1 + ((uint128_t) r1) * s3 + ((uint128_t) r2) * s2;
    r4 *= 19, r1 *= 19, r2 *= 19, r3 *= 19;
    t[0] += ((uint128_t) r4) * s1 + ((uint128_t) r1) * s4 + ((uint128_t) r2) * s3 + ((uint128_t) r3) * s2;
    t[1] += ((uint128_t) r4) * s2 + ((uint128_t) r2) * s4 + ((uint128_t) r3) * s3;
    t[2] += ((uint128_t) r4) * s3 + ((uint128_t) r3) * s4;
    t[3] += ((uint128_t) r4) * s4;
    r0 = (uint64_t)t[0] & 0x7ffffffffffffLL; c = (uint64_t)(t[0] >> 51);
    t[1] += c;      r1 = (uint64_t)t[1] & 0x7ffffffffffffLL; c = (uint64_t)(t[1] >> 51);
    t[2] += c;      r2 = (uint64_t)t[2] & 0x7ffffffffffffLL; c = (uint64_t)(t[2] >> 51);
    t[3] += c;      r3 = (uint64_t)t[3] & 0x7ffffffffffffLL; c = (uint64_t)(t[3] >> 51);
    t[4] += c;      r4 = (uint64_t)t[4] & 0x7ffffffffffffLL; c = (uint64_t)(t[4] >> 51);
    r0 +=   c * 19; c = r0 >> 51; r0 = r0 & 0x7ffffffffffffLL;
    r1 +=   c;      c = r1 >> 51; r1 = r1 & 0x7ffffffffffffLL;
    r2 +=   c;
    out.ulongs[0] = r0, out.ulongs[1] = r1, out.ulongs[2] = r2, out.ulongs[3] = r3, out.ulongs[4] = r4;
    return(out);
}

bits320 fsquare_times(const bits320 in,uint64_t count)
{
    uint128_t t[5]; uint64_t r0,r1,r2,r3,r4,c,d0,d1,d2,d4,d419; bits320 out;
    r0 = in.ulongs[0], r1 = in.ulongs[1], r2 = in.ulongs[2], r3 = in.ulongs[3], r4 = in.ulongs[4];
    do
    {
        d0 = r0 * 2;
        d1 = r1 * 2;
        d2 = r2 * 2 * 19;
        d419 = r4 * 19;
        d4 = d419 * 2;
        t[0] = ((uint128_t) r0) * r0 + ((uint128_t) d4) * r1 + (((uint128_t) d2) * (r3     ));
        t[1] = ((uint128_t) d0) * r1 + ((uint128_t) d4) * r2 + (((uint128_t) r3) * (r3 * 19));
        t[2] = ((uint128_t) d0) * r2 + ((uint128_t) r1) * r1 + (((uint128_t) d4) * (r3     ));
        t[3] = ((uint128_t) d0) * r3 + ((uint128_t) d1) * r2 + (((uint128_t) r4) * (d419   ));
        t[4] = ((uint128_t) d0) * r4 + ((uint128_t) d1) * r3 + (((uint128_t) r2) * (r2     ));

        r0 = (uint64_t)t[0] & 0x7ffffffffffffLL; c = (uint64_t)(t[0] >> 51);
        t[1] += c;      r1 = (uint64_t)t[1] & 0x7ffffffffffffLL; c = (uint64_t)(t[1] >> 51);
        t[2] += c;      r2 = (uint64_t)t[2] & 0x7ffffffffffffLL; c = (uint64_t)(t[2] >> 51);
        t[3] += c;      r3 = (uint64_t)t[3] & 0x7ffffffffffffL; c = (uint64_t)(t[3] >> 51);
        t[4] += c;      r4 = (uint64_t)t[4] & 0x7ffffffffffffLL; c = (uint64_t)(t[4] >> 51);
        r0 +=   c * 19; c = r0 >> 51; r0 = r0 & 0x7ffffffffffffLL;
        r1 +=   c;      c = r1 >> 51; r1 = r1 & 0x7ffffffffffffLL;
        r2 +=   c;
    } while( --count );
    out.ulongs[0] = r0, out.ulongs[1] = r1, out.ulongs[2] = r2, out.ulongs[3] = r3, out.ulongs[4] = r4;
    return(out);
}

// donna: Take a fully reduced polynomial form number and contract it into a little-endian, 32-byte array
bits256 fcontract(const bits320 input)
{
    uint128_t t[5]; int32_t i; bits256 out;
    for (i=0; i<5; i++)
        t[i] = input.ulongs[i];
    fcontract_iter(t,1), fcontract_iter(t,1);
    // donna: now t is between 0 and 2^255-1, properly carried.
    // donna: case 1: between 0 and 2^255-20. case 2: between 2^255-19 and 2^255-1.
    t[0] += 19, fcontract_iter(t,1);
    // now between 19 and 2^255-1 in both cases, and offset by 19.
    t[0] += 0x8000000000000 - 19;
    for (i=1; i<5; i++)
        t[i] += 0x8000000000000 - 1;
    // now between 2^255 and 2^256-20, and offset by 2^255.
    fcontract_iter(t,0);
    store_limb(out.bytes,t[0] | (t[1] << 51));
    store_limb(out.bytes+8,(t[1] >> 13) | (t[2] << 38));
    store_limb(out.bytes+16,(t[2] >> 26) | (t[3] << 25));
    store_limb(out.bytes+24,(t[3] >> 39) | (t[4] << 12));
    return(out);
}

bits256 curve25519(bits256 mysecret,bits256 basepoint)
{
    bits320 bp,x,z;
    mysecret.bytes[0] &= 0xf8, mysecret.bytes[31] &= 0x7f, mysecret.bytes[31] |= 0x40;
    bp = fexpand(basepoint);
    cmult(&x,&z,mysecret,bp);
    return(fcontract(fmul(x,crecip(z))));
}
#endif

// Calculates nQ where Q is the x-coordinate of a point on the curve
// resultx/resultz: the x coordinate of the resulting curve point (short form)
// n: a little endian, 32-byte number
// q: a point of the curve (short form)
void cmult(bits320 *resultx,bits320 *resultz,bits256 secret,const bits320 q)
{
    int32_t i,j; bits320 a,b,c,d,e,f,g,h,*t;
    bits320 Zero320bits,One320bits, *nqpqx = &a,*nqpqz = &b,*nqx = &c,*nqz = &d,*nqpqx2 = &e,*nqpqz2 = &f,*nqx2 = &g,*nqz2 = &h;
    memset(&Zero320bits,0,sizeof(Zero320bits));
    memset(&One320bits,0,sizeof(One320bits)), One320bits.ulongs[0] = 1;
    a = d = e = g = Zero320bits, b = c = f = h = One320bits;
    *nqpqx = q;
    for (i=0; i<32; i++)
    {
        uint8_t byte = secret.bytes[31 - i];
        for (j=0; j<8; j++)
        {
            const uint64_t bit = byte >> 7;
            swap_conditional(nqx,nqpqx,bit), swap_conditional(nqz,nqpqz,bit);
            fmonty(nqx2,nqz2,nqpqx2,nqpqz2,nqx,nqz,nqpqx,nqpqz,q);
            swap_conditional(nqx2,nqpqx2,bit), swap_conditional(nqz2,nqpqz2,bit);
            t = nqx, nqx = nqx2, nqx2 = t;
            t = nqz, nqz = nqz2, nqz2 = t;
            t = nqpqx, nqpqx = nqpqx2, nqpqx2 = t;
            t = nqpqz, nqpqz = nqpqz2, nqpqz2 = t;
            byte <<= 1;
        }
    }
    *resultx = *nqx, *resultz = *nqz;
}

#ifndef _WIN32
void OS_randombytes(unsigned char *x,long xlen);
#endif

bits256 rand256(int32_t privkeyflag)
{
    bits256 randval;
    #ifndef __WIN32
    OS_randombytes(randval.bytes,sizeof(randval));
    #else
    randombytes_buf(randval.bytes,sizeof(randval));
    #endif
    if ( privkeyflag != 0 )
        randval.bytes[0] &= 0xf8, randval.bytes[31] &= 0x7f, randval.bytes[31] |= 0x40;
    return(randval);
}

bits256 curve25519_basepoint9()
{
    bits256 basepoint;
    memset(&basepoint,0,sizeof(basepoint));
    basepoint.bytes[0] = 9;
    return(basepoint);
}

bits256 curve25519_keypair(bits256 *pubkeyp)
{
    bits256 privkey;
    privkey = rand256(1);
    *pubkeyp = curve25519(privkey,curve25519_basepoint9());
    //printf("[%llx %llx] ",privkey.txid,(*pubkeyp).txid);
    return(privkey);
}

bits256 curve25519_shared(bits256 privkey,bits256 otherpub)
{
    bits256 shared,hash;
    shared = curve25519(privkey,otherpub);
    vcalc_sha256(0,hash.bytes,shared.bytes,sizeof(shared));
    //printf("priv.%llx pub.%llx shared.%llx -> hash.%llx\n",privkey.txid,pubkey.txid,shared.txid,hash.txid);
    //hash.bytes[0] &= 0xf8, hash.bytes[31] &= 0x7f, hash.bytes[31] |= 64;
    return(hash);
}

int32_t curve25519_donna(uint8_t *mypublic,const uint8_t *secret,const uint8_t *basepoint)
{
    bits256 val,p,bp;
    memcpy(p.bytes,secret,sizeof(p));
    memcpy(bp.bytes,basepoint,sizeof(bp));
    val = curve25519(p,bp);
    memcpy(mypublic,val.bytes,sizeof(val));
    return(0);
}

uint64_t conv_NXTpassword(unsigned char *mysecret,unsigned char *mypublic,uint8_t *pass,int32_t passlen)
{
    static uint8_t basepoint[32] = {9};
    uint64_t addr; uint8_t hash[32];
    if ( pass != 0 && passlen != 0 )
        vcalc_sha256(0,mysecret,pass,passlen);
    mysecret[0] &= 248, mysecret[31] &= 127, mysecret[31] |= 64;
    curve25519_donna(mypublic,mysecret,basepoint);
    vcalc_sha256(0,hash,mypublic,32);
    memcpy(&addr,hash,sizeof(addr));
    return(addr);
}

uint256 komodo_kvprivkey(uint256 *pubkeyp,char *passphrase)
{
    uint256 privkey;
    conv_NXTpassword((uint8_t *)&privkey,(uint8_t *)pubkeyp,(uint8_t *)passphrase,(int32_t)strlen(passphrase));
    return(privkey);
}

uint256 komodo_kvsig(uint8_t *buf,int32_t len,uint256 _privkey)
{
    bits256 sig,hash,otherpub,checksig,pubkey,privkey; uint256 usig;
    memcpy(&privkey,&_privkey,sizeof(privkey));
    vcalc_sha256(0,hash.bytes,buf,len);
    otherpub = curve25519(hash,curve25519_basepoint9());
    pubkey = curve25519(privkey,curve25519_basepoint9());
    sig = curve25519_shared(privkey,otherpub);
    checksig = curve25519_shared(hash,pubkey);
    /*int32_t i; for (i=0; i<len; i++)
        printf("%02x",buf[i]);
    printf(" -> ");
    for (i=0; i<32; i++)
        printf("%02x",((uint8_t *)&privkey)[i]);
    printf(" -> ");
    for (i=0; i<32; i++)
        printf("%02x",((uint8_t *)&pubkey)[i]);
    printf(" pubkey\n");*/
    memcpy(&usig,&sig,sizeof(usig));
    return(usig);
}

int32_t komodo_kvsigverify(uint8_t *buf,int32_t len,uint256 _pubkey,uint256 sig)
{
    bits256 hash,checksig,pubkey; static uint256 zeroes;
    memcpy(&pubkey,&_pubkey,sizeof(pubkey));
    if ( memcmp(&pubkey,&zeroes,sizeof(pubkey)) != 0 )
    {
        vcalc_sha256(0,hash.bytes,buf,len);
        checksig = curve25519_shared(hash,pubkey);
        /*int32_t i; for (i=0; i<len; i++)
            printf("%02x",buf[i]);
        printf(" -> ");
        for (i=0; i<32; i++)
            printf("%02x",((uint8_t *)&hash)[i]);
        printf(" -> ");
        for (i=0; i<32; i++)
            printf("%02x",((uint8_t *)&pubkey)[i]);
        printf(" verify pubkey\n");
        for (i=0; i<32; i++)
            printf("%02x",((uint8_t *)&sig)[i]);
        printf(" sig vs");
        for (i=0; i<32; i++)
            printf("%02x",((uint8_t *)&checksig)[i]);
        printf(" checksig\n");*/
        if ( memcmp(&checksig,&sig,sizeof(sig)) != 0 )
            return(-1);
        //else printf("VALIDATED\n");
    }
    return(0);
}
