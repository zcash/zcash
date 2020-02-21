/******************************************************************************
 * Copyright © 2014-2020 The SuperNET Developers.                             *
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

#define DEXP2P_CHAIN ((char *)"DPOW")
#define DEXP2P_PUBKEYS ((char *)"pubkeys")

#include "dappinc.h"

#define NOTARIZATION_TIME 300   // minimum every 5 minutes
#define _NOTARIZATION_BLOCKS 10

struct dpowentry { bits256 ntzhash; uint32_t timestamp; int32_t height,ind; char payload[81]; } NN[64];

int32_t Num_authorized,NOTARIZATION_BLOCKS = _NOTARIZATION_BLOCKS;
cJSON *NOTARIZER_json;
char *Authorized[64][3];

uint64_t Rvals[0x100] =
{
    0xb4f898f421692acc, 0x2b2c0c298ef91317, 0x7108a286fc7fea53, 0xa73efcda0be414aa, 0x58d0c1401bc8a727, 0x282a13d5fee6c9d5, 0xf6f62e4c8425aafd, 0xfec440d71d02da42, 0x2380ea4e281a8ce8, 0x79b24b7b892167b4, 0x4939f30642fdd8f5, 0x64a32b7ca5673bcd, 0x5c2a0da8078cbdb8, 0x8d699897efe54b74, 0x27c1095a77cf6670, 0xd257054c0cc2844, 0x34323ebfd614b22d, 0x21af8cb02123188a, 0xcd7559884d987ff3, 0xe0e3b3b2d68e4281, 0xc058e83624b6f765, 0xfdd29109b1e17c00, 0x4665c0c4984a6b10, 0xd9600a7b51df403, 0x37ee8b362e324df8, 0x710fbb6111e8d008, 0x1d32701aeb0a5a26, 0x48196a4f0cd34f64, 0xdf2665d7ee764cb7, 0xf10900ca97f10e36, 0xe9930e8475dc145b, 0x89f60fd1aef5675e, 0x9129ed233786d5b, 0x5f69aa0477a103ad, 0xb32d695444d545bf, 0x8f2c5733a26123d0, 0xf0a1d44397c2a4c5, 0xac3cd1eddd6842d8, 0x4c1f1e7b6cf01217, 0x9d041bd875ee8041, 0x73f4d69a0335c6c0, 0x45b18d2e05505c18, 0x267d0c01bb1a29f, 0x31c67b7129775651, 0xea5029d2dc35fb41, 0xcef9a12eb4f0a185, 0xcc07507b05e9ab44, 0x3d4abcfb18f67fa7, 0x2f746aa98a1a7fb8, 0x4b08dc063ae3640b, 0xe9d058420350f187, 0x3b104d83d302c6d8, 0xa1a53a9575c52acc, 0xecf1e364a6db889f, 0x7202c19a18f042d5, 0x1e4656510a460487, 0xe5b8394617940b81, 0x4d137d77268b93c1, 0x4ccd67c55a6504bf, 0xebdc9e6abe57136c, 0x6cd5bbaaf00271a9, 0x9d13a6dec692614e, 0x4dbcc5e06ff879aa, 0x5c2ee8b2420b14d8, 0x3b7022ec044c3159, 0x540014aa62010383, 0x95bc80bd6ec4f88d, 0xaa307b047d8c795, 0x96607f12250eef38, 0x482f8772df86283, 0x5d442fc7ae7347f1, 0x5db14724a7a41df7, 0xc3e0e43f4a83c037, 0xc2c273d1bdf1d846, 0x5cdfb665827135bb, 0x456b5ccf80484d4, 0x5792ebeb54eda76, 0x999b85ded8c36a07, 0x977dfcc3211c0dbb, 0xae51e7374d908280, 0xd486ddcd80639fc1, 0x8abf48ce26ad4947, 0x59b129d819abdc56, 0xeb91f729e3a642ab, 0x1a3af2d3656cf597, 0x869401300a41e73b, 0x160f516ab6a040dd, 0x45a98caebdfab694, 0x4270e40755aa1582, 0x9083e1b3fc4cb1cb, 0x9a356758d462322, 0xad00702abbc79e0d, 0x5bcf728f8c02aa85, 0x2068dafed7571c24, 0x137c510573adaffe, 0x39c7ceb31ace43ef, 0x6164953c08c6c979, 0x94d0a196af39bbb1, 0xbf6984cf56077e50, 0x80b5a0067cd938c8, 0xc309ed11a7897b6a, 0x35d443e4407342a9, 0x2950f560918cdcc2, 0x35b55acb35a52a2e, 0xd6abe62ce7dd3fd6, 0x2442eb620c161e28, 0xf25107c428b6cec7, 0x2732dc51fc27f631, 0x1f1fbaf6480f101b, 0xe8616e203c2c35d9, 0x51f2611f0010173c, 0x12d180f6aa4e1958, 0x76a67a9c875ae191, 0x7eb81341d0b2d3af, 0x839b712a4a7ec82b, 0xfb93ff6a812de98a, 0x18f32a687c83eee0, 0xe5bda0b93fe8a5fd, 0x3717ccac93303b69, 0x241c4c4320b844b6, 0xc8bb94caa3a0a03a, 0xaed8c544f307a439, 0x8ae559d4ad420800, 0x2505c8eae1ab9e9d, 0x74fb853a33c9a668, 0xc91c40c86e670329, 0x56a85cb81d775f48, 0x850b3d1ce53754fb, 0x8492b45a1eb8d5e3, 0x82490f3a80f2f9b7, 0x187d481d27a0c06e, 0xd73e0af3b9feb59d, 0x62be87ab3af5f7df, 0x2f3e2cc1bbe3b181, 0xc522d4280a57dfed, 0xb55bd2b7e7e218a, 0x54c204d25f454ab4, 0xbfa64ed27610a5b5, 0x5d1becd256c9fd2e, 0xc44305839db9a0e, 0x44a12f8e4d6c8a7b, 0x2e676100a7bab1d5, 0x2c8da01e3084315e, 0x14a0aa999365693b, 0x1208033cc3620c35, 0x82e32123b5bac2b5, 0x449d400971b26852, 0xe35878ae43d803aa, 0x73d3afbec1a7ba85, 0x2180daf96298d72, 0xb6c4110c7f73cb75, 0x99a65214c2fa9c14, 0x703f2efdcb5a4e0d, 0xeb6e1f756b0668bc, 0x7ad2ff2f66be1ea, 0x77970f9dfcaa7cb, 0xbf2d1202bdd2d4bf, 0x3ddb9953f02b347a, 0x4175e1756e34467b, 0x708999b182204089, 0xe9305a6e702d5c6e, 0x7020b5f4e2d95b8e, 0xbfb2543671de54fb, 0xdbb5cc6d4542d394, 0xcc817c3d984ce329, 0xb3dce10ccae5ad7, 0x4192f705a7d1b23, 0x524c0029704a5c03, 0xedee6b1521ea98e3, 0x8fe2f76cd6b99dc6, 0x9387048f82ea5f12, 0xb5db50d28904d160, 0xf8ecc532fed6c6e9, 0xaf5eb29dcecfa662, 0xad34ba4ca1324811, 0x731312895e36388c, 0x12035eaf0b71ead8, 0x97513e85a1e0d304, 0xca816e3ed3398387, 0xa3a33cca3028b7a6, 0x77e8b273d6ae1526, 0x8fa36a080b18c985, 0xf1f6e44a2c63dced, 0x810ac55a66221e9c, 0xf0c6406a4e58b8db, 0x37a19f4911fbdf09, 0x85b859e9c663047c, 0x7c55f6a0d3ecdb78, 0xf70991ee19caadae, 0x21f1aba5b9090570, 0x271fa0ae99e854af, 0xb19851021cfb0b7b, 0x65e4d4f5ecca63fe, 0x48a010997f1feeda, 0xa53b012914e28865, 0x86c05b880fc2370d, 0x3a19575fb4738bbf, 0xaf275949deb93945, 0x18b1fe0b0cc30ae2, 0x8e44e8cfbe287336, 0x550e5c0fd442b874, 0x48e5c21df34c795, 0x878ac15171105266, 0xf938a9b0445b235, 0xc7a8f13252e3d542, 0x9fe03639dba7ddb9, 0xe8619cb2d911f088, 0x74f98031eaeda64e, 0x88506fce88c6dd56, 0xb625442d8564f74c, 0x22cff1d4e7903734, 0x933d063dc90dbc98, 0xc03b6c73b21c04e3, 0x19486be491459f63, 0x639acfca7700d8a3, 0x915d6aadc92ca78c, 0x49a37fe5e344796f, 0xf71c245403dbe81f, 0x782a09ecc76f1cfd, 0x31f0a0c25e4257b1, 0xd249b7b3ff143419, 0x46edf249f5d625a0, 0x17743966fb0e5c0e, 0x718466575376b690, 0xf121b9504e70999b, 0xf86ed3ea28e6f7de, 0x47ae68b696f47d30, 0x26e71f8a769a241f, 0xf0fa9612097458b8, 0xe67187b57819e18e, 0xec022acdbb7c6504, 0x74004abbda639c4f, 0x9aa5770bb47d75a2, 0x4bf583460e12be59, 0xeba3099cb60771e8, 0xf3d5c55061c506a6, 0x24e04ecb46a8533a, 0x5fa028765433f30d, 0x5722b9b03716a79a, 0xe9048eae10b8e8bf, 0x8d110fbad30ade2, 0xc8dafc2e7a5c0503, 0x69ac22a955fff0a4, 0xd6a37ef3b97a650b, 0xe72f55e32783d32b, 0x2d513c8988628c5a, 0x7e402e73d782512d, 0x665ddb3a8738bb93, 0x917abd36928de1ae, 0x46016b19d31adc4a, 0x46af5eb2301e84bc, 0x72bb2cc354cee71a, 0x50d57f064404480d, 0x18bc255b7623ef5c, 0x54a3f8395c49daa9, 0xac15649ea3a871df
};

// issue ./komodod -ac_name=DPOW -handle=xxx -dexp2p=2 -addnode=136.243.58.134 -pubkey=02/03... &
// add blocknotify=notarizer KMD "" %s
// add blocknotify=notarizer ASSETCHAIN "" %s
// add blocknotify=notarizer BTC "bitcoin-cli" %s
// add blocknotify=notarizer 3rdparty "3rdparty-cli" %s
// build notarizer and put in path: gcc cc/dapps/notarizer.c -lm -o notarizer; cp notarizer /usr/bin

int32_t dpow_hashind(char *coin,char *handle,int32_t ntzheight)
{
    uint8_t hbuf[128]; uint64_t rval=0; int32_t i,x,n=0;
    memset(hbuf,0,sizeof(hbuf));
    for (i=0; coin[i]!=0; i++)
        hbuf[n++] = coin[i];
    hbuf[n++] = 0;
    for (i=0; handle[i]!=0; i++)
        hbuf[n++] = handle[i];
    hbuf[n++] = 0;
    x = ntzheight;
    for (i=0; x!=0; i++,x>>=1)
        if ( (x & 1) != 0 )
            hbuf[n++] = (ntzheight + i) % 97;
    hbuf[n++] = i;
    for (i=0; i<n; i++)
        rval ^= Rvals[hbuf[i]];
    return(rval % 9973);
}

void dpow_authorizedcreate(char *handle,char *secpstr)
{
    Authorized[Num_authorized][0] = clonestr(handle);
    Authorized[Num_authorized][1] = clonestr(secpstr);
    Num_authorized++;
}

int32_t dpow_authorizedupdate(char *coin)
{
    cJSON *retjson,*item,*array; char *tagB,*pubkey,*retstr,*pstr; int32_t i,j,n,m,retval = 0;
    if ( (retjson= get_komodocli((char *)"",&retstr,DEXP2P_CHAIN,"DEX_list","0","0","handles","","","","")) != 0 )
    {
        if ( (array= jarray(&n,retjson,"matches")) != 0 )
        {
            for (i=0; i<n; i++)
            {
                item = jitem(array,i);
                if ( (pubkey= jstr(item,"senderpub")) != 0 && (tagB= jstr(item,"tagB")) != 0 && (pstr= jstr(item,"decrypted")) != 0 && strlen(pstr) == 66 )
                {
                    for (j=0; j<Num_authorized; j++)
                    {
                        if ( strcmp(tagB,Authorized[j][0]) == 0 && strcmp(pstr,Authorized[j][1]) == 0 && Authorized[j][2] == 0 )
                            Authorized[j][2] = clonestr(pubkey);
                    }
                }
            }
        }
        free_json(retjson);
        for (j=m=0; j<Num_authorized; j++)
        {
            if ( Authorized[j][2] == 0 )
            {
                if ( strcmp(coin,"KMD") == 0 )
                    fprintf(stderr,"%s ",Authorized[j][0]);
                m++;
            }
        }
        if ( strcmp(coin,"KMD") == 0 )
            fprintf(stderr,"NOT registered %d, Num_authorized.%d\n",m,Num_authorized);
    }
    return(retval);
}


static int _candidates_sortcmp(const void *a,const void *b)
{
#define nn_a ((uint64_t *)a)
#define nn_b ((uint64_t *)b)
    if ( nn_a[0] > nn_b[0] )
        return(-1);
    else if ( nn_a[0] < nn_b[0] )
        return(1);
    else
    {
        if ( nn_a[1] > nn_b[1] )
            return(-1);
        else if ( nn_a[1] < nn_b[1] )
            return(1);
    }
    return(0);
#undef nn_a
#undef nn_b
}

static int _NN_sortcmp(const void *a,const void *b)
{
#define nn_a ((struct dpowentry *)a)
#define nn_b ((struct dpowentry *)b)
    int32_t i;
    if ( nn_a->height > nn_b->height )
        return(-1);
    else if ( nn_a->height < nn_b->height )
        return(1);
    else
    {
        if ( nn_a->timestamp > nn_b->timestamp )
            return(-1);
        else if ( nn_a->timestamp < nn_b->timestamp )
            return(1);
        else
        {
            for (i=0; i<4; i++)
            {
                if ( nn_a->ntzhash.ulongs[i] >  nn_b->ntzhash.ulongs[i] )
                    return(-1);
                else if (  nn_a->ntzhash.ulongs[i] <  nn_b->ntzhash.ulongs[i] )
                    return(1);
            }
        }
    }
    return(0);
#undef nn_a
#undef nn_b
}

int32_t dpow_roundproposal(char *coin)
{
    uint8_t buf[4]; int32_t i,n,ntzheight,cmpind=-1,match0=0,matchB=0,cmpB=-1; uint64_t candidates[64][2]; char str[65],payload[65];
    for (i=n=0; i<Num_authorized; i++)
    {
        NN[n].ind = i;
        //fprintf(stderr,"%s %s %s\n",Authorized[i][0],Authorized[i][1],Authorized[i][2]!=0?Authorized[i][2]:"");
        if ( Authorized[i][0] != 0 && Authorized[i][1] != 0 && Authorized[i][2] != 0 && dpow_getmessage(NN[n].payload,sizeof(NN[n].payload),coin,"rounds",Authorized[i][2]) > 0 )
        {
            decode_hex(NN[n].ntzhash.bytes,32,NN[n].payload);
            decode_hex(buf,4,NN[n].payload + 32*2);
            NN[n].height = ((int32_t)buf[3] + ((int32_t)buf[2] << 8) + ((int32_t)buf[1] << 16) + ((int32_t)buf[0] << 24));
            decode_hex(buf,4,NN[n].payload + 32*2+8);
            NN[n].timestamp = ((int32_t)buf[3] + ((int32_t)buf[2] << 8) + ((int32_t)buf[1] << 16) + ((int32_t)buf[0] << 24));
            //fprintf(stderr,"%s.%d t.%u %s\n",coin,NN[n].height,NN[n].timestamp,Authorized[i][0]);
            n++;
        }
    }
    if ( n >= 13 )
    {
        qsort(NN,n,sizeof(NN[n]),_NN_sortcmp);
        for (i=0; i<n; i++)
        {
            //fprintf(stderr,"%-2d h.%d t.%u %s %s\n",NN[i].ind,NN[i].height,NN[i].timestamp,bits256_str(str,NN[i].ntzhash),Authorized[NN[i].ind][0]);
            if ( i > 0 )
            {
                if ( strcmp(NN[i].payload,NN[0].payload) == 0 )
                    match0++;
                else if ( cmpB < 0 )
                    cmpB = i;
                if ( cmpB >= 0 && i != cmpB && strcmp(NN[i].payload,NN[cmpB].payload) == 0 )
                    matchB++;
            }
        }
        fprintf(stderr,"%s num.%d match0.%d cmpB.%d matchB.%d\n",coin,n,match0,cmpB,matchB);
        if ( matchB > match0 && matchB > 33 )
            cmpind = cmpB;
        else if ( match0 > 33 )
            cmpind = 0;
        if ( cmpind >= 0 ) // search for all registered notaries with matching hash
        {
            ntzheight = NN[cmpind].height;
            sprintf(str,"%u",ntzheight);
            for (i=n=0; i<Num_authorized; i++)
            {
                //fprintf(stderr,"%s %s %s\n",Authorized[i][0],Authorized[i][1],Authorized[i][2]!=0?Authorized[i][2]:"");
                if ( Authorized[i][0] != 0 && Authorized[i][1] != 0 && Authorized[i][2] != 0 && dpow_getmessage(payload,sizeof(payload),coin,str,Authorized[i][2]) > 0 )
                {
                    if ( strncmp(payload,NN[cmpind].payload,strlen(payload)) == 0 )
                    {
                        candidates[n][1] = i;
                        candidates[n][0] = dpow_hashind(coin,Authorized[i][0],ntzheight);
                        n++;
                    }
                }
            }
            qsort(candidates,n,sizeof(candidates[n]),_candidates_sortcmp);
            for (i=0; i<13; i++)
                fprintf(stderr,"%s ",Authorized[candidates[i][1]][0]);
            fprintf(stderr,"h.%d t.%u %s signers\n",NN[i].height,NN[i].timestamp,bits256_str(str,NN[i].ntzhash));
            return(0);
        }
    } else fprintf(stderr,"%s only has num.%d\n",coin,n);
    return(-1);
}

void dpow_hashind_test(int32_t *histo,char *coin,int32_t height)
{
    int32_t i,n; uint64_t candidates[64][2];
    memset(candidates,0,sizeof(candidates));
    for (i=n=0; i<Num_authorized; i++)
    {
        if ( Authorized[i][0] != 0 )
        {
            candidates[n][1] = i;
            candidates[n][0] = dpow_hashind(coin,Authorized[i][0],height);
            n++;
        }
    }
    qsort(candidates,n,sizeof(candidates[n]),_candidates_sortcmp);
    for (i=0; i<13; i++)
        histo[candidates[i][1]]++;
}

int32_t main(int32_t argc,char **argv)
{
    int32_t i,n,height,nextheight,priority=8; char *coin,*handle,*secpstr,*pubkeys,*kcli,*hashstr,*acname=(char *)""; cJSON *retjson,*item,*authorized; bits256 blockhash; long fsize; uint32_t heighttime; char checkstr[65],str[65],str2[65];
    srand((int32_t)time(NULL));
    if ( (pubkeys= filestr(&fsize,DEXP2P_PUBKEYS)) == 0 )
    {
        fprintf(stderr,"cant load %s file\n",DEXP2P_PUBKEYS);
        exit(-1);
    }
    if ( (NOTARIZER_json= cJSON_Parse(pubkeys)) == 0 )
    {
        fprintf(stderr,"cant parse notarize json file (%s)\n",pubkeys);
        exit(-1);
    }
    if ( (authorized= jarray(&n,NOTARIZER_json,"authorized")) != 0 )
    {
        for (i=0; i<n; i++)
        {
            item = jitem(authorized,i);
            handle = jfieldname(item);
            secpstr = jstr(item,handle);
            dpow_authorizedcreate(handle,secpstr);
        }
    }
    if ( argc == 4 )
    {
        if ( dpow_pubkey() < 0 )
        {
            fprintf(stderr,"couldnt set pubkey for dPoW\n");
            return(-1);
        }
        coin = (char *)argv[1];
        if ( argv[2][0] != 0 )
            REFCOIN_CLI = (char *)argv[2];
        else
        {
            if ( strcmp(coin,"KMD") != 0 )
                acname = coin;
        }
        hashstr = (char *)argv[3];
        height = get_coinheight(coin,acname,&blockhash);
        get_coinmerkleroot(coin,acname,blockhash,&heighttime);
        bits256_str(checkstr,blockhash);
        if ( strcmp(checkstr,hashstr) == 0 )
        {
            fprintf(stderr,"%s: (%s) %s height.%d\n",coin,REFCOIN_CLI!=0?REFCOIN_CLI:"",checkstr,height);
            if ( (retjson= dpow_ntzdata(coin,priority,height,blockhash)) != 0 )
                free_json(retjson);
        } else fprintf(stderr,"coin.%s (%s) %s vs %s, height.%d\n",coin,REFCOIN_CLI!=0?REFCOIN_CLI:"",checkstr,hashstr,height);
        if ( strcmp("BTC",coin) != 0 )
        {
            bits256 prevntzhash,ntzhash,checkhash,chainhash; int32_t h,prevntzheight=0,ntzheight=0; uint32_t nexttime=0,ntztime=0,t,prevntztime=0; char hexstr[81]; cJSON *retjson2;
            dpow_pubkeyregister(priority);
            dpow_authorizedupdate(coin);
            if ( 0 )
            {
                int32_t z,minh,maxh,histo[64]; char *coinlist[] = { "DEX", "KMD", "SUPERNET", "BOTS", "BET", "HODL", "CRYPTO", "HUSH", "PIRATE" };
                memset(histo,0,sizeof(histo));
                for (z=0; z<sizeof(coinlist)/sizeof(*coinlist); z++)
                {
                    for (h=128; h<70000; h++)
                        dpow_hashind_test(histo,coinlist[z],h);
                    for (i=0; i<64; i++)
                        fprintf(stderr,"%d ",histo[i]);
                    fprintf(stderr,"%s histogram\n",coinlist[z]);
                }
                maxh = 0;
                minh = (1 << 30);
                for (z=0; z<64; z++)
                {
                    if ( histo[z] < minh )
                        minh = histo[z];
                    if ( histo[z] > maxh )
                        maxh = histo[z];
                }
                fprintf(stderr,"minh.%d vs maxh.%d\n",minh,maxh);
            }
            prevntzhash = dpow_ntzhash(coin,&prevntzheight,&prevntztime);
            if ( (retjson= get_getinfo(coin,acname)) != 0 )
            {
                ntzheight = juint(retjson,"notarized");
                ntzhash = jbits256(retjson,"notarizedhash");
                if ( ntzheight > prevntzheight )
                {
                    get_coinmerkleroot(coin,acname,ntzhash,&ntztime);
                    fprintf(stderr,"NOTARIZATION %s.%d %s t.%u\n",coin,ntzheight,bits256_str(str,ntzhash),ntztime);
                    bits256_str(hexstr,ntzhash);
                    sprintf(&hexstr[64],"%08x",ntzheight);
                    sprintf(&hexstr[72],"%08x",ntztime);
                    hexstr[80] = 0;
                    if ( (retjson2= dpow_broadcast(priority,hexstr,coin,"notarizations",DPOW_pubkeystr,"","")) != 0 )
                        free_json(retjson2);
                }
                else if ( ntzheight == prevntzheight )
                {
                    if ( memcmp(&prevntzhash,&ntzhash,32) != 0 )
                        fprintf(stderr,"NTZ ERROR %s.%d %s != %s\n",coin,ntzheight,bits256_str(str,ntzhash),bits256_str(str2,prevntzhash));
                    else if ( height > ntzheight )
                    {
                        for (h=height; h>ntzheight && h>height-100; h--)
                        {
                            checkhash = dpow_blockhash(coin,h);             // from network
                            chainhash = get_coinblockhash(coin,acname,h);   // from blockchain
                            if ( memcmp(&checkhash,&chainhash,sizeof(checkhash)) != 0 )
                            {
                                fprintf(stderr,"%s.%d: chainhash.%s != %s, must have  been reorged\n",coin,h,bits256_str(str,chainhash),bits256_str(str2,checkhash));
                                if ( (retjson2= dpow_ntzdata(coin,priority,h,chainhash)) != 0 )
                                    free_json(retjson2);
                            }
                        }
                    }
                }
                free_json(retjson);
                if ( strcmp("KMD",coin) != 0 )
                    NOTARIZATION_BLOCKS = 1;
                nextheight = ntzheight + NOTARIZATION_BLOCKS - (ntzheight % NOTARIZATION_BLOCKS);
                if ( nextheight < height - NOTARIZATION_BLOCKS/2 )
                {
                    nexttime = get_heighttime(coin,acname,nextheight);
                    if ( (time(NULL) - nexttime) > 2*NOTARIZATION_TIME ) // find a more recent block
                    {
                        for (i=NOTARIZATION_BLOCKS; nextheight+i < height-NOTARIZATION_BLOCKS/2 - 1; i+=NOTARIZATION_BLOCKS)
                        {
                            t =  get_heighttime(coin,acname,nextheight+i);
                            if ( 0 && NOTARIZATION_BLOCKS == 1 )
                                fprintf(stderr,"%s nextheight.%d lag.%d\n",coin,nextheight+i,(int32_t)(time(NULL) - t));
                            if ( (time(NULL) - t) < 3*NOTARIZATION_TIME/2 )
                            {
                                nextheight += i;
                                nexttime = t;
                                break;
                            }
                        }
                    }
                    // check ongoing rounds and prevent too close ones
                    if ( time(NULL) > nexttime + NOTARIZATION_TIME && height > nextheight+NOTARIZATION_BLOCKS/2 && time(NULL) > prevntztime+NOTARIZATION_TIME && nexttime > prevntztime+NOTARIZATION_TIME )
                    {
                        //if ( nexttime - dpow_roundpending(coin,acname) > NOTARIZATION_TIME )
                        {
                            checkhash = dpow_blockhash(coin,nextheight);
                            chainhash = get_coinblockhash(coin,acname,nextheight);
                            if ( memcmp(&checkhash,&chainhash,sizeof(checkhash)) == 0 )
                            {
                                bits256_str(hexstr,chainhash);
                                sprintf(&hexstr[64],"%08x",nextheight);
                                sprintf(&hexstr[72],"%08x",nexttime);
                                hexstr[80] = 0;
                                if ( dpow_hasmessage(hexstr,coin,"rounds",DPOW_pubkeystr) == 0 )
                                {
                                    if ( (retjson2= dpow_broadcast(priority,hexstr,coin,(char *)"rounds",DPOW_pubkeystr,"","")) != 0 )
                                    {
                                        free_json(retjson2);
                                        fprintf(stderr,"start notarization for %s.%d when ht.%d prevntz.%d\n",coin,nextheight,height,ntzheight);
                                        //if ( (retjson2= dpow_notarize(coin,nextheight)) != 0 )
                                        //    free_json(retjson2);
                                    }
                                }
                                for (i=0; i<3; i++)
                                {
                                    if ( dpow_roundproposal(coin) == 0 )
                                        break;
                                    sleep(3);
                                }
                                if ( i == 3 )
                                    fprintf(stderr,"no consensus\n");
                            } else fprintf(stderr,"%s.%d: chainhash.%s != %s, must have  been reorged\n",coin,nextheight,bits256_str(str,chainhash),bits256_str(str2,checkhash));
                        }
                    }
                }
            }
        }
    }
    return(0);
}

