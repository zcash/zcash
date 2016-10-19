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

#include <stdint.h>
#include <stdio.h>

#define CRYPTO777_PUBSECPSTR "020e46e79a2a8d12b9b5d12c7a91adb4e454edfae43c0a0cb805427d2ac7613fd9"

const char *Notaries[64][2] =
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
    { "yassin_EU", "033fb7231bb66484081952890d9a03f91164fb27d392d9152ec41336b71b15fbd0" },
    { "durerus_EU", "02bcbd287670bdca2c31e5d50130adb5dea1b53198f18abeec7211825f47485d57" },
    { "badass_SH", "026b49dd3923b78a592c1b475f208e23698d3f085c4c3b4906a59faf659fd9530b" },
    { "baddass_NA" "02afa1a9f948e1634a29dc718d218e9d150c531cfa852843a1643a02184a63c1a7" },
    { "pondsea_NA", "031bcfdbb62268e2ff8dfffeb9ddff7fe95fca46778c77eebff9c3829dfa1bb411" },
    { "rnr_EU", "0287aa4b73988ba26cf6565d815786caf0d2c4af704d7883d163ee89cd9977edec" },
    { "crackers_SH", "02313d72f9a16055737e14cfc528dcd5d0ef094cfce23d0348fe974b6b1a32e5f0" },
    { "grewal_SH", "03212a73f5d38a675ee3cdc6e82542a96c38c3d1c79d25a1ed2e42fcf6a8be4e68" },
    { "polycryptoblock_NA", "02708dcda7c45fb54b78469673c2587bfdd126e381654819c4c23df0e00b679622" },
    { "titomane_NA", "0387046d9745414fb58a0fa3599078af5073e10347e4657ef7259a99cb4f10ad47" },
    { "titomane_AE", "03cda6ca5c2d02db201488a54a548dbfc10533bdc275d5ea11928e8d6ab33c2185" },
    { "kolo_EU", "03f5c08dadffa0ffcafb8dd7ffc38c22887bd02702a6c9ac3440deddcf2837692b" },
};

int32_t IS_KOMODO_NOTARY,USE_EXTERNAL_PUBKEY,NOTARIZED_HEIGHT,Num_nutxos;
std::string NOTARY_PUBKEY;
uint256 NOTARIZED_HASH;
//char *komodo_getspendscript(uint256 hash,int32_t n);
struct nutxo_entry { uint256 txhash; uint64_t voutmask; int32_t notaryid; };
struct nutxo_entry NUTXOS[10000];

void komodo_nutxoadd(int32_t notaryid,uint256 txhash,uint64_t voutmask,int32_t numvouts)
{
    if ( numvouts > 1 )
    {
        NUTXOS[Num_nutxos].txhash = txhash;
        NUTXOS[Num_nutxos].voutmask = voutmask;
        NUTXOS[Num_nutxos].notaryid = notaryid;
        printf("Add NUTXO[%d] <- notaryid.%d %llx\n",Num_nutxos,notaryid,(long long)voutmask);
        Num_nutxos++;
    }
}

int32_t komodo_nutxofind(uint256 txhash,int32_t vout)
{
    int32_t i;
    for (i=0; i<Num_nutxos; i++)
    {
        if ( memcmp(&txhash,&NUTXOS[i].txhash,sizeof(txhash)) == 0 && ((1LL << vout) & NUTXOS[i].voutmask) != 0 )
            return(NUTXOS[i].notaryid);
    }
    return(-1);
}

int32_t komodo_blockindexcheck(CBlockIndex *pindex,uint32_t *nBitsp)
{
    // 1 -> valid notary block, change nBits to KOMODO_MINDIFF_NBITS
    // -1 -> invalid, ie, prior to notarized block
    CBlock block; int32_t i,height; char *coinbasestr;
    if ( pindex == 0 )
        return(0);
    if ( ReadBlockFromDisk(block,pindex,1) == 0 )
        return(0);
    if ( block.vtx.size() > 0 )
    {
        height = pindex->nHeight;
        coinbasestr = (char *)block.vtx[0].vout[0].scriptPubKey.ToString().c_str();
        for (i=0; i<64; i++)
        {
            if ( Notaries[i][0] == 0 || Notaries[i][1] == 0 || Notaries[i][0][0] == 0 || Notaries[i][1][0] == 0 )
                break;
            if ( strncmp(Notaries[i][1],coinbasestr,66) == 0 )
            {
                //printf("Notary.[%d] %s ht.%d (%s)\n",i,Notaries[i][0],height,coinbasestr);
                //*nBitsp = KOMODO_MINDIFF_NBITS;
                return(1);
            }
        }
    }
    // compare against elected notary pubkeys as of height
    return(0);
}

void komodo_connectblock(CBlockIndex *pindex,CBlock& block)
{
    char *scriptstr,*opreturnstr; uint64_t signedmask,voutmask; uint32_t notarizedheight;
    uint8_t opret[256]; uint256 kmdtxid,btctxid,txhash;
    int32_t i,j,k,opretlen,notaryid,len,numvouts,numvins,height,txn_count,flag;
    // update voting results and official (height, notaries[])
    if ( pindex != 0 )
    {
        height = pindex->nHeight;
        txn_count = block.vtx.size();
        notarizedheight = 0;
        signedmask = 0;
        for (i=0; i<txn_count; i++)
        {
            numvins = block.vtx[i].vin.size();
            txhash = block.vtx[i].GetHash();
            numvouts = block.vtx[i].vout.size();
            notaryid = -1;
            voutmask = 0;
            for (j=0; j<numvouts; j++)
            {
                scriptstr = (char *)block.vtx[i].vout[j].scriptPubKey.ToString().c_str();
                if ( strncmp(scriptstr,CRYPTO777_PUBSECPSTR,66) == 0 )
                    printf(">>>>>>>> ");
                if ( j == 1 && strncmp("OP_RETURN ",scriptstr,strlen("OP_RETURN ")) == 0 )
                {
                    flag = 0;
                    opreturnstr = &scriptstr[strlen("OP_RETURN ")];
                    len = (int32_t)strlen(opreturnstr);
                    if ( (len & 1) != 0 && opreturnstr[len-1] == '?' )
                        len--, flag++;
                    len >>= 1;
                    if ( len <= sizeof(opret) )
                    {
                        decode_hex(opret,len,opreturnstr);
                        if ( flag != 0 )
                            opret[len++] = 0; // horrible hack
                        opretlen = 0;
                        opretlen += iguana_rwbignum(0,&opret[opretlen],32,(uint8_t *)&kmdtxid);
                        opretlen += iguana_rwnum(0,&opret[opretlen],4,(uint8_t *)&notarizedheight);
                        opretlen += iguana_rwbignum(0,&opret[opretlen],32,(uint8_t *)&btctxid);
                        printf("signed.%llx ht.%d NOTARIZED.%d KMD.%s BTC.%s %s\n",(long long)signedmask,height,notarizedheight,kmdtxid.ToString().c_str(),btctxid.ToString().c_str(),scriptstr);
                        if ( signedmask != 0 && notarizedheight > NOTARIZED_HEIGHT )
                        {
                            NOTARIZED_HEIGHT = notarizedheight;
                            NOTARIZED_HASH = kmdtxid;
                        }
                    }
                }
                for (k=0; k<64; k++)
                {
                    if ( Notaries[k][0] == 0 || Notaries[k][1] == 0 || Notaries[k][0][0] == 0 || Notaries[k][1][0] == 0 )
                        break;
                    if ( strncmp(Notaries[k][1],scriptstr,66) == 0 )
                    {
                        //printf("%s ht.%d i.%d k.%d\n",Notaries[k][0],height,i,k);
                        //*nBitsp = KOMODO_MINDIFF_NBITS;
                        if ( notaryid < 0 )
                        {
                            notaryid = k;
                            voutmask |= (1LL << j);
                        }
                        else if ( notaryid != k )
                            printf("mismatch notaryid.%d k.%d\n",notaryid,k);
                        else voutmask |= (1LL << j);
                        break;
                    }
                }
                if ( notaryid >= 0 )
                    printf("k.%d %s ht.%d txi.%d in.%d out.%d vout.%d (%s)\n",notaryid,k>=0?Notaries[notaryid][0]:"",height,i,numvins,numvouts,j,txhash.ToString().c_str());
            }
            for (j=0; j<numvins; j++)
            {
                if ( (notaryid= komodo_nutxofind(block.vtx[i].vin[j].prevout.hash,block.vtx[i].vin[j].prevout.n)) >= 0 )
                    signedmask |= (1LL << notaryid);
            }
            if ( signedmask != 0 && notaryid >= 0 && voutmask != 0 )
                komodo_nutxoadd(notaryid,txhash,voutmask,numvouts);
        }
        if ( signedmask != 0 || notarizedheight != 0 )
        {
            printf("NOTARY SIGNED.%llx ht.%d txi.%d notaryht.%d\n",(long long)signedmask,height,i,notarizedheight);
        }
    } else printf("komodo_connectblock: unexpected null pindex\n");
}

int32_t komodo_is_notaryblock(CBlockHeader& blockhdr)
{
    //uint32_t nBits = 0;
    //return(komodo_blockindexcheck(mapBlockIndex[blockhdr.GetHash()],&nBits));
    return(0);
}

int32_t komodo_blockhdrcheck(CBlockHeader& blockhdr,uint32_t *nBitsp)
{
    int32_t retval;
    if ( (retval= komodo_is_notaryblock(blockhdr)) > 0 )
        *nBitsp = KOMODO_MINDIFF_NBITS;
    return(retval);
}

int32_t komodo_blockcheck(CBlock& block,uint32_t *nBitsp)
{
    return(komodo_blockhdrcheck(block,nBitsp));
}

#endif
