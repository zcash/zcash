/******************************************************************************
 * Copyright � 2014-2019 The SuperNET Developers.                             *
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

/*
 CCutilbits.cpp has very low level functions that are universally useful for all contracts and have low dependency from other sources
 */

#include "CCinclude.h"
#include "komodo_structs.h"

int32_t unstringbits(char *buf,uint64_t bits)
{
    int32_t i;
    for (i=0; i<8; i++,bits>>=8)
        if ( (buf[i]= (char)(bits & 0xff)) == 0 )
            break;
    buf[i] = 0;
    return(i);
}

uint64_t stringbits(char *str)
{
    uint64_t bits = 0;
    if ( str == 0 )
        return(0);
    int32_t i,n = (int32_t)strlen(str);
    if ( n > 8 )
        n = 8;
    for (i=n-1; i>=0; i--)
        bits = (bits << 8) | (str[i] & 0xff);
    //printf("(%s) -> %llx %llu\n",str,(long long)bits,(long long)bits);
    return(bits);
}

uint256 revuint256(uint256 txid)
{
    uint256 revtxid; int32_t i;
    for (i=31; i>=0; i--)
        ((uint8_t *)&revtxid)[31-i] = ((uint8_t *)&txid)[i];
    return(revtxid);
}

char *uint256_str(char *dest,uint256 txid)
{
    int32_t i,j=0;
    for (i=31; i>=0; i--)
        sprintf(&dest[j++ * 2],"%02x",((uint8_t *)&txid)[i]);
    dest[64] = 0;
    return(dest);
}

char *pubkey33_str(char *dest,uint8_t *pubkey33)
{
    int32_t i;
    if ( pubkey33 != 0 )
    {
        for (i=0; i<33; i++)
            sprintf(&dest[i * 2],"%02x",pubkey33[i]);
    } else dest[0] = 0;
    return(dest);
}

uint256 Parseuint256(const char *hexstr)
{
    uint256 txid; int32_t i; std::vector<unsigned char> txidbytes(ParseHex(hexstr));
    memset(&txid,0,sizeof(txid));
    if ( strlen(hexstr) == 64 )
    {
        for (i=31; i>=0; i--)
            ((uint8_t *)&txid)[31-i] = ((uint8_t *)txidbytes.data())[i];
    }
    return(txid);
}

CPubKey buf2pk(uint8_t *buf33)
{
    CPubKey pk; int32_t i; uint8_t *dest;
    dest = (uint8_t *)pk.begin();
    for (i=0; i<33; i++)
        dest[i] = buf33[i];
    return(pk);
}

CPubKey pubkey2pk(std::vector<uint8_t> vpubkey)
{
    CPubKey pk; 
    pk.Set(vpubkey.begin(), vpubkey.end());
    return(pk);
}

void CCLogPrintStr(const char *category, int level, const std::string &str)
{
    if (level < 0)
        level = 0;
    if (level > CCLOG_MAXLEVEL)
        level = CCLOG_MAXLEVEL;
    for (int i = level; i <= CCLOG_MAXLEVEL; i++)
        if (LogAcceptCategory((std::string(category) + std::string("-") + std::to_string(i)).c_str()) ||     // '-debug=cctokens-0', '-debug=cctokens-1',...
            i == 0 && LogAcceptCategory(std::string(category).c_str())) {                                  // also supporting '-debug=cctokens' for CCLOG_INFO
            LogPrintStr(str);
            break;
        }
}
