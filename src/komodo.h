/******************************************************************************
 * Copyright © 2014-2016 The SuperNET Developers.                             *
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

#ifndef H_KOMODO_H
#define H_KOMODO_H

#include <stdint.h>
#include <stdio.h>
#include <pthread.h>
#include "uthash.h"

#include "komodo_interest.h"

#define KOMODO_TESTNET_EXPIRATION 60000
#define KOMODO_ELECTION_GAP 1000
#define KOMODO_PUBKEYS_HEIGHT(height) ((int32_t)(((((height)+KOMODO_ELECTION_GAP*.5)/KOMODO_ELECTION_GAP) + 1) * KOMODO_ELECTION_GAP))

int32_t IS_KOMODO_NOTARY,USE_EXTERNAL_PUBKEY,NOTARIZED_HEIGHT,Num_nutxos,KOMODO_NUMNOTARIES = 64;
std::string NOTARY_PUBKEY;
uint8_t NOTARY_PUBKEY33[33];
uint256 NOTARIZED_HASH,NOTARIZED_BTCTXID;
pthread_mutex_t komodo_mutex;

struct nutxo_entry { UT_hash_handle hh; uint256 txhash; uint64_t voutmask; int32_t notaryid,height; } *NUTXOS;
struct knotary_entry { UT_hash_handle hh; uint8_t pubkey[33],notaryid; };
struct knotaries_entry { int32_t height,numnotaries; struct knotary_entry *Notaries; } Pubkeys[10000];
struct notarized_checkpoint { uint256 notarized_hash,notarized_btctxid; int32_t nHeight,notarized_height; } *NPOINTS; int32_t NUM_NPOINTS;

int32_t komodo_stateupdate(int32_t height,uint8_t notarypubs[][33],uint8_t numnotaries,uint8_t notaryid,uint256 txhash,uint64_t voutmask,uint8_t numvouts);
// add opreturn funcid
// pricefeeds

#define CRYPTO777_PUBSECPSTR "020e46e79a2a8d12b9b5d12c7a91adb4e454edfae43c0a0cb805427d2ac7613fd9"

const char *Notaries[][2] =
{
    { "jl777_testA", "03b7621b44118017a16043f19b30cc8a4cfe068ac4e42417bae16ba460c80f3828" },
    { "jl777_testB", "02ebfc784a4ba768aad88d44d1045d240d47b26e248cafaf1c5169a42d7a61d344" },
    { "pondsea_SH", "02209073bc0943451498de57f802650311b1f12aa6deffcd893da198a544c04f36" },
    { "crackers_EU", "0340c66cf2c41c41efb420af57867baa765e8468c12aa996bfd816e1e07e410728" },
    { "pondsea_EU", "0225aa6f6f19e543180b31153d9e6d55d41bc7ec2ba191fd29f19a2f973544e29d" },
    { "locomb_EU", "025c6d26649b9d397e63323d96db42a9d3caad82e1d6076970efe5056c00c0779b" },
    { "fullmoon_AE", "0204a908350b8142698fdb6fabefc97fe0e04f537adc7522ba7a1e8f3bec003d4a" },
    { "movecrypto_EU", "021ab53bc6cf2c46b8a5456759f9d608966eff87384c2b52c0ac4cc8dd51e9cc42" },
    { "badass_EU", "0209d48554768dd8dada988b98aca23405057ac4b5b46838a9378b95c3e79b9b9e" },
    { "crackers_NA", "029e1c01131974f4cd3f564cc0c00eb87a0f9721043fbc1ca60f9bd0a1f73f64a1" },
    { "proto_EU", "03681ffdf17c8f4f0008cefb7fa0779c5e888339cdf932f0974483787a4d6747c1" },
    { "jeezy_EU", "023cb3e593fb85c5659688528e9a4f1c4c7f19206edc7e517d20f794ba686fd6d6" },
    { "farl4web_EU", "035caa40684ace968677dca3f09098aa02b70e533da32390a7654c626e0cf908e1" },
    { "nxtswe_EU", "032fb104e5eaa704a38a52c126af8f67e870d70f82977e5b2f093d5c1c21ae5899" },
    { "traderbill_EU", "03196e8de3e2e5d872f31d79d6a859c8704a2198baf0af9c7b21e29656a7eb455f" },
    { "vanbreuk_EU", "024f3cad7601d2399c131fd070e797d9cd8533868685ddbe515daa53c2e26004c3" },
    { "titomane_EU", "03517fcac101fed480ae4f2caf775560065957930d8c1facc83e30077e45bdd199" },
    { "supernet_AE", "029d93ef78197dc93892d2a30e5a54865f41e0ca3ab7eb8e3dcbc59c8756b6e355" },
    { "supernet_EU", "02061c6278b91fd4ac5cab4401100ffa3b2d5a277e8f71db23401cc071b3665546" },
    { "supernet_NA", "033c073366152b6b01535e15dd966a3a8039169584d06e27d92a69889b720d44e1" },
    { "yassin_EU", "033fb7231bb66484081952890d9a03f91164fb27d392d9152ec41336b71b15fbd0" },
    { "durerus_EU", "02bcbd287670bdca2c31e5d50130adb5dea1b53198f18abeec7211825f47485d57" },
    { "badass_SH", "026b49dd3923b78a592c1b475f208e23698d3f085c4c3b4906a59faf659fd9530b" },
    { "badass_NA", "02afa1a9f948e1634a29dc718d218e9d150c531cfa852843a1643a02184a63c1a7" },
    { "pondsea_NA", "031bcfdbb62268e2ff8dfffeb9ddff7fe95fca46778c77eebff9c3829dfa1bb411" },
    { "rnr_EU", "0287aa4b73988ba26cf6565d815786caf0d2c4af704d7883d163ee89cd9977edec" },
    { "crackers_SH", "02313d72f9a16055737e14cfc528dcd5d0ef094cfce23d0348fe974b6b1a32e5f0" },
    { "grewal_SH", "03212a73f5d38a675ee3cdc6e82542a96c38c3d1c79d25a1ed2e42fcf6a8be4e68" },
    { "polycryptoblock_NA", "02708dcda7c45fb54b78469673c2587bfdd126e381654819c4c23df0e00b679622" },
    { "titomane_NA", "0387046d9745414fb58a0fa3599078af5073e10347e4657ef7259a99cb4f10ad47" },
    { "titomane_AE", "03cda6ca5c2d02db201488a54a548dbfc10533bdc275d5ea11928e8d6ab33c2185" },
    { "kolo_EU", "03f5c08dadffa0ffcafb8dd7ffc38c22887bd02702a6c9ac3440deddcf2837692b" },
    { "artik_NA", "0224e31f93eff0cc30eaf0b2389fbc591085c0e122c4d11862c1729d090106c842" },
    { "eclips_EU", "0339369c1f5a2028d44be7be6f8ec3b907fdec814f87d2dead97cab4edb71a42e9" },
    { "titomane_SH", "035f49d7a308dd9a209e894321f010d21b7793461b0c89d6d9231a3fe5f68d9960" },
};

static const uint32_t crc32_tab[] = {
	0x00000000, 0x77073096, 0xee0e612c, 0x990951ba, 0x076dc419, 0x706af48f,
	0xe963a535, 0x9e6495a3,	0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988,
	0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91, 0x1db71064, 0x6ab020f2,
	0xf3b97148, 0x84be41de,	0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7,
	0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec,	0x14015c4f, 0x63066cd9,
	0xfa0f3d63, 0x8d080df5,	0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172,
	0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b,	0x35b5a8fa, 0x42b2986c,
	0xdbbbc9d6, 0xacbcf940,	0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59,
	0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116, 0x21b4f4b5, 0x56b3c423,
	0xcfba9599, 0xb8bda50f, 0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924,
	0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d,	0x76dc4190, 0x01db7106,
	0x98d220bc, 0xefd5102a, 0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433,
	0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818, 0x7f6a0dbb, 0x086d3d2d,
	0x91646c97, 0xe6635c01, 0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e,
	0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457, 0x65b0d9c6, 0x12b7e950,
	0x8bbeb8ea, 0xfcb9887c, 0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65,
	0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2, 0x4adfa541, 0x3dd895d7,
	0xa4d1c46d, 0xd3d6f4fb, 0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0,
	0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9, 0x5005713c, 0x270241aa,
	0xbe0b1010, 0xc90c2086, 0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f,
	0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4, 0x59b33d17, 0x2eb40d81,
	0xb7bd5c3b, 0xc0ba6cad, 0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a,
	0xead54739, 0x9dd277af, 0x04db2615, 0x73dc1683, 0xe3630b12, 0x94643b84,
	0x0d6d6a3e, 0x7a6a5aa8, 0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1,
	0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe, 0xf762575d, 0x806567cb,
	0x196c3671, 0x6e6b06e7, 0xfed41b76, 0x89d32be0, 0x10da7a5a, 0x67dd4acc,
	0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5, 0xd6d6a3e8, 0xa1d1937e,
	0x38d8c2c4, 0x4fdff252, 0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b,
	0xd80d2bda, 0xaf0a1b4c, 0x36034af6, 0x41047a60, 0xdf60efc3, 0xa867df55,
	0x316e8eef, 0x4669be79, 0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236,
	0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f, 0xc5ba3bbe, 0xb2bd0b28,
	0x2bb45a92, 0x5cb36a04, 0xc2d7ffa7, 0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d,
	0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a, 0x9c0906a9, 0xeb0e363f,
	0x72076785, 0x05005713, 0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38,
	0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21, 0x86d3d2d4, 0xf1d4e242,
	0x68ddb3f8, 0x1fda836e, 0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777,
	0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c, 0x8f659eff, 0xf862ae69,
	0x616bffd3, 0x166ccf45, 0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2,
	0xa7672661, 0xd06016f7, 0x4969474d, 0x3e6e77db, 0xaed16a4a, 0xd9d65adc,
	0x40df0b66, 0x37d83bf0, 0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,
	0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6, 0xbad03605, 0xcdd70693,
	0x54de5729, 0x23d967bf, 0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94,
	0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d
};

uint32_t calc_crc32(uint32_t crc,const void *buf,size_t size)
{
	const uint8_t *p;
    
	p = (const uint8_t *)buf;
	crc = crc ^ ~0U;
    
	while (size--)
		crc = crc32_tab[(crc ^ *p++) & 0xFF] ^ (crc >> 8);
    
	return crc ^ ~0U;
}

int32_t iguana_rwnum(int32_t rwflag,uint8_t *serialized,int32_t len,void *endianedp)
{
    int32_t i; uint64_t x;
    if ( rwflag == 0 )
    {
        x = 0;
        for (i=len-1; i>=0; i--)
        {
            x <<= 8;
            x |= serialized[i];
        }
        switch ( len )
        {
            case 1: *(uint8_t *)endianedp = (uint8_t)x; break;
            case 2: *(uint16_t *)endianedp = (uint16_t)x; break;
            case 4: *(uint32_t *)endianedp = (uint32_t)x; break;
            case 8: *(uint64_t *)endianedp = (uint64_t)x; break;
        }
    }
    else
    {
        x = 0;
        switch ( len )
        {
            case 1: x = *(uint8_t *)endianedp; break;
            case 2: x = *(uint16_t *)endianedp; break;
            case 4: x = *(uint32_t *)endianedp; break;
            case 8: x = *(uint64_t *)endianedp; break;
        }
        for (i=0; i<len; i++,x >>= 8)
            serialized[i] = (uint8_t)(x & 0xff);
    }
    return(len);
}

int32_t iguana_rwbignum(int32_t rwflag,uint8_t *serialized,int32_t len,uint8_t *endianedp)
{
    int32_t i;
    if ( rwflag == 0 )
    {
        for (i=0; i<len; i++)
            endianedp[i] = serialized[i];
    }
    else
    {
        for (i=0; i<len; i++)
            serialized[i] = endianedp[i];
    }
    return(len);
}

int32_t _unhex(char c)
{
    if ( c >= '0' && c <= '9' )
        return(c - '0');
    else if ( c >= 'a' && c <= 'f' )
        return(c - 'a' + 10);
    else if ( c >= 'A' && c <= 'F' )
        return(c - 'A' + 10);
    return(-1);
}

int32_t is_hexstr(char *str,int32_t n)
{
    int32_t i;
    if ( str == 0 || str[0] == 0 )
        return(0);
    for (i=0; str[i]!=0; i++)
    {
        if ( n > 0 && i >= n )
            break;
        if ( _unhex(str[i]) < 0 )
            break;
    }
    if ( n == 0 )
        return(i);
    return(i == n);
}

int32_t unhex(char c)
{
    int32_t hex;
    if ( (hex= _unhex(c)) < 0 )
    {
        //printf("unhex: illegal hexchar.(%c)\n",c);
    }
    return(hex);
}

unsigned char _decode_hex(char *hex) { return((unhex(hex[0])<<4) | unhex(hex[1])); }

int32_t decode_hex(uint8_t *bytes,int32_t n,char *hex)
{
    int32_t adjust,i = 0;
    //printf("decode.(%s)\n",hex);
    if ( is_hexstr(hex,n) == 0 )
    {
        memset(bytes,0,n);
        return(n);
    }
    if ( n == 0 || (hex[n*2+1] == 0 && hex[n*2] != 0) )
    {
        if ( n > 0 )
        {
            bytes[0] = unhex(hex[0]);
            printf("decode_hex n.%d hex[0] (%c) -> %d hex.(%s) [n*2+1: %d] [n*2: %d %c] len.%ld\n",n,hex[0],bytes[0],hex,hex[n*2+1],hex[n*2],hex[n*2],(long)strlen(hex));
        }
        bytes++;
        hex++;
        adjust = 1;
    } else adjust = 0;
    if ( n > 0 )
    {
        for (i=0; i<n; i++)
            bytes[i] = _decode_hex(&hex[i*2]);
    }
    //bytes[i] = 0;
    return(n + adjust);
}

int32_t komodo_threshold(int32_t height,uint64_t signedmask)
{
    int32_t numnotaries,i,wt = 0;
    numnotaries = Pubkeys[height / KOMODO_ELECTION_GAP].numnotaries;
    for (i=0; i<numnotaries; i++)
        if ( ((1LL << i) & signedmask) != 0 )
            wt++;
    if ( wt > (numnotaries >> 1) || (wt > 7 && (signedmask & 3) != 0) )
        return(1); // N/2+1 || N/3 + devsig
    else return(0);
}

uint32_t komodo_txtime(uint256 hash)
{
    CTransaction tx;
    uint256 hashBlock;
    if (!GetTransaction(hash, tx, hashBlock, true))
    {
        //printf("null GetTransaction\n");
        return(tx.nLockTime);
    }
    return(0);
}

void komodo_nutxoadd(int32_t addflag,int32_t height,int32_t notaryid,uint256 txhash,uint64_t voutmask,int32_t numvouts)
{
    struct nutxo_entry *np;
    if ( numvouts > 1 && notaryid < 64 ) // change to ADD_HASH() and file based
    {
        pthread_mutex_lock(&komodo_mutex);
        np = (struct nutxo_entry *)calloc(1,sizeof(*np));
        np->height = height;
        np->txhash = txhash;
        np->voutmask = voutmask;
        np->notaryid = notaryid;
        HASH_ADD_KEYPTR(hh,NUTXOS,&np->txhash,sizeof(np->txhash),np);
        printf("Add NUTXO[%d] <- %s notaryid.%d t%u %s %llx\n",Num_nutxos,Notaries[notaryid][0],notaryid,komodo_txtime(txhash),txhash.ToString().c_str(),(long long)voutmask);
        if ( addflag != 0 )
            komodo_stateupdate(height,0,0,notaryid,txhash,voutmask,numvouts);
        Num_nutxos++;
        pthread_mutex_unlock(&komodo_mutex);
    }
}

int32_t komodo_nutxofind(int32_t height,uint256 txhash,int32_t vout) // change to HASH_FIND()
{
    struct nutxo_entry *np;
    pthread_mutex_lock(&komodo_mutex);
    HASH_FIND(hh,NUTXOS,&txhash,sizeof(txhash),np);
    pthread_mutex_unlock(&komodo_mutex);
    if ( np != 0 && ((1LL << vout) & np->voutmask) != 0 )
        return(np->notaryid);
    return(-1);
}

void komodo_notarysinit(int32_t height,uint8_t pubkeys[64][33],int32_t num)
{
    int32_t k,i,htind; struct knotary_entry *kp; struct knotaries_entry N;
    memset(&N,0,sizeof(N));
    pthread_mutex_lock(&komodo_mutex);
    for (k=0; k<num; k++)
    {
        kp = (struct knotary_entry *)calloc(1,sizeof(*kp));
        memcpy(kp->pubkey,pubkeys[k],33);
        kp->notaryid = k;
        HASH_ADD_KEYPTR(hh,N.Notaries,kp->pubkey,33,kp);
        for (i=0; i<33; i++)
            printf("%02x",pubkeys[k][i]);
        printf(" notarypubs.[%d]\n",k);
    }
    N.numnotaries = num;
    htind = KOMODO_PUBKEYS_HEIGHT(height) / KOMODO_ELECTION_GAP;
    if ( htind == 1 )
        htind = 0;
    for (i=htind; i<sizeof(Pubkeys)/sizeof(*Pubkeys); i++)
    {
        Pubkeys[i] = N;
        Pubkeys[i].height = i * KOMODO_ELECTION_GAP;
    }
    pthread_mutex_unlock(&komodo_mutex);
}

int32_t komodo_chosennotary(int32_t *notaryidp,int32_t height,uint8_t *pubkey33)
{
    // -1 if not notary, 0 if notary, 1 if special notary
    struct knotary_entry *kp; int32_t numnotaries,modval = -1;
    *notaryidp = -1;
    pthread_mutex_lock(&komodo_mutex);
    HASH_FIND(hh,Pubkeys[height/KOMODO_ELECTION_GAP].Notaries,pubkey33,33,kp);
    pthread_mutex_unlock(&komodo_mutex);
    if ( kp != 0 )
    {
        if ( (numnotaries= Pubkeys[height/KOMODO_ELECTION_GAP].numnotaries) > 0 )
        {
            *notaryidp = kp->notaryid;
            modval = ((height % numnotaries) == kp->notaryid);
            //printf("found notary.%d ht.%d modval.%d\n",kp->notaryid,height,modval);
        } else printf("unexpected zero notaries at height.%d\n",height);
    }
    //int32_t i; for (i=0; i<33; i++)
    //    printf("%02x",pubkey33[i]);
    //printf(" ht.%d notary.%d special.%d\n",height,*notaryidp,modval);
    return(modval);
}

void komodo_notarized_update(int32_t nHeight,int32_t notarized_height,uint256 notarized_hash,uint256 notarized_btctxid)
{
    struct notarized_checkpoint *np;
    NPOINTS = (struct notarized_checkpoint *)realloc(NPOINTS,(NUM_NPOINTS+1) * sizeof(*NPOINTS));
    np = &NPOINTS[NUM_NPOINTS++];
    memset(np,0,sizeof(*np));
    np->nHeight = nHeight;
    np->notarized_height = notarized_height;
    np->notarized_hash = notarized_hash;
    np->notarized_btctxid = notarized_btctxid;
}

int32_t komodo_notarizeddata(int32_t nHeight,uint256 *notarized_hashp,uint256 *notarized_btctxidp)
{
    struct notarized_checkpoint *np = 0; int32_t i;
    if ( NUM_NPOINTS > 0 )
    {
        for (i=0; i<NUM_NPOINTS; i++)
        {
            if ( NPOINTS[i].nHeight >= nHeight )
                break;
            np = &NPOINTS[i];
        }
    }
    if ( np != 0 )
    {
        *notarized_hashp = np->notarized_hash;
        *notarized_btctxidp = np->notarized_btctxid;
        return(np->notarized_height);
    }
    memset(notarized_hashp,0,sizeof(*notarized_hashp));
    return(-1);
}

int32_t komodo_stateupdate(int32_t height,uint8_t notarypubs[][33],uint8_t numnotaries,uint8_t notaryid,uint256 txhash,uint64_t voutmask,uint8_t numvouts)
{
    static FILE *fp; static int32_t errs,didinit; char fname[512]; int32_t ht,k,func; uint8_t num,pubkeys[64][33];
#ifdef WIN32
    sprintf(fname,"%s\\%s",GetDataDir(false).string().c_str(),(char *)"komodostate");
#else
    sprintf(fname,"%s/%s",GetDataDir(false).string().c_str(),(char *)"komodostate");
#endif
    if ( fp == 0 )
    {
        if ( (fp= fopen(fname,"rb+")) != 0 )
        {
            while ( (func= fgetc(fp)) != EOF )
            {
                if ( fread(&ht,1,sizeof(ht),fp) != sizeof(ht) )
                    errs++;
                //printf("func.(%d %c) ht.%d\n",func,func,ht);
                if ( func == 'P' )
                {
                    if ( (num= fgetc(fp)) < 64 )
                    {
                        if ( fread(pubkeys,33,num,fp) != num )
                            errs++;
                        else
                        {
                            printf("updated %d pubkeys at ht.%d\n",num,ht);
                            komodo_notarysinit(ht,pubkeys,num);
                        }
                    } else printf("illegal num.%d\n",num);
                    //printf("P[%d]\n",num);
                }
                else if ( func == 'N' )
                {
                    if ( fread(&NOTARIZED_HEIGHT,1,sizeof(NOTARIZED_HEIGHT),fp) != sizeof(NOTARIZED_HEIGHT) )
                        errs++;
                    if ( fread(&NOTARIZED_HASH,1,sizeof(NOTARIZED_HASH),fp) != sizeof(NOTARIZED_HASH) )
                        errs++;
                    if ( fread(&NOTARIZED_BTCTXID,1,sizeof(NOTARIZED_BTCTXID),fp) != sizeof(NOTARIZED_BTCTXID) )
                        errs++;
                    printf("load NOTARIZED %d %s\n",NOTARIZED_HEIGHT,NOTARIZED_HASH.ToString().c_str());
                    komodo_notarized_update(ht,NOTARIZED_HEIGHT,NOTARIZED_HASH,NOTARIZED_BTCTXID);
                }
                else if ( func == 'U' )
                {
                    uint8_t n,nid; uint256 hash; uint64_t mask;
                    n = fgetc(fp);
                    nid = fgetc(fp);
                    //printf("U %d %d\n",n,nid);
                    if ( fread(&mask,1,sizeof(mask),fp) != sizeof(mask) )
                        errs++;
                    if ( fread(&hash,1,sizeof(hash),fp) != sizeof(hash) )
                        errs++;
                    komodo_nutxoadd(0,ht,nid,hash,mask,n);
                }
                else if ( func == 'D' )
                {
                    //printf("D[%d]\n",ht);
                }
                else printf("illegal func.(%d %c)\n",func,func);
            }
        } else fp = fopen(fname,"wb+");
        printf("fname.(%s) fpos.%ld\n",fname,ftell(fp));
    }
    if ( fp != 0 )
    {
        if ( height < 0 )
        {
            height = -height;
            //printf("func D[%d] errs.%d\n",height,errs);
            fputc('D',fp);
            if ( fwrite(&height,1,sizeof(height),fp) != sizeof(height) )
                errs++;
        }
        else if ( notarypubs != 0 && numnotaries > 0 )
        {
            //printf("func P[%d] errs.%d\n",numnotaries,errs);
            fputc('P',fp);
            if ( fwrite(&height,1,sizeof(height),fp) != sizeof(height) )
                errs++;
            fputc(numnotaries,fp);
            if ( fwrite(notarypubs,33,numnotaries,fp) != numnotaries )
                errs++;
        }
        else if ( voutmask != 0 && numvouts > 0 )
        {
            //printf("func U %d %d errs.%d hashsize.%ld\n",numvouts,notaryid,errs,sizeof(txhash));
            fputc('U',fp);
            if ( fwrite(&height,1,sizeof(height),fp) != sizeof(height) )
                errs++;
            fputc(numvouts,fp);
            fputc(notaryid,fp);
            if ( fwrite(&voutmask,1,sizeof(voutmask),fp) != sizeof(voutmask) )
                errs++;
            if ( fwrite(&txhash,1,sizeof(txhash),fp) != sizeof(txhash) )
                errs++;
        }
        else if ( height != 0 )
        {
            //printf("func N ht.%d errs.%d\n",NOTARIZED_HEIGHT,errs);
            fputc('N',fp);
            if ( fwrite(&height,1,sizeof(height),fp) != sizeof(height) )
                errs++;
            if ( fwrite(&NOTARIZED_HEIGHT,1,sizeof(NOTARIZED_HEIGHT),fp) != sizeof(NOTARIZED_HEIGHT) )
                errs++;
            if ( fwrite(&NOTARIZED_HASH,1,sizeof(NOTARIZED_HASH),fp) != sizeof(NOTARIZED_HASH) )
                errs++;
            if ( fwrite(&NOTARIZED_BTCTXID,1,sizeof(NOTARIZED_BTCTXID),fp) != sizeof(NOTARIZED_BTCTXID) )
                errs++;
            komodo_notarized_update(height,NOTARIZED_HEIGHT,NOTARIZED_HASH,NOTARIZED_BTCTXID);
        }
        fflush(fp);
    }
}

void komodo_init()
{
    static int didinit; uint256 zero; int32_t k; uint8_t pubkeys[64][33];
    if ( didinit == 0 )
    {
        didinit = 1;
        pthread_mutex_init(&komodo_mutex,NULL);
        decode_hex(NOTARY_PUBKEY33,33,(char *)NOTARY_PUBKEY.c_str());
        KOMODO_NUMNOTARIES = (int32_t)(sizeof(Notaries)/sizeof(*Notaries));
        for (k=0; k<KOMODO_NUMNOTARIES; k++)
        {
            if ( Notaries[k][0] == 0 || Notaries[k][1] == 0 || Notaries[k][0][0] == 0 || Notaries[k][1][0] == 0 )
                break;
            decode_hex(pubkeys[k],33,(char *)Notaries[k][1]);
        }
        komodo_notarysinit(0,pubkeys,KOMODO_NUMNOTARIES);
        memset(&zero,0,sizeof(zero));
        komodo_stateupdate(0,0,0,0,zero,0,0);
    }
}

int32_t komodo_voutupdate(int32_t notaryid,uint8_t *scriptbuf,int32_t scriptlen,int32_t height,uint256 txhash,int32_t i,int32_t j,uint64_t *voutmaskp,int32_t *specialtxp,int32_t *notarizedheightp)
{
    int32_t k,opretlen,nid,len = 0; uint256 kmdtxid,btctxid; uint8_t crypto777[33];
    if ( scriptlen == 35 && scriptbuf[0] == 33 && scriptbuf[34] == 0xac )
    {
        decode_hex(crypto777,33,(char *)CRYPTO777_PUBSECPSTR);
        /*for (k=0; k<33; k++)
            printf("%02x",crypto777[k]);
        printf(" crypto777 ");
        for (k=0; k<scriptlen; k++)
            printf("%02x",scriptbuf[k]);
        printf(" <- script ht.%d i.%d j.%d cmp.%d\n",height,i,j,memcmp(crypto777,scriptbuf+1,33));*/
        if ( memcmp(crypto777,scriptbuf+1,33) == 0 )
        {
            *specialtxp = 1;
            printf(">>>>>>>> ");
        }
        else if ( komodo_chosennotary(&nid,height,scriptbuf + 1) >= 0 )
        {
            //printf("found notary.k%d\n",k);
            if ( notaryid < 64 )
            {
                if ( notaryid < 0 )
                {
                    notaryid = nid;
                    *voutmaskp |= (1LL << j);
                }
                else if ( notaryid != nid )
                {
                    printf("mismatch notaryid.%d k.%d\n",notaryid,nid);
                    notaryid = 64;
                    *voutmaskp = 0;
                }
                else *voutmaskp |= (1LL << j);
            }
        }
    }
    if ( j == 1 && scriptbuf[len++] == 0x6a )
    {
        if ( (opretlen= scriptbuf[len++]) == 0x4c )
            opretlen = scriptbuf[len++];
        else if ( opretlen == 0x4d )
        {
            opretlen = scriptbuf[len++];
            opretlen = (opretlen << 8) + scriptbuf[len++];
        }
        if ( opretlen >= 32*2+4 )
        {
            len += iguana_rwbignum(0,&scriptbuf[len],32,(uint8_t *)&kmdtxid);
            len += iguana_rwnum(0,&scriptbuf[len],4,(uint8_t *)notarizedheightp);
            len += iguana_rwbignum(0,&scriptbuf[len],32,(uint8_t *)&btctxid);
            //for (k=0; k<scriptlen; k++)
            //    printf("%02x",scriptbuf[k]);
            //printf(" <- script ht.%d i.%d j.%d\n",height,i,j);
            printf("ht.%d NOTARIZED.%d KMD.%s BTCTXID.%s\n",height,*notarizedheightp,kmdtxid.ToString().c_str(),btctxid.ToString().c_str());
            if ( *notarizedheightp > NOTARIZED_HEIGHT )
            {
                static uint256 zero;
                NOTARIZED_HEIGHT = *notarizedheightp;
                NOTARIZED_HASH = kmdtxid;
                NOTARIZED_BTCTXID = btctxid;
                komodo_stateupdate(height,0,0,0,zero,0,0);
            }
        }
    }
    return(notaryid);
}

void komodo_connectblock(CBlockIndex *pindex,CBlock& block)
{
    static int32_t didinit;
    char *scriptstr,*opreturnstr; uint64_t signedmask,voutmask;
    uint8_t scriptbuf[4096],pubkeys[64][33]; uint256 kmdtxid,btctxid,txhash;
    int32_t i,j,k,numvalid,specialtx,notarizedheight,notaryid,len,numvouts,numvins,height,txn_count,flag;
    komodo_init();
    if ( pindex != 0 )
    {
        height = pindex->nHeight;
        txn_count = block.vtx.size();
        for (i=0; i<txn_count; i++)
        {
            txhash = block.vtx[i].GetHash();
            numvouts = block.vtx[i].vout.size();
            notaryid = -1;
            voutmask = specialtx = notarizedheight = 0;
            for (j=0; j<numvouts; j++)
            {
                len = block.vtx[i].vout[j].scriptPubKey.size();
                if ( len <= sizeof(scriptbuf) )
                {
                    memcpy(scriptbuf,block.vtx[i].vout[j].scriptPubKey.data(),len);
                    notaryid = komodo_voutupdate(notaryid,scriptbuf,len,height,txhash,i,j,&voutmask,&specialtx,&notarizedheight);
                    if ( 0 && i > 0 )
                    {
                        for (k=0; k<len; k++)
                            printf("%02x",scriptbuf[k]);
                        printf(" <- notaryid.%d ht.%d i.%d j.%d numvouts.%d numvins.%d voutmask.%llx txid.(%s)\n",notaryid,height,i,j,numvouts,numvins,(long long)voutmask,txhash.ToString().c_str());
                    }
                }
            }
            if ( notaryid >= 0 && notaryid < 64 && voutmask != 0 )
                komodo_nutxoadd(1,height,notaryid,txhash,voutmask,numvouts);
            signedmask = 0;
            numvins = block.vtx[i].vin.size();
            for (j=0; j<numvins; j++)
            {
                if ( (k= komodo_nutxofind(height,block.vtx[i].vin[j].prevout.hash,block.vtx[i].vin[j].prevout.n)) >= 0 )
                    signedmask |= (1LL << k);
                else if ( 0 && signedmask != 0 )
                    printf("signedmask.%llx but ht.%d i.%d j.%d not found (%s %d)\n",(long long)signedmask,height,i,j,block.vtx[i].vin[j].prevout.hash.ToString().c_str(),block.vtx[i].vin[j].prevout.n);
            }
            if ( signedmask != 0 && (notarizedheight != 0 || specialtx != 0) )
            {
                printf("NOTARY SIGNED.%llx numvins.%d ht.%d txi.%d notaryht.%d specialtx.%d\n",(long long)signedmask,numvins,height,i,notarizedheight,specialtx);
                if ( specialtx != 0 && numvouts > 2 && komodo_threshold(height,signedmask) > 0 )
                {
                    numvalid = 0;
                    memset(pubkeys,0,sizeof(pubkeys));
                    for (j=1; j<numvouts; j++)
                    {
                        len = block.vtx[i].vout[j].scriptPubKey.size();
                        if ( len <= sizeof(scriptbuf) )
                        {
                            memcpy(scriptbuf,block.vtx[i].vout[j].scriptPubKey.data(),len);
                            if ( len == 35 && scriptbuf[0] == 33 && scriptbuf[34] == 0xac )
                            {
                                memcpy(pubkeys[numvalid++],scriptbuf+1,33);
                                for (k=0; k<33; k++)
                                    printf("%02x",scriptbuf[k+1]);
                                printf(" <- new notary.[%d]\n",j-1);
                            }
                        }
                    }
                    if ( numvalid > 13 )
                    {
                        memset(&txhash,0,sizeof(txhash));
                        komodo_stateupdate(height,pubkeys,numvalid,0,txhash,0,0);
                    }
                    printf("new notaries.%d newheight.%d from height.%d\n",numvouts-1,KOMODO_PUBKEYS_HEIGHT(height),height);
                }
            }
        }
    } else printf("komodo_connectblock: unexpected null pindex\n");
}

void komodo_disconnect(CBlockIndex *pindex,CBlock& block)
{
    komodo_init();
    //uint256 zero;
    //printf("disconnect ht.%d\n",pindex->nHeight);
    //memset(&zero,0,sizeof(zero));
    //komodo_stateupdate(-pindex->nHeight,0,0,0,zero,0,0);
}

int32_t komodo_block2height(CBlock *block)
{
    int32_t i,n,height = 0; uint8_t *ptr = (uint8_t *)block->vtx[0].vin[0].scriptSig.data();
    komodo_init();
    if ( block->vtx[0].vin[0].scriptSig.size() > 5 )
    {
        //for (i=0; i<6; i++)
        //    printf("%02x",ptr[i]);
        n = ptr[0];
        for (i=0; i<n; i++)
        {
            //03bb81000101(bb 187) (81 48001) (00 12288256)  <- coinbase.6 ht.12288256
            height += ((uint32_t)ptr[i+1] << (i*8));
            //printf("(%02x %x %d) ",ptr[i+1],((uint32_t)ptr[i+1] << (i*8)),height);
        }
        //printf(" <- coinbase.%d ht.%d\n",(int32_t)block->vtx[0].vin[0].scriptSig.size(),height);
    }
    return(height);
}

void komodo_block2pubkey33(uint8_t *pubkey33,CBlock& block)
{
    uint8_t *ptr = (uint8_t *)block.vtx[0].vout[0].scriptPubKey.data();
    komodo_init();
    memcpy(pubkey33,ptr+1,33);
}

void komodo_index2pubkey33(uint8_t *pubkey33,CBlockIndex *pindex,int32_t height)
{
    CBlock block;
    komodo_init();
    memset(pubkey33,0,33);
    if ( pindex != 0 )
    {
        if ( ReadBlockFromDisk(block,(const CBlockIndex *)pindex) != 0 )
        {
            komodo_block2pubkey33(pubkey33,block);
        }
    }
    else
    {
        // height -> pubkey33
        //printf("extending chaintip komodo_index2pubkey33 height.%d need to get pubkey33\n",height);
    }
}

int32_t komodo_opreturnscript(uint8_t *script,uint8_t type,uint8_t *opret,int32_t opretlen)
{
    int32_t offset = 0;
    script[offset++] = 0x6a;
    opretlen++;
    if ( opretlen >= 0x4c )
    {
        if ( opretlen > 0xff )
        {
            script[offset++] = 0x4d;
            script[offset++] = opretlen & 0xff;
            script[offset++] = (opretlen >> 8) & 0xff;
        }
        else
        {
            script[offset++] = 0x4c;
            script[offset++] = opretlen;
        }
    } else script[offset++] = opretlen;
    script[offset++] = type;
    memcpy(&script[offset],opret,opretlen);
    return(opretlen + offset);
}

int32_t komodo_opreturn(uint8_t *opret,int32_t maxsize)
{
    FILE *fp; char fname[512]; uint32_t crc32,check; int32_t i,n,retval,fsize,len=0; uint8_t data[8192];
#ifdef WIN32
    sprintf(fname,"%s\\%s",GetDataDir(false).string().c_str(),(char *)"komodofeed");
#else
    sprintf(fname,"%s/%s",GetDataDir(false).string().c_str(),(char *)"komodofeed");
#endif
    if ( (fp= fopen(fname,"rb")) != 0 )
    {
        fseek(fp,0,SEEK_END);
        fsize = (int32_t)ftell(fp);
        rewind(fp);
        if ( fsize <= maxsize-4 && fsize <= sizeof(data) && fsize > sizeof(crc32) )
        {
            if ( (retval= (int32_t)fread(data,1,fsize,fp)) == fsize )
            {
                uint32_t timestamp,kmdbtc,btcusd,cnyusd,pvals[32]; double KMDBTC,BTCUSD,CNYUSD;
                len = iguana_rwnum(0,data,sizeof(crc32),(void *)&crc32);
                check = calc_crc32(0,data+sizeof(crc32),(int32_t)(fsize-sizeof(crc32)));
                if ( check == crc32 )
                {
                    len += iguana_rwnum(0,&data[len],sizeof(uint32_t),(void *)&timestamp);
                    len += iguana_rwnum(0,&data[len],sizeof(uint32_t),(void *)&n);
                    len += iguana_rwnum(0,&data[len],sizeof(uint32_t),(void *)&kmdbtc); // /= 1000
                    len += iguana_rwnum(0,&data[len],sizeof(uint32_t),(void *)&btcusd); // *= 1000
                    len += iguana_rwnum(0,&data[len],sizeof(uint32_t),(void *)&cnyusd);
                    KMDBTC = ((double)kmdbtc / (1000000000. * 1000.));
                    BTCUSD = ((double)btcusd / (1000000000. / 1000.));
                    CNYUSD = ((double)cnyusd / 1000000000.);
                    for (i=0; i<n-3; i++)
                    {
                        len += iguana_rwnum(0,&data[len],sizeof(uint32_t),(void *)&pvals[i]);
                        printf("%u ",pvals[i]);
                    }
                    printf("t%u n.%d KMD %f BTC %f CNY %f (%f)\n",timestamp,n,KMDBTC,BTCUSD,CNYUSD,CNYUSD!=0?1./CNYUSD:0);
                    if ( timestamp > time(NULL)-60 )
                    {
                        n = komodo_opreturnscript(opret,'P',data+sizeof(crc32),(int32_t)(fsize-sizeof(crc32)));
                        for (i=0; i<n; i++)
                            printf("%02x",opret[i]);
                        printf(" coinbase opret[%d] crc32.%u:%u\n",n,crc32,check);
                    } else printf("t%u too old for %u\n",timestamp,(uint32_t)time(NULL));
                } else printf("crc32 %u mismatch %u\n",crc32,check);
            }
            else printf("fread.%d error != fsize.%d\n",retval,fsize);
        } else printf("fsize.%d > maxsize.%d or data[%d]\n",fsize,maxsize,(int32_t)sizeof(data));
        fclose(fp);
    }
    return(n);
}

#endif
