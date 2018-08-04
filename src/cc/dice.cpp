/******************************************************************************
 * Copyright © 2014-2018 The SuperNET Developers.                             *
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

#include "CCdice.h"

// timeout

/*
 in order to implement a dice game, we need a source of entropy, reasonably fast completion time and a way to manage the utxos.
 
 1. CC vout locks "house" funds with hash(entropy)
 2. bettor submits bet, with entropy, odds, houseid and sends combined amount into another CC vout.
 3. house account sends funds to winner/loser with proof of entropy
 4. if timeout, bettor gets refund
 
 2. and 3. can be done in mempool
 
 The house commits to an entropy value by including the hash of the entropy value  in the 'E' transaction.
 
 To bet, one of these 'E' transactions is used as the first input and its hashed entropy is combined with the unhashed entropy attached to the bet 'B' transaction.
 
 The house node monitors the 'B' transactions and if it sees one of its own, it creates either a winner 'W' or loser 'L' transaction, with proof of hash of entropy.
 
 In the even the house node doesnt respond before timeoutblocks, then anybody (including bettor) can undo the bet with funds going back to the house and bettor
 
 In order for people to play dice, someone (anyone) needs to create a funded dice plan and addfunding with enough utxo to allow players to find one.
 
createfunding:
 vins.*: normal inputs
 vout.0: CC vout for funding
 vout.1: owner vout
 vout.2: dice marker address vout for easy searching
 vout.3: normal change
 vout.n-1: opreturn 'F' sbits minbet maxbet maxodds timeoutblocks

addfunding (entropy):
 vins.*: normal inputs
 vout.0: CC vout for locked entropy funds
 vout.1: tag to owner address for entropy funds
 vout.2: normal change
 vout.n-1: opreturn 'E' sbits fundingtxid hentropy

bet:
 vin.0: entropy txid from house (must validate vin0 of 'E')
 vins.1+: normal inputs
 vout.0: CC vout for locked entropy
 vout.1: CC vout for locked bet
 vout.2: tag for bettor's address (txfee + odds)
 vout.3: change
 vout.n-1: opreturn 'B' sbits fundingtxid entropy

loser:
 vin.0: normal input
 vin.1: betTx CC vout.0 entropy from bet
 vin.2: betTx CC vout.1 bet amount from bet
 vin.3+: funding CC vout.0 from 'F', 'E', 'W', 'L' or 'T'
 vout.0: funding CC to entropy owner
 vout.1: tag to owner address for entropy funds
 vout.2: change to fundingpk
 vout.n-1: opreturn 'L' sbits fundingtxid hentropy proof
 
winner:
 same as loser, but vout.2 is winnings
 vout.3: change to fundingpk
 vout.n-1: opreturn 'W' sbits fundingtxid hentropy proof

timeout:
 same as winner, just without hentropy or proof
 
 */

#include "../endian.h"

static uint256 bettxids[128];

struct dicefinish_info
{
    uint256 fundingtxid,bettxid;
    uint64_t sbits;
    int32_t iswin;
};

bool mySendrawtransaction(std::string res)
{
    CTransaction tx; char str[65];
    if ( res.empty() == 0 && res.size() > 64 && is_hexstr((char *)res.c_str(),0) > 64 )
    {
        if ( DecodeHexTx(tx,res) != 0 )
        {
            fprintf(stderr,"%s\n%s\n",res.c_str(),uint256_str(str,tx.GetHash()));
            LOCK(cs_main);
            if ( myAddtomempool(tx) != 0 )
            {
                RelayTransaction(tx);
                fprintf(stderr,"added to mempool and broadcast\n");
                return(true);
            } else fprintf(stderr,"error adding to mempool\n");
        } else fprintf(stderr,"error decoding hex\n");
    }
    return(false);
}

void *dicefinish(void *_ptr)
{
    char str[65],str2[65],name[32]; std::string res; int32_t i,result,duplicate=0; struct dicefinish_info *ptr;
    ptr = (struct dicefinish_info *)_ptr;
    sleep(3); // wait for bettxid to be in mempool
    for (i=0; i<sizeof(bettxids)/sizeof(*bettxids); i++)
        if ( bettxids[i] == ptr->bettxid )
        {
            duplicate = 1;
            break;
        }
    if ( duplicate == 0 )
    {
        for (i=0; i<sizeof(bettxids)/sizeof(*bettxids); i++)
            if ( bettxids[i] == zeroid )
            {
                bettxids[i] = ptr->bettxid;
                break;
            }
        if ( i == sizeof(bettxids)/sizeof(*bettxids) )
            bettxids[rand() % i] = ptr->bettxid;
    }
    unstringbits(name,ptr->sbits);
    //fprintf(stderr,"duplicate.%d dicefinish.%d %s funding.%s bet.%s\n",duplicate,ptr->iswin,name,uint256_str(str,ptr->fundingtxid),uint256_str(str2,ptr->bettxid));
    if ( duplicate == 0 )
    {
        res = DiceBetFinish(&result,0,name,ptr->fundingtxid,ptr->bettxid,ptr->iswin);
        if ( result > 0 )
            mySendrawtransaction(res);
    }
    free(ptr);
    return(0);
}

void DiceQueue(int32_t iswin,uint64_t sbits,uint256 fundingtxid,uint256 bettxid)
{
    struct dicefinish_info *ptr = (struct dicefinish_info *)calloc(1,sizeof(*ptr));
    ptr->fundingtxid = fundingtxid;
    ptr->bettxid = bettxid;
    ptr->sbits = sbits;
    ptr->iswin = iswin;
    if ( ptr != 0 && pthread_create((pthread_t *)malloc(sizeof(pthread_t)),NULL,dicefinish,(void *)ptr) != 0 )
    {
        //fprintf(stderr,"DiceQueue.%d\n",iswin);
    } // small memory leak per DiceQueue
}

void endiancpy(uint8_t *dest,uint8_t *src,int32_t len)
{
    int32_t i,j=0;
#if defined(WORDS_BIGENDIAN)
    for (i=31; i>=0; i--)
        dest[j++] = src[i];
#else
    memcpy(dest,src,len);
#endif
}

CPubKey DiceFundingPk(CScript scriptPubKey)
{
    CPubKey pk; uint8_t *ptr,*dest; int32_t i;
    if ( scriptPubKey.size() == 35 )
    {
        ptr = (uint8_t *)scriptPubKey.data();
        dest = (uint8_t *)pk.begin();
        for (i=0; i<33; i++)
            dest[i] = ptr[i+1];
    } else fprintf(stderr,"DiceFundingPk invalid size.%d\n",(int32_t)scriptPubKey.size());
    return(pk);
}

uint256 DiceHashEntropy(uint256 &entropy,uint256 _txidpriv) // max 1 vout per txid used
{
    int32_t i; uint8_t _entropy[32],_hentropy[32]; bits256 tmp256,txidpub,txidpriv,mypriv,mypub,ssecret,ssecret2; uint256 hentropy;
    memset(&hentropy,0,32);
    endiancpy(txidpriv.bytes,(uint8_t *)&_txidpriv,32);
    txidpriv.bytes[0] &= 0xf8, txidpriv.bytes[31] &= 0x7f, txidpriv.bytes[31] |= 0x40;
    txidpub = curve25519(txidpriv,curve25519_basepoint9());

    Myprivkey(tmp256.bytes);
    vcalc_sha256(0,mypriv.bytes,tmp256.bytes,32);
    mypriv.bytes[0] &= 0xf8, mypriv.bytes[31] &= 0x7f, mypriv.bytes[31] |= 0x40;
    mypub = curve25519(mypriv,curve25519_basepoint9());

    ssecret = curve25519(mypriv,txidpub);
    ssecret2 = curve25519(txidpriv,mypub);
    if ( memcmp(ssecret.bytes,ssecret2.bytes,32) == 0 )
    {
        vcalc_sha256(0,(uint8_t *)&_entropy,ssecret.bytes,32);
        vcalc_sha256(0,(uint8_t *)&_hentropy,_entropy,32);
        endiancpy((uint8_t *)&entropy,_entropy,32);
        endiancpy((uint8_t *)&hentropy,_hentropy,32);
    }
    else
    {
        for (i=0; i<32; i++)
            fprintf(stderr,"%02x",ssecret.bytes[i]);
        fprintf(stderr," ssecret\n");
        for (i=0; i<32; i++)
            fprintf(stderr,"%02x",ssecret2.bytes[i]);
        fprintf(stderr," ssecret2 dont match\n");
    }
    //char str[65],str2[65];
    //fprintf(stderr,"generated house hentropy.%s <- entropy.%s\n",uint256_str(str,hentropy),uint256_str(str2,entropy));
    return(hentropy);
}

uint64_t DiceCalc(int64_t bet,int64_t odds,int64_t minbet,int64_t maxbet,int64_t maxodds,int64_t timeoutblocks,uint256 houseentropy,uint256 bettorentropy)
{
    uint8_t buf[64],_house[32],_bettor[32]; uint64_t winnings; arith_uint256 house,bettor; char str[65],str2[65];
    if ( odds < 10000 )
        return(0);
    else odds -= 10000;
    if ( bet < minbet || bet > maxbet || odds > maxodds )
    {
        fprintf(stderr,"bet size violation %.8f\n",(double)bet/COIN);
        return(0);
    }
    //fprintf(stderr,"calc house entropy %s vs bettor %s\n",uint256_str(str,houseentropy),uint256_str(str2,bettorentropy));

    endiancpy(buf,(uint8_t *)&houseentropy,32);
    endiancpy(&buf[32],(uint8_t *)&bettorentropy,32);
    vcalc_sha256(0,(uint8_t *)&_house,buf,64);
    endiancpy((uint8_t *)&house,_house,32);

    endiancpy(buf,(uint8_t *)&bettorentropy,32);
    endiancpy(&buf[32],(uint8_t *)&houseentropy,32);
    vcalc_sha256(0,(uint8_t *)&_bettor,buf,64);
    endiancpy((uint8_t *)&bettor,_bettor,32);
    if ( odds > 1 )
        bettor = (bettor / arith_uint256(odds));
    if ( bettor >= house )
        winnings = bet * (odds+1);
    else winnings = 0;
    return(winnings);
}

CScript EncodeDiceFundingOpRet(uint8_t funcid,uint64_t sbits,int64_t minbet,int64_t maxbet,int64_t maxodds,int64_t timeoutblocks)
{
    CScript opret; uint8_t evalcode = EVAL_DICE;
    opret << OP_RETURN << E_MARSHAL(ss << evalcode << 'F' << sbits << minbet << maxbet << maxodds << timeoutblocks);
    return(opret);
}

uint8_t DecodeDiceFundingOpRet(const CScript &scriptPubKey,uint64_t &sbits,int64_t &minbet,int64_t &maxbet,int64_t &maxodds,int64_t &timeoutblocks)
{
    std::vector<uint8_t> vopret; uint8_t *script,e,f;
    GetOpReturnData(scriptPubKey, vopret);
    script = (uint8_t *)vopret.data();
    if ( vopret.size() > 2 && E_UNMARSHAL(vopret,ss >> e; ss >> f; ss >> sbits; ss >> minbet; ss >> maxbet; ss >> maxodds; ss >> timeoutblocks) != 0 )
    {
        if ( e == EVAL_DICE && f == 'F' )
            return(f);
    }
    return(0);
}

CScript EncodeDiceOpRet(uint8_t funcid,uint64_t sbits,uint256 fundingtxid,uint256 hash,uint256 proof)
{
    CScript opret; uint8_t evalcode = EVAL_DICE;
    opret << OP_RETURN << E_MARSHAL(ss << evalcode << funcid << sbits << fundingtxid << hash << proof);
    return(opret);
}

uint8_t DecodeDiceOpRet(uint256 txid,const CScript &scriptPubKey,uint64_t &sbits,uint256 &fundingtxid,uint256 &hash,uint256 &proof)
{
    std::vector<uint8_t> vopret; uint8_t *script,e,f,funcid; int64_t minbet,maxbet,maxodds,timeoutblocks;
    GetOpReturnData(scriptPubKey,vopret);
    if ( vopret.size() > 2 )
    {
        script = (uint8_t *)vopret.data();
        if ( script[0] == EVAL_DICE )
        {
            if ( script[1] == 'F' )
            {
                if ( E_UNMARSHAL(vopret,ss >> e; ss >> f; ss >> sbits; ss >> minbet; ss >> maxbet; ss >> maxodds; ss >> timeoutblocks) != 0 )
                {
                    memset(&hash,0,32);
                    fundingtxid = txid;
                    return('F');
                } else fprintf(stderr,"unmarshal error for F\n");
            }
            else if ( E_UNMARSHAL(vopret,ss >> e; ss >> f; ss >> sbits; ss >> fundingtxid; ss >> hash; ss >> proof) != 0 )
            {
                if ( e == EVAL_DICE && (f == 'B' || f == 'W' || f == 'L' || f == 'T' || f == 'E') )
                    return(f);
                else fprintf(stderr,"mismatched e.%02x f.(%c)\n",e,f);
            }
        } else fprintf(stderr,"script[0] %02x != EVAL_DICE\n",script[0]);
    } else fprintf(stderr,"not enough opret.[%d]\n",(int32_t)vopret.size());
    return(0);
}

uint256 DiceGetEntropy(CTransaction tx,uint8_t reffuncid)
{
    uint256 hash,fundingtxid,proof; uint64_t sbits; int32_t numvouts;
    if ( (numvouts= tx.vout.size()) > 0 && DecodeDiceOpRet(tx.GetHash(),tx.vout[numvouts-1].scriptPubKey,sbits,fundingtxid,hash,proof) == reffuncid )
        return(hash);
    else return(zeroid);
}

uint64_t IsDicevout(struct CCcontract_info *cp,const CTransaction& tx,int32_t v)
{
    char destaddr[64];
    if ( tx.vout[v].scriptPubKey.IsPayToCryptoCondition() != 0 )
    {
        if ( Getscriptaddress(destaddr,tx.vout[v].scriptPubKey) > 0 && strcmp(destaddr,cp->unspendableCCaddr) == 0 )
            return(tx.vout[v].nValue);
    }
    return(0);
}

int64_t DiceAmounts(uint64_t &inputs,uint64_t &outputs,struct CCcontract_info *cp,Eval *eval,const CTransaction &tx)
{
    CTransaction vinTx; uint256 hashBlock; int32_t i,numvins,numvouts; uint64_t assetoshis;
    numvins = tx.vin.size();
    numvouts = tx.vout.size();
    inputs = outputs = 0;
    for (i=0; i<numvins; i++)
    {
        if ( (*cp->ismyvin)(tx.vin[i].scriptSig) != 0 )
        {
            if ( eval->GetTxUnconfirmed(tx.vin[i].prevout.hash,vinTx,hashBlock) == 0 )
                return eval->Invalid("always should find vin, but didnt");
            else
            {
                if ( (assetoshis= IsDicevout(cp,vinTx,tx.vin[i].prevout.n)) != 0 )
                    inputs += assetoshis;
            }
        }
    }
    for (i=0; i<numvouts; i++)
    {
        //fprintf(stderr,"i.%d of numvouts.%d\n",i,numvouts);
        if ( (assetoshis= IsDicevout(cp,tx,i)) != 0 )
            outputs += assetoshis;
    }
    return(inputs - outputs);
}

bool DiceIsmine(const CScript scriptPubKey)
{
    char destaddr[64],myaddr[64];
    Getscriptaddress(destaddr,scriptPubKey);
    Getscriptaddress(myaddr,CScript() << Mypubkey() << OP_CHECKSIG);
    return(strcmp(destaddr,myaddr) == 0);
}

int32_t DiceIsWinner(uint256 &entropy,uint256 txid,CTransaction tx,CTransaction vinTx,uint256 bettorentropy,uint64_t sbits,int64_t minbet,int64_t maxbet,int64_t maxodds,int64_t timeoutblocks,uint256 fundingtxid)
{
    uint64_t vinsbits,winnings; uint256 vinproof,vinfundingtxid,hentropy,hentropy2; uint8_t funcid;
    //char str[65],str2[65];
    if ( vinTx.vout.size() > 1 && DiceIsmine(vinTx.vout[1].scriptPubKey) != 0 && vinTx.vout[0].scriptPubKey.IsPayToCryptoCondition() != 0 )
    {
        if ( ((funcid= DecodeDiceOpRet(txid,vinTx.vout[vinTx.vout.size()-1].scriptPubKey,vinsbits,vinfundingtxid,hentropy,vinproof)) == 'E' || funcid == 'W' || funcid == 'L') && sbits == vinsbits && fundingtxid == vinfundingtxid )
        {
            hentropy2 = DiceHashEntropy(entropy,vinTx.vin[0].prevout.hash);
            if ( hentropy == hentropy2 )
            {
                winnings = DiceCalc(tx.vout[1].nValue,tx.vout[2].nValue,minbet,maxbet,maxodds,timeoutblocks,entropy,bettorentropy);
                char str[65]; fprintf(stderr,"%s winnings %.8f bet %.8f at odds %d:1\n",uint256_str(str,tx.GetHash()),(double)winnings/COIN,(double)tx.vout[1].nValue/COIN,(int32_t)(tx.vout[2].nValue-10000));
                //fprintf(stderr,"I am house entropy %.8f entropy.(%s) vs %s -> winnings %.8f\n",(double)vinTx.vout[0].nValue/COIN,uint256_str(str,entropy),uint256_str(str2,hash),(double)winnings/COIN);
                if ( winnings == 0 )
                {
                    // queue 'L' losing tx
                    return(-1);
                }
                else
                {
                    // queue 'W' winning tx
                    return(1);
                }
            } else fprintf(stderr,"hentropy != hentropy2\n");
        } else fprintf(stderr,"funcid.%c sbits %llx vs %llx cmp.%d\n",funcid,(long long)sbits,(long long)vinsbits,fundingtxid == vinfundingtxid);
    } //else fprintf(stderr,"notmine or not CC\n");
    return(0);
}

bool DiceVerifyTimeout(CTransaction &betTx,int32_t timeoutblocks)
{
    int32_t numblocks;
    if ( CCduration(numblocks,betTx.GetHash()) <= 0 )
        return(false);
    return(numblocks >= timeoutblocks);
}

bool DiceValidate(struct CCcontract_info *cp,Eval *eval,const CTransaction &tx)
{
    uint256 txid,fundingtxid,vinfundingtxid,vinhentropy,vinproof,hashBlock,hash,proof,entropy; int64_t minbet,maxbet,maxodds,timeoutblocks,odds,winnings; uint64_t vinsbits,sbits,amount,inputs,outputs,txfee=10000; int32_t numvins,numvouts,preventCCvins,preventCCvouts,i,iswin; uint8_t funcid; CScript fundingPubKey; CTransaction fundingTx,vinTx,vinofvinTx; char CCaddr[64];
    numvins = tx.vin.size();
    numvouts = tx.vout.size();
    preventCCvins = preventCCvouts = -1;
    if ( numvouts < 1 )
        return eval->Invalid("no vouts");
    else
    {
        txid = tx.GetHash();
        if ( (funcid=  DecodeDiceOpRet(txid,tx.vout[numvouts-1].scriptPubKey,sbits,fundingtxid,hash,proof)) != 0 )
        {
            if ( eval->GetTxUnconfirmed(fundingtxid,fundingTx,hashBlock) == 0 )
                return eval->Invalid("cant find fundingtxid");
            else if ( fundingTx.vout.size() > 0 && DecodeDiceFundingOpRet(fundingTx.vout[fundingTx.vout.size()-1].scriptPubKey,sbits,minbet,maxbet,maxodds,timeoutblocks) != 'F' )
                return eval->Invalid("fundingTx not valid");
            fundingPubKey = fundingTx.vout[1].scriptPubKey;
            switch ( funcid )
            {
                case 'F':
                    //vins.*: normal inputs
                    //vout.0: CC vout for funding
                    //vout.1: normal marker vout for easy searching
                    //vout.2: normal change
                    //vout.n-1: opreturn 'F' sbits minbet maxbet maxodds timeoutblocks
                    return eval->Invalid("unexpected DiceValidate for createfunding");
                    break;
                case 'E': // check sig of vin to match fundingtxid in the 'B' tx
                    //vins.*: normal inputs
                    //vout.0: CC vout for locked entropy funds
                    //vout.1: tag to owner address for entropy funds
                    //vout.2: normal change
                    //vout.n-1: opreturn 'E' sbits fundingtxid hentropy
                    return eval->Invalid("unexpected DiceValidate for addfunding entropy");
                    break;
                case 'B':
                    //vin.0: entropy txid from house
                    //vins.1+: normal inputs
                    //vout.0: CC vout for locked entropy
                    //vout.1: CC vout for locked bet
                    //vout.2: tag for bettor's address (txfee + odds)
                    //vout.3: change
                    //vout.n-1: opreturn 'B' sbits fundingtxid entropy
                    preventCCvouts = 2;
                    preventCCvins = 1;
                    if ( IsCCInput(tx.vin[0].scriptSig) == 0 )
                        return eval->Invalid("vin.0 is normal for bet");
                    else if ( tx.vout[0].scriptPubKey.IsPayToCryptoCondition() == 0 )
                        return eval->Invalid("vout.0 is normal for bet");
                    else if ( tx.vout[1].scriptPubKey.IsPayToCryptoCondition() == 0 )
                        return eval->Invalid("vout.1 is normal for bet");
                    else if ( eval->GetTxUnconfirmed(tx.vin[0].prevout.hash,vinTx,hashBlock) == 0 )
                        return eval->Invalid("always should find vin.0, but didnt for bet");
                    else if ( vinTx.vout[1].scriptPubKey != fundingPubKey )
                        return eval->Invalid("entropy tx not fundingPubKey for bet");
                    else if ( ConstrainVout(tx.vout[0],1,cp->unspendableCCaddr,vinTx.vout[tx.vin[0].prevout.n].nValue) == 0 )
                        return eval->Invalid("vout[0] != entropy nValue for bet");
                    else if ( ConstrainVout(tx.vout[1],1,cp->unspendableCCaddr,0) == 0 )
                        return eval->Invalid("vout[1] constrain violation for bet");
                    else if ( tx.vout[2].nValue > txfee+maxodds || tx.vout[2].nValue < txfee )
                        return eval->Invalid("vout[2] nValue violation for bet");
                    else if ( eval->GetTxUnconfirmed(vinTx.vin[0].prevout.hash,vinofvinTx,hashBlock) == 0 || vinofvinTx.vout.size() < 1 )
                        return eval->Invalid("always should find vinofvin.0, but didnt for bet");
                    else if ( vinTx.vin[0].prevout.hash != fundingtxid )
                    {
                        if ( vinofvinTx.vout[vinTx.vin[0].prevout.n].scriptPubKey != fundingPubKey )
                        {
                            uint8_t *ptr0,*ptr1; int32_t i; char str[65];
                            fprintf(stderr,"bidTx.%s\n",uint256_str(str,txid));
                            fprintf(stderr,"entropyTx.%s v%d\n",uint256_str(str,tx.vin[0].prevout.hash),(int32_t)tx.vin[0].prevout.n);
                            fprintf(stderr,"entropyTx vin0 %s v%d\n",uint256_str(str,vinTx.vin[0].prevout.hash),(int32_t)vinTx.vin[0].prevout.n);
                            ptr0 = (uint8_t *)vinofvinTx.vout[vinTx.vin[0].prevout.n].scriptPubKey.data();
                            ptr1 = (uint8_t *)fundingPubKey.data();
                            for (i=0; i<vinofvinTx.vout[vinTx.vin[0].prevout.n].scriptPubKey.size(); i++)
                                fprintf(stderr,"%02x",ptr0[i]);
                            fprintf(stderr," script vs ");
                            for (i=0; i<fundingPubKey.size(); i++)
                                fprintf(stderr,"%02x",ptr1[i]);
                            fprintf(stderr," (%c) entropy vin.%d fundingPubKey mismatch %s\n",funcid,vinTx.vin[0].prevout.n,uint256_str(str,vinTx.vin[0].prevout.hash));
                            return eval->Invalid("vin1 of entropy tx not fundingPubKey for bet");
                        }
                    }
                    if ( (iswin= DiceIsWinner(entropy,txid,tx,vinTx,hash,sbits,minbet,maxbet,maxodds,timeoutblocks,fundingtxid)) != 0 )
                    {
                        // will only happen for fundingPubKey
                        DiceQueue(iswin,sbits,fundingtxid,txid);
                    }
                    break;
                case 'L':
                case 'W':
                case 'T':
                    //vin.0: normal input
                    //vin.1: betTx CC vout.0 entropy from bet
                    //vin.2: betTx CC vout.1 bet amount from bet
                    //vin.3+: funding CC vout.0 from 'F', 'E', 'W', 'L' or 'T'
                    //vout.1: tag to owner address for entropy funds
                    preventCCvouts = 1;
                    DiceAmounts(inputs,outputs,cp,eval,tx);
                    if ( IsCCInput(tx.vin[1].scriptSig) == 0 || IsCCInput(tx.vin[2].scriptSig) == 0 )
                        return eval->Invalid("vin0 or vin1 normal vin for bet");
                    else if ( tx.vin[1].prevout.hash != tx.vin[2].prevout.hash )
                        return eval->Invalid("vin0 != vin1 prevout.hash for bet");
                    else if ( eval->GetTxUnconfirmed(tx.vin[1].prevout.hash,vinTx,hashBlock) == 0 )
                        return eval->Invalid("always should find vin.0, but didnt for wlt");
                    else if ( vinTx.vout.size() < 3 || DecodeDiceOpRet(tx.vin[1].prevout.hash,vinTx.vout[vinTx.vout.size()-1].scriptPubKey,vinsbits,vinfundingtxid,vinhentropy,vinproof) != 'B' )
                        return eval->Invalid("not betTx for vin0/1 for wlt");
                    else if ( sbits != vinsbits || fundingtxid != vinfundingtxid )
                        return eval->Invalid("sbits or fundingtxid mismatch for wlt");
                    else if ( fundingPubKey != tx.vout[1].scriptPubKey )
                        return eval->Invalid("tx.vout[1] != fundingPubKey for wlt");
                    if ( funcid == 'L' )
                    {
                        //vout.0: funding CC to entropy owner
                        //vout.n-1: opreturn 'L' sbits fundingtxid hentropy proof
                        if ( ConstrainVout(tx.vout[0],1,cp->unspendableCCaddr,inputs) == 0 )
                            return eval->Invalid("vout[0] != inputs-txfee for loss");
                        else if ( tx.vout[2].scriptPubKey != fundingPubKey )
                        {
                            if ( tx.vout[2].scriptPubKey.size() == 0 || ((uint8_t *)tx.vout[2].scriptPubKey.data())[0] != 0x6a )
                                return eval->Invalid("vout[2] not send to fundingPubKey for loss");
                        }
                        iswin = -1;
                    }
                    else
                    {
                        //vout.0: funding CC change to entropy owner
                        //vout.2: normal output to bettor's address
                        //vout.n-1: opreturn 'W' sbits fundingtxid hentropy proof
                        odds = vinTx.vout[2].nValue - txfee;
                        if ( ConstrainVout(tx.vout[0],1,cp->unspendableCCaddr,0) == 0 )
                            return eval->Invalid("vout[0] != inputs-txfee for win/timeout");
                        else if ( tx.vout[2].scriptPubKey != vinTx.vout[2].scriptPubKey )
                            return eval->Invalid("vout[2] scriptPubKey mismatch for win/timeout");
                        else if ( tx.vout[2].nValue != (odds+1)*vinTx.vout[1].nValue )
                            return eval->Invalid("vout[2] payut mismatch for win/timeout");
                        else if ( inputs != (outputs + tx.vout[2].nValue) && inputs != (outputs + tx.vout[2].nValue+txfee) )
                        {
                            fprintf(stderr,"inputs %.8f != outputs %.8f + %.8f\n",(double)inputs/COIN,(double)outputs/COIN,(double)tx.vout[2].nValue/COIN);
                            return eval->Invalid("CC funds mismatch for win/timeout");
                        }
                        else if ( tx.vout[3].scriptPubKey != fundingPubKey )
                        {
                            if ( tx.vout[3].scriptPubKey.size() == 0 || ((uint8_t *)tx.vout[3].scriptPubKey.data())[0] != 0x6a )
                                return eval->Invalid("vout[3] not send to fundingPubKey for win/timeout");
                        }
                        iswin = (funcid == 'W');
                    }
                    if ( iswin != 0 )
                    {
                        //char str[65],str2[65];
                        entropy = DiceGetEntropy(vinTx,'B');
                        vcalc_sha256(0,(uint8_t *)&hash,(uint8_t *)&proof,32);
                        //fprintf(stderr,"calculated house hentropy.%s\n",uint256_str(str,hash));
                        //fprintf(stderr,"verify house entropy %s vs bettor %s\n",uint256_str(str,proof),uint256_str(str2,entropy));
                        winnings = DiceCalc(vinTx.vout[1].nValue,vinTx.vout[2].nValue,minbet,maxbet,maxodds,timeoutblocks,proof,entropy);
                        if ( (winnings == 0 && iswin > 0) || (winnings > 0 && iswin < 0) )
                            return eval->Invalid("DiceCalc mismatch for win/loss");
                    }
                    else if ( DiceVerifyTimeout(vinTx,timeoutblocks) == 0 )
                        return eval->Invalid("invalid timeout claim for timeout");
                    break;
            }
        }
        return(PreventCC(eval,tx,preventCCvins,numvins,preventCCvouts,numvouts));
    }
    return(true);
}

uint64_t AddDiceInputs(struct CCcontract_info *cp,CMutableTransaction &mtx,CPubKey pk,uint64_t total,int32_t maxinputs)
{
    char coinaddr[64],str[65]; uint64_t sbits,nValue,totalinputs = 0; uint256 txid,hash,proof,hashBlock,fundingtxid; CTransaction tx; int32_t j,vout,n = 0; uint8_t funcid;
    std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> > unspentOutputs;
    GetCCaddress(cp,coinaddr,pk);
    SetCCunspents(unspentOutputs,coinaddr);
    for (std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> >::const_iterator it=unspentOutputs.begin(); it!=unspentOutputs.end(); it++)
    {
        txid = it->first.txhash;
        vout = (int32_t)it->first.index;
        if ( it->second.satoshis < 1000000 )
            continue;
        //fprintf(stderr,"(%s) %s/v%d %.8f\n",coinaddr,uint256_str(str,txid),vout,(double)it->second.satoshis/COIN);
        for (j=0; j<mtx.vin.size(); j++)
            if ( txid == mtx.vin[j].prevout.hash && vout == mtx.vin[j].prevout.n )
                break;
        if ( j != mtx.vin.size() )
            continue;
        if ( GetTransaction(txid,tx,hashBlock,false) != 0 && tx.vout.size() > 0 && tx.vout[vout].scriptPubKey.IsPayToCryptoCondition() != 0 && myIsutxo_spentinmempool(txid,vout) == 0 )
        {
            if ( (funcid= DecodeDiceOpRet(txid,tx.vout[tx.vout.size()-1].scriptPubKey,sbits,fundingtxid,hash,proof)) != 0 )
            {
                if ( funcid == 'F' || funcid == 'E' || funcid == 'W' || funcid == 'L' || funcid == 'T' )
                {
                    if ( total != 0 && maxinputs != 0 )
                        mtx.vin.push_back(CTxIn(txid,vout,CScript()));
                    totalinputs += it->second.satoshis;
                    n++;
                    if ( (total > 0 && totalinputs >= total) || (maxinputs > 0 && n >= maxinputs) )
                        break;
                }
            } else fprintf(stderr,"null funcid\n");
        }
    }
    return(totalinputs);
}

uint64_t DicePlanFunds(uint64_t &entropyval,uint256 &entropytxid,uint64_t refsbits,struct CCcontract_info *cp,CPubKey dicepk,uint256 reffundingtxid)
{
    char coinaddr[64],str[65]; uint64_t sbits,nValue,totalinputs = 0; uint256 hash,txid,proof,hashBlock,fundingtxid; CScript fundingPubKey; CTransaction tx,vinTx; int32_t vout,first=0,n=0; uint8_t funcid;
    std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> > unspentOutputs;
    if ( GetTransaction(reffundingtxid,tx,hashBlock,false) != 0 && tx.vout.size() > 1 && ConstrainVout(tx.vout[0],1,cp->unspendableCCaddr,0) != 0 )
    {
        fundingPubKey = tx.vout[1].scriptPubKey;
    } else return(0);
    GetCCaddress(cp,coinaddr,dicepk);
    SetCCunspents(unspentOutputs,coinaddr);
    entropyval = 0;
    for (std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> >::const_iterator it=unspentOutputs.begin(); it!=unspentOutputs.end(); it++)
    {
        txid = it->first.txhash;
        vout = (int32_t)it->first.index;
        if ( GetTransaction(txid,tx,hashBlock,false) != 0 && tx.vout[vout].scriptPubKey.IsPayToCryptoCondition() != 0 && myIsutxo_spentinmempool(txid,vout) == 0 )
        {
            //char str[65],str2[65];
            if ( (funcid= DecodeDiceOpRet(txid,tx.vout[tx.vout.size()-1].scriptPubKey,sbits,fundingtxid,hash,proof)) != 0 )
            {
                if ( (funcid == 'F' && reffundingtxid == txid) || reffundingtxid == fundingtxid )
                {
                    if ( refsbits == sbits && (nValue= IsDicevout(cp,tx,vout)) > 10000 && (funcid == 'F' || funcid == 'E' || funcid == 'W' || funcid == 'L' || funcid == 'T')  )
                    {
                        if ( funcid != 'F' && funcid != 'T' )
                        {
                            n++;
                            fprintf(stderr,"%s.(%c %.8f) ",uint256_str(str,txid),funcid,(double)nValue/COIN);
                        }
                        totalinputs += nValue;
                        if ( first == 0 && (funcid == 'E' || funcid == 'W' || funcid == 'L') )
                        {
                            fprintf(stderr,"check first\n");
                            if ( fundingPubKey == tx.vout[1].scriptPubKey )
                            {
                                if ( funcid == 'E' && fundingtxid != tx.vin[0].prevout.hash )
                                {
                                    if ( GetTransaction(tx.vin[0].prevout.hash,vinTx,hashBlock,false) == 0 )
                                    {
                                        fprintf(stderr,"cant find entropy vin0 %s or vin0prev %d vouts[%d]\n",uint256_str(str,tx.vin[0].prevout.hash),tx.vin[0].prevout.n,(int32_t)vinTx.vout.size());
                                        continue;
                                    }
                                    if ( vinTx.vout[tx.vin[0].prevout.n].scriptPubKey != fundingPubKey )
                                    {
                                        uint8_t *ptr0,*ptr1; int32_t i; char str[65];
                                        ptr0 = (uint8_t *)vinTx.vout[tx.vin[0].prevout.n].scriptPubKey.data();
                                        ptr1 = (uint8_t *)fundingPubKey.data();
                                        for (i=0; i<vinTx.vout[tx.vin[0].prevout.n].scriptPubKey.size(); i++)
                                            fprintf(stderr,"%02x",ptr0[i]);
                                        fprintf(stderr," script vs ");
                                        for (i=0; i<fundingPubKey.size(); i++)
                                            fprintf(stderr,"%02x",ptr1[i]);
                                        fprintf(stderr," (%c) entropy vin.%d fundingPubKey mismatch %s\n",funcid,tx.vin[0].prevout.n,uint256_str(str,tx.vin[0].prevout.hash));
                                        continue;
                                    }
                                } //else fprintf(stderr,"not E or is funding\n");
                                entropytxid = txid;
                                entropyval = tx.vout[0].nValue;
                                first = 1;
                            }
                            else
                            {
                                uint8_t *ptr0,*ptr1; int32_t i; char str[65];
                                ptr0 = (uint8_t *)tx.vout[1].scriptPubKey.data();
                                ptr1 = (uint8_t *)fundingPubKey.data();
                                for (i=0; i<tx.vout[1].scriptPubKey.size(); i++)
                                    fprintf(stderr,"%02x",ptr0[i]);
                                fprintf(stderr," script vs ");
                                for (i=0; i<fundingPubKey.size(); i++)
                                    fprintf(stderr,"%02x",ptr1[i]);
                                fprintf(stderr," (%c) tx vin.%d fundingPubKey mismatch %s\n",funcid,tx.vin[0].prevout.n,uint256_str(str,tx.vin[0].prevout.hash));
                            }
                        }
                    } else fprintf(stderr,"%s %c refsbits.%llx sbits.%llx nValue %.8f\n",uint256_str(str,txid),funcid,(long long)refsbits,(long long)sbits,(double)nValue/COIN);
                } //else fprintf(stderr,"else case funcid (%c) %d %s vs %s\n",funcid,funcid,uint256_str(str,reffundingtxid),uint256_str(str2,fundingtxid));
            } //else fprintf(stderr,"funcid.%d %c skipped %.8f\n",funcid,funcid,(double)tx.vout[vout].nValue/COIN);
        }
    }
    fprintf(stderr,"numentropy tx %d: %.8f\n",n,(double)totalinputs/COIN);
    return(totalinputs);
}

bool DicePlanExists(CScript &fundingPubKey,uint256 &fundingtxid,struct CCcontract_info *cp,uint64_t refsbits,CPubKey dicepk,int64_t &minbet,int64_t &maxbet,int64_t &maxodds,int64_t &timeoutblocks)
{
    char CCaddr[64]; uint64_t sbits; uint256 txid,hashBlock; CTransaction tx;
    std::vector<std::pair<CAddressIndexKey, CAmount> > txids;
    GetCCaddress(cp,CCaddr,dicepk);
    SetCCtxids(txids,cp->normaladdr);
    if ( fundingtxid != zeroid ) // avoid scan unless creating new funding plan
    {
        if ( GetTransaction(fundingtxid,tx,hashBlock,false) != 0 && tx.vout.size() > 1 && ConstrainVout(tx.vout[0],1,CCaddr,0) != 0 )
        {
            if ( DecodeDiceFundingOpRet(tx.vout[tx.vout.size()-1].scriptPubKey,sbits,minbet,maxbet,maxodds,timeoutblocks) == 'F' && sbits == refsbits )
            {
                fundingPubKey = tx.vout[1].scriptPubKey;
                return(true);
            }
        }
        return(false);
    }
    for (std::vector<std::pair<CAddressIndexKey, CAmount> >::const_iterator it=txids.begin(); it!=txids.end(); it++)
    {
        //int height = it->first.blockHeight;
        txid = it->first.txhash;
        if ( fundingtxid != zeroid && txid != fundingtxid )
            continue;
        if ( GetTransaction(txid,tx,hashBlock,false) != 0 && tx.vout.size() > 0 && ConstrainVout(tx.vout[0],1,CCaddr,0) != 0 )
        {
            if ( DecodeDiceFundingOpRet(tx.vout[tx.vout.size()-1].scriptPubKey,sbits,minbet,maxbet,maxodds,timeoutblocks) == 'F' )
            {
                if ( sbits == refsbits )
                {
                    fundingPubKey = tx.vout[1].scriptPubKey;
                    fundingtxid = txid;
                    return(true);
                }
            }
        }
    }
    return(false);
}

struct CCcontract_info *Diceinit(CScript &fundingPubKey,uint256 reffundingtxid,struct CCcontract_info *C,char *planstr,uint64_t &txfee,CPubKey &mypk,CPubKey &dicepk,uint64_t &sbits,int64_t &minbet,int64_t &maxbet,int64_t &maxodds,int64_t &timeoutblocks)
{
    struct CCcontract_info *cp; int32_t cmpflag;
    cp = CCinit(C,EVAL_DICE);
    if ( txfee == 0 )
        txfee = 10000;
    mypk = pubkey2pk(Mypubkey());
    dicepk = GetUnspendable(cp,0);
    sbits = stringbits(planstr);
    if ( reffundingtxid == zeroid )
        cmpflag = 0;
    else cmpflag = 1;
    if ( DicePlanExists(fundingPubKey,reffundingtxid,cp,sbits,dicepk,minbet,maxbet,maxodds,timeoutblocks) != cmpflag )
    {
        fprintf(stderr,"Dice plan (%s) already exists cmpflag.%d\n",planstr,cmpflag);
        return(0);
    }
    return(cp);
}

UniValue DiceInfo(uint256 diceid)
{
    UniValue result(UniValue::VOBJ); CPubKey dicepk; uint256 hashBlock,entropytxid; CTransaction vintx; int64_t minbet,maxbet,maxodds,timeoutblocks; uint64_t sbits,funding,entropyval; char str[67],numstr[65]; struct CCcontract_info *cp,C;
    if ( GetTransaction(diceid,vintx,hashBlock,false) == 0 )
    {
        fprintf(stderr,"cant find fundingtxid\n");
        result.push_back(Pair("error","cant find fundingtxid"));
        return(result);
    }
    if ( vintx.vout.size() > 0 && DecodeDiceFundingOpRet(vintx.vout[vintx.vout.size()-1].scriptPubKey,sbits,minbet,maxbet,maxodds,timeoutblocks) == 0 )
    {
        fprintf(stderr,"fundingtxid isnt dice creation txid\n");
        result.push_back(Pair("error","fundingtxid isnt dice creation txid"));
        return(result);
    }
    result.push_back(Pair("result","success"));
    result.push_back(Pair("fundingtxid",uint256_str(str,diceid)));
    unstringbits(str,sbits);
    result.push_back(Pair("name",str));
    result.push_back(Pair("sbits",sbits));
    sprintf(numstr,"%.8f",(double)minbet/COIN);
    result.push_back(Pair("minbet",numstr));
    sprintf(numstr,"%.8f",(double)maxbet/COIN);
    result.push_back(Pair("maxbet",numstr));
    result.push_back(Pair("maxodds",maxodds));
    result.push_back(Pair("timeoutblocks",timeoutblocks));
    cp = CCinit(&C,EVAL_DICE);
    dicepk = GetUnspendable(cp,0);
    funding = DicePlanFunds(entropyval,entropytxid,sbits,cp,dicepk,diceid);
    sprintf(numstr,"%.8f",(double)funding/COIN);
    result.push_back(Pair("funding",numstr));
    return(result);
}

UniValue DiceList()
{
    UniValue result(UniValue::VARR); std::vector<std::pair<CAddressIndexKey, CAmount> > addressIndex; struct CCcontract_info *cp,C; uint256 txid,hashBlock; CTransaction vintx; uint64_t sbits; int64_t minbet,maxbet,maxodds,timeoutblocks; char str[65];
    cp = CCinit(&C,EVAL_DICE);
    SetCCtxids(addressIndex,cp->normaladdr);
    for (std::vector<std::pair<CAddressIndexKey, CAmount> >::const_iterator it=addressIndex.begin(); it!=addressIndex.end(); it++)
    {
        txid = it->first.txhash;
        if ( GetTransaction(txid,vintx,hashBlock,false) != 0 )
        {
            if ( vintx.vout.size() > 0 && DecodeDiceFundingOpRet(vintx.vout[vintx.vout.size()-1].scriptPubKey,sbits,minbet,maxbet,maxodds,timeoutblocks) != 0 )
            {
                result.push_back(uint256_str(str,txid));
            }
        }
    }
    return(result);
}

std::string DiceCreateFunding(uint64_t txfee,char *planstr,int64_t funds,int64_t minbet,int64_t maxbet,int64_t maxodds,int64_t timeoutblocks)
{
    CMutableTransaction mtx; uint256 zero; CScript fundingPubKey; CPubKey mypk,dicepk; int64_t a,b,c,d; uint64_t sbits; struct CCcontract_info *cp,C;
    if ( funds < 0 || minbet < 0 || maxbet < 0 || maxodds < 1 || timeoutblocks < 0 || timeoutblocks > 1440 )
    {
        fprintf(stderr,"negative parameter error\n");
        return(0);
    }
    memset(&zero,0,sizeof(zero));
    if ( (cp= Diceinit(fundingPubKey,zero,&C,planstr,txfee,mypk,dicepk,sbits,a,b,c,d)) == 0 )
        return(0);
    if ( AddNormalinputs(mtx,mypk,funds+3*txfee,60) > 0 )
    {
        mtx.vout.push_back(MakeCC1vout(cp->evalcode,funds,dicepk));
        mtx.vout.push_back(CTxOut(txfee,CScript() << ParseHex(HexStr(mypk)) << OP_CHECKSIG));
        mtx.vout.push_back(CTxOut(txfee,CScript() << ParseHex(HexStr(dicepk)) << OP_CHECKSIG));
        return(FinalizeCCTx(0,cp,mtx,mypk,txfee,EncodeDiceFundingOpRet('F',sbits,minbet,maxbet,maxodds,timeoutblocks)));
    }
    fprintf(stderr,"cant find enough inputs\n");
    return(0);
}

std::string DiceAddfunding(uint64_t txfee,char *planstr,uint256 fundingtxid,int64_t amount)
{
    CMutableTransaction mtx; CScript fundingPubKey,scriptPubKey; uint256 entropy,hentropy; CPubKey mypk,dicepk; uint64_t sbits; struct CCcontract_info *cp,C; int64_t minbet,maxbet,maxodds,timeoutblocks;
    if ( amount < 0 )
    {
        fprintf(stderr,"negative parameter error\n");
        return(0);
    }
    if ( (cp= Diceinit(fundingPubKey,fundingtxid,&C,planstr,txfee,mypk,dicepk,sbits,minbet,maxbet,maxodds,timeoutblocks)) == 0 )
        return(0);
    scriptPubKey = CScript() << ParseHex(HexStr(mypk)) << OP_CHECKSIG;
    if ( 0 )
    {
        uint8_t *ptr0,*ptr1; int32_t i;
        ptr0 = (uint8_t *)scriptPubKey.data();
        ptr1 = (uint8_t *)fundingPubKey.data();
        for (i=0; i<35; i++)
            fprintf(stderr,"%02x",ptr0[i]);
        fprintf(stderr," script vs ");
        for (i=0; i<35; i++)
            fprintf(stderr,"%02x",ptr1[i]);
        fprintf(stderr," funding\n");
    }
    if ( scriptPubKey == fundingPubKey )
    {
        if ( AddNormalinputs(mtx,mypk,amount+2*txfee,60) > 0 )
        {
            hentropy = DiceHashEntropy(entropy,mtx.vin[0].prevout.hash);
            mtx.vout.push_back(MakeCC1vout(cp->evalcode,amount,dicepk));
            mtx.vout.push_back(CTxOut(txfee,fundingPubKey));
            return(FinalizeCCTx(0,cp,mtx,mypk,txfee,EncodeDiceOpRet('E',sbits,fundingtxid,hentropy,zeroid)));
        } else fprintf(stderr,"cant find enough inputs\n");
    } else fprintf(stderr,"only fund creator can add more funds (entropy)\n");
    return(0);
}

std::string DiceBet(uint64_t txfee,char *planstr,uint256 fundingtxid,int64_t bet,int32_t odds)
{
    CMutableTransaction mtx; CScript fundingPubKey; CPubKey mypk,dicepk; uint64_t sbits,entropyval; int64_t funding,minbet,maxbet,maxodds,timeoutblocks; uint256 entropytxid,entropy,hentropy; struct CCcontract_info *cp,C;
    if ( bet < 0 || odds < 1 )
    {
        fprintf(stderr,"negative parameter error\n");
        return(0);
    }
    if ( (cp= Diceinit(fundingPubKey,fundingtxid,&C,planstr,txfee,mypk,dicepk,sbits,minbet,maxbet,maxodds,timeoutblocks)) == 0 )
        return(0);
    if ( bet < minbet || bet > maxbet || odds > maxodds )
    {
        fprintf(stderr,"Dice plan %s illegal bet %.8f: minbet %.8f maxbet %.8f or odds %d vs max.%d\n",planstr,(double)bet/COIN,(double)minbet/COIN,(double)maxbet/COIN,(int32_t)odds,(int32_t)maxodds);
        return(0);
    }
    if ( (funding= DicePlanFunds(entropyval,entropytxid,sbits,cp,dicepk,fundingtxid)) >= 2*bet*odds+txfee && entropyval != 0 )
    {
        if ( myIsutxo_spentinmempool(entropytxid,0) != 0 )
        {
            fprintf(stderr,"entropy txid is spent\n");
            return(0);
        }
        mtx.vin.push_back(CTxIn(entropytxid,0,CScript()));
        if ( AddNormalinputs(mtx,mypk,bet+2*txfee+odds,60) > 0 )
        {
            hentropy = DiceHashEntropy(entropy,mtx.vin[0].prevout.hash);
            mtx.vout.push_back(MakeCC1vout(cp->evalcode,entropyval,dicepk));
            mtx.vout.push_back(MakeCC1vout(cp->evalcode,bet,dicepk));
            mtx.vout.push_back(CTxOut(txfee+odds,CScript() << ParseHex(HexStr(mypk)) << OP_CHECKSIG));
            return(FinalizeCCTx(0,cp,mtx,mypk,txfee,EncodeDiceOpRet('B',sbits,fundingtxid,entropy,zeroid)));
        } else fprintf(stderr,"cant find enough inputs %.8f note enough for %.8f\n",(double)funding/COIN,(double)bet/COIN);
    }
    if ( entropyval == 0 && funding != 0 )
        fprintf(stderr,"cant find dice entropy inputs\n");
    else fprintf(stderr,"cant find dice inputs\n");
    return(0);
}

std::string DiceBetFinish(int32_t *resultp,uint64_t txfee,char *planstr,uint256 fundingtxid,uint256 bettxid,int32_t winlosetimeout)
{
    CMutableTransaction mtx; CScript scriptPubKey,fundingPubKey; CTransaction betTx,entropyTx; uint256 hentropyproof,entropytxid,hashBlock,bettorentropy,entropy,hentropy; CPubKey mypk,dicepk,fundingpk; struct CCcontract_info *cp,C; int64_t inputs,CCchange=0,odds,fundsneeded,minbet,maxbet,maxodds,timeoutblocks; uint8_t funcid=0; int32_t iswin=0; uint64_t entropyval,sbits;
    *resultp = 0;
    //char str[65]; fprintf(stderr,"DiceBetFinish.%s %s\n",planstr,uint256_str(str,bettxid));
    if ( (cp= Diceinit(fundingPubKey,fundingtxid,&C,planstr,txfee,mypk,dicepk,sbits,minbet,maxbet,maxodds,timeoutblocks)) == 0 )
    {
        fprintf(stderr,"Diceinit error\n");
        return("0");
    }
    fundingpk = DiceFundingPk(fundingPubKey);
    if ( winlosetimeout != 0 )
    {
        scriptPubKey = CScript() << ParseHex(HexStr(mypk)) << OP_CHECKSIG;
        if ( scriptPubKey != fundingPubKey )
        {
            //fprintf(stderr,"only dice fund creator can submit winner or loser\n");
            winlosetimeout = 0;
        }
    }
    if ( AddNormalinputs(mtx,mypk,txfee,1) == 0 )
    {
        fprintf(stderr,"no txfee inputs for win/lose\n");
        return("0");
    }
    if ( GetTransaction(bettxid,betTx,hashBlock,false) != 0 && GetTransaction(betTx.vin[0].prevout.hash,entropyTx,hashBlock,false) != 0 )
    {
        bettorentropy = DiceGetEntropy(betTx,'B');
        if ( winlosetimeout == 0 || (iswin= DiceIsWinner(hentropyproof,bettxid,betTx,entropyTx,bettorentropy,sbits,minbet,maxbet,maxodds,timeoutblocks,fundingtxid)) != 0 )
        {
            if ( winlosetimeout != 0 )
                winlosetimeout = iswin;
            if ( iswin == winlosetimeout )
            {
                if ( myIsutxo_spentinmempool(bettxid,0) != 0 || myIsutxo_spentinmempool(bettxid,1) != 0 )
                {
                    fprintf(stderr,"bettxid already spent\n");
                    return("0");
                }
                //fprintf(stderr,"iswin.%d matches\n",iswin);
                mtx.vin.push_back(CTxIn(bettxid,0,CScript()));
                mtx.vin.push_back(CTxIn(bettxid,1,CScript()));
                if ( iswin == 0 )
                {
                    funcid = 'T';
                    if ( DiceVerifyTimeout(betTx,timeoutblocks) == 0 ) // hasnt timed out yet
                    {
                        return("0");
                    }
                    else
                    {
                        hentropy = hentropyproof = zeroid;
                        iswin = 1;
                    }
                }
                if ( iswin > 0 )
                {
                    if ( funcid != 'T' )
                        funcid = 'W';
                    odds = (betTx.vout[2].nValue - txfee);
                    if ( odds < 1 || odds > maxodds )
                    {
                        fprintf(stderr,"illegal odds.%d vs maxodds.%d\n",(int32_t)odds,(int32_t)maxodds);
                        return("0");
                    }
                    CCchange = betTx.vout[0].nValue;
                    fundsneeded = txfee + odds*betTx.vout[1].nValue;
                    if ( (inputs= AddDiceInputs(cp,mtx,dicepk,fundsneeded,60)) > 0 )
                    {
                        if ( inputs > fundsneeded )
                            CCchange += (inputs - fundsneeded);
                        mtx.vout.push_back(MakeCC1vout(cp->evalcode,CCchange,dicepk));
                        mtx.vout.push_back(CTxOut(txfee,fundingPubKey));
                        mtx.vout.push_back(CTxOut((odds+1) * betTx.vout[1].nValue,betTx.vout[2].scriptPubKey));
                    }
                    else
                    {
                        fprintf(stderr,"not enough inputs for %.8f\n",(double)fundsneeded/COIN);
                        return("0");
                    }
                }
                else
                {
                    funcid = 'L';
                    mtx.vout.push_back(MakeCC1vout(cp->evalcode,betTx.vout[0].nValue + betTx.vout[1].nValue,dicepk));
                    mtx.vout.push_back(CTxOut(txfee,fundingPubKey));
                }
                if ( winlosetimeout != 0 )
                    hentropy = DiceHashEntropy(entropy,mtx.vin[0].prevout.hash);
                *resultp = 1;
                //char str[65],str2[65];
                //fprintf(stderr,"iswin.%d house entropy %s vs bettor %s\n",iswin,uint256_str(str,hentropyproof),uint256_str(str2,bettorentropy));
                return(FinalizeCCTx(0,cp,mtx,fundingpk,txfee,EncodeDiceOpRet(funcid,sbits,fundingtxid,hentropy,hentropyproof)));
            } else fprintf(stderr,"iswin.%d does not match.%d\n",iswin,winlosetimeout);
        }
        else
        {
            *resultp = -1;
            fprintf(stderr,"iswin.%d winlosetimeout.%d\n",iswin,winlosetimeout);
            return("0");
        }
    }
    *resultp = -1;
    fprintf(stderr,"couldnt find bettx or entropytx\n");
    return("0");
}

double DiceStatus(uint64_t txfee,char *planstr,uint256 fundingtxid,uint256 bettxid)
{
    CScript fundingPubKey,scriptPubKey; CTransaction spenttx,betTx; uint256 hash,proof,txid,hashBlock,spenttxid; CPubKey mypk,dicepk,fundingpk; struct CCcontract_info *cp,C; int32_t i,result,vout,n=0; int64_t minbet,maxbet,maxodds,timeoutblocks; uint64_t sbits; char coinaddr[64]; std::string res;
    if ( (cp= Diceinit(fundingPubKey,fundingtxid,&C,planstr,txfee,mypk,dicepk,sbits,minbet,maxbet,maxodds,timeoutblocks)) == 0 )
    {
        fprintf(stderr,"Diceinit error\n");
        return(0.);
    }
    fundingpk = DiceFundingPk(fundingPubKey);
    scriptPubKey = CScript() << ParseHex(HexStr(mypk)) << OP_CHECKSIG;
    GetCCaddress(cp,coinaddr,dicepk);
    if ( bettxid == zeroid ) // scan
    {
        std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> > unspentOutputs;
        SetCCunspents(unspentOutputs,coinaddr);
        for (std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> >::const_iterator it=unspentOutputs.begin(); it!=unspentOutputs.end(); it++)
        {
            txid = it->first.txhash;
            vout = (int32_t)it->first.index;
            if ( GetTransaction(txid,betTx,hashBlock,false) != 0 && betTx.vout[vout].scriptPubKey.IsPayToCryptoCondition() != 0 )
            {
                if ( DecodeDiceOpRet(txid,betTx.vout[betTx.vout.size()-1].scriptPubKey,sbits,fundingtxid,hash,proof) == 'B' )
                {
                    res = DiceBetFinish(&result,txfee,planstr,fundingtxid,txid,scriptPubKey == fundingPubKey);
                    if ( result > 0 )
                    {
                        mySendrawtransaction(res);
                        n++;
                    }
                }
            }
        }
        if ( scriptPubKey == fundingPubKey )
        {
            for (i=0; i<=n; i++)
            {
                res = DiceAddfunding(txfee,planstr,fundingtxid,COIN);
                fprintf(stderr,"ENTROPY tx:\n");
                mySendrawtransaction(res);
            }
        }
        return(n);
    }
    else
    {
        if ( (vout= myIsutxo_spent(spenttxid,bettxid,1)) >= 0 )
        {
            if ( GetTransaction(spenttxid,spenttx,hashBlock,false) != 0 && spenttx.vout.size() > 2 )
            {
                if ( spenttx.vout[2].scriptPubKey == fundingPubKey )
                    return(0.);
                else return((double)spenttx.vout[2].nValue/COIN);
            } else return(0.);
        }
        else if ( scriptPubKey == fundingPubKey )
            res = DiceBetFinish(&result,txfee,planstr,fundingtxid,bettxid,1);
        else res = DiceBetFinish(&result,txfee,planstr,fundingtxid,bettxid,0);
        if ( result > 0 )
        {
            mySendrawtransaction(res);
            sleep(1);
            if ( (vout= myIsutxo_spent(spenttxid,bettxid,1)) >= 0 )
            {
                if ( GetTransaction(spenttxid,spenttx,hashBlock,false) != 0 && spenttx.vout.size() >= 2 )
                {
                    if ( spenttx.vout[2].scriptPubKey == fundingPubKey || ((uint8_t *)spenttx.vout[2].scriptPubKey.data())[0] == 0x6a )
                        return(0.);
                    else return((double)spenttx.vout[2].nValue/COIN);
                } else return(0.);
            }
            fprintf(stderr,"didnt find dicefinish tx\n");
        } else return(-1.);
    }
    return(0.);
}
