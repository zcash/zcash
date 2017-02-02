/******************************************************************************
 * Copyright © 2014-2017 The SuperNET Developers.                             *
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

#define USD 0

#define MAX_CURRENCIES 32
extern char CURRENCIES[][8];

uint64_t M1SUPPLY[] = { 3317900000000, 6991604000000, 667780000000000, 1616854000000, 331000000000, 861909000000, 584629000000, 46530000000, // major currencies
    45434000000000, 16827000000000, 3473357229000, 306435000000, 27139000000000, 2150641000000, 347724099000, 1469583000000, 749543000000, 1826110000000, 2400434000000, 1123925000000, 3125276000000, 13975000000000, 317657000000, 759706000000000, 354902000000, 2797061000000, 162189000000, 163745000000, 1712000000000, 39093000000, 1135490000000000, 80317000000,
    100000000 };

#define MIND 1000
uint32_t MINDENOMS[] = { MIND, MIND, 100*MIND, MIND, MIND, MIND, MIND, MIND, // major currencies
    10*MIND, 100*MIND, 10*MIND, MIND, 100*MIND, 10*MIND, MIND, 10*MIND, MIND, 10*MIND, 10*MIND, 10*MIND, 10*MIND, 100*MIND, MIND, 1000*MIND, MIND, 10*MIND, MIND, MIND, 10*MIND, MIND, 10000*MIND, 10*MIND, // end of currencies
10*MIND,
};

int32_t Peggy_inds[539] = {289, 404, 50, 490, 59, 208, 87, 508, 366, 288, 13, 38, 159, 440, 120, 480, 361, 104, 534, 195, 300, 362, 489, 108, 143, 220, 131, 244, 133, 473, 315, 439, 210, 456, 219, 352, 153, 444, 397, 491, 286, 479, 519, 384, 126, 369, 155, 427, 373, 360, 135, 297, 256, 506, 322, 425, 501, 251, 75, 18, 420, 537, 443, 438, 407, 145, 173, 78, 340, 240, 422, 160, 329, 32, 127, 128, 415, 495, 372, 522, 60, 238, 129, 364, 471, 140, 171, 215, 378, 292, 432, 526, 252, 389, 459, 350, 233, 408, 433, 51, 423, 19, 62, 115, 211, 22, 247, 197, 530, 7, 492, 5, 53, 318, 313, 283, 169, 464, 224, 282, 514, 385, 228, 175, 494, 237, 446, 105, 150, 338, 346, 510, 6, 348, 89, 63, 536, 442, 414, 209, 216, 227, 380, 72, 319, 259, 305, 334, 236, 103, 400, 176, 267, 355, 429, 134, 257, 527, 111, 287, 386, 15, 392, 535, 405, 23, 447, 399, 291, 112, 74, 36, 435, 434, 330, 520, 335, 201, 478, 17, 162, 483, 33, 130, 436, 395, 93, 298, 498, 511, 66, 487, 218, 65, 309, 419, 48, 214, 377, 409, 462, 139, 349, 4, 513, 497, 394, 170, 307, 241, 185, 454, 29, 367, 465, 194, 398, 301, 229, 212, 477, 303, 39, 524, 451, 116, 532, 30, 344, 85, 186, 202, 517, 531, 515, 230, 331, 466, 147, 426, 234, 304, 64, 100, 416, 336, 199, 383, 200, 166, 258, 95, 188, 246, 136, 90, 68, 45, 312, 354, 184, 314, 518, 326, 401, 269, 217, 512, 81, 88, 272, 14, 413, 328, 393, 198, 226, 381, 161, 474, 353, 337, 294, 295, 302, 505, 137, 207, 249, 46, 98, 27, 458, 482, 262, 253, 71, 25, 0, 40, 525, 122, 341, 107, 80, 165, 243, 168, 250, 375, 151, 503, 124, 52, 343, 371, 206, 178, 528, 232, 424, 163, 273, 191, 149, 493, 177, 144, 193, 388, 1, 412, 265, 457, 255, 475, 223, 41, 430, 76, 102, 132, 96, 97, 316, 472, 213, 263, 3, 317, 324, 274, 396, 486, 254, 205, 285, 101, 21, 279, 58, 467, 271, 92, 538, 516, 235, 332, 117, 500, 529, 113, 445, 390, 358, 79, 34, 488, 245, 83, 509, 203, 476, 496, 347, 280, 12, 84, 485, 323, 452, 10, 146, 391, 293, 86, 94, 523, 299, 91, 164, 363, 402, 110, 321, 181, 138, 192, 469, 351, 276, 308, 277, 428, 182, 260, 55, 152, 157, 382, 121, 507, 225, 61, 431, 31, 106, 327, 154, 16, 49, 499, 73, 70, 449, 460, 187, 24, 248, 311, 275, 158, 387, 125, 67, 284, 35, 463, 190, 179, 266, 376, 221, 42, 26, 290, 357, 268, 43, 167, 99, 374, 242, 156, 239, 403, 339, 183, 320, 180, 306, 379, 441, 20, 481, 141, 77, 484, 69, 410, 502, 172, 417, 118, 461, 261, 47, 333, 450, 296, 453, 368, 359, 437, 421, 264, 504, 281, 270, 114, 278, 56, 406, 448, 411, 521, 418, 470, 123, 455, 148, 356, 468, 109, 204, 533, 365, 8, 345, 174, 370, 28, 57, 11, 2, 231, 310, 196, 119, 82, 325, 44, 342, 37, 189, 142, 222, 9, 54, };

uint64_t peggy_smooth_coeffs[sizeof(Peggy_inds)/sizeof(*Peggy_inds)] =	// numprimes.13
{
    962714545, 962506087, 962158759, 961672710, 961048151, 960285354, 959384649, 958346426, 957171134, // x.8
    955859283, 954411438, 952828225, 951110328, 949258485, 947273493, 945156207, 942907532, 940528434, // x.17
    938019929, 935383089, 932619036, 929728945, 926714044, 923575608, 920314964, 916933485, 913432593, // x.26
    909813756, 906078486, 902228342, 898264923, 894189872, 890004874, 885711650, 881311964, 876807614, // x.35
    872200436, 867492300, 862685110, 857780804, 852781347, 847688737, 842505000, 837232189, 831872382, // x.44
    826427681, 820900212, 815292123, 809605581, 803842772, 798005901, 792097186, 786118864, 780073180, // x.53
    773962395, 767788778, 761554609, 755262175, 748913768, 742511686, 736058231, 729555707, 723006417, // x.62
    716412665, 709776755, 703100984, 696387648, 689639036, 682857428, 676045100, 669204315, 662337327, // x.71
    655446378, 648533696, 641601496, 634651978, 627687325, 620709702, 613721256, 606724115, 599720386, // x.80
    592712154, 585701482, 578690411, 571680955, 564675105, 557674825, 550682053, 543698699, 536726645, // x.89
    529767743, 522823816, 515896658, 508988029, 502099660, 495233249, 488390461, 481572928, 474782249, // x.98
    468019988, 461287675, 454586804, 447918836, 441285195, 434687268, 428126409, 421603932, 415121117, // x.107
    408679208, 402279408, 395922888, 389610779, 383344175, 377124134, 370951677, 364827785, 358753406, // x.116
    352729449, 346756785, 340836251, 334968645, 329154729, 323395230, 317690838, 312042206, 306449955, // x.125
    300914667, 295436891, 290017141, 284655897, 279353604, 274110676, 268927490, 263804394, 258741701, // x.134
    253739694, 248798623, 243918709, 239100140, 234343077, 229647649, 225013957, 220442073, 215932043, // x.143
    211483883, 207097585, 202773112, 198510404, 194309373, 190169909, 186091877, 182075118, 178119452, // x.152
    174224676, 170390565, 166616873, 162903335, 159249664, 155655556, 152120688, 148644718, 145227287, // x.161
    141868021, 138566528, 135322401, 132135218, 129004542, 125929924, 122910901, 119946997, 117037723, // x.170
    114182582, 111381062, 108632643, 105936795, 103292978, 100700645, 98159238, 95668194, 93226942, // x.179
    90834903, 88491495, 86196126, 83948203, 81747126, 79592292, 77483092, 75418916, 73399150, // x.188
    71423178, 69490383, 67600142, 65751837, 63944844, 62178541, 60452305, 58765515, 57117547, // x.197
    55507781, 53935597, 52400377, 50901505, 49438366, 48010349, 46616844, 45257246, 43930951, // x.206
    42637360, 41375878, 40145912, 38946876, 37778185, 36639262, 35529533, 34448428, 33395384, // x.215
    32369842, 31371249, 30399057, 29452725, 28531717, 27635503, 26763558, 25915365, 25090413, // x.224
    24288196, 23508216, 22749980, 22013003, 21296806, 20600917, 19924870, 19268206, 18630475, // x.233
    18011231, 17410035, 16826458, 16260073, 15710466, 15177224, 14659944, 14158231, 13671694, // x.242
    13199950, 12742625, 12299348, 11869759, 11453500, 11050225, 10659590, 10281262, 9914910, // x.251
    9560213, 9216856, 8884529, 8562931, 8251764, 7950739, 7659571, 7377984, 7105706, // x.260
    6842471, 6588020, 6342099, 6104460, 5874861, 5653066, 5438844, 5231969, 5032221, // x.269
    4839386, 4653254, 4473620, 4300287, 4133059, 3971747, 3816167, 3666139, 3521488, // x.278
    3382043, 3247640, 3118115, 2993313, 2873079, 2757266, 2645728, 2538325, 2434919, // x.287
    2335380, 2239575, 2147382, 2058677, 1973342, 1891262, 1812325, 1736424, 1663453, // x.296
    1593311, 1525898, 1461118, 1398879, 1339091, 1281666, 1226519, 1173569, 1122736, // x.305
    1073944, 1027117, 982185, 939076, 897725, 858065, 820033, 783568, 748612, // x.314
    715108, 682999, 652233, 622759, 594527, 567488, 541597, 516808, 493079, // x.323
    470368, 448635, 427841, 407948, 388921, 370725, 353326, 336692, 320792, // x.332
    305596, 291075, 277202, 263950, 251292, 239204, 227663, 216646, 206130, // x.341
    196094, 186517, 177381, 168667, 160356, 152430, 144874, 137671, 130806, // x.350
    124264, 118031, 112093, 106437, 101050, 95921, 91039, 86391, 81968, // x.359
    77759, 73755, 69945, 66322, 62877, 59602, 56488, 53528, 50716, // x.368
    48043, 45505, 43093, 40803, 38629, 36564, 34604, 32745, 30980, // x.377
    29305, 27717, 26211, 24782, 23428, 22144, 20927, 19774, 18681, // x.386
    17646, 16665, 15737, 14857, 14025, 13237, 12491, 11786, 11118, // x.395
    10487, 9890, 9325, 8791, 8287, 7810, 7359, 6933, 6531, // x.404
    6151, 5792, 5453, 5133, 4831, 4547, 4278, 4024, 3785, // x.413
    3560, 3347, 3147, 2958, 2779, 2612, 2454, 2305, 2164, // x.422
    2032, 1908, 1791, 1681, 1577, 1480, 1388, 1302, 1221, // x.431
    1145, 1073, 1006, 942, 883, 827, 775, 725, 679, // x.440
    636, 595, 557, 521, 487, 456, 426, 399, 373, // x.449
    348, 325, 304, 284, 265, 248, 231, 216, 202, // x.458
    188, 175, 164, 153, 142, 133, 124, 115, 107, // x.467
    100, 93, 87, 81, 75, 70, 65, 61, 56, // x.476
    53, 49, 45, 42, 39, 36, 34, 31, 29, // x.485
    27, 25, 23, 22, 20, 19, 17, 16, 15, // x.494
    14, 13, 12, 11, 10, 9, 9, 8, 7, // x.503
    7, 6, 6, 5, 5, 5, 4, 4, 4, // x.512
    3, 3, 3, 3, 2, 2, 2, 2, 2, // x.521
    2, 2, 1, 1, 1, 1, 1, 1, 1, // x.530
    1, 1, 1, 1, 1, 1, 0, 0, // isum 100000000000
};

uint64_t komodo_maxallowed(int32_t baseid)
{
    uint64_t mult,val = COIN * (uint64_t)10000;
    if ( baseid < 0 || baseid >= 32 )
        return(0);
    if ( baseid < 10 )
        val *= 4;
    mult = MINDENOMS[baseid] / MIND;
    return(mult * val);
}

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
    uint32_t kmdbtc,btcusd,cnyusd; int32_t i,n,nonz,len = 0;
    if ( data[0] == 'P' && data[5] == 35 )
        data++;
    len += iguana_rwnum(0,&data[len],sizeof(uint32_t),(void *)timestampp);
    len += iguana_rwnum(0,&data[len],sizeof(uint32_t),(void *)&n);
    if ( n != 35 )
    {
        printf("dpow_readprices illegal n.%d\n",n);
        return(-1);
    }
    len += iguana_rwnum(0,&data[len],sizeof(uint32_t),(void *)&kmdbtc); // /= 1000
    len += iguana_rwnum(0,&data[len],sizeof(uint32_t),(void *)&btcusd); // *= 1000
    len += iguana_rwnum(0,&data[len],sizeof(uint32_t),(void *)&cnyusd);
    *KMDBTCp = ((double)kmdbtc / (1000000000. * 1000.));
    *BTCUSDp = ((double)btcusd / (1000000000. / 1000.));
    *CNYUSDp = ((double)cnyusd / 1000000000.);
    for (i=nonz=0; i<n-3; i++)
    {
        if ( pvals[i] != 0 )
            nonz++;
        //else if ( nonz != 0 )
        //    printf("pvals[%d] is zero\n",i);
        len += iguana_rwnum(0,&data[len],sizeof(uint32_t),(void *)&pvals[i]);
        //printf("%u ",pvals[i]);
    }
    /*if ( nonz < n-3 )
    {
        //printf("nonz.%d n.%d retval -1\n",nonz,n);
        return(-1);
    }*/
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
                    if ( dpow_readprices(&data[len],&timestamp,&KMDBTC,&BTCUSD,&CNYUSD,pvals) > 0 )
                    {
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
                    }
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
        memcpy(fiat,&pubkey33[1],3);
        fiat[3] = 0;
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
            portable_mutex_lock(&komodo_mutex);
            PVALS = (uint32_t *)realloc(PVALS,(NUM_PRICES+1) * sizeof(*PVALS) * 36);
            PVALS[36 * NUM_PRICES] = height;
            memcpy(&PVALS[36 * NUM_PRICES + 1],pvals,sizeof(*pvals) * 35);
            NUM_PRICES++;
            portable_mutex_unlock(&komodo_mutex);
            if ( 0 )
                printf("OP_RETURN.%d KMD %.8f BTC %.6f CNY %.6f NUM_PRICES.%d (%llu %llu %llu)\n",height,KMDBTC,BTCUSD,CNYUSD,NUM_PRICES,(long long)kmdbtc,(long long)btcusd,(long long)cnyusd);
        }
    }
}

uint64_t komodo_paxcorrelation(uint64_t *votes,int32_t numvotes,uint64_t seed)
{
    int32_t i,j,k,ind,zeroes,wt,nonz; int64_t delta; uint64_t lastprice,tolerance,den,densum,sum=0;
    for (sum=i=zeroes=nonz=0; i<numvotes; i++)
    {
        if ( votes[i] == 0 )
            zeroes++;
        else sum += votes[i], nonz++;
    }
    if ( nonz < (numvotes >> 2) )
        return(0);
    sum /= nonz;
    lastprice = sum;
    for (i=0; i<numvotes; i++)
    {
        if ( votes[i] == 0 )
            votes[i] = lastprice;
        else lastprice = votes[i];
    }
    tolerance = sum / 50;
    for (k=0; k<numvotes; k++)
    {
        ind = Peggy_inds[(k + seed) % numvotes];
        i = (int32_t)(ind % numvotes);
        wt = 0;
        if ( votes[i] != 0 )
        {
            for (j=0; j<numvotes; j++)
            {
                if ( votes[j] != 0 )
                {
                    if ( (delta= (votes[i] - votes[j])) < 0 )
                        delta = -delta;
                        if ( delta <= tolerance )
                        {
                            wt++;
                            if ( wt > (numvotes >> 1) )
                                break;
                        }
                }
            }
        }
        if ( wt > (numvotes >> 1) )
        {
            ind = i;
            for (densum=sum=j=0; j<numvotes; j++)
            {
                den = peggy_smooth_coeffs[j];
                densum += den;
                sum += (den * votes[(ind + j) % numvotes]);
                //printf("(%llu/%llu %.8f) ",(long long)sum,(long long)densum,(double)sum/densum);
            }
            if ( densum != 0 )
                sum /= densum;
            //sum = (sum * basevolume);
            //printf("paxprice seed.%llx sum %.8f densum %.8f\n",(long long)seed,dstr(sum),dstr(densum));
            break;
        }
    }
    return(sum);
}

uint64_t komodo_paxcalc(uint32_t *pvals,int32_t baseid,int32_t relid,uint64_t basevolume,uint64_t kmdbtc,uint64_t btcusd)
{
    uint32_t pvalb,pvalr; uint64_t usdvol,baseusd,usdkmd,baserel,ranked[32];
    if ( basevolume > KOMODO_PAXMAX )
    {
        printf("paxcalc overflow %.8f\n",dstr(basevolume));
        return(0);
    }
    if ( (pvalb= pvals[baseid]) != 0 )
    {
        if ( relid == MAX_CURRENCIES )
        {
            if ( kmdbtc == 0 )
                kmdbtc = pvals[MAX_CURRENCIES];
            if ( btcusd == 0 )
                btcusd = pvals[MAX_CURRENCIES + 1];
            if ( kmdbtc < 25000000 )
                kmdbtc = 25000000;
            if ( pvals[USD] != 0 && kmdbtc != 0 && btcusd != 0 )
            {
                baseusd = (((uint64_t)pvalb * 1000000000) / pvals[USD]);
                usdvol = komodo_paxvol(basevolume,baseusd);
                usdkmd = ((uint64_t)kmdbtc * 1000000000) / btcusd;
                //printf("kmdbtc.%llu btcusd.%llu base -> USD %llu, usdkmd %llu usdvol %llu -> %llu\n",(long long)kmdbtc,(long long)btcusd,(long long)baseusd,(long long)usdkmd,(long long)usdvol,(long long)(MINDENOMS[USD] * komodo_paxvol(usdvol,usdkmd) / (MINDENOMS[baseid]/100)));
                //printf("usdkmd.%llu basevolume.%llu baseusd.%llu paxvol.%llu usdvol.%llu -> %.8f\n",(long long)usdkmd,(long long)basevolume,(long long)baseusd,(long long)komodo_paxvol(basevolume,baseusd),(long long)usdvol,dstr(MINDENOMS[USD] * komodo_paxvol(usdvol,usdkmd)));
                return(MINDENOMS[USD] * komodo_paxvol(usdvol,usdkmd) / (MINDENOMS[baseid]/100));
            } //else printf("zero val in KMD conv %llu %llu %llu\n",(long long)pvals[USD],(long long)kmdbtc,(long long)btcusd);
        }
        else if ( baseid == relid )
        {
            if ( baseid != MAX_CURRENCIES )
            {
                pax_rank(ranked,pvals);
                //printf("%s M1 percentage %.8f\n",CURRENCIES[baseid],dstr(10 * ranked[baseid]));
                return(10 * ranked[baseid]); // scaled percentage of M1 total
            } else return(basevolume);
        }
        else if ( (pvalr= pvals[relid]) != 0 )
        {
            baserel = ((uint64_t)pvalb * 1000000000) / pvalr;
            //printf("baserel.%lld %lld %lld %.8f %.8f\n",(long long)baserel,(long long)MINDENOMS[baseid],(long long)MINDENOMS[relid],dstr(MINDENOMS[baseid]/MINDENOMS[relid]),dstr(MINDENOMS[relid]/MINDENOMS[baseid]));
            if ( MINDENOMS[baseid] > MINDENOMS[relid] )
                basevolume /= (MINDENOMS[baseid] / MINDENOMS[relid]);
            else if ( MINDENOMS[baseid] < MINDENOMS[relid] )
                basevolume *= (MINDENOMS[relid] / MINDENOMS[baseid]);
            return(komodo_paxvol(basevolume,baserel));
        }
        else printf("null pval for %s\n",CURRENCIES[relid]);
    } else printf("null pval for %s\n",CURRENCIES[baseid]);
    return(0);
}

uint64_t _komodo_paxprice(uint64_t *kmdbtcp,uint64_t *btcusdp,int32_t height,char *base,char *rel,uint64_t basevolume,uint64_t kmdbtc,uint64_t btcusd)
{
    int32_t baseid=-1,relid=-1,i; uint32_t *ptr;
    if ( height > 10 )
        height -= 10;
    if ( (baseid= komodo_baseid(base)) >= 0 && (relid= komodo_baseid(rel)) >= 0 )
    {
        //portable_mutex_lock(&komodo_mutex);
        for (i=NUM_PRICES-1; i>=0; i--)
        {
            ptr = &PVALS[36 * i];
            if ( *ptr < height )
            {
                if ( kmdbtcp != 0 && btcusdp != 0 )
                {
                    *kmdbtcp = ptr[MAX_CURRENCIES + 1] / 539;
                    *btcusdp = ptr[MAX_CURRENCIES + 2] / 539;
                }
                //portable_mutex_unlock(&komodo_mutex);
                if ( kmdbtc != 0 && btcusd != 0 )
                    return(komodo_paxcalc(&ptr[1],baseid,relid,basevolume,kmdbtc,btcusd));
                else return(0);
            }
        }
        //portable_mutex_unlock(&komodo_mutex);
    } //else printf("paxprice invalid base.%s %d, rel.%s %d\n",base,baseid,rel,relid);
    return(0);
}

int32_t komodo_kmdbtcusd(int32_t rwflag,uint64_t *kmdbtcp,uint64_t *btcusdp,int32_t height)
{
    static uint64_t *KMDBTCS,*BTCUSDS; static int32_t maxheight = 0; int32_t incr = 10000;
    if ( height >= maxheight )
    {
        //printf("height.%d maxheight.%d incr.%d\n",height,maxheight,incr);
        if ( height >= maxheight+incr )
            incr = (height - (maxheight+incr) + 1000);
        KMDBTCS = (uint64_t *)realloc(KMDBTCS,((incr + maxheight) * sizeof(*KMDBTCS)));
        memset(&KMDBTCS[maxheight],0,(incr * sizeof(*KMDBTCS)));
        BTCUSDS = (uint64_t *)realloc(BTCUSDS,((incr + maxheight) * sizeof(*BTCUSDS)));
        memset(&BTCUSDS[maxheight],0,(incr * sizeof(*BTCUSDS)));
        maxheight += incr;
    }
    if ( rwflag == 0 )
    {
        *kmdbtcp = KMDBTCS[height];
        *btcusdp = BTCUSDS[height];
    }
    else
    {
        KMDBTCS[height] = *kmdbtcp;
        BTCUSDS[height] = *btcusdp;
    }
    if ( *kmdbtcp != 0 && *btcusdp != 0 )
        return(0);
    else return(-1);
}

uint64_t komodo_paxpriceB(uint64_t seed,int32_t height,char *base,char *rel,uint64_t basevolume)
{
    int32_t i,j,k,ind,zeroes,numvotes,wt,nonz; int64_t delta; uint64_t lastprice,tolerance,den,densum,sum=0,votes[sizeof(Peggy_inds)/sizeof(*Peggy_inds)],btcusds[sizeof(Peggy_inds)/sizeof(*Peggy_inds)],kmdbtcs[sizeof(Peggy_inds)/sizeof(*Peggy_inds)],kmdbtc,btcusd;
    if ( basevolume > KOMODO_PAXMAX )
    {
        printf("komodo_paxprice overflow %.8f\n",dstr(basevolume));
        return(0);
    }
    if ( strcmp(base,"KMD") == 0 || strcmp(base,"kmd") == 0 )
    {
        printf("kmd cannot be base currency\n");
        return(0);
    }
    numvotes = (int32_t)(sizeof(Peggy_inds)/sizeof(*Peggy_inds));
    memset(votes,0,sizeof(votes));
    //if ( komodo_kmdbtcusd(0,&kmdbtc,&btcusd,height) < 0 ) crashes when via passthru GUI use
    {
        memset(btcusds,0,sizeof(btcusds));
        memset(kmdbtcs,0,sizeof(kmdbtcs));
        for (i=0; i<numvotes; i++)
        {
            _komodo_paxprice(&kmdbtcs[numvotes-1-i],&btcusds[numvotes-1-i],height-i,base,rel,100000,0,0);
            //printf("(%llu %llu) ",(long long)kmdbtcs[numvotes-1-i],(long long)btcusds[numvotes-1-i]);
        }
        kmdbtc = komodo_paxcorrelation(kmdbtcs,numvotes,seed) * 539;
        btcusd = komodo_paxcorrelation(btcusds,numvotes,seed) * 539;
        //komodo_kmdbtcusd(1,&kmdbtc,&btcusd,height);
    }
    for (i=nonz=0; i<numvotes; i++)
    {
        if ( (votes[numvotes-1-i]= _komodo_paxprice(0,0,height-i,base,rel,100000,kmdbtc,btcusd)) == 0 )
            zeroes++;
        else nonz++;
    }
    if ( nonz <= (numvotes >> 1) )
    {
        //printf("kmdbtc %llu btcusd %llu ",(long long)kmdbtc,(long long)btcusd);
        //printf("komodo_paxprice nonz.%d of numvotes.%d seed.%llu\n",nonz,numvotes,(long long)seed);
        return(0);
    }
    return(komodo_paxcorrelation(votes,numvotes,seed) * basevolume / 100000);
}

uint64_t komodo_paxprice(uint64_t *seedp,int32_t height,char *base,char *rel,uint64_t basevolume)
{
    int32_t i,nonz=0; int64_t diff; uint64_t price,seed,sum = 0;
    if ( ASSETCHAINS_SYMBOL[0] == 0 && chainActive.Tip() != 0 && height > chainActive.Tip()->nHeight )
    {
        if ( height != 1381319936 )
            printf("height.%d vs tip.%d\n",height,chainActive.Tip()->nHeight);
        return(0);
    }
    *seedp = komodo_seed(height);
    portable_mutex_lock(&komodo_mutex);
    for (i=0; i<17; i++)
    {
        if ( (price= komodo_paxpriceB(*seedp,height-i,base,rel,basevolume)) != 0 )
        {
            sum += price;
            nonz++;
            if ( 0 && i == 1 && nonz == 2 )
            {
                diff = (((int64_t)price - (sum >> 1)) * 10000);
                if ( diff < 0 )
                    diff = -diff;
                diff /= price;
                printf("(%llu %llu %lld).%lld ",(long long)price,(long long)(sum>>1),(long long)(((int64_t)price - (sum >> 1)) * 10000),(long long)diff);
                if ( diff < 33 )
                    break;
            }
            else if ( 0 && i == 3 && nonz == 4 )
            {
                diff = (((int64_t)price - (sum >> 2)) * 10000);
                if ( diff < 0 )
                    diff = -diff;
                diff /= price;
                printf("(%llu %llu %lld).%lld ",(long long)price,(long long)(sum>>2),(long long) (((int64_t)price - (sum >> 2)) * 10000),(long long)diff);
                if ( diff < 20 )
                    break;
            }
        }
        if ( height < 165000 )
            break;
    }
    portable_mutex_unlock(&komodo_mutex);
    if ( nonz != 0 )
        sum /= nonz;
    //printf("-> %lld %s/%s i.%d ht.%d\n",(long long)sum,base,rel,i,height);
    return(sum);
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
            prices[num] = komodo_paxcalc(&ptr[1],baseid,relid,COIN,0,0);
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
    komodo_stateupdate(height,0,0,0,zero,0,0,pvals,numpvals,0,0,0,0,0,0);
    //printf("komodo_paxpricefeed vout OP_RETURN.%d prices numpvals.%d opretlen.%d\n",height,numpvals,opretlen);
}

uint64_t PAX_fiatdest(uint64_t *seedp,int32_t tokomodo,char *destaddr,uint8_t pubkey33[33],char *coinaddr,int32_t height,char *origbase,int64_t fiatoshis)
{
    uint8_t shortflag = 0; char base[4]; int32_t i,baseid; uint8_t addrtype,rmd160[20]; int64_t komodoshis = 0;
    *seedp = komodo_seed(height);
    if ( (baseid= komodo_baseid(origbase)) < 0 || baseid == MAX_CURRENCIES )
    {
        if ( origbase[0] != 0 )
            printf("[%s] PAX_fiatdest illegal base.(%s)\n",ASSETCHAINS_SYMBOL,origbase);
        return(0);
    }
    for (i=0; i<3; i++)
        base[i] = toupper(origbase[i]);
    base[i] = 0;
    if ( fiatoshis < 0 )
        shortflag = 1, fiatoshis = -fiatoshis;
    komodoshis = komodo_paxprice(seedp,height,base,(char *)"KMD",(uint64_t)fiatoshis);
    //printf("PAX_fiatdest ht.%d price %s %.8f -> KMD %.8f seed.%llx\n",height,base,(double)fiatoshis/COIN,(double)komodoshis/COIN,(long long)*seedp);
    if ( bitcoin_addr2rmd160(&addrtype,rmd160,coinaddr) == 20 )
    {
        PAX_pubkey(1,pubkey33,&addrtype,rmd160,base,&shortflag,tokomodo != 0 ? &komodoshis : &fiatoshis);
        bitcoin_address(destaddr,KOMODO_PUBTYPE,pubkey33,33);
    }
    return(komodoshis);
}
