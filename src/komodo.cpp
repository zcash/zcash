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
#include "komodo.h"
#include "komodo_extern_globals.h"
#include "komodo_notary.h"
#include "mem_read.h"

void komodo_currentheight_set(int32_t height)
{
    char symbol[KOMODO_ASSETCHAIN_MAXLEN],dest[KOMODO_ASSETCHAIN_MAXLEN]; struct komodo_state *sp;
    if ( (sp= komodo_stateptr(symbol,dest)) != 0 )
        sp->CURRENT_HEIGHT = height;
}

extern NSPV_inforesp NSPV_inforesult;

int32_t komodo_currentheight()
{
    char symbol[KOMODO_ASSETCHAIN_MAXLEN],dest[KOMODO_ASSETCHAIN_MAXLEN]; struct komodo_state *sp;
    if ( KOMODO_NSPV_SUPERLITE )
    {
        return (NSPV_inforesult.height);
    }
    if ( (sp= komodo_stateptr(symbol,dest)) != 0 )
        return(sp->CURRENT_HEIGHT);
    else return(0);
}

int32_t komodo_parsestatefile(struct komodo_state *sp,FILE *fp,char *symbol,char *dest)
{
    int32_t func;

    try
    {
        if ( (func= fgetc(fp)) != EOF )
        {
            bool matched = false;
            if ( ASSETCHAINS_SYMBOL[0] == 0 && strcmp(symbol,"KMD") == 0 )
                matched = true;
            else 
                matched = (strcmp(symbol,ASSETCHAINS_SYMBOL) == 0);

            int32_t ht;
            if ( fread(&ht,1,sizeof(ht),fp) != sizeof(ht) )
                throw komodo::parse_error("Unable to read height from file");
            if ( func == 'P' )
            {
                std::shared_ptr<komodo::event_pubkeys> pk = std::make_shared<komodo::event_pubkeys>(fp, ht);
                if ( (KOMODO_EXTERNAL_NOTARIES && matched ) || (strcmp(symbol,"KMD") == 0 && !KOMODO_EXTERNAL_NOTARIES) )
                {
                    komodo_eventadd_pubkeys(sp, symbol, ht, pk);
                }
            }
            else if ( func == 'N' || func == 'M' )
            {
                std::shared_ptr<komodo::event_notarized> evt = std::make_shared<komodo::event_notarized>(fp, ht, dest, func == 'M');
                komodo_eventadd_notarized(sp, symbol, ht, evt);
            }
            else if ( func == 'U' ) // deprecated
            {
                std::shared_ptr<komodo::event_u> evt = std::make_shared<komodo::event_u>(fp, ht);
            }
            else if ( func == 'K' || func == 'T')
            {
                std::shared_ptr<komodo::event_kmdheight> evt = std::make_shared<komodo::event_kmdheight>(fp, ht, func == 'T');
                komodo_eventadd_kmdheight(sp, symbol, ht, evt);
            }
            else if ( func == 'R' )
            {
                std::shared_ptr<komodo::event_opreturn> evt = std::make_shared<komodo::event_opreturn>(fp, ht);
                // check for oversized opret
                if ( evt->opret.size() < 16384*4 )
                    komodo_eventadd_opreturn(sp, symbol, ht, evt);
            }
            else if ( func == 'D' )
            {
                printf("unexpected function D[%d]\n",ht);
            }
            else if ( func == 'V' )
            {
                std::shared_ptr<komodo::event_pricefeed> evt = std::make_shared<komodo::event_pricefeed>(fp, ht);
                komodo_eventadd_pricefeed(sp, symbol, ht, evt);
            }
        } // retrieved the func
    }
    catch(const komodo::parse_error& pe)
    {
        LogPrintf("Error occurred in parsestatefile: %s\n", pe.what());
        func = -1;
    }
    return func;
}

int32_t komodo_parsestatefiledata(struct komodo_state *sp,uint8_t *filedata,long *fposp,long datalen,char *symbol,char *dest)
{
    int32_t func = -1;

    try
    {
        long fpos = *fposp;
        if ( fpos < datalen )
        {
            func = filedata[fpos++];

            bool matched = false;
            if ( ASSETCHAINS_SYMBOL[0] == 0 && strcmp(symbol,"KMD") == 0 )
                matched = true;
            else 
                matched = (strcmp(symbol,ASSETCHAINS_SYMBOL) == 0);

            int32_t ht;
            if ( mem_read(ht, filedata, fpos, datalen) != sizeof(ht) )
                throw komodo::parse_error("Unable to parse height from file data");
            if ( func == 'P' )
            {
                std::shared_ptr<komodo::event_pubkeys> pk = std::make_shared<komodo::event_pubkeys>(filedata, fpos, datalen, ht);
                if ( (KOMODO_EXTERNAL_NOTARIES && matched ) || (strcmp(symbol,"KMD") == 0 && !KOMODO_EXTERNAL_NOTARIES) )
                {
                    komodo_eventadd_pubkeys(sp, symbol, ht, pk);
                }
            }
            else if ( func == 'N' || func == 'M' )
            {
                std::shared_ptr<komodo::event_notarized> ntz = 
                        std::make_shared<komodo::event_notarized>(filedata, fpos, datalen, ht, dest, func == 'M');
                komodo_eventadd_notarized(sp, symbol, ht, ntz);
            }
            else if ( func == 'U' ) // deprecated
            {
                std::shared_ptr<komodo::event_u> u = 
                        std::make_shared<komodo::event_u>(filedata, fpos, datalen, ht);
            }
            else if ( func == 'K' || func == 'T' )
            {
                std::shared_ptr<komodo::event_kmdheight> kmd_ht = 
                        std::make_shared<komodo::event_kmdheight>(filedata, fpos, datalen, ht, func == 'T');
                komodo_eventadd_kmdheight(sp, symbol, ht, kmd_ht);
            }
            else if ( func == 'R' )
            {
                std::shared_ptr<komodo::event_opreturn> opret = 
                        std::make_shared<komodo::event_opreturn>(filedata, fpos, datalen, ht);
                komodo_eventadd_opreturn(sp, symbol, ht, opret);
            }
            else if ( func == 'D' )
            {
                printf("unexpected function D[%d]\n",ht);
            }
            else if ( func == 'V' )
            {
                std::shared_ptr<komodo::event_pricefeed> pf = 
                        std::make_shared<komodo::event_pricefeed>(filedata, fpos, datalen, ht);
                komodo_eventadd_pricefeed(sp, symbol, ht, pf);
            }
            *fposp = fpos;
        }
    }
    catch( const komodo::parse_error& pe)
    {
        LogPrintf("Unable to parse state file data. Error: %s\n", pe.what());
        func = -1;
    }
    return func;
}

/***
 * @brief persist event to file stream
 * @param evt the event
 * @param fp the file
 * @returns the number of bytes written
 */
size_t write_event(std::shared_ptr<komodo::event> evt, FILE *fp)
{
    std::stringstream ss;
    ss << evt;
    std::string buf = ss.str();
    return fwrite(buf.c_str(), buf.size(), 1, fp);
}

void komodo_stateupdate(int32_t height,uint8_t notarypubs[][33],uint8_t numnotaries,
        uint8_t notaryid,uint256 txhash,uint64_t voutmask,uint8_t numvouts,uint32_t *pvals,
        uint8_t numpvals,int32_t KMDheight,uint32_t KMDtimestamp,uint64_t opretvalue,
        uint8_t *opretbuf,uint16_t opretlen,uint16_t vout,uint256 MoM,int32_t MoMdepth)
{
    static FILE *fp; 
    static int32_t errs,didinit; 
    static uint256 zero;
    struct komodo_state *sp; 
    char fname[512],symbol[KOMODO_ASSETCHAIN_MAXLEN],dest[KOMODO_ASSETCHAIN_MAXLEN]; 
    int32_t retval,ht,func; 
    uint8_t num,pubkeys[64][33];

    if ( didinit == 0 )
    {
        portable_mutex_init(&KOMODO_KV_mutex);
        portable_mutex_init(&KOMODO_CC_mutex);
        didinit = 1;
    }
    if ( (sp= komodo_stateptr(symbol,dest)) == 0 )
    {
        KOMODO_INITDONE = (uint32_t)time(NULL);
        printf("[%s] no komodo_stateptr\n",ASSETCHAINS_SYMBOL);
        return;
    }
    if ( fp == 0 )
    {
        komodo_statefname(fname,ASSETCHAINS_SYMBOL,(char *)"komodostate");
        if ( (fp= fopen(fname,"rb+")) != 0 )
        {
            if ( (retval= komodo_faststateinit(sp,fname,symbol,dest)) > 0 )
                fseek(fp,0,SEEK_END);
            else
            {
                fprintf(stderr,"komodo_faststateinit retval.%d\n",retval);
                while ( komodo_parsestatefile(sp,fp,symbol,dest) >= 0 )
                    ;
            }
        } else fp = fopen(fname,"wb+");
        KOMODO_INITDONE = (uint32_t)time(NULL);
    }
    if ( height <= 0 )
    {
        return;
    }
    if ( fp != 0 ) // write out funcid, height, other fields, call side effect function
    {
        if ( KMDheight != 0 )
        {
            std::shared_ptr<komodo::event_kmdheight> kmd_ht = std::make_shared<komodo::event_kmdheight>(height);
            kmd_ht->kheight = KMDheight;
            kmd_ht->timestamp = KMDtimestamp;
            write_event(kmd_ht, fp);
            komodo_eventadd_kmdheight(sp,symbol,height,kmd_ht);
        }
        else if ( opretbuf != 0 && opretlen > 0 )
        {
            std::shared_ptr<komodo::event_opreturn> evt = std::make_shared<komodo::event_opreturn>(height);
            evt->txid = txhash;
            evt->vout = vout;
            evt->value = opretvalue;
            for(uint16_t i = 0; i < opretlen; ++i)
                evt->opret.push_back(opretbuf[i]);
            write_event(evt, fp);
            komodo_eventadd_opreturn(sp,symbol,height,evt);
        }
        else if ( notarypubs != 0 && numnotaries > 0 )
        {
            std::shared_ptr<komodo::event_pubkeys> pk = std::make_shared<komodo::event_pubkeys>(height);
            pk->num = numnotaries;
            memcpy(pk->pubkeys, notarypubs, 33 * 64);
            write_event(pk, fp);
            komodo_eventadd_pubkeys(sp,symbol,height,pk);
        }
        else if ( voutmask != 0 && numvouts > 0 )
        {
            std::shared_ptr<komodo::event_u> evt = std::make_shared<komodo::event_u>(height);
            evt->n = numvouts;
            evt->nid = notaryid;
            memcpy(evt->mask, &voutmask, sizeof(voutmask));
            memcpy(evt->hash, &txhash, sizeof(txhash));
           write_event(evt, fp);
        }
        else if ( pvals != 0 && numpvals > 0 )
        {
            int32_t i,nonz = 0;
            for (i=0; i<32; i++)
                if ( pvals[i] != 0 )
                    nonz++;
            if ( nonz >= 32 )
            {
                std::shared_ptr<komodo::event_pricefeed> evt = std::make_shared<komodo::event_pricefeed>(height);
                evt->num = numpvals;
                for( uint8_t i = 0; i < evt->num; ++i)
                    evt->prices[i] = pvals[i];
                write_event(evt, fp);
                komodo_eventadd_pricefeed(sp,symbol,height,evt);
            }
        }
        else if ( height != 0 )
        {
            if ( sp != nullptr )
            {
                std::shared_ptr<komodo::event_notarized> evt = std::make_shared<komodo::event_notarized>(height, dest);
                evt->blockhash = sp->LastNotarizedHash();
                evt->desttxid = sp->LastNotarizedDestTxId();
                evt->notarizedheight = sp->LastNotarizedHeight();
                evt->MoM = sp->LastNotarizedMoM();
                evt->MoMdepth = sp->LastNotarizedMoMDepth();
                write_event(evt, fp);
                komodo_eventadd_notarized(sp,symbol,height,evt);
            }
        }
        fflush(fp);
    }
}

int32_t komodo_validate_chain(uint256 srchash,int32_t notarized_height)
{
    static int32_t last_rewind; int32_t rewindtarget; CBlockIndex *pindex; struct komodo_state *sp; char symbol[KOMODO_ASSETCHAIN_MAXLEN],dest[KOMODO_ASSETCHAIN_MAXLEN];
    if ( (sp= komodo_stateptr(symbol,dest)) == 0 )
        return(0);
    if ( IsInitialBlockDownload() == 0 && ((pindex= komodo_getblockindex(srchash)) == 0 || pindex->GetHeight() != notarized_height) )
    {
        if ( sp->LastNotarizedHeight() > 0 && sp->LastNotarizedHeight() < notarized_height )
            rewindtarget = sp->LastNotarizedHeight() - 1;
        else if ( notarized_height > 101 )
            rewindtarget = notarized_height - 101;
        else rewindtarget = 0;
        if ( rewindtarget != 0 && rewindtarget > KOMODO_REWIND && rewindtarget > last_rewind )
        {
            if ( last_rewind != 0 )
            {
                fprintf(stderr,"%s FORK detected. notarized.%d %s not in this chain! last notarization %d -> rewindtarget.%d\n",
                        ASSETCHAINS_SYMBOL,notarized_height,srchash.ToString().c_str(),sp->LastNotarizedHeight(),rewindtarget);
            }
            last_rewind = rewindtarget;
        }
        return(0);
    } else return(1);
}

int32_t komodo_voutupdate(bool fJustCheck,int32_t *isratificationp,int32_t notaryid,uint8_t *scriptbuf,
        int32_t scriptlen,int32_t height,uint256 txhash,int32_t i,int32_t j,uint64_t *voutmaskp,
        int32_t *specialtxp,int32_t *notarizedheightp,uint64_t value,int32_t notarized,
        uint64_t signedmask,uint32_t timestamp)
{
    static uint256 zero; static FILE *signedfp;
    int32_t opretlen,nid,offset,k,MoMdepth,matched,len = 0; uint256 MoM,srchash,desttxid; uint8_t crypto777[33]; struct komodo_state *sp; char symbol[KOMODO_ASSETCHAIN_MAXLEN],dest[KOMODO_ASSETCHAIN_MAXLEN];
    if ( (sp= komodo_stateptr(symbol,dest)) == 0 )
        return(-1);
    if ( scriptlen == 35 && scriptbuf[0] == 33 && scriptbuf[34] == 0xac )
    {
        if ( i == 0 && j == 0 && memcmp(NOTARY_PUBKEY33,scriptbuf+1,33) == 0 && IS_KOMODO_NOTARY )
        {
            printf("%s KOMODO_LASTMINED.%d -> %d\n",ASSETCHAINS_SYMBOL,KOMODO_LASTMINED,height);
            prevKOMODO_LASTMINED = KOMODO_LASTMINED;
            KOMODO_LASTMINED = height;
        }
        decode_hex(crypto777,33,CRYPTO777_PUBSECPSTR);
        /*for (k=0; k<33; k++)
            printf("%02x",crypto777[k]);
        printf(" crypto777 ");
        for (k=0; k<scriptlen; k++)
            printf("%02x",scriptbuf[k]);
        printf(" <- script ht.%d i.%d j.%d cmp.%d\n",height,i,j,memcmp(crypto777,scriptbuf+1,33));*/
        if ( memcmp(crypto777,scriptbuf+1,33) == 0 )
        {
            *specialtxp = 1;
            //printf(">>>>>>>> ");
        }
        else if ( komodo_chosennotary(&nid,height,scriptbuf + 1,timestamp) >= 0 )
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
                    //for (i=0; i<33; i++)
                    //    printf("%02x",scriptbuf[i+1]);
                    //printf(" %s mismatch notaryid.%d k.%d\n",ASSETCHAINS_SYMBOL,notaryid,nid);
                    notaryid = 64;
                    *voutmaskp = 0;
                }
                else *voutmaskp |= (1LL << j);
            }
        }
    }
    if ( scriptbuf[len++] == 0x6a )
    {
        struct komodo_ccdata ccdata; struct komodo_ccdataMoMoM MoMoMdata;
        int32_t validated = 0,nameoffset,opoffset = 0;
        if ( (opretlen= scriptbuf[len++]) == 0x4c )
            opretlen = scriptbuf[len++];
        else if ( opretlen == 0x4d )
        {
            opretlen = scriptbuf[len++];
            opretlen += (scriptbuf[len++] << 8);
        }
        opoffset = len;
        matched = 0;
        if ( ASSETCHAINS_SYMBOL[0] == 0 )
        {
            if ( strcmp("KMD",(char *)&scriptbuf[len+32 * 2 + 4]) == 0 )
                matched = 1;
        }
        else
        {
            if ( scriptbuf[len] == 'K' )
            {
                //fprintf(stderr,"i.%d j.%d KV OPRET len.%d %.8f\n",i,j,opretlen,dstr(value));
                komodo_stateupdate(height,0,0,0,txhash,0,0,0,0,0,0,value,&scriptbuf[len],opretlen,j,zero,0);
                return(-1);
            }
            if ( strcmp(ASSETCHAINS_SYMBOL,(char *)&scriptbuf[len+32*2+4]) == 0 )
                matched = 1;
        }
        offset = 32 * (1 + matched) + 4;
        nameoffset = (int32_t)strlen((char *)&scriptbuf[len+offset]);
        nameoffset++;
        memset(&ccdata,0,sizeof(ccdata));
        strncpy(ccdata.symbol,(char *)&scriptbuf[len+offset],sizeof(ccdata.symbol));
        if ( j == 1 && opretlen >= len+offset-opoffset )
        {
            memset(&MoMoMdata,0,sizeof(MoMoMdata));
            if ( matched == 0 && signedmask != 0 && bitweight(signedmask) >= KOMODO_MINRATIFY )
                notarized = 1;
            if ( strcmp("PIZZA",ccdata.symbol) == 0 || strncmp("TXSCL",ccdata.symbol,5) == 0 || strcmp("BEER",ccdata.symbol) == 0)
                notarized = 1;
            if ( 0 && opretlen != 149 )
                printf("[%s].%d (%s) matched.%d i.%d j.%d notarized.%d %llx opretlen.%d len.%d offset.%d opoffset.%d\n",ASSETCHAINS_SYMBOL,height,ccdata.symbol,matched,i,j,notarized,(long long)signedmask,opretlen,len,offset,opoffset);
            len += iguana_rwbignum(0,&scriptbuf[len],32,(uint8_t *)&srchash);
            len += iguana_rwnum(0,&scriptbuf[len],sizeof(*notarizedheightp),(uint8_t *)notarizedheightp);
            if ( matched != 0 )
                len += iguana_rwbignum(0,&scriptbuf[len],32,(uint8_t *)&desttxid);
            if ( matched != 0 )
                validated = komodo_validate_chain(srchash,*notarizedheightp);
            else validated = 1;
            // Any notarization that is matched and has a decodable op_return is enough to pay notaries. Otherwise bugs! 
            if ( fJustCheck && matched != 0 )
                return(-2);
            if ( notarized != 0 && validated != 0 )
            {
                //sp->NOTARIZED_HEIGHT = *notarizedheightp;
                //sp->NOTARIZED_HASH = srchash;
                //sp->NOTARIZED_DESTTXID = desttxid;
                memset(&MoM,0,sizeof(MoM));
                MoMdepth = 0;
                len += nameoffset;
                ccdata.MoMdata.notarized_height = *notarizedheightp;
                ccdata.MoMdata.height = height;
                ccdata.MoMdata.txi = i;
                //printf("nameoffset.%d len.%d + 36 %d opoffset.%d vs opretlen.%d\n",nameoffset,len,len+36,opoffset,opretlen);
                if ( len+36-opoffset <= opretlen )
                {
                    len += iguana_rwbignum(0,&scriptbuf[len],32,(uint8_t *)&MoM);
                    len += iguana_rwnum(0,&scriptbuf[len],sizeof(MoMdepth),(uint8_t *)&MoMdepth);
                    ccdata.MoMdata.MoM = MoM;
                    ccdata.MoMdata.MoMdepth = MoMdepth & 0xffff;
                    if ( len+sizeof(ccdata.CCid)-opoffset <= opretlen )
                    {
                        len += iguana_rwnum(0,&scriptbuf[len],sizeof(ccdata.CCid),(uint8_t *)&ccdata.CCid);
                        //if ( ((MoMdepth>>16) & 0xffff) != (ccdata.CCid & 0xffff) )
                        //    fprintf(stderr,"%s CCid mismatch %u != %u\n",ASSETCHAINS_SYMBOL,((MoMdepth>>16) & 0xffff),(ccdata.CCid & 0xffff));
                        ccdata.len = sizeof(ccdata.CCid);
                        if ( ASSETCHAINS_SYMBOL[0] != 0 )
                        {
                            // MoMoM, depth, numpairs, (notarization ht, MoMoM offset)
                            if ( len+48-opoffset <= opretlen && strcmp(ccdata.symbol,ASSETCHAINS_SYMBOL) == 0 )
                            {
                                len += iguana_rwnum(0,&scriptbuf[len],sizeof(uint32_t),(uint8_t *)&MoMoMdata.kmdstarti);
                                len += iguana_rwnum(0,&scriptbuf[len],sizeof(uint32_t),(uint8_t *)&MoMoMdata.kmdendi);
                                len += iguana_rwbignum(0,&scriptbuf[len],sizeof(MoMoMdata.MoMoM),(uint8_t *)&MoMoMdata.MoMoM);
                                len += iguana_rwnum(0,&scriptbuf[len],sizeof(uint32_t),(uint8_t *)&MoMoMdata.MoMoMdepth);
                                len += iguana_rwnum(0,&scriptbuf[len],sizeof(uint32_t),(uint8_t *)&MoMoMdata.numpairs);
                                MoMoMdata.len += sizeof(MoMoMdata.MoMoM) + sizeof(uint32_t)*4;
                                if ( len+MoMoMdata.numpairs*8-opoffset == opretlen )
                                {
                                    MoMoMdata.pairs = (struct komodo_ccdatapair *)calloc(MoMoMdata.numpairs,sizeof(*MoMoMdata.pairs));
                                    for (k=0; k<MoMoMdata.numpairs; k++)
                                    {
                                        len += iguana_rwnum(0,&scriptbuf[len],sizeof(int32_t),(uint8_t *)&MoMoMdata.pairs[k].notarized_height);
                                        len += iguana_rwnum(0,&scriptbuf[len],sizeof(uint32_t),(uint8_t *)&MoMoMdata.pairs[k].MoMoMoffset);
                                        MoMoMdata.len += sizeof(uint32_t) * 2;
                                    }
                                } else ccdata.len = MoMoMdata.len = 0;
                            } else ccdata.len = MoMoMdata.len = 0;
                        }
                    }
                    if ( MoM == zero || (MoMdepth&0xffff) > *notarizedheightp || (MoMdepth&0xffff) < 0 )
                    {
                        memset(&MoM,0,sizeof(MoM));
                        MoMdepth = 0;
                    }
                    else
                    {
                        if ( matched != 0 )
                            printf("[%s] matched.%d VALID (%s) MoM.%s [%d] CCid.%u\n",ASSETCHAINS_SYMBOL,matched,ccdata.symbol,MoM.ToString().c_str(),MoMdepth&0xffff,(MoMdepth>>16)&0xffff);
                    }
                    if ( MoMoMdata.pairs != 0 )
                        free(MoMoMdata.pairs);
                    memset(&ccdata,0,sizeof(ccdata));
                    memset(&MoMoMdata,0,sizeof(MoMoMdata));
                }
                
                if ( matched != 0 && *notarizedheightp > sp->LastNotarizedHeight() && *notarizedheightp < height )
                {
                    sp->SetLastNotarizedHeight(*notarizedheightp);
                    sp->SetLastNotarizedHash(srchash);
                    sp->SetLastNotarizedDestTxId(desttxid);
                    if ( MoM != zero && (MoMdepth&0xffff) > 0 )
                    {
                        sp->SetLastNotarizedMoM(MoM);
                        sp->SetLastNotarizedMoMDepth(MoMdepth);
                    }
                    komodo_stateupdate(height,0,0,0,zero,0,0,0,0,0,0,0,0,0,0,sp->LastNotarizedMoM(),sp->LastNotarizedMoMDepth());
                    printf("[%s] ht.%d NOTARIZED.%d %s.%s %sTXID.%s lens.(%d %d) MoM.%s %d\n",
                            ASSETCHAINS_SYMBOL,height,sp->LastNotarizedHeight(),
                            ASSETCHAINS_SYMBOL[0]==0?"KMD":ASSETCHAINS_SYMBOL,srchash.ToString().c_str(),
                            ASSETCHAINS_SYMBOL[0]==0?"BTC":"KMD",desttxid.ToString().c_str(),opretlen,len,
                            sp->LastNotarizedMoM().ToString().c_str(),sp->LastNotarizedMoMDepth());
                    
                    if ( ASSETCHAINS_SYMBOL[0] == 0 )
                    {
                        if ( signedfp == 0 )
                        {
                            char fname[512];
                            komodo_statefname(fname,ASSETCHAINS_SYMBOL,(char *)"signedmasks");
                            if ( (signedfp= fopen(fname,"rb+")) == 0 )
                                signedfp = fopen(fname,"wb");
                            else fseek(signedfp,0,SEEK_END);
                        }
                        if ( signedfp != 0 )
                        {
                            fwrite(&height,1,sizeof(height),signedfp);
                            fwrite(&signedmask,1,sizeof(signedmask),signedfp);
                            fflush(signedfp);
                        }
                        if ( opretlen > len && scriptbuf[len] == 'A' )
                        {
                            //for (i=0; i<opretlen-len; i++)
                            //    printf("%02x",scriptbuf[len+i]);
                            //printf(" Found extradata.[%d] %d - %d\n",opretlen-len,opretlen,len);
                            komodo_stateupdate(height,0,0,0,txhash,0,0,0,0,0,0,value,&scriptbuf[len],opretlen-len+4+3+(scriptbuf[1] == 0x4d),j,zero,0);
                        }
                    }
                }
            } else if ( opretlen != 149 && height > 600000 && matched != 0 )
                printf("%s validated.%d notarized.%d %llx reject ht.%d NOTARIZED.%d prev.%d %s.%s DESTTXID.%s len.%d opretlen.%d\n",
                        ccdata.symbol,validated,notarized,(long long)signedmask,height,*notarizedheightp,sp->LastNotarizedHeight(),
                        ASSETCHAINS_SYMBOL[0]==0?"KMD":ASSETCHAINS_SYMBOL,srchash.ToString().c_str(),desttxid.ToString().c_str(),len,opretlen);
        }
        else if ( matched != 0 && i == 0 && j == 1 && opretlen == 149 )
        {
            if ( notaryid >= 0 && notaryid < 64 )
                komodo_paxpricefeed(height,&scriptbuf[len],opretlen);
        }
        else if ( matched != 0 )
        {
            //int32_t k; for (k=0; k<scriptlen; k++)
            //    printf("%02x",scriptbuf[k]);
            //printf(" <- script ht.%d i.%d j.%d value %.8f %s\n",height,i,j,dstr(value),ASSETCHAINS_SYMBOL);
            if ( opretlen >= 32*2+4 && strcmp(ASSETCHAINS_SYMBOL[0]==0?"KMD":ASSETCHAINS_SYMBOL,(char *)&scriptbuf[len+32*2+4]) == 0 )
            {
                for (k=0; k<32; k++)
                    if ( scriptbuf[len+k] != 0 )
                        break;
                if ( k == 32 )
                {
                    *isratificationp = 1;
                    printf("ISRATIFICATION (%s)\n",(char *)&scriptbuf[len+32*2+4]);
                }
            }

            if ( *isratificationp == 0 && (signedmask != 0 || (scriptbuf[len] != 'X' && scriptbuf[len] != 'A')) ) // && scriptbuf[len] != 'I')
                komodo_stateupdate(height,0,0,0,txhash,0,0,0,0,0,0,value,&scriptbuf[len],opretlen,j,zero,0);
        }
    }
    return(notaryid);
}

// Special tx have vout[0] -> CRYPTO777
// with more than KOMODO_MINRATIFY pay2pubkey outputs -> ratify
// if all outputs to notary -> notary utxo
// if txi == 0 && 2 outputs and 2nd OP_RETURN, len == 32*2+4 -> notarized, 1st byte 'P' -> pricefeed
// OP_RETURN: 'D' -> deposit, 'W' -> withdraw

int32_t gettxout_scriptPubKey(uint8_t *scriptPubKey,int32_t maxsize,uint256 txid,int32_t n);

int32_t komodo_notarycmp(uint8_t *scriptPubKey,int32_t scriptlen,uint8_t pubkeys[64][33],int32_t numnotaries,uint8_t rmd160[20])
{
    int32_t i;
    if ( scriptlen == 25 && memcmp(&scriptPubKey[3],rmd160,20) == 0 )
        return(0);
    else if ( scriptlen == 35 )
    {
        for (i=0; i<numnotaries; i++)
            if ( memcmp(&scriptPubKey[1],pubkeys[i],33) == 0 )
                return(i);
    }
    return(-1);
}

// int32_t (!!!)
/*
    read blackjok3rtt comments in main.cpp 
*/
int32_t komodo_connectblock(bool fJustCheck, CBlockIndex *pindex,CBlock& block)
{
    static int32_t hwmheight;
    int32_t staked_era; static int32_t lastStakedEra;
    std::vector<int32_t> notarisations;
    uint64_t signedmask,voutmask; char symbol[KOMODO_ASSETCHAIN_MAXLEN],dest[KOMODO_ASSETCHAIN_MAXLEN]; struct komodo_state *sp;
    uint8_t scriptbuf[10001],pubkeys[64][33],rmd160[20],scriptPubKey[35]; uint256 zero,btctxid,txhash;
    int32_t i,j,k,numnotaries,notarized,scriptlen,isratification,nid,numvalid,specialtx,notarizedheight,notaryid,len,numvouts,numvins,height,txn_count;
    if ( pindex == 0 )
    {
        fprintf(stderr,"komodo_connectblock null pindex\n");
        return(0);
    }
    memset(&zero,0,sizeof(zero));
    komodo_init(pindex->GetHeight());
    KOMODO_INITDONE = (uint32_t)time(NULL);
    if ( (sp= komodo_stateptr(symbol,dest)) == 0 )
    {
        fprintf(stderr,"unexpected null komodostateptr.[%s]\n",ASSETCHAINS_SYMBOL);
        return(0);
    }
    //fprintf(stderr,"%s connect.%d\n",ASSETCHAINS_SYMBOL,pindex->nHeight);
    // Wallet Filter. Disabled here. Cant be activated by notaries or pools with some changes.
    if ( is_STAKED(ASSETCHAINS_SYMBOL) != 0 || STAKED_NOTARY_ID > -1 )
    {
        staked_era = STAKED_era(pindex->GetBlockTime());
        if ( !fJustCheck && staked_era != lastStakedEra )
        {
            uint8_t tmp_pubkeys[64][33];
            int8_t numSN = numStakedNotaries(tmp_pubkeys,staked_era);
            UpdateNotaryAddrs(tmp_pubkeys,numSN);
            STAKED_ERA = staked_era;
            lastStakedEra = staked_era;
        }
    }
    numnotaries = komodo_notaries(pubkeys,pindex->GetHeight(),pindex->GetBlockTime());
    calc_rmd160_sha256(rmd160,pubkeys[0],33);
    if ( pindex->GetHeight() > hwmheight )
        hwmheight = pindex->GetHeight();
    else
    {
        if ( pindex->GetHeight() != hwmheight )
        {
            printf("%s hwmheight.%d vs pindex->GetHeight().%d t.%u reorg.%d\n",ASSETCHAINS_SYMBOL,hwmheight,pindex->GetHeight(),(uint32_t)pindex->nTime,hwmheight-pindex->GetHeight());
            komodo_purge_ccdata((int32_t)pindex->GetHeight());
            hwmheight = pindex->GetHeight();
        }
        if (!fJustCheck)
        {
            komodo_event_rewind(sp,symbol,pindex->GetHeight());
            komodo_stateupdate(pindex->GetHeight(),0,0,0,zero,0,0,0,0,-pindex->GetHeight(),pindex->nTime,0,0,0,0,zero,0);
        }
    }
    komodo_currentheight_set(chainActive.LastTip()->GetHeight());
    int transaction = 0;
    if ( pindex != 0 )
    {
        height = pindex->GetHeight();
        txn_count = block.vtx.size();
        for (i=0; i<txn_count; i++)
        {
            if ( (is_STAKED(ASSETCHAINS_SYMBOL) != 0 && staked_era == 0) || (is_STAKED(ASSETCHAINS_SYMBOL) == 255) ) {
                // in era gap or chain banned, no point checking any invlaid notarisations.
                break;
            }
            // Notary pay chains need notarisation in position 1, ignore the rest on validation. Check notarisation is 1 on check.
            if ( !fJustCheck && i > 1 && ASSETCHAINS_NOTARY_PAY[0] != 0 )
                break;
            txhash = block.vtx[i].GetHash();
            numvouts = block.vtx[i].vout.size();
            notaryid = -1;
            voutmask = specialtx = notarizedheight = isratification = notarized = 0;
            signedmask = (height < 91400) ? 1 : 0;
            numvins = block.vtx[i].vin.size();
            for (j=0; j<numvins; j++)
            {
                if ( i == 0 && j == 0 )
                    continue;
                if ( (scriptlen= gettxout_scriptPubKey(scriptPubKey,sizeof(scriptPubKey),block.vtx[i].vin[j].prevout.hash,block.vtx[i].vin[j].prevout.n)) > 0 )
                {
                    if ( (k= komodo_notarycmp(scriptPubKey,scriptlen,pubkeys,numnotaries,rmd160)) >= 0 )
                        signedmask |= (1LL << k);
                    else if ( 0 && numvins >= 17 )
                    {
                        int32_t k;
                        for (k=0; k<scriptlen; k++)
                            printf("%02x",scriptPubKey[k]);
                        printf(" scriptPubKey doesnt match any notary vini.%d of %d\n",j,numvins);
                    }
                } //else printf("cant get scriptPubKey for ht.%d txi.%d vin.%d\n",height,i,j);
            }
            numvalid = bitweight(signedmask);
            if ( ((height < 90000 || (signedmask & 1) != 0) && numvalid >= KOMODO_MINRATIFY) ||
                (numvalid >= KOMODO_MINRATIFY && ASSETCHAINS_SYMBOL[0] != 0) ||
                numvalid > (numnotaries/5) )
            {
                if ( !fJustCheck && ASSETCHAINS_SYMBOL[0] != 0)
                {
                    static FILE *signedfp;
                    if ( signedfp == 0 )
                    {
                        char fname[512];
                        komodo_statefname(fname,ASSETCHAINS_SYMBOL,(char *)"signedmasks");
                        if ( (signedfp= fopen(fname,"rb+")) == 0 )
                            signedfp = fopen(fname,"wb");
                        else fseek(signedfp,0,SEEK_END);
                    }
                    if ( signedfp != 0 )
                    {
                        fwrite(&height,1,sizeof(height),signedfp);
                        fwrite(&signedmask,1,sizeof(signedmask),signedfp);
                        fflush(signedfp);
                    }
                    transaction = i;
                    printf("[%s] ht.%d txi.%d signedmask.%llx numvins.%d numvouts.%d <<<<<<<<<<<  notarized\n",ASSETCHAINS_SYMBOL,height,i,(long long)signedmask,numvins,numvouts);
                }
                notarized = 1;
            }
            // simulate DPoW in regtest mode for dpowconfs tests/etc
            if ( Params().NetworkIDString() == "regtest" && ( height%7 == 0) ) {
                notarized              = 1;
                sp->SetLastNotarizedHeight(height);
                sp->SetLastNotarizedHash(block.GetHash());
                sp->SetLastNotarizedDestTxId(txhash);
            }
            for (j=0; j<numvouts; j++)
            {
                len = block.vtx[i].vout[j].scriptPubKey.size();
                
                if ( len >= sizeof(uint32_t) && len <= sizeof(scriptbuf) )
                {
                    memcpy(scriptbuf,(uint8_t *)&block.vtx[i].vout[j].scriptPubKey[0],len);
                    notaryid = komodo_voutupdate(fJustCheck,&isratification,notaryid,scriptbuf,len,height,txhash,i,j,&voutmask,&specialtx,&notarizedheight,(uint64_t)block.vtx[i].vout[j].nValue,notarized,signedmask,(uint32_t)chainActive.LastTip()->GetBlockTime());
                    if ( fJustCheck && notaryid == -2 )
                    {
                        // We see a valid notarisation here, save its location.
                        notarisations.push_back(i);
                    }
                    if ( 0 && i > 0 )
                    {
                        for (k=0; k<len; k++)
                            printf("%02x",scriptbuf[k]);
                        printf(" <- notaryid.%d ht.%d i.%d j.%d numvouts.%d numvins.%d voutmask.%llx txid.(%s)\n",notaryid,height,i,j,numvouts,numvins,(long long)voutmask,txhash.ToString().c_str());
                    }
                }
            }
            if ( 0 && ASSETCHAINS_SYMBOL[0] == 0 )
                printf("[%s] ht.%d txi.%d signedmask.%llx numvins.%d numvouts.%d notarized.%d special.%d isratification.%d\n",ASSETCHAINS_SYMBOL,height,i,(long long)signedmask,numvins,numvouts,notarized,specialtx,isratification);
            if ( !fJustCheck && (notarized != 0 && (notarizedheight != 0 || specialtx != 0)) )
            {
                if ( isratification != 0 )
                {
                    printf("%s NOTARY SIGNED.%llx numvins.%d ht.%d txi.%d notaryht.%d specialtx.%d\n",ASSETCHAINS_SYMBOL,(long long)signedmask,numvins,height,i,notarizedheight,specialtx);
                    printf("ht.%d specialtx.%d isratification.%d numvouts.%d signed.%llx numnotaries.%d\n",height,specialtx,isratification,numvouts,(long long)signedmask,numnotaries);
                }
                if ( specialtx != 0 && isratification != 0 && numvouts > 2 )
                {
                    numvalid = 0;
                    memset(pubkeys,0,sizeof(pubkeys));
                    for (j=1; j<numvouts-1; j++)
                    {
                        len = block.vtx[i].vout[j].scriptPubKey.size();
                        if ( len >= sizeof(uint32_t) && len <= sizeof(scriptbuf) )
                        {
                            memcpy(scriptbuf,(uint8_t *)&block.vtx[i].vout[j].scriptPubKey[0],len);
                            if ( len == 35 && scriptbuf[0] == 33 && scriptbuf[34] == 0xac )
                            {
                                memcpy(pubkeys[numvalid++],scriptbuf+1,33);
                                for (k=0; k<33; k++)
                                    printf("%02x",scriptbuf[k+1]);
                                printf(" <- new notary.[%d]\n",j-1);
                            }
                        }
                    }
                    if ( ASSETCHAINS_SYMBOL[0] != 0 || height < 100000 )
                    {
                        if ( ((signedmask & 1) != 0 && numvalid >= KOMODO_MINRATIFY) || bitweight(signedmask) > (numnotaries/3) )
                        {
                            memset(&txhash,0,sizeof(txhash));
                            komodo_stateupdate(height,pubkeys,numvalid,0,txhash,0,0,0,0,0,0,0,0,0,0,zero,0);
                            printf("RATIFIED! >>>>>>>>>> new notaries.%d newheight.%d from height.%d\n",numvalid,(((height+KOMODO_ELECTION_GAP/2)/KOMODO_ELECTION_GAP)+1)*KOMODO_ELECTION_GAP,height);
                        } else printf("signedmask.%llx numvalid.%d wt.%d numnotaries.%d\n",(long long)signedmask,numvalid,bitweight(signedmask),numnotaries);
                    }
                }
            }
        }
        if ( !fJustCheck && IS_KOMODO_NOTARY && ASSETCHAINS_SYMBOL[0] == 0 )
            printf("%s ht.%d\n",ASSETCHAINS_SYMBOL[0] == 0 ? "KMD" : ASSETCHAINS_SYMBOL,height);
        if ( !fJustCheck && pindex->GetHeight() == hwmheight )
            komodo_stateupdate(height,0,0,0,zero,0,0,0,0,height,(uint32_t)pindex->nTime,0,0,0,0,zero,0);
    } 
    else 
        { fprintf(stderr,"komodo_connectblock: unexpected null pindex\n"); return(0); }
    //KOMODO_INITDONE = (uint32_t)time(NULL);
    //fprintf(stderr,"%s end connect.%d\n",ASSETCHAINS_SYMBOL,pindex->GetHeight());
    if (fJustCheck)
    {
        if ( notarisations.size() == 0 )
            return(0);
        if ( notarisations.size() == 1 && notarisations[0] == 1 )
            return(1);
        if ( notarisations.size() > 1 || (notarisations.size() == 1 && notarisations[0] != 1) )
            return(-1);
        
        fprintf(stderr,"komodo_connectblock: unxexpected behaviour when fJustCheck == true, report blackjok3rtt plz ! \n");
        /* this needed by gcc-8, it counts here that control reaches end of non-void function without this.
           by default, we count that if control reached here -> the valid notarization isnt in position 1 or there are too many notarizations in this block.
        */
        return(-1); 
    }
    else return(0);
}
