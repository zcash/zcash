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

int32_t NUM_PRICES; uint32_t *PVALS;

#define USD 0

#define MAX_CURRENCIES 32
char CURRENCIES[][8] = { "USD", "EUR", "JPY", "GBP", "AUD", "CAD", "CHF", "NZD", // major currencies
    "CNY", "RUB", "MXN", "BRL", "INR", "HKD", "TRY", "ZAR", "PLN", "NOK", "SEK", "DKK", "CZK", "HUF", "ILS", "KRW", "MYR", "PHP", "RON", "SGD", "THB", "BGN", "IDR", "HRK",
    "KMD" };

uint64_t M1SUPPLY[] = { 3317900000000, 6991604000000, 667780000000000, 1616854000000, 331000000000, 861909000000, 584629000000, 46530000000, // major currencies
    45434000000000, 16827000000000, 3473357229000, 306435000000, 27139000000000, 2150641000000, 347724099000, 1469583000000, 749543000000, 1826110000000, 2400434000000, 1123925000000, 3125276000000, 13975000000000, 317657000000, 759706000000000, 354902000000, 2797061000000, 162189000000, 163745000000, 1712000000000, 39093000000, 1135490000000000, 80317000000,
    100000000 };

#define MIND 1000
uint32_t MINDENOMS[] = { MIND, MIND, 100*MIND, MIND, MIND, MIND, MIND, MIND, // major currencies
    10*MIND, 100*MIND, 10*MIND, MIND, 100*MIND, 10*MIND, MIND, 10*MIND, MIND, 10*MIND, 10*MIND, 10*MIND, 10*MIND, 100*MIND, MIND, 1000*MIND, MIND, 10*MIND, MIND, MIND, 10*MIND, MIND, 10000*MIND, 10*MIND, // end of currencies
10*MIND,
};

uint64_t komodo_paxvol(uint64_t volume,uint64_t price)
{
    if ( volume < 10000000000 )
        return((volume * price) / 1000000000);
    else if ( volume < (uint64_t)10 * 10000000000 )
        return((volume * (price / 10)) / 100000000);
    else if ( volume < (uint64_t)100 * 10000000000 )
        return(((volume / 10) * (price / 10)) / 10000000);
    else if ( volume < (uint64_t)1000 * 10000000000 )
        return(((volume / 10) * (price / 100)) / 1000000);
    else if ( volume < (uint64_t)10000 * 10000000000 )
        return(((volume / 100) * (price / 100)) / 100000);
    else if ( volume < (uint64_t)100000 * 10000000000 )
        return(((volume / 100) * (price / 1000)) / 10000);
    else if ( volume < (uint64_t)1000000 * 10000000000 )
        return(((volume / 1000) * (price / 1000)) / 1000);
    else if ( volume < (uint64_t)10000000 * 10000000000 )
        return(((volume / 1000) * (price / 10000)) / 100);
    else return(((volume / 10000) * (price / 10000)) / 10);
}

void pax_rank(uint64_t *ranked,uint32_t *pvals)
{
    int32_t i; uint64_t vals[32],sum = 0;
    for (i=0; i<32; i++)
    {
        vals[i] = komodo_paxvol(M1SUPPLY[i] / MINDENOMS[i],pvals[i]);
        sum += vals[i];
    }
    for (i=0; i<32; i++)
    {
        ranked[i] = (vals[i] * 1000000000) / sum;
        //printf("%.6f ",(double)ranked[i]/1000000000.);
    }
    //printf("sum %llu\n",(long long)sum);
};

int32_t dpow_readprices(uint8_t *data,uint32_t *timestampp,double *KMDBTCp,double *BTCUSDp,double *CNYUSDp,uint32_t *pvals)
{
    uint32_t kmdbtc,btcusd,cnyusd; int32_t i,n,len = 0;
    len += iguana_rwnum(0,&data[len],sizeof(uint32_t),(void *)timestampp);
    len += iguana_rwnum(0,&data[len],sizeof(uint32_t),(void *)&n);
    len += iguana_rwnum(0,&data[len],sizeof(uint32_t),(void *)&kmdbtc); // /= 1000
    len += iguana_rwnum(0,&data[len],sizeof(uint32_t),(void *)&btcusd); // *= 1000
    len += iguana_rwnum(0,&data[len],sizeof(uint32_t),(void *)&cnyusd);
    *KMDBTCp = ((double)kmdbtc / (1000000000. * 1000.));
    *BTCUSDp = ((double)btcusd / (1000000000. / 1000.));
    *CNYUSDp = ((double)cnyusd / 1000000000.);
    for (i=0; i<n-3; i++)
    {
        len += iguana_rwnum(0,&data[len],sizeof(uint32_t),(void *)&pvals[i]);
        //printf("%u ",pvals[i]);
    }
    pvals[i++] = kmdbtc;
    pvals[i++] = btcusd;
    pvals[i++] = cnyusd;
    //printf("OP_RETURN prices\n");
    return(n);
}

int32_t komodo_pax_opreturn(uint8_t *opret,int32_t maxsize)
{
    static uint32_t lastcrc;
    FILE *fp; char fname[512]; uint32_t crc32,check,timestamp; int32_t i,n=0,retval,fsize,len=0; uint8_t data[8192];
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
                len = iguana_rwnum(0,data,sizeof(crc32),(void *)&crc32);
                check = calc_crc32(0,data+sizeof(crc32),(int32_t)(fsize-sizeof(crc32)));
                if ( check == crc32 )
                {
                    double KMDBTC,BTCUSD,CNYUSD; uint32_t pvals[128];
                    dpow_readprices(&data[len],&timestamp,&KMDBTC,&BTCUSD,&CNYUSD,pvals);
                    if ( 0 && lastcrc != crc32 )
                    {
                        for (i=0; i<32; i++)
                            printf("%u ",pvals[i]);
                        printf("t%u n.%d KMD %f BTC %f CNY %f (%f)\n",timestamp,n,KMDBTC,BTCUSD,CNYUSD,CNYUSD!=0?1./CNYUSD:0);
                    }
                    if ( timestamp > time(NULL)-600 )
                    {
                        n = komodo_opreturnscript(opret,'P',data+sizeof(crc32),(int32_t)(fsize-sizeof(crc32)));
                        if ( 0 && lastcrc != crc32 )
                        {
                            for (i=0; i<n; i++)
                                printf("%02x",opret[i]);
                            printf(" coinbase opret[%d] crc32.%u:%u\n",n,crc32,check);
                        }
                    } //else printf("t%u too old for %u\n",timestamp,(uint32_t)time(NULL));
                    lastcrc = crc32;
                } else printf("crc32 %u mismatch %u\n",crc32,check);
            } else printf("fread.%d error != fsize.%d\n",retval,fsize);
        } else printf("fsize.%d > maxsize.%d or data[%d]\n",fsize,maxsize,(int32_t)sizeof(data));
        fclose(fp);
    }
    return(n);
}

/*uint32_t PAX_val32(double val)
 {
 uint32_t val32 = 0; struct price_resolution price;
 if ( (price.Pval= val*1000000000) != 0 )
 {
 if ( price.Pval > 0xffffffff )
 printf("Pval overflow error %lld\n",(long long)price.Pval);
 else val32 = (uint32_t)price.Pval;
 }
 return(val32);
 }*/

int32_t PAX_pubkey(int32_t rwflag,uint8_t *pubkey33,uint8_t *addrtypep,uint8_t rmd160[20],char fiat[4],uint8_t *shortflagp,int64_t *fiatoshisp)
{
    if ( rwflag != 0 )
    {
        memset(pubkey33,0,33);
        pubkey33[0] = 0x02 | (*shortflagp != 0);
        memcpy(&pubkey33[1],fiat,3);
        iguana_rwnum(rwflag,&pubkey33[4],sizeof(*fiatoshisp),(void *)fiatoshisp);
        pubkey33[12] = *addrtypep;
        memcpy(&pubkey33[13],rmd160,20);
    }
    else
    {
        *shortflagp = (pubkey33[0] == 0x03);
        memcpy(fiat,&pubkey33[1],4);
        iguana_rwnum(rwflag,&pubkey33[4],sizeof(*fiatoshisp),(void *)fiatoshisp);
        if ( *shortflagp != 0 )
            *fiatoshisp = -(*fiatoshisp);
        *addrtypep = pubkey33[12];
        memcpy(rmd160,&pubkey33[13],20);
    }
    return(33);
}

double PAX_val(uint32_t pval,int32_t baseid)
{
    //printf("PAX_val baseid.%d pval.%u\n",baseid,pval);
    if ( baseid >= 0 && baseid < MAX_CURRENCIES )
        return(((double)pval / 1000000000.) / MINDENOMS[baseid]);
    return(0.);
}

void komodo_pvals(int32_t height,uint32_t *pvals,uint8_t numpvals)
{
    int32_t i,nonz; uint32_t kmdbtc,btcusd,cnyusd; double KMDBTC,BTCUSD,CNYUSD;
    if ( numpvals >= 35 )
    {
        for (nonz=i=0; i<32; i++)
        {
            if ( pvals[i] != 0 )
                nonz++;
            //printf("%u ",pvals[i]);
        }
        if ( nonz == 32 )
        {
            kmdbtc = pvals[i++];
            btcusd = pvals[i++];
            cnyusd = pvals[i++];
            KMDBTC = ((double)kmdbtc / (1000000000. * 1000.));
            BTCUSD = ((double)btcusd / (1000000000. / 1000.));
            CNYUSD = ((double)cnyusd / 1000000000.);
            PVALS = (uint32_t *)realloc(PVALS,(NUM_PRICES+1) * sizeof(*PVALS) * 36);
            PVALS[36 * NUM_PRICES] = height;
            memcpy(&PVALS[36 * NUM_PRICES + 1],pvals,sizeof(*pvals) * 35);
            NUM_PRICES++;
            if ( 0 )
                printf("OP_RETURN.%d KMD %.8f BTC %.6f CNY %.6f NUM_PRICES.%d (%llu %llu %llu)\n",height,KMDBTC,BTCUSD,CNYUSD,NUM_PRICES,(long long)kmdbtc,(long long)btcusd,(long long)cnyusd);
        }
    }
}

int32_t komodo_baseid(char *origbase)
{
    int32_t i; char base[64];
    for (i=0; origbase[i]!=0&&i<sizeof(base); i++)
        base[i] = toupper((int32_t)(origbase[i] & 0xff));
    base[i] = 0;
    for (i=0; i<=MAX_CURRENCIES; i++)
        if ( strcmp(CURRENCIES[i],base) == 0 )
            return(i);
    printf("illegal base.(%s) %s\n",origbase,base);
    return(-1);
}

uint64_t komodo_paxcalc(uint32_t *pvals,int32_t baseid,int32_t relid,uint64_t basevolume)
{
    uint32_t pvalb,pvalr,kmdbtc,btcusd; uint64_t usdvol,baseusd,usdkmd,baserel,ranked[32];
    if ( basevolume > 1000000*COIN )
        return(0);
    if ( (pvalb= pvals[baseid]) != 0 )
    {
        if ( relid == MAX_CURRENCIES )
        {
            kmdbtc = pvals[MAX_CURRENCIES];
            btcusd = pvals[MAX_CURRENCIES + 1];
            if ( pvals[USD] != 0 && kmdbtc != 0 && btcusd != 0 )
            {
                baseusd = ((uint64_t)pvalb * 1000000000) / pvals[USD];
                usdvol = komodo_paxvol(basevolume,baseusd) / MINDENOMS[baseid];
                usdkmd = ((uint64_t)btcusd * 1000000000) / kmdbtc;
                //printf("base -> USD %llu, BTC %llu KMDUSD %llu\n",(long long)baseusd,(long long)btcusd,(long long)kmdusd);
                printf("usdkmd.%llu basevolume.%llu baseusd.%llu paxvol.%llu usdvol.%llu\n",(long long)usdkmd,(long long)basevolume,(long long)baseusd,(long long)komodo_paxvol(basevolume,baseusd),(long long)usdvol);
                return(MINDENOMS[USD] * komodo_paxvol(usdvol,usdkmd));
            }
        }
        else if ( baseid == relid )
        {
            if ( baseid != MAX_CURRENCIES )
            {
                pax_rank(ranked,pvals);
                return(10 * ranked[baseid]); // map to percentage
            }
        }
        else if ( (pvalr= pvals[relid]) != 0 )
        {
            baserel = ((uint64_t)pvalb * 1000000000) / pvalr;
            return(komodo_paxvol(basevolume,baserel));
        }
    }
    return(0);
}

uint64_t komodo_paxprice(int32_t height,char *base,char *rel,uint64_t basevolume)
{
    int32_t baseid=-1,relid=-1,i; uint32_t *ptr;
    if ( (baseid= komodo_baseid(base)) >= 0 && (relid= komodo_baseid(rel)) >= 0 )
    {
        for (i=NUM_PRICES-1; i>=0; i--)
        {
            ptr = &PVALS[36 * i];
            if ( *ptr < height )
                return(komodo_paxcalc(&ptr[1],baseid,relid,basevolume));
        }
    } else printf("paxprice invalid base.%s %d, rel.%s %d\n",base,baseid,rel,relid);
    return(0);
}

int32_t komodo_paxprices(int32_t *heights,uint64_t *prices,int32_t max,char *base,char *rel)
{
    int32_t baseid=-1,relid=-1,i,num = 0; uint32_t *ptr;
    if ( (baseid= komodo_baseid(base)) >= 0 && (relid= komodo_baseid(rel)) >= 0 )
    {
        for (i=NUM_PRICES-1; i>=0; i--)
        {
            ptr = &PVALS[36 * i];
            heights[num] = *ptr;
            prices[num] = komodo_paxcalc(&ptr[1],baseid,relid,COIN);
            num++;
            if ( num >= max )
                return(num);
        }
    }
    return(num);
}

void komodo_paxpricefeed(int32_t height,uint8_t *pricefeed,int32_t opretlen)
{
    double KMDBTC,BTCUSD,CNYUSD; uint32_t numpvals,timestamp,pvals[128]; uint256 zero;
    numpvals = dpow_readprices(pricefeed,&timestamp,&KMDBTC,&BTCUSD,&CNYUSD,pvals);
    memset(&zero,0,sizeof(zero));
    komodo_stateupdate(height,0,0,0,zero,0,0,pvals,numpvals,0,0,0,0);
    printf("komodo_paxpricefeed vout OP_RETURN.%d prices numpvals.%d opretlen.%d\n",height,numpvals,opretlen);
}

uint64_t PAX_fiatdest(char *destaddr,uint8_t pubkey33[33],char *coinaddr,int32_t height,char *origbase,int64_t fiatoshis)
{
    uint8_t shortflag = 0; char base[4]; int32_t i; uint8_t addrtype,rmd160[20]; uint64_t komodoshis = 0;
    fiatbuf[0] = 0;
    if ( strcmp(base,(char *)"KMD") == 0 || strcmp(base,(char *)"kmd") == 0 )
        return(0);
    for (i=0; i<3; i++)
        base[i] = toupper(origbase[i]);
    base[i] = 0;
    if ( fiatoshis < 0 )
        shortflag = 1, fiatoshis = -fiatoshis;
    komodoshis = komodo_paxprice(height,base,(char *)"KMD",(uint64_t)fiatoshis);
    if ( bitcoin_addr2rmd160(&addrtype,rmd160,coinaddr) == 20 )
    {
        PAX_pubkey(1,pubkey33,&addrtype,rmd160,base,&shortflag,&fiatoshis);
        bitcoin_address(destaddr,KOMODO_PUBTYPE,pubkey33,33);
    }
    return(komodoshis);
}
