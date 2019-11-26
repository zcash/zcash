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

#include "CCOracles.h"
#include <secp256k1.h>

/*
 An oracles CC has the purpose of converting offchain data into onchain data
 simplest would just be to have a pubkey(s) that are trusted to provide such data, but this wont need to have a CC involved at all and can just be done by convention
 
 That begs the question, "what would an oracles CC do?"
 A couple of things come to mind, ie. payments to oracles for future offchain data and maybe some sort of dispute/censoring ability
 
 first step is to define the data that the oracle is providing. A simple name:description tx can be created to define the name and description of the oracle data.
 linked to this txid would be two types of transactions:
    a) oracle providers
    b) oracle data users
 
 In order to be resistant to sybil attacks, the feedback mechanism needs to have a cost. combining with the idea of payments for data, the oracle providers will be ranked by actual payments made to each oracle for each data type.
 
 Implementation notes:
  In order to maintain good performance even under heavy usage, special marker utxo are used. Actually a pair of them. When a provider registers to be a data provider, a special unspendable normal output is created to allow for quick scanning. Since the marker is based on the oracletxid, it becomes a single address where all the providers can be found. 
 
  A convention is used so that the datafee can be changed by registering again. it is assumed that there wont be too many of these datafee changes. if more than one from the same provider happens in the same block, the lower price is used.
 
  The other efficiency issue is finding the most recent data point. We want to create a linked list of all data points, going back to the first one. In order to make this efficient, a special and unique per provider/oracletxid baton utxo is used. This should have exactly one utxo, so the search would be a direct lookup and it is passed on from one data point to the next. There is some small chance that the baton utxo is spent in a non-data transaction, so provision is made to allow for recreating a baton utxo in case it isnt found. The baton utxo is a convenience and doesnt affect validation
 
 Required transactions:
 0) create oracle description -> just needs to create txid for oracle data
 1) register as oracle data provider with price -> become a registered oracle data provider
 2) pay provider for N oracle data points -> lock funds for oracle provider
 3) publish oracle data point -> publish data and collect payment
 
 The format string is a set of chars with the following meaning:
  's' -> <256 char string
  'S' -> <65536 char string
  'd' -> <256 binary data
  'D' -> <65536 binary data
  'c' -> 1 byte signed little endian number, 'C' unsigned
  't' -> 2 byte signed little endian number, 'T' unsigned
  'i' -> 4 byte signed little endian number, 'I' unsigned
  'l' -> 8 byte signed little endian number, 'L' unsigned
  'h' -> 32 byte hash
 
 create:
 vins.*: normal inputs
 vout.0: txfee tag to oracle normal address
 vout.1: change, if any
 vout.n-1: opreturn with name and description and format for data
 
 register:
 vins.*: normal inputs
 vout.0: txfee tag to normal marker address
 vout.1: baton CC utxo
 vout.2: change, if any
 vout.n-1: opreturn with oracletxid, pubkey and price per data point
 
 subscribe:
 vins.*: normal inputs
 vout.0: subscription fee to publishers CC address
 vout.1: change, if any
 vout.n-1: opreturn with oracletxid, registered provider's pubkey, amount
 
 data:
 vin.0: normal input
 vin.1: baton CC utxo (most of the time)
 vin.2+: subscription or data vout.0
 vout.0: change to publishers CC address
 vout.1: baton CC utxo
 vout.2: payment for dataprovider
 vout.3: change, if any
 vout.n-1: opreturn with oracletxid, prevbatontxid and data in proper format
 
 data (without payment) this is not needed as publisher can pay themselves!
 vin.0: normal input
 vin.1: baton CC utxo
 vout.0: txfee to publishers normal address
 vout.1: baton CC utxo
 vout.2: change, if any
 vout.n-1: opreturn with oracletxid, prevbatontxid and data in proper format

*/
extern int32_t komodo_get_current_height();
#define PUBKEY_SPOOFING_FIX_ACTIVATION 1563148800
#define CC_MARKER_VALUE 10000

// start of consensus code
CScript EncodeOraclesCreateOpRet(uint8_t funcid,std::string name,std::string description,std::string format)
{
    CScript opret; uint8_t evalcode = EVAL_ORACLES;
    opret << OP_RETURN << E_MARSHAL(ss << evalcode << funcid << name << format << description);
    return(opret);
}

uint8_t DecodeOraclesCreateOpRet(const CScript &scriptPubKey,std::string &name,std::string &description,std::string &format)
{
    std::vector<uint8_t> vopret; uint8_t *script,e,f,funcid;
    GetOpReturnData(scriptPubKey,vopret);
    script = (uint8_t *)vopret.data();
    if ( vopret.size() > 2 && script[0] == EVAL_ORACLES )
    {
        if ( script[1] == 'C' )
        {
            if ( E_UNMARSHAL(vopret,ss >> e; ss >> f; ss >> name; ss >> format; ss >> description) != 0 )
            {
                return(script[1]);
            } else fprintf(stderr,"DecodeOraclesCreateOpRet unmarshal error for C\n");
        }
    }
    return(0);
}

CScript EncodeOraclesOpRet(uint8_t funcid,uint256 oracletxid,CPubKey pk,int64_t num)
{
    CScript opret; uint8_t evalcode = EVAL_ORACLES;

    opret << OP_RETURN << E_MARSHAL(ss << evalcode << funcid << oracletxid << pk << num);
    return(opret);
}

uint8_t DecodeOraclesOpRet(const CScript &scriptPubKey,uint256 &oracletxid,CPubKey &pk,int64_t &num)
{
    std::vector<uint8_t> vopret; uint8_t *script,e,f;

    GetOpReturnData(scriptPubKey,vopret);
    script = (uint8_t *)vopret.data();
    if ( vopret.size() > 2 && script[0] == EVAL_ORACLES )
    {
        if (script[0] == EVAL_ORACLES && (script[1]== 'R' || script[1] == 'S' || script[1] == 'F') && E_UNMARSHAL(vopret,ss >> e; ss >> f; ss >> oracletxid; ss >> pk; ss >> num)!=0)
            return(f);
        else return(script[1]);
    }
    return(0);
}

CScript EncodeOraclesData(uint8_t funcid,uint256 oracletxid,uint256 batontxid,CPubKey pk,std::vector <uint8_t>data)
{
    CScript opret; uint8_t evalcode = EVAL_ORACLES;
    opret << OP_RETURN << E_MARSHAL(ss << evalcode << funcid << oracletxid << batontxid << pk << data);
    return(opret);
}

uint8_t DecodeOraclesData(const CScript &scriptPubKey,uint256 &oracletxid,uint256 &batontxid,CPubKey &pk,std::vector <uint8_t>&data)
{
    std::vector<uint8_t> vopret; uint8_t *script,e,f;
    GetOpReturnData(scriptPubKey,vopret);
    script = (uint8_t *)vopret.data();
    if ( vopret.size() > 2 && E_UNMARSHAL(vopret,ss >> e; ss >> f; ss >> oracletxid; ss >> batontxid; ss >> pk; ss >> data) != 0 )
    {
        if ( e == EVAL_ORACLES && f == 'D' )
            return(f);
        //else fprintf(stderr,"DecodeOraclesData evalcode.%d f.%c\n",e,f);
    } //else fprintf(stderr,"DecodeOraclesData not enough opereturn data\n");
    return(0);
}

CPubKey OracleBatonPk(char *batonaddr,struct CCcontract_info *cp)
{
    static secp256k1_context *ctx;
    size_t clen = CPubKey::PUBLIC_KEY_SIZE;
    secp256k1_pubkey pubkey; CPubKey batonpk; uint8_t priv[32]; int32_t i;
    if ( ctx == 0 )
        ctx = secp256k1_context_create(SECP256K1_CONTEXT_SIGN);
    Myprivkey(priv);
    cp->unspendableEvalcode2 = EVAL_ORACLES;
    for (i=0; i<32; i++)
        cp->unspendablepriv2[i] = (priv[i] ^ cp->CCpriv[i]);
    while ( secp256k1_ec_seckey_verify(ctx,cp->unspendablepriv2) == 0 )
    {
        for (i=0; i<32; i++)
            fprintf(stderr,"%02x",cp->unspendablepriv2[i]);
        fprintf(stderr," invalid privkey\n");
        if ( secp256k1_ec_privkey_tweak_add(ctx,cp->unspendablepriv2,priv) != 0 )
            break;
    }
    if ( secp256k1_ec_pubkey_create(ctx,&pubkey,cp->unspendablepriv2) != 0 )
    {
        secp256k1_ec_pubkey_serialize(ctx,(unsigned char*)batonpk.begin(),&clen,&pubkey,SECP256K1_EC_COMPRESSED);
        cp->unspendablepk2 = batonpk;
        Getscriptaddress(batonaddr,MakeCC1vout(cp->evalcode,0,batonpk).scriptPubKey);
        //fprintf(stderr,"batonpk.(%s) -> %s\n",(char *)HexStr(batonpk).c_str(),batonaddr);
        strcpy(cp->unspendableaddr2,batonaddr);
    } else fprintf(stderr,"error creating pubkey\n");
    memset(priv,0,sizeof(priv));
    return(batonpk);
}

int64_t OracleCurrentDatafee(uint256 reforacletxid,char *markeraddr,CPubKey publisher)
{
    uint256 txid,oracletxid,hashBlock; int64_t datafee=0,dfee; int32_t dheight=0,vout,height,numvouts; CTransaction tx; CPubKey pk;
    std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> > unspentOutputs;
    SetCCunspents(unspentOutputs,markeraddr,false);
    for (std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> >::const_iterator it=unspentOutputs.begin(); it!=unspentOutputs.end(); it++)
    {
        txid = it->first.txhash;
        vout = (int32_t)it->first.index;
        height = (int32_t)it->second.blockHeight;
        if ( myGetTransaction(txid,tx,hashBlock) != 0 && (numvouts= tx.vout.size()) > 0 )
        {
            if ( DecodeOraclesOpRet(tx.vout[numvouts-1].scriptPubKey,oracletxid,pk,dfee) == 'R' )
            {
                if ( oracletxid == reforacletxid && pk == publisher )
                {
                    if ( height > dheight || (height == dheight && dfee < datafee) )
                    {
                        dheight = height;
                        datafee = dfee;
                        if ( 0 && dheight != 0 )
                            fprintf(stderr,"set datafee %.8f height.%d\n",(double)datafee/COIN,height);
                    }
                }
            }
        }
    }
    return(datafee);
}

int64_t OracleDatafee(CScript &scriptPubKey,uint256 oracletxid,CPubKey publisher)
{
    CTransaction oracletx; char markeraddr[64]; uint256 hashBlock; std::string name,description,format; int32_t numvouts; int64_t datafee = 0;
    if ( myGetTransaction(oracletxid,oracletx,hashBlock) != 0 && (numvouts= oracletx.vout.size()) > 0 )
    {
        if ( DecodeOraclesCreateOpRet(oracletx.vout[numvouts-1].scriptPubKey,name,description,format) == 'C' )
        {
            CCtxidaddr(markeraddr,oracletxid);
            datafee = OracleCurrentDatafee(oracletxid,markeraddr,publisher);
        }
        else
        {
            fprintf(stderr,"Could not decode op_ret from transaction %s\nscriptPubKey: %s\n", oracletxid.GetHex().c_str(), oracletx.vout[numvouts-1].scriptPubKey.ToString().c_str());
        }
    }
    return(datafee);
}

static uint256 myIs_baton_spentinmempool(uint256 batontxid,int32_t batonvout)
{
    std::vector<CTransaction> tmp_txs;
    myGet_mempool_txs(tmp_txs,EVAL_ORACLES,'D');
    for (std::vector<CTransaction>::const_iterator it=tmp_txs.begin(); it!=tmp_txs.end(); it++)
    {
        const CTransaction &tx = *it;
        if ( tx.vout.size() > 0 && tx.vin.size() > 1 && batontxid == tx.vin[1].prevout.hash && batonvout == tx.vin[1].prevout.n )
        {
            const uint256 &txid = tx.GetHash();
            //char str[65]; fprintf(stderr,"found baton spent in mempool %s\n",uint256_str(str,txid));
            return(txid);
        }
    }
    return(batontxid);
}

uint256 OracleBatonUtxo(uint64_t txfee,struct CCcontract_info *cp,uint256 reforacletxid,char *batonaddr,CPubKey publisher,std::vector <uint8_t> &dataarg)
{
    uint256 txid,oracletxid,hashBlock,btxid,batontxid = zeroid; int64_t dfee; int32_t dheight=0,vout,height,numvouts; CTransaction tx; CPubKey pk; uint8_t *ptr; std::vector<uint8_t> vopret,data;
    std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> > unspentOutputs;
    SetCCunspents(unspentOutputs,batonaddr,true);
    for (std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> >::const_iterator it=unspentOutputs.begin(); it!=unspentOutputs.end(); it++)
    {
        txid = it->first.txhash;
        vout = (int32_t)it->first.index;
        height = (int32_t)it->second.blockHeight;
        if ( it->second.satoshis != txfee )
        {
            fprintf(stderr,"it->second.satoshis %llu != %llu txfee\n",(long long)it->second.satoshis,(long long)txfee);
            continue;
        }
        if ( myGetTransaction(txid,tx,hashBlock) != 0 && (numvouts= tx.vout.size()) > 0 )
        {
            GetOpReturnData(tx.vout[numvouts-1].scriptPubKey,vopret);
            if ( vopret.size() > 2 )
            {
                ptr = (uint8_t *)vopret.data();
                if ( (ptr[1] == 'D' && DecodeOraclesData(tx.vout[numvouts-1].scriptPubKey,oracletxid,btxid,pk,data) == 'D') || (ptr[1] == 'R' && DecodeOraclesOpRet(tx.vout[numvouts-1].scriptPubKey,oracletxid,pk,dfee) == 'R') )
                {
                    if ( oracletxid == reforacletxid && pk == publisher )
                    {
                        if ( height > dheight )
                        {
                            dheight = height;
                            batontxid = txid;
                            if ( ptr[1] == 'D' )
                                dataarg = data;
                            //char str[65]; fprintf(stderr,"set batontxid %s height.%d\n",uint256_str(str,batontxid),height);
                        }
                    }
                }
            }
        }
    }
    while ( myIsutxo_spentinmempool(ignoretxid,ignorevin,batontxid,1) != 0 )
        batontxid = myIs_baton_spentinmempool(batontxid,1);
    return(batontxid);
}

uint256 OraclesBatontxid(uint256 reforacletxid,CPubKey refpk)
{
    std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> > unspentOutputs;
    CTransaction regtx; uint256 hash,txid,batontxid,oracletxid; CPubKey pk; int32_t numvouts,height,maxheight=0; int64_t datafee; char markeraddr[64],batonaddr[64]; std::vector <uint8_t> data; struct CCcontract_info *cp,C;
    batontxid = zeroid;
    cp = CCinit(&C,EVAL_ORACLES);
    CCtxidaddr(markeraddr,reforacletxid);
    SetCCunspents(unspentOutputs,markeraddr,false);
    //char str[67]; fprintf(stderr,"markeraddr.(%s) %s\n",markeraddr,pubkey33_str(str,(uint8_t *)&refpk));
    for (std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> >::const_iterator it=unspentOutputs.begin(); it!=unspentOutputs.end(); it++)
    {
        txid = it->first.txhash;
        //fprintf(stderr,"check %s\n",uint256_str(str,txid));
        height = (int32_t)it->second.blockHeight;
        if ( myGetTransaction(txid,regtx,hash) != 0 )
        {
            if ( regtx.vout.size() > 0 && DecodeOraclesOpRet(regtx.vout[regtx.vout.size()-1].scriptPubKey,oracletxid,pk,datafee) == 'R' && oracletxid == reforacletxid && pk == refpk )
            {
                Getscriptaddress(batonaddr,regtx.vout[1].scriptPubKey);
                batontxid = OracleBatonUtxo(10000,cp,oracletxid,batonaddr,pk,data);
                break;
            }
        }
    }
    return(batontxid);
}

int32_t oracle_format(uint256 *hashp,int64_t *valp,char *str,uint8_t fmt,uint8_t *data,int32_t offset,int32_t datalen)
{
    char _str[65]; int32_t sflag = 0,i,val32,len = 0,slen = 0,dlen = 0; uint32_t uval32; uint16_t uval16; int16_t val16; int64_t val = 0; uint64_t uval = 0;
    *valp = 0;
    *hashp = zeroid;
    if ( str != 0 )
        str[0] = 0;
    switch ( fmt )
    {
        case 's': slen = data[offset++]; break;
        case 'S': slen = data[offset++]; slen |= ((int32_t)data[offset++] << 8); break;
        case 'd': dlen = data[offset++]; break;
        case 'D': dlen = data[offset++]; dlen |= ((int32_t)data[offset++] << 8); break;
        case 'c': len = 1; sflag = 1; break;
        case 'C': len = 1; break;
        case 't': len = 2; sflag = 1; break;
        case 'T': len = 2; break;
        case 'i': len = 4; sflag = 1; break;
        case 'I': len = 4; break;
        case 'l': len = 8; sflag = 1; break;
        case 'L': len = 8; break;
        case 'h': len = 32; break;
        default: return(-1); break;
    }
    if ( slen != 0 )
    {
        if ( str != 0 )
        {
            if ( slen < IGUANA_MAXSCRIPTSIZE && offset+slen <= datalen )
            {
                for (i=0; i<slen; i++)
                    str[i] = data[offset++];
                str[i] = 0;
            } else return(-1);
        }
    }
    else if ( dlen != 0 )
    {
        if ( str != 0 )
        {
            if ( dlen < IGUANA_MAXSCRIPTSIZE && offset+dlen <= datalen )
            {
                for (i=0; i<dlen; i++)
                    sprintf(&str[i<<1],"%02x",data[offset++]);
                str[i<<1] = 0;
            } else return(-1);
        }
    }
    else if ( len != 0 && len+offset <= datalen )
    {
        if ( len == 32 )
        {
            iguana_rwbignum(0,&data[offset],len,(uint8_t *)hashp);
            if ( str != 0 )
                sprintf(str,"%s",uint256_str(_str,*hashp));
        }
        else
        {
            if ( sflag != 0 )
            {
                switch ( len )
                {
                    case 1: val = (int8_t)data[offset]; break;
                    case 2: iguana_rwnum(0,&data[offset],len,(void *)&val16); val = val16; break;
                    case 4: iguana_rwnum(0,&data[offset],len,(void *)&val32); val = val32; break;
                    case 8: iguana_rwnum(0,&data[offset],len,(void *)&val); break;
                }
                if ( str != 0 )
                    sprintf(str,"%lld",(long long)val);
                *valp = val;
            }
            else
            {
                switch ( len )
                {
                    case 1: uval = data[offset]; break;
                    case 2: iguana_rwnum(0,&data[offset],len,(void *)&uval16); uval = uval16; break;
                    case 4: iguana_rwnum(0,&data[offset],len,(void *)&uval32); uval = uval32; break;
                    case 8: iguana_rwnum(0,&data[offset],len,(void *)&uval); break;
                }
                if ( str != 0 )
                    sprintf(str,"%llu",(long long)uval);
                *valp = (int64_t)uval;
            }
        }
        offset += len;
    } else return(-1);
    return(offset);
}

int32_t oracle_parse_data_format(std::vector<uint8_t> data,std::string format)
{
    int64_t offset=0,len=0; char fmt;

    for (int i=0; i<format.size();i++)
    {
        fmt=format[i];
        switch (fmt)
        {
            case 's': len = data[offset++]; break;
            case 'S': len = data[offset++]; len |= ((int32_t)data[offset++] << 8); break;
            case 'd': len = data[offset++]; break;
            case 'D': len = data[offset++]; len |= ((int32_t)data[offset++] << 8); break;
            case 'c': len = 1; break;
            case 'C': len = 1; break;
            case 't': len = 2; break;
            case 'T': len = 2; break;
            case 'i': len = 4; break;
            case 'I': len = 4; break;
            case 'l': len = 8; break;
            case 'L': len = 8; break;
            case 'h': len = 32; break;
            default: return(0); break;
        }
        if (len>data.size()-offset) return (0);
        if (fmt=='S' || fmt=='s')
        {
            for (int j=offset;j<offset+len;j++)
                if (data[j]<32 || data[j]>127) return (0);
        }
        offset+=len;
    }
    if (offset!=data.size()) return (0);
    else return (offset);
}

int64_t _correlate_price(int64_t *prices,int32_t n,int64_t price)
{
    int32_t i,count = 0; int64_t diff,threshold = (price >> 8);
    for (i=0; i<n; i++)
    {
        if ( (diff= (price - prices[i])) < 0 )
            diff = -diff;
        if ( diff <= threshold )
            count++;
    }
    if ( count < (n >> 1) )
        return(0);
    else return(price);
}

int64_t correlate_price(int32_t height,int64_t *prices,int32_t n)
{
    int32_t i,j; int64_t price = 0;
    if ( n == 1 )
        return(prices[0]);
    for (i=0; i<n; i++)
    {
        j = (height + i) % n;
        if ( prices[j] != 0 && (price= _correlate_price(prices,n,prices[j])) != 0 )
            break;
    }
    for (i=0; i<n; i++)
        fprintf(stderr,"%llu ",(long long)prices[i]);
    fprintf(stderr,"-> %llu ht.%d\n",(long long)price,height);
    return(price);
}

int64_t OracleCorrelatedPrice(int32_t height,std::vector <int64_t> origprices)
{
    std::vector <int64_t> sorted; int32_t i,n; int64_t *prices,price;
    if ( (n= origprices.size()) == 1 )
        return(origprices[0]);
    std::sort(origprices.begin(), origprices.end());
    prices = (int64_t *)calloc(n,sizeof(*prices));
    i = 0;
    for (std::vector<int64_t>::const_iterator it=sorted.begin(); it!=sorted.end(); it++)
        prices[i++] = *it;
    price = correlate_price(height,prices,i);
    free(prices);
    return(price);
}

int32_t oracleprice_add(std::vector<struct oracleprice_info> &publishers,CPubKey pk,int32_t height,std::vector <uint8_t> data,int32_t maxheight)
{
    struct oracleprice_info item; int32_t flag = 0;
    for (std::vector<struct oracleprice_info>::iterator it=publishers.begin(); it!=publishers.end(); it++)
    {
        if ( pk == it->pk )
        {
            flag = 1;
            if ( height > it->height )
            {
                it->height = height;
                it->data = data;
                return(height);
            }
        }
    }
    if ( flag == 0 )
    {
        item.pk = pk;
        item.data = data;
        item.height = height;
        publishers.push_back(item);
        return(height);
    } else return(0);
}

/*int64_t OraclePrice(int32_t height,uint256 reforacletxid,char *markeraddr,char *format)
{
    std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> > unspentOutputs;
    CTransaction regtx; uint256 hash,txid,oracletxid,batontxid; CPubKey pk; int32_t i,ht,maxheight=0; int64_t datafee,price; char batonaddr[64]; std::vector <uint8_t> data; struct CCcontract_info *cp,C; std::vector <struct oracleprice_info> publishers; std::vector <int64_t> prices;
    if ( format[0] != 'L' )
        return(0);
    cp = CCinit(&C,EVAL_ORACLES);
    SetCCunspents(unspentOutputs,markeraddr,false);
    for (std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> >::const_iterator it=unspentOutputs.begin(); it!=unspentOutputs.end(); it++)
    {
        txid = it->first.txhash;
        ht = (int32_t)it->second.blockHeight;
        if ( myGetTransaction(txid,regtx,hash) != 0 )
        {
            if ( regtx.vout.size() > 0 && DecodeOraclesOpRet(regtx.vout[regtx.vout.size()-1].scriptPubKey,oracletxid,pk,datafee) == 'R' && oracletxid == reforacletxid )
            {
                Getscriptaddress(batonaddr,regtx.vout[1].scriptPubKey);
                batontxid = OracleBatonUtxo(10000,cp,oracletxid,batonaddr,pk,data);
                if ( batontxid != zeroid && (ht= oracleprice_add(publishers,pk,ht,data,maxheight)) > maxheight )
                    maxheight = ht;
            }
        }
    }
    if ( maxheight > 10 )
    {
        for (std::vector<struct oracleprice_info>::const_iterator it=publishers.begin(); it!=publishers.end(); it++)
        {
            if ( it->height >= maxheight-10 )
            {
                oracle_format(&hash,&price,0,'L',(uint8_t *)it->data.data(),0,(int32_t)it->data.size());
                if ( price != 0 )
                    prices.push_back(price);
            }
        }
        return(OracleCorrelatedPrice(height,prices));
    }
    return(0);
}*/

int64_t IsOraclesvout(struct CCcontract_info *cp,const CTransaction& tx,int32_t v)
{
    //char destaddr[64];
    if ( tx.vout[v].scriptPubKey.IsPayToCryptoCondition() != 0 )
    {
        //if ( Getscriptaddress(destaddr,tx.vout[v].scriptPubKey) > 0 && strcmp(destaddr,cp->unspendableCCaddr) == 0 )
            return(tx.vout[v].nValue);
    }
    return(0);
}

bool OraclesDataValidate(struct CCcontract_info *cp,Eval* eval,const CTransaction &tx,uint256 oracletxid,CPubKey publisher,int64_t datafee)
{
    static uint256 zerohash;
    CTransaction vinTx; uint256 hashBlock,activehash; int32_t i,numvins,numvouts; int64_t inputs=0,outputs=0,assetoshis; CScript scriptPubKey;
    numvins = tx.vin.size();
    numvouts = tx.vout.size();
    if ( OracleDatafee(scriptPubKey,oracletxid,publisher) != datafee )
        return eval->Invalid("mismatched datafee");
    scriptPubKey = MakeCC1vout(cp->evalcode,0,publisher).scriptPubKey;
    for (i=0; i<numvins; i++)
    {
        //fprintf(stderr,"vini.%d\n",i);
        if ( (*cp->ismyvin)(tx.vin[i].scriptSig) != 0 )
        {
            if ( i == 0 )
                return eval->Invalid("unexpected vin.0 is CC");
            //fprintf(stderr,"vini.%d check mempool\n",i);
            else if ( eval->GetTxUnconfirmed(tx.vin[i].prevout.hash,vinTx,hashBlock) == 0 )
                return eval->Invalid("cant find vinTx");
            else
            {
                //fprintf(stderr,"vini.%d check hash and vout\n",i);
                //if ( hashBlock == zerohash )
                //    return eval->Invalid("cant Oracles from mempool");
                if ( (assetoshis= IsOraclesvout(cp,vinTx,tx.vin[i].prevout.n)) != 0 )
                {
                    if ( i == 1 && vinTx.vout[1].scriptPubKey != tx.vout[1].scriptPubKey )
                        return eval->Invalid("baton violation");
                    else if ( i != 1 && scriptPubKey == vinTx.vout[tx.vin[i].prevout.n].scriptPubKey )
                        inputs += assetoshis;
                }
            }
        }
    }
    for (i=0; i<numvouts; i++)
    {
        //fprintf(stderr,"i.%d of numvouts.%d\n",i,numvouts);
        if ( (assetoshis= IsOraclesvout(cp,tx,i)) != 0 )
        {
            if ( i < 2 )
            {
                if ( i == 0 )
                {
                    if ( tx.vout[0].scriptPubKey == scriptPubKey )
                        outputs += assetoshis;
                    else return eval->Invalid("invalid CC vout CC destination");
                }
            }
        }
    }
    if ( inputs != outputs+datafee )
    {
        fprintf(stderr,"inputs %llu vs outputs %llu + datafee %llu\n",(long long)inputs,(long long)outputs,(long long)datafee);
        return eval->Invalid("mismatched inputs != outputs + datafee");
    }
    else return(true);
}

/*nt32_t GetLatestTimestamp(int32_t height)
{
    if ( KOMODO_NSPV_SUPERLITE ) return (NSPV_blocktime(height));
    return(komodo_heightstamp(height));
} */

bool OraclesValidate(struct CCcontract_info *cp,Eval* eval,const CTransaction &tx, uint32_t nIn)
{
    uint256 oracletxid,batontxid,txid; uint64_t txfee=10000; int32_t numvins,numvouts,preventCCvins,preventCCvouts; int64_t amount; uint256 hashblock;
    uint8_t *script; std::vector<uint8_t> vopret,data; CPubKey publisher,tmppk,oraclespk; char tmpaddress[64],vinaddress[64],oraclesaddr[64];
    CTransaction tmptx; std::string name,desc,format;

    numvins = tx.vin.size();
    numvouts = tx.vout.size();
    preventCCvins = preventCCvouts = -1;
    if ( numvouts < 1 )
        return eval->Invalid("no vouts");
    else
    {
        GetOpReturnData(tx.vout[numvouts-1].scriptPubKey,vopret);
        if ( vopret.size() > 2 )
        {   
            oraclespk=GetUnspendable(cp,0);
            Getscriptaddress(oraclesaddr,CScript() << ParseHex(HexStr(oraclespk)) << OP_CHECKSIG);
            script = (uint8_t *)vopret.data();
            switch ( script[1] )
            {
                case 'C': // create
                    // vins.*: normal inputs
                    // vout.0: txfee tag to oracle normal address
                    // vout.1: change, if any
                    // vout.n-1: opreturn with name and description and format for data
                    return eval->Invalid("unexpected OraclesValidate for create");
                    break;
                case 'F': // fund (activation on Jul 15th 2019 00:00)
                    // vins.*: normal inputs
                    // vout.0: txfee to oracle CC address of users pubkey
                    // vout.1: change, if any
                    // vout.n-1: opreturn with createtxid, pubkey and amount
                    return eval->Invalid("unexpected OraclesValidate for create");
                    break;
                case 'R': // register
                    // vin.0: normal inputs
                    // vin.n-1: CC input from pubkeys oracle CC addres - to prove that register came from pubkey that is registred (activation on Jul 15th 2019 00:00)
                    // vout.0: txfee tag to normal marker address
                    // vout.1: baton CC utxo
                    // vout.2: marker from oraclesfund tx to normal pubkey address (activation on Jul 15th 2019 00:00)
                    // vout.n-2: change, if any
                    // vout.n-1: opreturn with createtxid, pubkey and price per data point
                    if (GetLatestTimestamp(eval->GetCurrentHeight())>PUBKEY_SPOOFING_FIX_ACTIVATION)
                    {
                        if ((numvouts=tx.vout.size()) < 1 || DecodeOraclesOpRet(tx.vout[numvouts-1].scriptPubKey,oracletxid,tmppk,amount)!='R')
                            return eval->Invalid("invalid oraclesregister OP_RETURN data!"); 
                        else if (myGetTransaction(oracletxid,tmptx,hashblock) == 0)
                            return eval->Invalid("invalid oraclescreate txid!");
                        else if ((numvouts=tmptx.vout.size()) < 1 || DecodeOraclesCreateOpRet(tmptx.vout[numvouts-1].scriptPubKey,name,desc,format)!='C')
                            return eval->Invalid("invalid oraclescreate OP_RETURN data!"); 
                        else if ( IsCCInput(tmptx.vin[0].scriptSig) != 0 )
                            return eval->Invalid("vin.0 is normal for oraclescreate!");
                        else if (ConstrainVout(tmptx.vout[0],0,oraclesaddr,txfee)==0)
                            return eval->Invalid("invalid marker for oraclescreate!");
                        else if ( IsCCInput(tx.vin[0].scriptSig) != 0 )
                            return eval->Invalid("vin.0 is normal for oraclesregister!");
                        else if ((*cp->ismyvin)(tx.vin[1].scriptSig) == 0 && (*cp->ismyvin)(tx.vin[tx.vin.size()-1].scriptSig) == 0)
                            return eval->Invalid("there is no CC vin from oraclesfund tx");
                        else if ((*cp->ismyvin)(tx.vin[1].scriptSig) == 1 && (myGetTransaction(tx.vin[1].prevout.hash,tmptx,hashblock)==0 || DecodeOraclesOpRet(tmptx.vout[tmptx.vout.size()-1].scriptPubKey,txid,tmppk,amount)!='F'
                                || tmptx.vout[tx.vin[1].prevout.n].nValue!=CC_MARKER_VALUE || !Getscriptaddress(vinaddress,tmptx.vout[tx.vin[1].prevout.n].scriptPubKey)
                                || !GetCCaddress(cp,tmpaddress,tmppk) || strcmp(tmpaddress,vinaddress)!=0) || oracletxid!=txid)
                            return eval->Invalid("invalid vin.1 for oraclesregister, it must be CC vin or pubkey not same as vin pubkey, register and fund tx must be done from owner of pubkey that registers to oracle!!");    
                        else if ((*cp->ismyvin)(tx.vin[tx.vin.size()-1].scriptSig) == 1 && (myGetTransaction(tx.vin[tx.vin.size()-1].prevout.hash,tmptx,hashblock)==0 || DecodeOraclesOpRet(tmptx.vout[tmptx.vout.size()-1].scriptPubKey,txid,tmppk,amount)!='F'
                                || tmptx.vout[tx.vin[tx.vin.size()-1].prevout.n].nValue!=CC_MARKER_VALUE || !Getscriptaddress(vinaddress,tmptx.vout[tx.vin[tx.vin.size()-1].prevout.n].scriptPubKey)
                                || !GetCCaddress(cp,tmpaddress,tmppk) || strcmp(tmpaddress,vinaddress)!=0) || oracletxid!=txid)
                            return eval->Invalid("invalid vin."+std::to_string(tx.vin.size()-1)+" for oraclesregister, it must be CC vin or pubkey not same as vin pubkey, register and fund tx must be done from owner of pubkey that registers to oracle!!");
                        else if (CCtxidaddr(tmpaddress,oracletxid).IsValid() && ConstrainVout(tx.vout[0],0,tmpaddress,txfee)==0)
                            return eval->Invalid("invalid marker for oraclesregister!");
                        else if (!Getscriptaddress(tmpaddress,CScript() << ParseHex(HexStr(tmppk)) << OP_CHECKSIG) || ConstrainVout(tx.vout[2],0,tmpaddress,CC_MARKER_VALUE)==0)
                            return eval->Invalid("pubkey in OP_RETURN and in vout.2 not matching, register must be done from owner of pubkey that registers to oracle!");
                    }
                    else return eval->Invalid("unexpected OraclesValidate for register");
                    break;
                case 'S': // subscribe
                    // vins.*: normal inputs
                    // vout.0: subscription fee to publishers CC address
                    // vout.1: change, if any
                    // vout.n-1: opreturn with createtxid, registered provider's pubkey, amount
                    return eval->Invalid("unexpected OraclesValidate for subscribe");
                    break;
                case 'D': // data
                    // vin.0: normal input
                    // vin.1: baton CC utxo (most of the time)
                    // vin.2+: subscription vout.0
                    // vout.0: change to publishers CC address
                    // vout.1: baton CC utxo
                    // vout.2: payment for dataprovider
                    // vout.3: change, if any
                    if ( numvins >= 2 && numvouts >= 3 && DecodeOraclesData(tx.vout[numvouts-1].scriptPubKey,oracletxid,batontxid,publisher,data) == 'D' )
                    {
                        if ( OraclesDataValidate(cp,eval,tx,oracletxid,publisher,tx.vout[2].nValue) != 0 )
                        {
                            return(true);
                        } else return(false);
                    }
                    return eval->Invalid("unexpected OraclesValidate 'D' tx invalid");
                    break;
                default:
                    fprintf(stderr,"illegal oracles funcid.(%c)\n",script[1]);
                    return eval->Invalid("unexpected OraclesValidate funcid");
                    break;
            }
        } else return eval->Invalid("unexpected oracles missing funcid");
        return(PreventCC(eval,tx,preventCCvins,numvins,preventCCvouts,numvouts));
    }
    return(true);
}
// end of consensus code

// helper functions for rpc calls in rpcwallet.cpp

int64_t AddOracleInputs(struct CCcontract_info *cp,CMutableTransaction &mtx,uint256 oracletxid,CPubKey pk,int64_t total,int32_t maxinputs)
{
    char coinaddr[64],funcid; int64_t nValue,price,totalinputs = 0; uint256 tmporacletxid,tmpbatontxid,txid,hashBlock; std::vector<uint8_t> origpubkey,data; CTransaction vintx; int32_t numvouts,vout,n = 0;
    std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> > unspentOutputs; CPubKey tmppk; int64_t tmpnum;
    GetCCaddress(cp,coinaddr,pk);
    SetCCunspents(unspentOutputs,coinaddr,true);
    //fprintf(stderr,"addoracleinputs from (%s)\n",coinaddr);
    for (std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> >::const_iterator it=unspentOutputs.begin(); it!=unspentOutputs.end(); it++)
    {
        txid = it->first.txhash;
        vout = (int32_t)it->first.index;
        //char str[65]; fprintf(stderr,"oracle check %s/v%d\n",uint256_str(str,txid),vout);
        if ( myGetTransaction(txid,vintx,hashBlock) != 0 && (numvouts=vintx.vout.size()-1)>0)
        {
            if ((funcid=DecodeOraclesOpRet(vintx.vout[numvouts].scriptPubKey,tmporacletxid,tmppk,tmpnum))!=0 && (funcid=='S' || funcid=='D'))
            {
                if (funcid=='D' && DecodeOraclesData(vintx.vout[numvouts].scriptPubKey,tmporacletxid,tmpbatontxid,tmppk,data)==0)
                    fprintf(stderr,"invalid oraclesdata transaction \n");
                else if (tmporacletxid==oracletxid)
                {  
                    // get valid CC payments
                    if ( (nValue= IsOraclesvout(cp,vintx,vout)) >= 10000 && myIsutxo_spentinmempool(ignoretxid,ignorevin,txid,vout) == 0 )
                    {
                        if ( total != 0 && maxinputs != 0 )
                            mtx.vin.push_back(CTxIn(txid,vout,CScript()));
                        nValue = it->second.satoshis;
                        totalinputs += nValue;
                        n++;
                        if ( (total > 0 && totalinputs >= total) || (maxinputs > 0 && n >= maxinputs) )
                            break;
                    } //else fprintf(stderr,"nValue %.8f or utxo memspent\n",(double)nValue/COIN);
                }            
            }
        } else fprintf(stderr,"couldnt find transaction\n");
    }
    return(totalinputs);
}

int64_t LifetimeOraclesFunds(struct CCcontract_info *cp,uint256 oracletxid,CPubKey publisher)
{
    char coinaddr[64]; CPubKey pk; int64_t total=0,num; uint256 txid,hashBlock,subtxid; CTransaction subtx;
    std::vector<uint256> txids;
    GetCCaddress(cp,coinaddr,publisher);
    SetCCtxids(txids,coinaddr,true,cp->evalcode,oracletxid,'S');
    //fprintf(stderr,"scan lifetime of %s\n",coinaddr);
    for (std::vector<uint256>::const_iterator it=txids.begin(); it!=txids.end(); it++)
    {
        txid = *it;
        if ( myGetTransaction(txid,subtx,hashBlock) != 0 )
        {
            if ( subtx.vout.size() > 0 && DecodeOraclesOpRet(subtx.vout[subtx.vout.size()-1].scriptPubKey,subtxid,pk,num) == 'S' && subtxid == oracletxid && pk == publisher )
            {
                total += subtx.vout[0].nValue;
            }
        }
    }
    return(total);
}

int64_t AddMyOraclesFunds(struct CCcontract_info *cp,CMutableTransaction &mtx,CPubKey pk,uint256 oracletxid)
{
    char coinaddr[64],funcid; int64_t nValue,tmpamount; uint256 tmporacletxid,txid,hashBlock,ignoretxid; int32_t numvouts,vout,ignorevin;
    std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> > unspentOutputs; CTransaction vintx; CPubKey tmppk;

    GetCCaddress(cp,coinaddr,pk);
    SetCCunspents(unspentOutputs,coinaddr,true);
    for (std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> >::const_iterator it=unspentOutputs.begin(); it!=unspentOutputs.end(); it++)
    {
        txid = it->first.txhash;
        vout = (int32_t)it->first.index;
        nValue = it->second.satoshis;
        if ( myGetTransaction(txid,vintx,hashBlock) != 0 && (numvouts=vintx.vout.size())>0)
        {
            if ((funcid=DecodeOraclesOpRet(vintx.vout[numvouts-1].scriptPubKey,tmporacletxid,tmppk,tmpamount))!=0 && funcid=='F' && tmppk==pk 
            && tmporacletxid==oracletxid && tmpamount==nValue && myIsutxo_spentinmempool(ignoretxid,ignorevin,txid,vout)==0)
            {
                mtx.vin.push_back(CTxIn(txid,vout,CScript()));
                return (nValue);
            }
        } else fprintf(stderr,"couldnt find transaction\n");
    }

    std::vector<CTransaction> tmp_txs;
    myGet_mempool_txs(tmp_txs,EVAL_ORACLES,'F');
    for (std::vector<CTransaction>::const_iterator it=tmp_txs.begin(); it!=tmp_txs.end(); it++)
    {
        const CTransaction &txmempool = *it;
        const uint256 &hash = txmempool.GetHash();
        nValue=txmempool.vout[0].nValue;

        if ((funcid=DecodeOraclesOpRet(txmempool.vout[txmempool.vout.size()-1].scriptPubKey,tmporacletxid,tmppk,tmpamount))!=0 && funcid=='F' 
        && tmppk==pk && tmporacletxid==oracletxid && tmpamount==nValue && myIsutxo_spentinmempool(ignoretxid,ignorevin,hash,0)==0)
        {
            mtx.vin.push_back(CTxIn(hash,0,CScript()));
            return (nValue);
        }
    }
    return (0);
}

UniValue OracleCreate(const CPubKey& pk, int64_t txfee,std::string name,std::string description,std::string format)
{
    CMutableTransaction mtx = CreateNewContextualCMutableTransaction(Params().GetConsensus(), komodo_nextheight());
    CPubKey mypk,Oraclespk; struct CCcontract_info *cp,C; char fmt; 

    cp = CCinit(&C,EVAL_ORACLES);
    if ( name.size() > 32)
        CCERR_RESULT("oraclescc",CCLOG_INFO, stream << "name."<< (int32_t)name.size() << " must be less then 32");   
    if (description.size() > 4096)
        CCERR_RESULT("oraclescc",CCLOG_INFO, stream << "description."<< (int32_t)description.size() << " must be less then 4096");
    if (format.size() > 4096 )
        CCERR_RESULT("oraclescc",CCLOG_INFO, stream << "format."<< (int32_t)format.size() << " must be less then 4096");
    if ( name.size() == 0 )
        CCERR_RESULT("oraclescc",CCLOG_INFO, stream << "name must not be empty");   
    for(int i = 0; i < format.size(); i++)
    {
        fmt=format[i];
        switch (fmt)
        {
            case 's': case 'S': case 'd': case 'D':
            case 'c': case 'C': case 't': case 'T':
            case 'i': case 'I': case 'l': case 'L':
            case 'h': break;
            default: CCERR_RESULT("oraclescc",CCLOG_INFO, stream << "invalid format type");
        }
    }    
    if ( txfee == 0 )
        txfee = 10000;
    mypk = pk.IsValid()?pk:pubkey2pk(Mypubkey());
    Oraclespk = GetUnspendable(cp,0);
    if ( AddNormalinputs(mtx,mypk,2*txfee,3,pk.IsValid()) > 0 )
    {
        mtx.vout.push_back(CTxOut(txfee,CScript() << ParseHex(HexStr(Oraclespk)) << OP_CHECKSIG));
        return(FinalizeCCTxExt(pk.IsValid(),0,cp,mtx,mypk,txfee,EncodeOraclesCreateOpRet('C',name,description,format)));
    }
    CCERR_RESULT("oraclescc",CCLOG_INFO, stream << "error adding normal inputs");
}

UniValue OracleFund(const CPubKey& pk, int64_t txfee,uint256 oracletxid)
{
    CMutableTransaction mtx = CreateNewContextualCMutableTransaction(Params().GetConsensus(), komodo_nextheight()); CTransaction tx;
    CPubKey mypk,oraclespk; struct CCcontract_info *cp,C; std::string name,desc,format; int32_t numvouts; uint256 hashBlock;

    if (GetLatestTimestamp(komodo_currentheight())<PUBKEY_SPOOFING_FIX_ACTIVATION)
        CCERR_RESULT("oraclescc",CCLOG_INFO, stream << "oraclesfund not active yet, activation scheduled for July 15th");
    if (myGetTransaction(oracletxid,tx,hashBlock)==0 || (numvouts=tx.vout.size())<=0)
        CCERR_RESULT("oraclecc",CCLOG_INFO, stream << "cant find oracletxid " << oracletxid.GetHex());
    if (DecodeOraclesCreateOpRet(tx.vout[numvouts-1].scriptPubKey,name,desc,format)!='C')
        CCERR_RESULT("oraclecc",CCLOG_INFO, stream << "invalid oracletxid " << oracletxid.GetHex());
    cp = CCinit(&C,EVAL_ORACLES);
    if ( txfee == 0 )
        txfee = 10000;
    mypk = pk.IsValid()?pk:pubkey2pk(Mypubkey());
    if (AddNormalinputs(mtx,mypk,txfee+CC_MARKER_VALUE,2,pk.IsValid()))    
    {
        mtx.vout.push_back(MakeCC1vout(cp->evalcode,CC_MARKER_VALUE,mypk));
        return(FinalizeCCTxExt(pk.IsValid(),0,cp,mtx,mypk,txfee,EncodeOraclesOpRet('F',oracletxid,mypk,CC_MARKER_VALUE)));
    }
    CCERR_RESULT("oraclescc",CCLOG_INFO, stream << "error adding normal inputs");
}

UniValue OracleRegister(const CPubKey& pk, int64_t txfee,uint256 oracletxid,int64_t datafee)
{
    CMutableTransaction mtx = CreateNewContextualCMutableTransaction(Params().GetConsensus(), komodo_nextheight());
    CPubKey mypk,markerpubkey,batonpk,oraclespk; struct CCcontract_info *cp,C; char markeraddr[64],batonaddr[64];
    std::string name,desc,format; int32_t numvouts; uint256 hashBlock; CTransaction tx;

    cp = CCinit(&C,EVAL_ORACLES);
    if ( txfee == 0 )
        txfee = 10000;
    if (myGetTransaction(oracletxid,tx,hashBlock)==0 || (numvouts=tx.vout.size())<=0)
        CCERR_RESULT("oraclecc",CCLOG_INFO, stream << "cant find oracletxid " << oracletxid.GetHex());
    if (DecodeOraclesCreateOpRet(tx.vout[numvouts-1].scriptPubKey,name,desc,format)!='C')
        CCERR_RESULT("oraclecc",CCLOG_INFO, stream << "invalid oracletxid " << oracletxid.GetHex());
    if ( datafee < txfee )
        CCERR_RESULT("oraclescc",CCLOG_INFO, stream << "datafee must be txfee or more");
    mypk = pk.IsValid()?pk:pubkey2pk(Mypubkey());
    oraclespk = GetUnspendable(cp,0);
    batonpk = OracleBatonPk(batonaddr,cp);
    markerpubkey = CCtxidaddr(markeraddr,oracletxid);
    if (AddNormalinputs(mtx,mypk,3*txfee,4,pk.IsValid()))
    {
        if (GetLatestTimestamp(komodo_currentheight())>PUBKEY_SPOOFING_FIX_ACTIVATION && AddMyOraclesFunds(cp,mtx,mypk,oracletxid)!=CC_MARKER_VALUE)
            CCERR_RESULT("oraclescc",CCLOG_INFO, stream << "error adding inputs from your Oracles CC address, please fund it first with oraclesfund rpc!");
        mtx.vout.push_back(CTxOut(txfee,CScript() << ParseHex(HexStr(markerpubkey)) << OP_CHECKSIG));
        mtx.vout.push_back(MakeCC1vout(cp->evalcode,txfee,batonpk));
        if (GetLatestTimestamp(komodo_get_current_height())>PUBKEY_SPOOFING_FIX_ACTIVATION) mtx.vout.push_back(CTxOut(txfee,CScript() << ParseHex(HexStr(mypk)) << OP_CHECKSIG));
        return(FinalizeCCTxExt(pk.IsValid(),0,cp,mtx,mypk,txfee,EncodeOraclesOpRet('R',oracletxid,mypk,datafee)));
    }
    CCERR_RESULT("oraclescc",CCLOG_INFO, stream << "error adding normal inputs");
}

UniValue OracleSubscribe(const CPubKey& pk, int64_t txfee,uint256 oracletxid,CPubKey publisher,int64_t amount)
{
    CMutableTransaction mtx = CreateNewContextualCMutableTransaction(Params().GetConsensus(), komodo_nextheight()); CTransaction tx;
    CPubKey mypk,markerpubkey; struct CCcontract_info *cp,C; char markeraddr[64]; std::string name,desc,format; int32_t numvouts; uint256 hashBlock;
    cp = CCinit(&C,EVAL_ORACLES);
    if ( txfee == 0 )
        txfee = 10000;
    if (myGetTransaction(oracletxid,tx,hashBlock)==0 || (numvouts=tx.vout.size())<=0)
        CCERR_RESULT("oraclecc",CCLOG_INFO, stream << "cant find oracletxid " << oracletxid.GetHex());
    if (DecodeOraclesCreateOpRet(tx.vout[numvouts-1].scriptPubKey,name,desc,format)!='C')
        CCERR_RESULT("oraclecc",CCLOG_INFO, stream << "invalid oracletxid " << oracletxid.GetHex());
    mypk = pk.IsValid()?pk:pubkey2pk(Mypubkey());
    markerpubkey = CCtxidaddr(markeraddr,oracletxid);
    if ( AddNormalinputs(mtx,mypk,amount + 2*txfee,64,pk.IsValid()) > 0 )
    {
        mtx.vout.push_back(MakeCC1vout(cp->evalcode,amount,publisher));
        mtx.vout.push_back(CTxOut(txfee,CScript() << ParseHex(HexStr(markerpubkey)) << OP_CHECKSIG));
        return(FinalizeCCTxExt(pk.IsValid(),0,cp,mtx,mypk,txfee,EncodeOraclesOpRet('S',oracletxid,mypk,amount)));
    }
    CCERR_RESULT("oraclescc",CCLOG_INFO, stream << "error adding normal inputs");
}

UniValue OracleData(const CPubKey& pk, int64_t txfee,uint256 oracletxid,std::vector <uint8_t> data)
{
    CMutableTransaction mtx = CreateNewContextualCMutableTransaction(Params().GetConsensus(), komodo_nextheight());
    CScript pubKey; CPubKey mypk,batonpk; int64_t offset,datafee,inputs,CCchange = 0; struct CCcontract_info *cp,C; uint256 batontxid,hashBlock;
    char coinaddr[64],batonaddr[64]; std::vector <uint8_t> prevdata; CTransaction tx; std::string name,description,format; int32_t len,numvouts;

    cp = CCinit(&C,EVAL_ORACLES);
    mypk = pk.IsValid()?pk:pubkey2pk(Mypubkey());
    if ( data.size() > 8192 )
        CCERR_RESULT("oraclescc",CCLOG_INFO, stream << "datasize " << (int32_t)data.size() << " is too big");
    if ( (datafee= OracleDatafee(pubKey,oracletxid,mypk)) <= 0 )
        CCERR_RESULT("oraclescc",CCLOG_INFO, stream << "datafee " << (double)datafee/COIN << "is illegal");
    if ( myGetTransaction(oracletxid,tx,hashBlock) != 0 && (numvouts=tx.vout.size()) > 0 )
    {
        if ( DecodeOraclesCreateOpRet(tx.vout[numvouts-1].scriptPubKey,name,description,format) == 'C' )
        {
            if (oracle_parse_data_format(data,format)==0)
                CCERR_RESULT("oraclescc",CCLOG_INFO, stream << "data does not match length or content format specification");
        }
        else
            CCERR_RESULT("oraclescc",CCLOG_INFO, stream << "invalid oracle txid opret data");
    }
    else
        CCERR_RESULT("oraclescc",CCLOG_INFO, stream << "invalid oracle txid");
    if ( txfee == 0 )
        txfee = 10000;
    GetCCaddress(cp,coinaddr,mypk);
    if ( AddNormalinputs(mtx,mypk,2*txfee,3,pk.IsValid()) > 0 ) // have enough funds even if baton utxo not there
    {
        batonpk = OracleBatonPk(batonaddr,cp);
        batontxid = OracleBatonUtxo(txfee,cp,oracletxid,batonaddr,mypk,prevdata);
        if ( batontxid != zeroid ) // not impossible to fail, but hopefully a very rare event
            mtx.vin.push_back(CTxIn(batontxid,1,CScript()));
        else fprintf(stderr,"warning: couldnt find baton utxo %s\n",batonaddr);
        if ( (inputs= AddOracleInputs(cp,mtx,oracletxid,mypk,datafee,60)) > 0 )
        {
            if ( inputs > datafee )
                CCchange = (inputs - datafee);
            mtx.vout.push_back(MakeCC1vout(cp->evalcode,CCchange,mypk));
            mtx.vout.push_back(MakeCC1vout(cp->evalcode,CC_MARKER_VALUE,batonpk));
            mtx.vout.push_back(CTxOut(datafee,CScript() << ParseHex(HexStr(mypk)) << OP_CHECKSIG));
            return(FinalizeCCTxExt(pk.IsValid(),0,cp,mtx,mypk,txfee,EncodeOraclesData('D',oracletxid,batontxid,mypk,data)));
        } else
            CCERR_RESULT("oraclescc",CCLOG_INFO, stream << "couldnt find enough oracle inputs " << coinaddr << ", limit 1 per utxo");
    }
    CCERR_RESULT("oraclescc",CCLOG_INFO, stream << "couldnt add normal inputs");
}

UniValue OracleFormat(uint8_t *data,int32_t datalen,char *format,int32_t formatlen)
{
    UniValue obj(UniValue::VARR); uint256 hash; int32_t i,j=0; int64_t val; char str[IGUANA_MAXSCRIPTSIZE*2+1];
    for (i=0; i<formatlen && j<datalen; i++)
    {
        str[0] = 0;
        j = oracle_format(&hash,&val,str,format[i],data,j,datalen);
        obj.push_back(str);
        if ( j < 0 )
            break;
        if ( j >= datalen )
            break;
    }
    return(obj);
}

UniValue OracleDataSample(uint256 reforacletxid,uint256 txid)
{
    UniValue result(UniValue::VOBJ); CTransaction tx,oracletx; uint256 hashBlock,btxid,oracletxid;  std::string error;
    CPubKey pk; std::string name,description,format; int32_t numvouts; std::vector<uint8_t> data; char str[67], *formatstr = 0;
    
    result.push_back(Pair("result","success"));
    if ( myGetTransaction(reforacletxid,oracletx,hashBlock) != 0 && (numvouts=oracletx.vout.size()) > 0 )
    {
        if ( DecodeOraclesCreateOpRet(oracletx.vout[numvouts-1].scriptPubKey,name,description,format) == 'C' )
        {
            if ( myGetTransaction(txid,tx,hashBlock) != 0 && (numvouts=tx.vout.size()) > 0 )
            {
                if ( DecodeOraclesData(tx.vout[numvouts-1].scriptPubKey,oracletxid,btxid,pk,data) == 'D' && reforacletxid == oracletxid )
                {
                    if ( (formatstr= (char *)format.c_str()) == 0 )
                        formatstr = (char *)"";                    
                    result.push_back(Pair("txid",uint256_str(str,txid)));
                    result.push_back(Pair("data",OracleFormat((uint8_t *)data.data(),(int32_t)data.size(),formatstr,(int32_t)format.size())));
                    return(result);                  
                }
                else error="invalid data tx";
            }
            else error="cannot find data txid";
        }
        else error="invalid oracles txid";
    }
    else error="cannot find oracles txid";
    result.push_back(Pair("result","error")); 
    result.push_back(Pair("error",error));    
    return(result);
}

UniValue OracleDataSamples(uint256 reforacletxid,char* batonaddr,int32_t num)
{
    UniValue result(UniValue::VOBJ),b(UniValue::VARR); CTransaction tx,oracletx; uint256 txid,hashBlock,btxid,oracletxid; 
    CPubKey pk; std::string name,description,format; int32_t numvouts,n=0,vout; std::vector<uint8_t> data; char *formatstr = 0, addr[64];
    std::vector<uint256> txids; int64_t nValue;
    
    result.push_back(Pair("result","success"));
    if ( myGetTransaction(reforacletxid,oracletx,hashBlock) != 0 && (numvouts=oracletx.vout.size()) > 0 )
    {
        if ( DecodeOraclesCreateOpRet(oracletx.vout[numvouts-1].scriptPubKey,name,description,format) == 'C' )
        {
            std::vector<CTransaction> tmp_txs;
            myGet_mempool_txs(tmp_txs,EVAL_ORACLES,'D');
            for (std::vector<CTransaction>::const_iterator it=tmp_txs.begin(); it!=tmp_txs.end(); it++)
            {
                const CTransaction &txmempool = *it;
                const uint256 &hash = txmempool.GetHash();
                if ((numvouts=txmempool.vout.size())>0 && txmempool.vout[1].nValue==CC_MARKER_VALUE && DecodeOraclesData(txmempool.vout[numvouts-1].scriptPubKey,oracletxid,btxid,pk,data) == 'D' && reforacletxid == oracletxid )
                {
                    Getscriptaddress(addr,txmempool.vout[1].scriptPubKey);
                    if (strcmp(addr,batonaddr)!=0) continue;
                    if ( (formatstr= (char *)format.c_str()) == 0 )
                        formatstr = (char *)"";
                    UniValue a(UniValue::VOBJ);
                    a.push_back(Pair("txid",hash.GetHex()));
                    a.push_back(Pair("data",OracleFormat((uint8_t *)data.data(),(int32_t)data.size(),formatstr,(int32_t)format.size())));            
                    b.push_back(a);
                    if ( ++n >= num && num != 0)
                    {
                        result.push_back(Pair("samples",b));
                        return(result);
                    }
                }
            }
            SetCCtxids(txids,batonaddr,true,EVAL_ORACLES,reforacletxid,'D');
            if (txids.size()>0)
            {
                for (std::vector<uint256>::const_iterator it=txids.end()-1; it!=txids.begin(); it--)
                {
                    txid=*it;
                    if (myGetTransaction(txid,tx,hashBlock) != 0 && (numvouts=tx.vout.size()) > 0 )
                    {
                        if ( tx.vout[1].nValue==CC_MARKER_VALUE && DecodeOraclesData(tx.vout[numvouts-1].scriptPubKey,oracletxid,btxid,pk,data) == 'D' && reforacletxid == oracletxid )
                        {
                            if ( (formatstr= (char *)format.c_str()) == 0 )
                                formatstr = (char *)"";
                            UniValue a(UniValue::VOBJ);
                            a.push_back(Pair("txid",txid.GetHex()));
                            a.push_back(Pair("data",OracleFormat((uint8_t *)data.data(),(int32_t)data.size(),formatstr,(int32_t)format.size())));                            
                            b.push_back(a);
                            if ( ++n >= num && num != 0)
                            {
                                result.push_back(Pair("samples",b));
                                return(result);
                            }
                        }
                    }
                }
            }
        }
        else
            CCERR_RESULT("pegscc",CCLOG_INFO, stream << "invalid oracletxid " << oracletxid.GetHex());
    }
    else
        CCERR_RESULT("oraclescc",CCLOG_INFO, stream << "cant find oracletxid " << oracletxid.GetHex());
    result.push_back(Pair("samples",b));
    return(result);
}

UniValue OracleInfo(uint256 origtxid)
{
    UniValue result(UniValue::VOBJ),a(UniValue::VARR);
    std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> > unspentOutputs; int32_t height;
    CMutableTransaction mtx = CreateNewContextualCMutableTransaction(Params().GetConsensus(), komodo_nextheight());
    CTransaction tx; std::string name,description,format; uint256 hashBlock,txid,oracletxid,batontxid; CPubKey pk;
    struct CCcontract_info *cp,C; int64_t datafee,funding; char str[67],markeraddr[64],numstr[64],batonaddr[64]; std::vector <uint8_t> data;
    std::map<CPubKey,std::pair<uint256,int32_t>> publishers;

    cp = CCinit(&C,EVAL_ORACLES);
    CCtxidaddr(markeraddr,origtxid);
    if ( myGetTransaction(origtxid,tx,hashBlock) == 0 )
    {
        fprintf(stderr,"cant find oracleid\n");
        result.push_back(Pair("result","error"));
        result.push_back(Pair("error","cant find oracleid"));
        return(result);
    }
    if ( myGetTransaction(origtxid,tx,hashBlock) != 0 )
    {
        if ( tx.vout.size() > 0 && DecodeOraclesCreateOpRet(tx.vout[tx.vout.size()-1].scriptPubKey,name,description,format) == 'C' )
        {
            result.push_back(Pair("result","success"));
            result.push_back(Pair("txid",uint256_str(str,origtxid)));
            result.push_back(Pair("name",name));
            result.push_back(Pair("description",description));
            result.push_back(Pair("format",format));
            result.push_back(Pair("marker",markeraddr));
            SetCCunspents(unspentOutputs,markeraddr,false);
            for (std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> >::const_iterator it=unspentOutputs.begin(); it!=unspentOutputs.end(); it++)
            {
                txid = it->first.txhash;
                height = (int32_t)it->second.blockHeight;
                if ( myGetTransaction(txid,tx,hashBlock) != 0 && tx.vout.size() > 0 &&
                    DecodeOraclesOpRet(tx.vout[tx.vout.size()-1].scriptPubKey,oracletxid,pk,datafee) == 'R' && oracletxid == origtxid )
                {
                    if (publishers.find(pk)==publishers.end() || height>publishers[pk].second)
                    {
                        publishers[pk].first=txid;
                        publishers[pk].second=height;
                    }
                }
            }
            for (std::map<CPubKey,std::pair<uint256,int32_t>>::iterator it = publishers.begin(); it != publishers.end(); ++it)
            {
                if ( myGetTransaction(it->second.first,tx,hashBlock) != 0 && DecodeOraclesOpRet(tx.vout[tx.vout.size()-1].scriptPubKey,oracletxid,pk,datafee) == 'R')
                {
                    UniValue obj(UniValue::VOBJ);
                    obj.push_back(Pair("publisher",pubkey33_str(str,(uint8_t *)pk.begin())));
                    Getscriptaddress(batonaddr,tx.vout[1].scriptPubKey);
                    batontxid = OracleBatonUtxo(10000,cp,oracletxid,batonaddr,pk,data);
                    obj.push_back(Pair("baton",batonaddr));
                    obj.push_back(Pair("batontxid",uint256_str(str,batontxid)));
                    funding = LifetimeOraclesFunds(cp,oracletxid,pk);
                    sprintf(numstr,"%.8f",(double)funding/COIN);
                    obj.push_back(Pair("lifetime",numstr));
                    funding = AddOracleInputs(cp,mtx,oracletxid,pk,0,0);
                    sprintf(numstr,"%.8f",(double)funding/COIN);
                    obj.push_back(Pair("funds",numstr));
                    sprintf(numstr,"%.8f",(double)datafee/COIN);
                    obj.push_back(Pair("datafee",numstr));
                    a.push_back(obj);
                }
            }
            result.push_back(Pair("registered",a));
        }
        else
            CCERR_RESULT("pegscc",CCLOG_INFO, stream << "invalid oracletxid " << oracletxid.GetHex());
    }
    else
        CCERR_RESULT("oraclescc",CCLOG_INFO, stream << "cant find oracletxid " << oracletxid.GetHex());
    return(result);
}

UniValue OraclesList()
{
    UniValue result(UniValue::VARR); std::vector<uint256> txids; struct CCcontract_info *cp,C; uint256 txid,hashBlock; CTransaction createtx; std::string name,description,format; char str[65];
    cp = CCinit(&C,EVAL_ORACLES);
    SetCCtxids(txids,cp->normaladdr,false,cp->evalcode,zeroid,'C');
    for (std::vector<uint256>::const_iterator it=txids.begin(); it!=txids.end(); it++)
    {
        txid = *it;
        if ( myGetTransaction(txid,createtx,hashBlock) != 0 )
        {
            if ( createtx.vout.size() > 0 && DecodeOraclesCreateOpRet(createtx.vout[createtx.vout.size()-1].scriptPubKey,name,description,format) == 'C' )
            {
                result.push_back(uint256_str(str,txid));
            }
        }
    }
    return(result);
}
