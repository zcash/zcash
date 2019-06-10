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

#include "CCMarmara.h"

 /*
  Marmara CC is for the MARMARA project

  'R': two forms for initial issuance and for accepting existing
  vins normal
  vout0 approval to senderpk (issuer or owner of baton)

  'I'
  vin0 approval from 'R'
  vins1+ normal
  vout0 baton to 1st receiverpk
  vout1 marker to Marmara so all issuances can be tracked (spent when loop is closed)

  'T'
  vin0 approval from 'R'
  vin1 baton from 'I'/'T'
  vins2+ normal
  vout0 baton to next receiverpk (following the unspent baton back to original is the credit loop)

  'S'
  vin0 'I' marker
  vin1 baton
  vins CC utxos from credit loop

  'D' default/partial payment

  'L' lockfunds

  'K' pubkey in cc vout opret which locked funds in loop 

 */

 // start of consensus code

int64_t IsMarmaravout(struct CCcontract_info *cp, const CTransaction& tx, int32_t v)
{
    char destaddr[KOMODO_ADDRESS_BUFSIZE];
    if (tx.vout[v].scriptPubKey.IsPayToCryptoCondition() != 0)
    {
        if (Getscriptaddress(destaddr, tx.vout[v].scriptPubKey) && strcmp(destaddr, cp->unspendableCCaddr) == 0)
            return(tx.vout[v].nValue);
    }
    return(0);
}

// Get randomized within range [3 month...2 year] using ind as seed(?)
/* not used now
int32_t MarmaraRandomize(uint32_t ind)
{
    uint64_t val64; uint32_t val, range = (MARMARA_MAXLOCK - MARMARA_MINLOCK);
    val64 = komodo_block_prg(ind);
    val = (uint32_t)(val64 >> 32);
    val ^= (uint32_t)val64;
    return((val % range) + MARMARA_MINLOCK);
}
*/

// get random but fixed for the height param unlock height within 3 month..2 year interval  -- discontinued
// now always returns maxheight
int32_t MarmaraUnlockht(int32_t height)
{
/*  uint32_t ind = height / MARMARA_GROUPSIZE;
    height = (height / MARMARA_GROUPSIZE) * MARMARA_GROUPSIZE;
    return(height + MarmaraRandomize(ind)); */
    return MARMARA_V2LOCKHEIGHT;
}

uint8_t DecodeMarmaraCoinbaseOpRet(const CScript scriptPubKey, CPubKey &pk, int32_t &height, int32_t &unlockht)
{
    vscript_t vopret; uint8_t *script, e, f, funcid;
    GetOpReturnData(scriptPubKey, vopret);
    script = (uint8_t *)vopret.data();
    
    /* if (0)
    {
        int32_t i;

        std::cerr << __func__ << " ";
        for (i = 0; i < vopret.size(); i++)
            fprintf(stderr, "%02x", script[i]);
        fprintf(stderr, " <- opret\n");
    } */
    if (vopret.size() > 2 && script[0] == EVAL_MARMARA)
    {
        if (script[1] == 'C' || script[1] == 'P' || script[1] == 'L')
        {
            if (E_UNMARSHAL(vopret, ss >> e; ss >> f; ss >> pk; ss >> height; ss >> unlockht) != 0)
            {
                return(script[1]);
            }
            else 
                fprintf(stderr, "%s unmarshal error for %c\n", __func__, script[1]);
        } 
        //else 
        //  fprintf(stderr,"%s script[1] is %d != 'C' %d or 'P' %d or 'L' %d\n", __func__, script[1],'C','P','L');
    }
    else 
        fprintf(stderr, "%s vopret.size() is %d\n", __func__, (int32_t)vopret.size());
    return(0);
}

CScript EncodeMarmaraCoinbaseOpRet(uint8_t funcid, CPubKey pk, int32_t ht)
{
    CScript opret; int32_t unlockht; uint8_t evalcode = EVAL_MARMARA;
    unlockht = MarmaraUnlockht(ht);
    opret << OP_RETURN << E_MARSHAL(ss << evalcode << funcid << pk << ht << unlockht);
 /*   if (0)
    {
        vscript_t vopret; uint8_t *script, i;
        GetOpReturnData(opret, vopret);
        script = (uint8_t *)vopret.data();
        {
            std::cerr << __func__ << " ";
            for (i = 0; i < vopret.size(); i++)
                fprintf(stderr, "%02x", script[i]);
            fprintf(stderr, " <- gen opret.%c\n", funcid);
        }
    } */
    return(opret);
}

CScript MarmaraEncodeLoopOpret(uint8_t funcid, uint256 createtxid, CPubKey senderpk, int64_t amount, int32_t matures, std::string currency)
{
    CScript opret; uint8_t evalcode = EVAL_MARMARA;
    opret << OP_RETURN << E_MARSHAL(ss << evalcode << funcid << createtxid << senderpk << amount << matures << currency);
    return(opret);
}

uint8_t MarmaraDecodeLoopOpret(const CScript scriptPubKey, uint256 &createtxid, CPubKey &senderpk, int64_t &amount, int32_t &matures, std::string &currency)
{
    vscript_t vopret; uint8_t *script, e, f;
    GetOpReturnData(scriptPubKey, vopret);
    script = (uint8_t *)vopret.data();
    if (vopret.size() > 2 && E_UNMARSHAL(vopret, ss >> e; ss >> f; ss >> createtxid; ss >> senderpk; ss >> amount; ss >> matures; ss >> currency) != 0)
    {
        return(f);
    }
    return(0);
}

// finds the initial creation txid retrieving it from the any of the loop txn opret
int32_t MarmaraGetcreatetxid(uint256 &createtxid, uint256 txid)
{
    CTransaction tx; 
    uint256 hashBlock; 
  
    createtxid = zeroid;
    if (myGetTransaction(txid, tx, hashBlock) != 0 && tx.vout.size() > 1)
    {
        uint8_t funcid;
        int32_t matures;
        std::string currency;
        CPubKey senderpk;
        int64_t amount;

        if ((funcid = MarmaraDecodeLoopOpret(tx.vout.back().scriptPubKey, createtxid, senderpk, amount, matures, currency)) == 'I' || funcid == 'T') {
            std::cerr << __func__ << " found createtxid for funcid=I,T createtxid=" << createtxid.GetHex() << std::endl;
            return(0);
        }
        else if (funcid == 'R')
        {
            if (createtxid == zeroid)
                createtxid = txid;
            std::cerr << __func__ << " found createtxid for funcid=R createtxid=" << createtxid.GetHex() << std::endl;
            return(0);
        }
    }
    return(-1);
}

// finds the current baton starting from any baton txid
// also returns all the previous baton txids from the create tx apart from the baton txid
int32_t MarmaraGetbatontxid(std::vector<uint256> &creditloop, uint256 &batontxid, uint256 txid)
{
    uint256 createtxid, spenttxid; 
    int64_t value; 
    int32_t vini, height, n = 0, vout = 0;
    
    batontxid = zeroid;
    if (MarmaraGetcreatetxid(createtxid, txid) == 0) // retrieve the initial creation txid
    {
        txid = createtxid;
        //fprintf(stderr,"%s txid.%s -> createtxid %s\n", __func__, txid.GetHex().c_str(),createtxid.GetHex().c_str());
        while (CCgetspenttxid(spenttxid, vini, height, txid, vout) == 0)
        {
            creditloop.push_back(txid);
            //fprintf(stderr,"%d: %s\n",n,txid.GetHex().c_str());
            n++;
            if ((value = CCgettxout(spenttxid, vout, 1, 1)) == 10000)
            {
                batontxid = spenttxid;
                //fprintf(stderr,"%s got baton %s %.8f\n", __func__, batontxid.GetHex().c_str(),(double)value/COIN);
                return(n);
            }
            else if (value > 0)
            {
                batontxid = spenttxid;
                fprintf(stderr, "%s n.%d got false baton %s/v%d %.8f\n", __func__, n, batontxid.GetHex().c_str(), vout, (double)value / COIN);
                return(n);
            }
            // get funcid
            txid = spenttxid;
        }
    }
    return(-1);
}

// returns scriptPubKey with 1of2 addr for coinbase tx in createNewBlock in miner.cpp 
CScript Marmara_scriptPubKey(int32_t height, CPubKey pk)
{
    CTxOut ccvout; struct CCcontract_info *cp, C; CPubKey Marmarapk;
    cp = CCinit(&C, EVAL_MARMARA);
    Marmarapk = GetUnspendable(cp, 0);
    if (height > 0 && (height & 1) == 0 && pk.size() == 33)
    {
        ccvout = MakeCC1of2vout(EVAL_MARMARA, 0, Marmarapk, pk);
        //char coinaddr[KOMODO_ADDRESS_BUFSIZE];
        //Getscriptaddress(coinaddr,ccvout.scriptPubKey);
        //fprintf(stderr,"Marmara_scriptPubKey %s ht.%d -> %s\n",HexStr(pk).c_str(),height,coinaddr);
    }
    return(ccvout.scriptPubKey);
}

// set marmara coinbase opret for even blocks
CScript MarmaraCoinbaseOpret(uint8_t funcid, int32_t height, CPubKey pk)
{
    uint8_t *ptr;
    //fprintf(stderr,"height.%d pksize.%d\n",height,(int32_t)pk.size());
    if (height > 0 && (height & 1) == 0 && pk.size() == 33)
        return(EncodeMarmaraCoinbaseOpRet(funcid, pk, height));
    else
        return(CScript());
}

// half of the blocks (with even heights) should be mined as activated (to some unlock height)
// validates opreturn for even blocks
int32_t MarmaraValidateCoinbase(int32_t height, CTransaction tx)
{
    struct CCcontract_info *cp, C; CPubKey Marmarapk, pk; int32_t ht, unlockht; CTxOut ccvout;
    cp = CCinit(&C, EVAL_MARMARA);
    Marmarapk = GetUnspendable(cp, 0);
    
/*    if (0) // not used
    {
        int32_t d, histo[365 * 2 + 30];
        memset(histo, 0, sizeof(histo));
        for (ht = 2; ht < 100; ht++)
            fprintf(stderr, "%d ", MarmaraUnlockht(ht));
        fprintf(stderr, " <- first 100 unlock heights\n");
        for (ht = 2; ht < 1000000; ht += MARMARA_GROUPSIZE)
        {
            d = (MarmaraUnlockht(ht) - ht) / 1440;
            if (d < 0 || d > sizeof(histo) / sizeof(*histo))
                fprintf(stderr, "d error.%d at ht.%d\n", d, ht);
            else histo[d]++;
        }

        std::cerr << __func__ << " ";
        for (ht = 0; ht < sizeof(histo) / sizeof(*histo); ht++)
            fprintf(stderr, "%d ", histo[ht]);
        fprintf(stderr, "<- unlock histogram[%d] by days locked\n", (int32_t)(sizeof(histo) / sizeof(*histo)));
    } */

    if ((height & 1) != 0) // odd block - no marmara opret
        return(0);

    if (tx.vout.size() == 2 && tx.vout[1].nValue == 0)
    {
        if (DecodeMarmaraCoinbaseOpRet(tx.vout[1].scriptPubKey, pk, ht, unlockht) == 'C')
        {
            if (ht == height && MarmaraUnlockht(height) == unlockht)
            {
                //fprintf(stderr,"ht.%d -> unlock.%d\n",ht,unlockht);
                ccvout = MakeCC1of2vout(EVAL_MARMARA, 0, Marmarapk, pk);
                if (ccvout.scriptPubKey == tx.vout[0].scriptPubKey)
                    return(0);
                char addr0[KOMODO_ADDRESS_BUFSIZE], addr1[KOMODO_ADDRESS_BUFSIZE];
                Getscriptaddress(addr0, ccvout.scriptPubKey);
                Getscriptaddress(addr1, tx.vout[0].scriptPubKey);
                fprintf(stderr, "%s ht.%d mismatched CCvout scriptPubKey %s vs %s pk.%d %s\n", __func__, height, addr0, addr1, (int32_t)pk.size(), HexStr(pk).c_str());
            }
            else 
                fprintf(stderr, "%s ht.%d %d vs %d unlock.%d\n", __func__, height, MarmaraUnlockht(height), ht, unlockht);
        }
        else 
            fprintf(stderr, "%s ht.%d error decoding coinbase opret\n", __func__, height);
    }
    return(-1);
}

bool MarmaraPoScheck(char *destaddr, CScript opret, CTransaction staketx)
{
    CPubKey Marmarapk, pk; 
    int32_t height, unlockht; 
    uint8_t funcid; 
    char coinaddr[KOMODO_ADDRESS_BUFSIZE]; 
    struct CCcontract_info *cp, C;

    //fprintf(stderr,"%s %s numvins.%d numvouts.%d %.8f opret[%d]\n", _func__, staketx.GetHash().ToString().c_str(), (int32_t)staketx.vin.size(), (int32_t)staketx.vout.size(), (double)staketx.vout[0].nValue/COIN, (int32_t)opret.size());
    if (staketx.vout.size() == 2 && opret == staketx.vout[1].scriptPubKey)
    {
        cp = CCinit(&C, EVAL_MARMARA);
        funcid = DecodeMarmaraCoinbaseOpRet(opret, pk, height, unlockht);
        Marmarapk = GetUnspendable(cp, 0);
        GetCCaddress1of2(cp, coinaddr, Marmarapk, pk);
        //fprintf(stderr,"%s matched opret! funcid.%c ht.%d unlock.%d %s\n", __func__,funcid,height,unlockht,coinaddr);
        return(strcmp(destaddr, coinaddr) == 0);
    }
    return(0);
}

// enumerates mypk activated cc vouts
// calls a callback allowing to do something with the utxos (add to staking utxo array)
// TODO: maybe better to use AddMarmaraInputs with a callback for unification...
template <class T>
static void EnumMyActivated(T func)
{
    struct CCcontract_info *cp, C;
    std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> > activatedOutputs;

    cp = CCinit(&C, EVAL_MARMARA);
    CPubKey mypk = pubkey2pk(Mypubkey());
    CPubKey Marmarapk = GetUnspendable(cp, NULL);

    char activatedaddr[KOMODO_ADDRESS_BUFSIZE];
    GetCCaddress1of2(cp, activatedaddr, Marmarapk, mypk);

    // add activated coins for mypk:
    SetCCunspents(activatedOutputs, activatedaddr, true);

    // add my activated coins:
    fprintf(stderr,"%s check activatedaddr.(%s)\n", __func__, activatedaddr);
    for (std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> >::const_iterator it = activatedOutputs.begin(); it != activatedOutputs.end(); it++)
    {
        CTransaction tx; uint256 hashBlock;
        CBlockIndex *pindex;

        uint256 txid = it->first.txhash;
        int32_t nvout = (int32_t)it->first.index;
        CAmount nValue;

        if ((nValue = it->second.satoshis) < COIN)   // skip small values
            continue;

        fprintf(stderr,"%s check on activatedaddr txid=%s/vout=%d\n", __func__, txid.GetHex().c_str(), nvout);

        // TODO: change to the non-locking version:
        if (GetTransaction(txid, tx, hashBlock, true) && (pindex = komodo_getblockindex(hashBlock)) != 0 && myIsutxo_spentinmempool(ignoretxid, ignorevin, txid, nvout) == 0)
        {
            char utxoaddr[KOMODO_ADDRESS_BUFSIZE] = "";

            Getscriptaddress(utxoaddr, tx.vout[nvout].scriptPubKey);
            if (strcmp(activatedaddr, utxoaddr) == 0)  // check if real vout address matches index address (as another key could be used in the addressindex)
            {
                CPubKey pk;
                if (CheckEitherOpRet(IsActivatedOpret, tx, nvout, pk))
                {
                    // call callback function:
                    func(activatedaddr, tx, nvout, pindex);
                    std::cerr << __func__ << " found my activated 1of2 addr txid=" << txid.GetHex() << " vout=" << nvout << std::endl;
                }
                else
                    std::cerr << __func__ << " skipped activated 1of2 addr txid=" << txid.GetHex() << " vout=" << nvout << " cant decode opret or not mypk" << std::endl;
            }
            else
                std::cerr << __func__ << " skipped activated 1of2 addr txid=" << txid.GetHex() << " vout=" << nvout << " uxto addr not matched index" << std::endl;
        }
    }
}

// enumerates mypk locked in loop cc vouts
// calls a callback allowing to do something with the utxos (add to staking utxo array)
// TODO: maybe better to use AddMarmaraInputs with a callback for unification...
template <class T>
static void EnumMyLockedInLoop(T func)
{
    char markeraddr[KOMODO_ADDRESS_BUFSIZE];
    std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> > markerOutputs;

    struct CCcontract_info *cp, C;
    cp = CCinit(&C, EVAL_MARMARA);
    CPubKey mypk = pubkey2pk(Mypubkey());
    CPubKey Marmarapk = GetUnspendable(cp, NULL);

    GetCCaddress(cp, markeraddr, Marmarapk);
    SetCCunspents(markerOutputs, markeraddr, true);

    // enum all createtxids:
    fprintf(stderr, "%s check markeraddr.(%s)\n", __func__, markeraddr);
    for (std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> >::const_iterator it = markerOutputs.begin(); it != markerOutputs.end(); it++)
    {
        CTransaction isssuancetx;
        uint256 hashBlock;
        uint256 txid = it->first.txhash;
        int32_t nvout = (int32_t)it->first.index;

        fprintf(stderr, "%s check on markeraddr txid=%s/vout=%d\n", __func__, txid.GetHex().c_str(), nvout);
        if (nvout == 1 && GetTransaction(txid, isssuancetx, hashBlock, true))  // TODO: change to the non-locking version
        {
            if (!isssuancetx.IsCoinBase() && isssuancetx.vout.size() > 2 && isssuancetx.vout.back().nValue == 0)
            {
                int32_t matures;
                std::string currency;
                CPubKey senderpk;
                uint256 createtxid;
                CAmount amount;

                if (MarmaraDecodeLoopOpret(isssuancetx.vout.back().scriptPubKey, createtxid, senderpk, amount, matures, currency) == 'I')
                {
                    char loopaddr[KOMODO_ADDRESS_BUFSIZE];
                    std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> > loopOutputs;
                    char txidaddr[KOMODO_ADDRESS_BUFSIZE];
                    CPubKey createtxidPk = CCtxidaddr(txidaddr, createtxid);

                    GetCCaddress1of2(cp, loopaddr, Marmarapk, createtxidPk);
                    SetCCunspents(loopOutputs, loopaddr, true);

                    // enum all locked-in-loop addresses:
                    fprintf(stderr, "%s check loopaddr.(%s)\n", __func__, loopaddr);
                    for (std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> >::const_iterator it = loopOutputs.begin(); it != loopOutputs.end(); it++)
                    {
                        CTransaction looptx;
                        uint256 hashBlock;
                        CBlockIndex *pindex;
                        uint256 txid = it->first.txhash;
                        int32_t nvout = (int32_t)it->first.index;

                        fprintf(stderr, "%s check on loopaddr txid=%s/vout=%d\n", __func__, txid.GetHex().c_str(), nvout);

                        if (GetTransaction(txid, looptx, hashBlock, true) && (pindex = komodo_getblockindex(hashBlock)) != 0 && myIsutxo_spentinmempool(ignoretxid, ignorevin, txid, nvout) == 0)  // TODO: change to the non-locking version
                        {
                            /* lock-in-loop cant be mined */                   /* now it could be cc opret, not necessary OP_RETURN vout in the back */
                            if (!looptx.IsCoinBase() && looptx.vout.size() > 0 /* && looptx.vout.back().nValue == 0 */)  
                            {
                                char utxoaddr[KOMODO_ADDRESS_BUFSIZE] = "";

                                Getscriptaddress(utxoaddr, looptx.vout[nvout].scriptPubKey);
                                if (strcmp(loopaddr, utxoaddr) == 0)  // check if real vout address matches index address (as another key could be used in the addressindex)
                                {
                                    CPubKey pk;

                                    if (CheckEitherOpRet(IsLockInLoopOpret, looptx, nvout, pk) && mypk == pk) {  // check mypk in opret
                                        // call callbak func:
                                        func(loopaddr, looptx, nvout, pindex);
                                        std::cerr << __func__ << " found my lock-in-loop 1of2 addr txid=" << txid.GetHex() << " vout=" << nvout << std::endl;
                                    }
                                    else
                                        std::cerr << __func__ << " skipped lock-in-loop 1of2 addr txid=" << txid.GetHex() << " vout=" << nvout << " cant decode opret or not mypk" << std::endl;
                                }
                                else
                                    std::cerr << __func__ << " skipped lock-in-loop 1of2 addr txid=" << txid.GetHex() << " vout=" << nvout << " uxto addr not matched index" << std::endl;
                            }
                        }
                    }
                }
            }
        }
        else
            std::cerr << __func__ << " error getting tx=" << txid.GetHex() << std::endl;
    }
}


// add marmara special UTXO from activated and lock-in-loop addresses for staking
// called from PoS code
struct komodo_staking *MarmaraGetStakingUtxos(struct komodo_staking *array, int32_t *numkp, int32_t *maxkp, uint8_t *hashbuf)
{
    // add activated utxos for mypk:
    EnumMyActivated([&](char *activatedaddr, const CTransaction & tx, int32_t nvout, CBlockIndex *pindex) 
    {
        array = komodo_addutxo(array, numkp, maxkp, (uint32_t)pindex->nTime, (uint64_t)tx.vout[nvout].nValue, tx.GetHash(), nvout, activatedaddr, hashbuf, tx.vout[nvout].scriptPubKey);
    });

    // add lock-in-loops utxos for mypk:
    EnumMyLockedInLoop([&](char *activatedaddr, const CTransaction & tx, int32_t nvout, CBlockIndex *pindex)
    {
        array = komodo_addutxo(array, numkp, maxkp, (uint32_t)pindex->nTime, (uint64_t)tx.vout[nvout].nValue, tx.GetHash(), nvout, activatedaddr, hashbuf, tx.vout[nvout].scriptPubKey);
    });
   
    return array;
}

// returns stake preferences for activated and locked vouts
int32_t MarmaraGetStakeMultiplier(const CTransaction & tx, int32_t nvout)
{
    CPubKey mypk = pubkey2pk(Mypubkey());
    CPubKey opretpk;

    if (nvout > 0 && nvout < tx.vout.size()) // check boundary
    {
        if (CheckEitherOpRet(IsLockInLoopOpret, tx, nvout, opretpk) && mypk == opretpk) {  // check if opret is lock-in-loop and cc vout is mypk
            if (tx.vout[nvout].scriptPubKey.IsPayToCryptoCondition()) {
                uint8_t funcid = 0;
                uint256 createtxid;
                int64_t amount;
                int32_t matures;
                std::string currency;

                if (MarmaraDecodeLoopOpret(tx.vout.back().scriptPubKey, createtxid, opretpk, amount, matures, currency) != 0)     {
                    struct CCcontract_info *cp, C;
                    cp = CCinit(&C, EVAL_MARMARA);
                    CPubKey Marmarapk = GetUnspendable(cp, NULL);

                    char txidaddr[KOMODO_ADDRESS_BUFSIZE];
                    char lockInLoop1of2addr[KOMODO_ADDRESS_BUFSIZE];
                    char voutaddr[KOMODO_ADDRESS_BUFSIZE];
                    CPubKey createtxidPk = CCtxidaddr(txidaddr, createtxid);
                    GetCCaddress1of2(cp, lockInLoop1of2addr, Marmarapk, createtxidPk);

                    Getscriptaddress(voutaddr, tx.vout[nvout].scriptPubKey);
                    if (strcmp(lockInLoop1of2addr, voutaddr) == 0) {  // check vout address is lock-in-loop address
                        std::cerr << __func__ << "picked for stake x3 as lock-in-loop" << " txid=" << tx.GetHash().GetHex() << " nvout=" << nvout << std::endl;
                        return 3;  // staked 3 times for lock-in-loop
                    }
                }
            }
        }

        if (CheckEitherOpRet(IsActivatedOpret, tx, nvout, opretpk))   // check if this is activated opret 
        {
            if (tx.vout[nvout].scriptPubKey.IsPayToCryptoCondition())    {    
                struct CCcontract_info *cp, C;
                cp = CCinit(&C, EVAL_MARMARA);
                CPubKey Marmarapk = GetUnspendable(cp, NULL);

                char activated1of2addr[KOMODO_ADDRESS_BUFSIZE];
                char voutaddr[KOMODO_ADDRESS_BUFSIZE];
                GetCCaddress1of2(cp, activated1of2addr, Marmarapk, mypk);

                Getscriptaddress(voutaddr, tx.vout[nvout].scriptPubKey);
                if (strcmp(activated1of2addr, voutaddr) == 0) {  // check vout address is my activated address
                    std::cerr << __func__ << "picked for stake x2 as activated" << " txid=" << tx.GetHash().GetHex() << " nvout=" << nvout << std::endl;
                    return 2;  // staked 3 times for activated
                }
            }
        }
    }
    return 1;
}

bool MarmaraValidate(struct CCcontract_info *cp, Eval* eval, const CTransaction &tx, uint32_t nIn)
{
    vscript_t vopret; CTransaction vinTx; uint256 hashBlock;  int32_t numvins, numvouts, i, ht, unlockht, vht, vunlockht; uint8_t funcid, vfuncid, *script; CPubKey pk, vpk;
    if (ASSETCHAINS_MARMARA == 0)
        return eval->Invalid("-ac_marmara must be set for marmara CC");
    numvins = tx.vin.size();
    numvouts = tx.vout.size();
    if (numvouts < 1)
        return eval->Invalid("no vouts");
    else if (tx.vout.size() >= 2)
    {
        GetOpReturnData(tx.vout[tx.vout.size() - 1].scriptPubKey, vopret);
        script = (uint8_t *)vopret.data();
        if (vopret.size() < 2 || script[0] != EVAL_MARMARA)
            return eval->Invalid("no opreturn");
        funcid = script[1];
        if (funcid == 'P')
        {
            funcid = DecodeMarmaraCoinbaseOpRet(tx.vout[tx.vout.size() - 1].scriptPubKey, pk, ht, unlockht);
            for (i = 0; i < numvins; i++)
            {
                if ((*cp->ismyvin)(tx.vin[i].scriptSig) != 0)
                {
                    if (eval->GetTxUnconfirmed(tx.vin[i].prevout.hash, vinTx, hashBlock) == 0)
                        return eval->Invalid("cant find vinTx");
                    else
                    {
                        if (vinTx.IsCoinBase() == 0)
                            return eval->Invalid("noncoinbase input");
                        else if (vinTx.vout.size() != 2)
                            return eval->Invalid("coinbase doesnt have 2 vouts");
                        vfuncid = DecodeMarmaraCoinbaseOpRet(vinTx.vout[1].scriptPubKey, vpk, vht, vunlockht);
                        if (vfuncid != 'C' || vpk != pk || vunlockht != unlockht)
                            return eval->Invalid("mismatched opreturn");
                    }
                }
            }
            return(true);
        }
        else if (funcid == 'L') // lock -> lock funds with a unlockht
        {
            return(true);
        }
        else if (funcid == 'R') // receive -> agree to receive 'I' from pk, amount, currency, dueht
        {
            return(true);
        }
        else if (funcid == 'I') // issue -> issue currency to pk with due date height
        {
            return(true);
        }
        else if (funcid == 'T') // transfer -> given 'R' transfer 'I' or 'T' to the pk of 'R'
        {
            return(true);
        }
        else if (funcid == 'S') // settlement -> automatically spend issuers locked funds, given 'I'
        {
            return(true);
        }
        else if (funcid == 'D') // insufficient settlement
        {
            return(true);
        }
        else if (funcid == 'C') // coinbase
        {
            return(true);
        }
        // staking only for locked utxo
    }
    return eval->Invalid("fall through error");
}
// end of consensus code

// helper functions for rpc calls in rpcwallet.cpp

// add mined coins
int64_t AddMarmaraCoinbases(struct CCcontract_info *cp, CMutableTransaction &mtx, int32_t firstheight, CPubKey poolpk, int32_t maxinputs)
{
    char coinaddr[KOMODO_ADDRESS_BUFSIZE]; 
    CPubKey Marmarapk, pk; 
    int64_t nValue, totalinputs = 0; 
    uint256 txid, hashBlock; 
    CTransaction vintx; 
    int32_t unlockht, ht, vout, unlocks, n = 0;
    std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> > unspentOutputs;

    Marmarapk = GetUnspendable(cp, 0);
    GetCCaddress1of2(cp, coinaddr, Marmarapk, poolpk);
    SetCCunspents(unspentOutputs, coinaddr, true);
    unlocks = MarmaraUnlockht(firstheight);

    //fprintf(stderr,"%s check coinaddr.(%s)\n", __func__, coinaddr);
    for (std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> >::const_iterator it = unspentOutputs.begin(); it != unspentOutputs.end(); it++)
    {
        txid = it->first.txhash;
        vout = (int32_t)it->first.index;
        //fprintf(stderr,"%s txid.%s/v%d\n", __func__, txid.GetHex().c_str(), vout);
        if ( myGetTransaction(txid,vintx,hashBlock) != 0 )
        {
            if (vintx.IsCoinBase() != 0 && vintx.vout.size() == 2 && vintx.vout[1].nValue == 0)
            {
                if (DecodeMarmaraCoinbaseOpRet(vintx.vout[1].scriptPubKey, pk, ht, unlockht) == 'C' && unlockht == unlocks && pk == poolpk && ht >= firstheight)
                {
                    if ((nValue = vintx.vout[vout].nValue) > 0 && myIsutxo_spentinmempool(ignoretxid, ignorevin, txid, vout) == 0)
                    {
                        if (maxinputs != 0)
                            mtx.vin.push_back(CTxIn(txid, vout, CScript()));
                        nValue = it->second.satoshis;
                        totalinputs += nValue;
                        n++;
                        if (maxinputs > 0 && n >= maxinputs)
                            break;
                    } 
                    //else 
                    //  fprintf(stderr,"%s nValue.%8f\n", __func__, (double)nValue/COIN);
                } 
                //else 
                //    fprintf(stderr,"%s decode error unlockht.%d vs %d is-pool-pk.%d\n", __func__, unlockht, unlocks, pk == poolpk);
            }
            else
                std::cerr << __func__ << " not coinbase" << std::endl;
        }
        else
            std::cerr << __func__ << " error getting tx" << txid.GetHex() << std::endl;
    }
    return(totalinputs);
}

// is opret for activated coins, returns pk from opret
static bool IsActivatedOpret(const CScript &spk, CPubKey &pk) 
{ 
    uint8_t funcid = 0;
    int32_t ht, unlockht;
   
    return (funcid = DecodeMarmaraCoinbaseOpRet(spk, pk, ht, unlockht)) == 'C' || funcid == 'P' || funcid == 'L';
}

// is opret for lock-in-loop coins, returns pk from opret
static bool IsLockInLoopOpret(const CScript &spk, CPubKey &pk)
{
    uint8_t funcid = 0;
    uint256 createtxid;
    int64_t amount;
    int32_t matures;
    std::string currency;

    return MarmaraDecodeLoopOpret(spk, createtxid, pk, amount, matures, currency) != 0;
}

// checks opret by calling CheckOpretFunc for two cases:
// opret in cc vout data is checked first and considered primary
// opret in the last vout is checked second and considered secondary
static bool CheckEitherOpRet(bool(*CheckOpretFunc)(const CScript &, CPubKey &), const CTransaction &tx, int32_t nvout, CPubKey & pk)
{
    CScript opret, dummy;
    std::vector< vscript_t > vParams;
    bool isccopret = false, opretok = false;

    // first check cc opret
    tx.vout[nvout].scriptPubKey.IsPayToCryptoCondition(&dummy, vParams);
    if (vParams.size() > 0) {
        COptCCParams p = COptCCParams(vParams[0]);
        if (p.vData.size() > 0) {
            opret << OP_RETURN << p.vData[0]; // reconstruct opret for CheckOpretFunc function
            std::cerr << __func__ << " ccopret=" << opret.ToString() << std::endl;
            if (CheckOpretFunc(opret, pk)) {
                isccopret = true;
                opretok = true;
            }
        }
    }

    // check last opret:
    if (!opretok) {  // right opret not found in cc vout then check opret in the back of vouts
        if (nvout < tx.vout.size()) {   // there might be opret in the back
            opret = tx.vout.back().scriptPubKey;
            if (CheckOpretFunc(opret, pk)) {
                isccopret = false;
                opretok = true;
            }
        }
    }

    // print opret evalcode and funcid for debug logging:
    vscript_t vprintopret;
    uint8_t funcid = 0, evalcode = 0;
    if (GetOpReturnData(opret, vprintopret) && vprintopret.size() >= 2) {
        evalcode = vprintopret.begin()[0];
        funcid = vprintopret.begin()[1];
    }
    std::cerr << __func__ << " opret eval=" << (int)evalcode << " funcid=" << (char)(funcid ? funcid : ' ') << " isccopret=" << isccopret << std::endl;
    
    return opretok;
}


// add activated or locked-in-loop coins from 1of2 address (for lock-in-loop mypk not checked, so all locked-in-loop added for an address):
int64_t AddMarmarainputs(bool (*CheckOpretFunc)(const CScript &, CPubKey &), CMutableTransaction &mtx, std::vector<CPubKey> &pubkeys, char *unspentaddr, int64_t total, int32_t maxinputs)
{
    int64_t threshold, nValue, totalinputs = 0; 
    int32_t n = 0;
    std::vector<int64_t> vals;
    std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> > unspentOutputs;

    SetCCunspents(unspentOutputs, unspentaddr, true);
    if (maxinputs > CC_MAXVINS)
        maxinputs = CC_MAXVINS;
    if (maxinputs > 0)
        threshold = total / maxinputs;
    else 
        threshold = total;

    if (CheckOpretFunc == NULL)  // no function to check opret
        return -1;

    std::cerr << __func__ << " adding from addr=" << unspentaddr << " total=" << total << std::endl;
    for (std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> >::const_iterator it = unspentOutputs.begin(); it != unspentOutputs.end(); it++)
    {
        uint256 txid = it->first.txhash;
        int32_t nvout = (int32_t)it->first.index;
        uint256 hashBlock;
        CTransaction tx;

        if (it->second.satoshis < threshold)
            continue;

        // check if vin might be already added to mtx:
        if (std::find_if(mtx.vin.begin(), mtx.vin.end(), [&](CTxIn v) {return (v.prevout.hash == txid && v.prevout.n == nvout); }) != mtx.vin.end())
            continue;

        if (myGetTransaction(txid, tx, hashBlock) != 0 && tx.vout.size() > 0 && 
            tx.vout[nvout].scriptPubKey.IsPayToCryptoCondition() != 0 && myIsutxo_spentinmempool(ignoretxid, ignorevin, txid, nvout) == 0)
        {
            CPubKey pk;
            CScript opret, dummy;
            std::vector< vscript_t > vParams;
            bool isccopret = false, opretok = false;

            // considers 2 cases:
            // opret is in the last vout
            // opret is in cc vout data
            if (CheckEitherOpRet(CheckOpretFunc, tx, nvout, pk))
            {
                char utxoaddr[KOMODO_ADDRESS_BUFSIZE];

                Getscriptaddress(utxoaddr, tx.vout[nvout].scriptPubKey);
                if (strcmp(unspentaddr, utxoaddr) == 0)  // check if the real vout address matches the index address (as another key could be used in the addressindex)
                {
                    std::cerr << __func__ << " found good vintx for addr=" << unspentaddr << " txid=" << txid.GetHex() << " nvout=" << nvout << " satoshis=" << it->second.satoshis << " isccopret=" << isccopret << std::endl;

                    if (total != 0 && maxinputs != 0)
                    {
                        mtx.vin.push_back(CTxIn(txid, nvout, CScript()));
                        pubkeys.push_back(pk);
                    }
                    totalinputs += it->second.satoshis;
                    vals.push_back(it->second.satoshis);
                    n++;
                    if (maxinputs != 0 && total == 0)  
                        continue;
                    if ((total > 0 && totalinputs >= total) || (maxinputs > 0 && n >= maxinputs))
                        break;
                }
            }
            else
                std::cerr << __func__ << " addr=" << unspentaddr << " txid=" << txid.GetHex() << " cant check opret" << std::endl;
        }
    }
    if (maxinputs != 0 && total == 0)
    {
        std::sort(vals.begin(), vals.end());
        totalinputs = 0;
        for (int32_t i = 0; i < maxinputs && i < vals.size(); i++)
            totalinputs += vals[i];
    }

    std::cerr << __func__ << " for addr=" << unspentaddr << " found total=" << totalinputs << std::endl;
    return(totalinputs);
}

// lock the amount on the specified block height
UniValue MarmaraLock(int64_t txfee, int64_t amount)
{
    CMutableTransaction tmpmtx, mtx = CreateNewContextualCMutableTransaction(Params().GetConsensus(), komodo_nextheight());
    UniValue result(UniValue::VOBJ);
    struct CCcontract_info *cp, C;
    CPubKey Marmarapk, mypk, pk;
    //int32_t unlockht, /*refunlockht,*/ nvout, ht, numvouts;
    int64_t nValue, val, inputsum = 0, threshold, remains, change = 0;
    std::string rawtx, errorstr;
    char mynormaladdr[KOMODO_ADDRESS_BUFSIZE], activated1of2addr[KOMODO_ADDRESS_BUFSIZE];
    uint256 txid, hashBlock;
    CTransaction tx;
    uint8_t funcid;

    if (txfee == 0)
        txfee = 10000;

    int32_t height = komodo_nextheight();
    // as opret creation function MarmaraCoinbaseOpret creates opret only for even blocks - adjust this base height to even value
    if ((height & 1) != 0)
         height++;

    cp = CCinit(&C, EVAL_MARMARA);
    mypk = pubkey2pk(Mypubkey());
    Marmarapk = GetUnspendable(cp, 0);

    Getscriptaddress(mynormaladdr, CScript() << ParseHex(HexStr(mypk)) << OP_CHECKSIG);
    if ((val = CCaddress_balance(mynormaladdr, 0)) < amount) // if not enough funds in the wallet
        val -= 2 * txfee;    // dont take all, should al least 1 txfee remained 
    else
        val = amount;
    if (val > txfee)
        inputsum = AddNormalinputs2(mtx, val + txfee, CC_MAXVINS / 2);  //added '+txfee' because if 'inputsum' exactly was equal to 'val' we'd exit from insufficient funds 
    //fprintf(stderr,"%s added normal inputs=%.8f required val+txfee=%.8f\n", __func__, (double)inputsum/COIN,(double)(val+txfee)/COIN);

    // lock the amount on 1of2 address:
    mtx.vout.push_back(MakeCC1of2vout(EVAL_MARMARA, amount, Marmarapk, mypk));

    if (inputsum < amount + txfee)  // if not enough normal inputs for collateral
    {
        //refunlockht = MarmaraUnlockht(height);  // randomized 

        result.push_back(Pair("normalfunds", ValueFromAmount(inputsum)));
        result.push_back(Pair("height", height));
        //result.push_back(Pair("unlockht", refunlockht));

        // fund remainder to add:
        remains = (amount + txfee) - inputsum;

        std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> > unspentOutputs;
        GetCCaddress1of2(cp, activated1of2addr, Marmarapk, mypk);
        SetCCunspents(unspentOutputs, activated1of2addr, true);
        threshold = remains / (MARMARA_VINS + 1);
        uint8_t mypriv[32];
        Myprivkey(mypriv);
        CCaddr1of2set(cp, Marmarapk, mypk, mypriv, activated1of2addr);

        // try to add collateral remainder from the activated  fund (and re-lock it):
        /* we cannot do this any more as activated funds are locked to the max height
           we need first unlock them to normal to move to activated again:
        for (std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> >::const_iterator it = unspentOutputs.begin(); it != unspentOutputs.end(); it++)
        {
            txid = it->first.txhash;
            nvout = (int32_t)it->first.index;
            if ((nValue = it->second.satoshis) < threshold)
                continue;
            if (myGetTransaction(txid, tx, hashBlock) != 0 && (numvouts = tx.vout.size()) > 0 && nvout < numvouts && tx.vout[nvout].scriptPubKey.IsPayToCryptoCondition() != 0 && myIsutxo_spentinmempool(ignoretxid, ignorevin, txid, nvout) == 0)
            {
                if ((funcid = DecodeMarmaraCoinbaseOpRet(tx.vout[numvouts - 1].scriptPubKey, pk, ht, unlockht)) == 'C' || funcid == 'P' || funcid == 'L')
                {
                    if (unlockht < refunlockht)  // if allowed to unlock already
                    {
                        mtx.vin.push_back(CTxIn(txid, nvout, CScript()));
                        //fprintf(stderr,"merge CC vout %s/v%d %.8f unlockht.%d < ref.%d\n",txid.GetHex().c_str(),vout,(double)nValue/COIN,unlockht,refunlockht);
                        inputsum += nValue;
                        remains -= nValue;
                        if (inputsum >= amount + txfee)
                        {
                            //fprintf(stderr,"inputsum %.8f >= amount %.8f, update amount\n",(double)inputsum/COIN,(double)amount/COIN);
                            amount = inputsum - txfee;
                            break;
                        }
                    }
                }
                else    
                    std::cerr << __func__, " incorrect funcid in tx from locked fund" << std::endl;
            }
            else  
                std::cerr << __func__ << " could not load or incorrect tx from locked fund" << std::endl;
        }   */

        memset(mypriv,0,sizeof(mypriv));
    }
    if (inputsum >= amount + txfee)
    {
        if (inputsum > amount + txfee)
        {
            change = (inputsum - amount);
            mtx.vout.push_back(CTxOut(change, CScript() << ParseHex(HexStr(mypk)) << OP_CHECKSIG));
        }
        rawtx = FinalizeCCTx(0, cp, mtx, mypk, txfee, MarmaraCoinbaseOpret('L', height, mypk));
        if (rawtx.size() == 0)
        {
            errorstr = (char *)"couldnt finalize CCtx";
        }
        else
        {
            result.push_back(Pair("result", (char *)"success"));
            result.push_back(Pair("hex", rawtx));
            return(result);
        }
    }
    else
        errorstr = (char *)"insufficient funds";
    result.push_back(Pair("result", (char *)"error"));
    result.push_back(Pair("error", errorstr));
    return(result);
}

int32_t MarmaraSignature(uint8_t *utxosig, CMutableTransaction &mtx)
{
    uint256 txid, hashBlock; uint8_t *ptr; int32_t i, siglen, vout, numvouts; CTransaction tx; std::string rawtx; CPubKey mypk; std::vector<CPubKey> pubkeys; struct CCcontract_info *cp, C; int64_t txfee;
    txfee = 10000;
    vout = mtx.vin[0].prevout.n;
    if ( myGetTransaction(mtx.vin[0].prevout.hash,tx,hashBlock) != 0 && (numvouts= tx.vout.size()) > 1 && vout < numvouts )
    {
        cp = CCinit(&C, EVAL_MARMARA);
        mypk = pubkey2pk(Mypubkey());
        pubkeys.push_back(mypk);
        rawtx = FinalizeCCTx(0, cp, mtx, mypk, txfee, tx.vout[numvouts - 1].scriptPubKey, pubkeys);
        if (rawtx.size() > 0)
        {
            siglen = mtx.vin[0].scriptSig.size();
            ptr = &mtx.vin[0].scriptSig[0];

            std::cerr << __func__ << " ";
            for (i = 0; i < siglen; i++)
            {
                utxosig[i] = ptr[i];
                //fprintf(stderr,"%02x",ptr[i]);
            }
            //fprintf(stderr," got signed rawtx.%s siglen.%d\n",rawtx.c_str(),siglen);
            return(siglen);
        }
    }
    return(0);
}

// jl777: decide on what unlockht settlement change should have -> from utxo making change

UniValue MarmaraSettlement(int64_t txfee, uint256 refbatontxid)
{
    CMutableTransaction mtx = CreateNewContextualCMutableTransaction(Params().GetConsensus(), komodo_nextheight());
    UniValue result(UniValue::VOBJ), a(UniValue::VARR);
    std::vector<uint256> creditloop;
    uint256 batontxid, refcreatetxid, hashBlock;
    uint8_t funcid;
    int32_t numerrs = 0, n, refmatures;
    int64_t refamount, inputsum;
    std::string refcurrency, rawtx;
    CTransaction tx, batontx;
    char loop1of2addr[KOMODO_ADDRESS_BUFSIZE], myCCaddr[KOMODO_ADDRESS_BUFSIZE], destaddr[KOMODO_ADDRESS_BUFSIZE], batonCCaddr[KOMODO_ADDRESS_BUFSIZE], str[2], txidaddr[KOMODO_ADDRESS_BUFSIZE];
    struct CCcontract_info *cp, C;

    if (txfee == 0)
        txfee = 10000;

    cp = CCinit(&C, EVAL_MARMARA);
    CPubKey mypk = pubkey2pk(Mypubkey());
    uint8_t marmarapriv[32];
    CPubKey Marmarapk = GetUnspendable(cp, marmarapriv);
    
    int64_t change = 0;
    int32_t height = chainActive.LastTip()->GetHeight();
    if ((n = MarmaraGetbatontxid(creditloop, batontxid, refbatontxid)) > 0)
    {
        if (myGetTransaction(batontxid, batontx, hashBlock) && batontx.vout.size() > 1)
        {
            CPubKey pk;

            if ((funcid = MarmaraDecodeLoopOpret(batontx.vout.back().scriptPubKey, refcreatetxid, pk, refamount, refmatures, refcurrency)) != 0)
            {
                if (refcreatetxid != creditloop[0])
                {
                    result.push_back(Pair("result", (char *)"error"));
                    result.push_back(Pair("error", (char *)"invalid refcreatetxid, setting to creditloop[0]"));
                    return(result);
                }
                else if (chainActive.LastTip()->GetHeight() < refmatures)
                {
                    fprintf(stderr, "doesnt mature for another %d blocks\n", refmatures - chainActive.LastTip()->GetHeight());
                    result.push_back(Pair("result", (char *)"error"));
                    result.push_back(Pair("error", (char *)"cant settle immature creditloop"));
                    return(result);
                }
                else if ((refmatures & 1) == 0)
                {
                    result.push_back(Pair("result", (char *)"error"));
                    result.push_back(Pair("error", (char *)"cant automatic settle even maturity heights"));
                    return(result);
                }
                else if (n < 1)
                {
                    result.push_back(Pair("result", (char *)"error"));
                    result.push_back(Pair("error", (char *)"creditloop too short"));
                    return(result);
                }
                // remaining = refamount;
                GetCCaddress(cp, myCCaddr, Mypubkey());
                Getscriptaddress(batonCCaddr, batontx.vout[0].scriptPubKey);
                if (strcmp(myCCaddr, batonCCaddr) == 0) // if mypk user owns the baton
                {
                    std::vector<CPubKey> pubkeys;

                    mtx.vin.push_back(CTxIn(n == 1 ? batontxid : creditloop[1], 1, CScript())); // spend issuance marker - close the loop

                    if (AddNormalinputs2(mtx, txfee, 4) < txfee) {
                        result.push_back(Pair("result", (char *)"error"));
                        result.push_back(Pair("error", (char *)"cant add normal inputs for txfee"));
                        return(result);
                    }

                    char lockInLoop1of2addr[KOMODO_ADDRESS_BUFSIZE], txidaddr[KOMODO_ADDRESS_BUFSIZE];
                    CPubKey createtxidPk = CCtxidaddr(txidaddr, refcreatetxid);
                    GetCCaddress1of2(cp, lockInLoop1of2addr, Marmarapk, createtxidPk);  // 1of2 lock-in-loop address

                    CC *lockInLoop1of2cond = MakeCCcond1of2(EVAL_MARMARA, Marmarapk, createtxidPk);
                    CCAddVintxCond(cp, lockInLoop1of2cond, marmarapriv); //add probe condition to spend from the lock-in-loop address
                    cc_free(lockInLoop1of2cond);

                    std::cerr << __func__ << "calling AddMarmaraInputs for lock-in-loop addr=" << lockInLoop1of2addr << " adding amount=" << refamount << std::endl;
                    if ((inputsum = AddMarmarainputs(IsLockInLoopOpret, mtx, pubkeys, lockInLoop1of2addr, refamount, MARMARA_VINS)) >= refamount)
                    {
                        change = (inputsum - refamount);
                        mtx.vout.push_back(CTxOut(refamount, CScript() << ParseHex(HexStr(mypk)) << OP_CHECKSIG));   // locked-in-loop money is released to mypk doing the settlement
                        if (change > txfee) {
                            std::cerr << __func__ << "error: change remained=" << change << ", sent to lock-in-loop addr=" << lockInLoop1of2addr << std::endl;
                            mtx.vout.push_back(MakeCC1of2vout(EVAL_MARMARA, change, Marmarapk, createtxidPk));
                        }
                        rawtx = FinalizeCCTx(0, cp, mtx, mypk, txfee, MarmaraEncodeLoopOpret('S', refcreatetxid, mypk, 0, refmatures, refcurrency), pubkeys);
                        if (rawtx.empty()) {
                            result.push_back(Pair("result", "error"));
                            result.push_back(Pair("error", "couldnt finalize CCtx"));
                            std::cerr << __func__ << " bad mtx=" << HexStr(E_MARSHAL(ss << mtx)) << std::endl;
                        }
                        else {
                            result.push_back(Pair("result", (char *)"success"));
                            result.push_back(Pair("hex", rawtx));
                        }
                        return(result);
                    }
                    // pubkeys.push_back(Marmarapk);
                    // mtx.vin.push_back(CTxIn(batontxid, 0, CScript()));
                    // pubkeys.push_back(mypk);
                    /*
                    for (i = 1; i < n; i++)  //iterate through all issuers/endorsers (i=0 is 1st receiver approval/request txid)
                    {
                        if (myGetTransaction(creditloop[i], tx, hashBlock) != 0 && tx.vout.size() > 1)
                        {
                            if ((funcid = MarmaraDecodeLoopOpret(tx.vout.back().scriptPubKey, createtxid, pk, amount, matures, currency)) != 0)  // get endorser's pk
                            {
                                GetCCaddress1of2(cp, activated1of2addr, Marmarapk, pk);  // 1of2 address where the current endorser's activated money is
                                // dimxy: why is locked height not checked? - now not needed
                                if ((inputsum = AddMarmarainputs(IsActivatedOpret, mtx, pubkeys, activated1of2addr, remaining, MARMARA_VINS)) >= remaining) // add as much as possible amount
                                {
                                    change = (inputsum - remaining);
                                    mtx.vout.push_back(CTxOut(amount, CScript() << ParseHex(HexStr(mypk)) << OP_CHECKSIG));   // money is released to mypk who's doing the settlement
                                    if (change > txfee)
                                        mtx.vout.push_back(MakeCC1of2vout(EVAL_MARMARA, change, Marmarapk, pk));
                                    rawtx = FinalizeCCTx(0, cp, mtx, mypk, txfee, MarmaraEncodeLoopOpret('S', createtxid, mypk, 0, refmatures, currency), pubkeys);
                                    result.push_back(Pair("result", (char *)"success"));
                                    result.push_back(Pair("hex", rawtx));
                                    return(result);
                                }
                                else
                                {
                                    remaining -= inputsum;
                                }
                                if (mtx.vin.size() >= CC_MAXVINS - MARMARA_VINS)
                                    break;
                            }
                            else 
                                fprintf(stderr, "%s null funcid for creditloop[%d]\n", __func__, i);
                        }
                        else fprintf(stderr, "%s couldnt get creditloop[%d]\n", __func__, i);
                    } */
                    // if (refamount - remaining > 2 * txfee)
                    if (inputsum < refamount)
                    {
                        int64_t remaining = refamount - inputsum;
                        mtx.vout.push_back(CTxOut(txfee, CScript() << ParseHex(HexStr(CCtxidaddr(txidaddr, refcreatetxid))) << OP_CHECKSIG)); // failure marker
                        //if (refamount - remaining > 3 * txfee)
                        //    mtx.vout.push_back(CTxOut(refamount - remaining - 2 * txfee, CScript() << ParseHex(HexStr(mypk)) << OP_CHECKSIG));
                        mtx.vout.push_back(CTxOut(refamount - remaining - txfee, CScript() << ParseHex(HexStr(mypk)) << OP_CHECKSIG));

                        rawtx = FinalizeCCTx(0, cp, mtx, mypk, txfee, MarmaraEncodeLoopOpret('D', refcreatetxid, mypk, -remaining, refmatures, refcurrency), pubkeys);  //some remainder left
                        if (rawtx.empty()) {
                            result.push_back(Pair("result", "error"));
                            result.push_back(Pair("error", "couldnt finalize CCtx"));
                            std::cerr << __func__ << " bad mtx=" << HexStr(E_MARSHAL(ss << mtx)) << std::endl;
                        }
                        else {
                            result.push_back(Pair("result", (char *)"error"));
                            result.push_back(Pair("error", (char *)"insufficient funds"));
                            result.push_back(Pair("hex", rawtx));
                            result.push_back(Pair("remaining", ValueFromAmount(remaining)));
                        }
                    }
                    else
                    {
                        // jl777: maybe fund a txfee to report no funds avail
                        result.push_back(Pair("result", (char *)"error"));
                        result.push_back(Pair("error", (char *)"no funds available at all"));
                    }
                }
                else
                {
                    result.push_back(Pair("result", (char *)"error"));
                    result.push_back(Pair("error", (char *)"this node does not have the baton"));
                    result.push_back(Pair("myCCaddr", myCCaddr));
                    result.push_back(Pair("batonCCaddr", batonCCaddr));
                }
            }
            else
            {
                result.push_back(Pair("result", (char *)"error"));
                result.push_back(Pair("error", (char *)"couldnt get batontxid opret"));
            }
        }
        else
        {
            result.push_back(Pair("result", (char *)"error"));
            result.push_back(Pair("error", (char *)"couldnt find batontxid"));
        }
    }
    else
    {
        result.push_back(Pair("result", (char *)"error"));
        result.push_back(Pair("error", (char *)"couldnt get creditloop for the baton"));
    }
    return(result);
}

int32_t MarmaraGetCreditloops(int64_t &totalamount, std::vector<uint256> &issuances, int64_t &totalclosed, std::vector<uint256> &closed, struct CCcontract_info *cp, int32_t firstheight, int32_t lastheight, int64_t minamount, int64_t maxamount, CPubKey refpk, std::string refcurrency)
{
    char coinaddr[KOMODO_ADDRESS_BUFSIZE]; CPubKey Marmarapk, senderpk; int64_t amount; uint256 createtxid, txid, hashBlock; CTransaction tx; int32_t vout, matures, n = 0; std::string currency;
    std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> > unspentOutputs;
    Marmarapk = GetUnspendable(cp, 0);
    GetCCaddress(cp, coinaddr, Marmarapk);
    SetCCunspents(unspentOutputs, coinaddr, true);
    // do all txid, conditional on spent/unspent
    //fprintf(stderr,"%s check coinaddr.(%s)\n", __func__, coinaddr);
    for (std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> >::const_iterator it = unspentOutputs.begin(); it != unspentOutputs.end(); it++)
    {
        txid = it->first.txhash;
        vout = (int32_t)it->first.index;
        //fprintf(stderr,"%s on coinaddr txid=%s/vout=%d\n", __func__, txid.GetHex().c_str(), vout);
        if (vout == 1 && myGetTransaction(txid, tx, hashBlock) != 0)  // changed to the non-locking version
        {
            if (tx.IsCoinBase() == 0 && tx.vout.size() > 2 && tx.vout.back().nValue == 0)
            {
                if (MarmaraDecodeLoopOpret(tx.vout.back().scriptPubKey, createtxid, senderpk, amount, matures, currency) == 'I')
                {
                    n++;
                    if (currency == refcurrency && matures >= firstheight && matures <= lastheight && amount >= minamount && amount <= maxamount && (refpk.size() == 0 || senderpk == refpk))
                    {
                        issuances.push_back(txid);
                        totalamount += amount;
                    }
                }
            }
        }
        else
            std::cerr << __func__ << " error getting tx=" << txid.GetHex() << std::endl;
    }
    return(n);
}

// create request tx for issuing or transfer baton (cheque) 
// the first call makes the credit loop creation tx
// txid of returned tx is approvaltxid
UniValue MarmaraReceive(int64_t txfee, CPubKey senderpk, int64_t amount, std::string currency, int32_t matures, uint256 batontxid, bool automaticflag)
{
    CMutableTransaction mtx = CreateNewContextualCMutableTransaction(Params().GetConsensus(), komodo_nextheight());
    UniValue result(UniValue::VOBJ); 
    struct CCcontract_info *cp, C; 
    int64_t batonamount; 
    std::string rawtx;

    cp = CCinit(&C, EVAL_MARMARA);
    if (txfee == 0)
        txfee = 10000;
    
    if (automaticflag != 0 && (matures & 1) == 0)
        matures++;
    else if (automaticflag == 0 && (matures & 1) != 0)
        matures++;

    CPubKey mypk = pubkey2pk(Mypubkey());
    uint256 createtxid = zeroid;
    const char *errorstr = NULL;

    if (batontxid != zeroid && MarmaraGetcreatetxid(createtxid, batontxid) < 0)
        errorstr = "cant get createtxid from batontxid";
    else if (currency != MARMARA_CURRENCY)
        errorstr = "for now, only MARMARA loops are supported";
    else if (amount <= txfee)
        errorstr = "amount must be for more than txfee";
    else if (matures <= chainActive.LastTip()->GetHeight())
        errorstr = "it must mature in the future";

    
    if (createtxid != zeroid) {
        // check original cheque params:
        CTransaction looptx;
        uint256 hashBlock;
        uint256 emptytxid;
        int32_t opretmatures;
        std::string opretcurrency;
        CPubKey opretsenderpk;
        int64_t opretamount;

        if (!GetTransaction(batontxid.IsNull() ? createtxid : batontxid, looptx, hashBlock, true) || looptx.vout.size() < 1 ||
            MarmaraDecodeLoopOpret(looptx.vout.back().scriptPubKey, emptytxid, opretsenderpk, opretamount, opretmatures, opretcurrency) == 0)
            errorstr = "cant decode looptx opreturn data";
        else if (senderpk != opretsenderpk)
            errorstr = "current baton holder does not match the requested sender pk";
        else if (opretamount != opretamount)
            errorstr = "amount does not match requested amount";
        else if (matures != opretmatures)
            errorstr = "mature height does not match requested mature height";
        else if (currency != opretcurrency)
            errorstr = "currency does not match requested currency";
    }

    if (errorstr == NULL)
    {
        if (batontxid != zeroid)
            batonamount = txfee;
        else 
            batonamount = 2 * txfee;  // dimxy why is this?
        if (AddNormalinputs(mtx, mypk, batonamount + txfee, 1) > 0)
        {
            errorstr = (char *)"couldnt finalize CCtx";
            mtx.vout.push_back(MakeCC1vout(EVAL_MARMARA, batonamount, senderpk));
            rawtx = FinalizeCCTx(0, cp, mtx, mypk, txfee, MarmaraEncodeLoopOpret('R', createtxid, senderpk, amount, matures, currency));
            if (rawtx.size() > 0)
                errorstr = 0;
        }
        else 
            errorstr = (char *)"dont have enough normal inputs for 2*txfee";
    }
    if (rawtx.size() == 0 || errorstr != 0)
    {
        result.push_back(Pair("result", "error"));
        if (errorstr != 0)
            result.push_back(Pair("error", errorstr));
    }
    else
    {
        result.push_back(Pair("result", (char *)"success"));
        result.push_back(Pair("hex", rawtx));
        result.push_back(Pair("funcid", "R"));
        result.push_back(Pair("createtxid", createtxid.GetHex()));
        if (batontxid != zeroid)
            result.push_back(Pair("batontxid", batontxid.GetHex()));
        result.push_back(Pair("senderpk", HexStr(senderpk)));
        result.push_back(Pair("amount", ValueFromAmount(amount)));
        result.push_back(Pair("matures", matures));
        result.push_back(Pair("currency", currency));
    }
    return(result);
}


static int32_t RedistributeLockedRemainder(CMutableTransaction &mtx, struct CCcontract_info *cp, const std::vector<uint256> &creditloop, uint256 batontxid, int64_t amountToDistribute)
{
    CPubKey Marmarapk; 
    int32_t endorsersNumber = creditloop.size(); // number of endorsers, 0 is createtxid, last is batontxid
    CPubKey dummypk;
    int64_t amount, inputsum, change;
    int32_t matures;
    std::string currency;
    uint8_t funcid;
    std::vector <CPubKey> pubkeys;
    CTransaction createtx;
    uint256 hashBlock, dummytxid;
    uint256 createtxid = creditloop[0];

    uint8_t marmarapriv[32];
    Marmarapk = GetUnspendable(cp, marmarapriv);

    if (endorsersNumber < 1)  // nobody to return to
        return 0;

    if (GetTransaction(createtxid, createtx, hashBlock, false) && createtx.vout.size() > 1 &&
        MarmaraDecodeLoopOpret(createtx.vout.back().scriptPubKey, dummytxid, dummypk, amount, matures, currency) != 0)  // get amount value
    {
        char lockInLoop1of2addr[KOMODO_ADDRESS_BUFSIZE], txidaddr[KOMODO_ADDRESS_BUFSIZE];
        CPubKey createtxidPk = CCtxidaddr(txidaddr, createtxid);
        GetCCaddress1of2(cp, lockInLoop1of2addr, Marmarapk, createtxidPk);  // 1of2 lock-in-loop address 

        std::cerr << __func__ << "calling AddMarmaraInputs for lock-in-loop addr=" << lockInLoop1of2addr << " adds amount=" << amount << std::endl;
        if ((inputsum = AddMarmarainputs(IsLockInLoopOpret, mtx, pubkeys, lockInLoop1of2addr, amount, MARMARA_VINS)) >= amount / endorsersNumber) 
        {
            if (mtx.vin.size() >= CC_MAXVINS) {// vin number limit
                std::cerr << __func__ << " too many vins!" << std::endl;
                return -1;
            }

            int64_t amountReturned = 0;
            for (int32_t i = 1; i < creditloop.size() + 1; i ++)  //iterate through all issuers/endorsers (i=0 is 1st receiver approval tx, n + 1 batontxid)
            {
                CTransaction issuancetx;
                uint256 hashBlock;
                CPubKey issuerpk;
                uint256 currenttxid = i < creditloop.size() ? creditloop[i] : batontxid;

                if (GetTransaction(currenttxid, issuancetx, hashBlock, false) && issuancetx.vout.size() > 1)
                {
                    if ((funcid = MarmaraDecodeLoopOpret(issuancetx.vout.back().scriptPubKey, createtxid, issuerpk, amount, matures, currency)) != 0)  // get endorser's pk
                    {
                        int64_t amountToPk = amountToDistribute / endorsersNumber;
                        mtx.vout.push_back(CTxOut(amountToPk, CScript() << ParseHex(HexStr(issuerpk)) << OP_CHECKSIG));  // coins returned to each previous issuer normal 
                        std::cerr << __func__ << " sending normal amount=" << amountToPk << " to pk=" << HexStr(issuerpk) << std::endl;
                        amountReturned += amountToPk;
                    }
                    else    {
                        std::cerr << __func__ << " null funcid for creditloop[" << i << "]" << std::endl;
                        return -1;
                    }
                }
                else   {
                    std::cerr << __func__ << " cant load tx for creditloop[" << i << "]" << std::endl;
                    return -1;
                }
            }
            change = (inputsum - amountReturned);

            // return change to the lock-in-loop fund, distribute for pubkeys:
            if (change > 0) {
                if (pubkeys.size() != endorsersNumber) {
                    std::cerr << __func__ << " internal error not matched pubkeys.size()=" << pubkeys.size() << " endorsersNumber=" << endorsersNumber << std::endl;
                    return -1;
                }
                for (auto pk : pubkeys) {
                    CScript opret = MarmaraEncodeLoopOpret('K', createtxid, pk, amount, matures, currency);
                    vscript_t vopret;

                    GetOpReturnData(opret, vopret);
                    //std::vector< vscript_t > vData{ E_MARSHAL(ss << (uint8_t)EVAL_MARMARA << 'K' << vscript_t(pk.begin(), pk.end())) };   // add mypk to vout to identify who has locked coins in the credit loop
                    std::vector< vscript_t > vData{ vopret };    // add mypk to vout to identify who has locked coins in the credit loop
                    mtx.vout.push_back(MakeCC1of2vout(EVAL_MARMARA, change / pubkeys.size(), Marmarapk, createtxidPk, &vData));  // TODO: losing remainder?

                    std::cerr << __func__ << " distributing to loop change/pubkeys.size()=" << change / pubkeys.size() << " marked with pk=" << HexStr(pk) << std::endl;
                }
            }

            CC *lockInLoop1of2cond = MakeCCcond1of2(EVAL_MARMARA, Marmarapk, createtxidPk);  
            CCAddVintxCond(cp, lockInLoop1of2cond, marmarapriv); //add probe condition to spend from the lock-in-loop address
            cc_free(lockInLoop1of2cond);

        }
        else  {
            std::cerr << __func__  << " couldnt get lock-in-loop amount to return to endorsers" << std::endl;
            return -1;
        }
    }
    else {
        std::cerr << __func__ << "could not load createtx" << std::endl;
        return -1;
    }
    return 0;
}


// issue or transfer coins to the next receiver
UniValue MarmaraIssue(int64_t txfee, uint8_t funcid, CPubKey receiverpk, int64_t amount, std::string currency, int32_t matures, uint256 requesttxid, uint256 batontxid)
{
    CMutableTransaction mtx = CreateNewContextualCMutableTransaction(Params().GetConsensus(), komodo_nextheight());
    UniValue result(UniValue::VOBJ); 
    std::string rawtx; uint256 createtxid; 
    std::vector<uint256> creditloop;

    struct CCcontract_info *cp, C;
    cp = CCinit(&C, EVAL_MARMARA);

    if (txfee == 0)
        txfee = 10000;

    // make sure less than maxlength (?)

    CPubKey Marmarapk = GetUnspendable(cp, NULL);
    CPubKey mypk = pubkey2pk(Mypubkey());
    std::string errorstr;

    if (MarmaraGetcreatetxid(createtxid, requesttxid) < 0)
        errorstr = "cant get createtxid from requesttxid";
    if (requesttxid.IsNull())
        errorstr = "requesttxid cant be empty";
    else if (currency != MARMARA_CURRENCY)
        errorstr = "for now, only MARMARA loops are supported";
    else if (amount <= txfee)
        errorstr = "amount must be for more than txfee";
    else if (matures <= chainActive.LastTip()->GetHeight())
        errorstr = "it must mature in the future";

    if (errorstr.empty())
    {
        // check requested cheque params:
        CTransaction requestx;
        uint256 hashBlock;
        uint256 emptytxid;
        int32_t opretmatures;
        std::string opretcurrency;
        CPubKey opretsenderpk;
        int64_t opretamount;

        // TODO: do we need here check tx for mempool?
        if (!GetTransaction(requesttxid, requestx, hashBlock, true) || requestx.vout.size() < 1 ||
            MarmaraDecodeLoopOpret(requestx.vout.back().scriptPubKey, emptytxid, opretsenderpk, opretamount, opretmatures, opretcurrency) == 0)
            errorstr = "cant decode request tx opreturn data";
        else if (mypk != opretsenderpk)
            errorstr = "mypk does not match requested sender pk";
        else if (opretamount != opretamount)
            errorstr = "amount does not match requested amount";
        else if (matures != opretmatures)
            errorstr = "mature height does not match requested mature height";
        else if (currency != opretcurrency)
            errorstr = "currency does not match requested currency";
    }

    if (errorstr.empty())
    {
        char activated1of2addr[KOMODO_ADDRESS_BUFSIZE];
        //char myccaddr[KOMODO_ADDRESS_BUFSIZE];
        int64_t inputsum;
        std::vector<CPubKey> pubkeys;

        uint256 dummytxid;
        int32_t endorsersNumber = MarmaraGetbatontxid(creditloop, dummytxid, requesttxid);  // need n
        int64_t amountToLock = (endorsersNumber > 0 ? amount / (endorsersNumber + 1) : amount);
        

        GetCCaddress1of2(cp, activated1of2addr, Marmarapk, mypk);  // 1of2 address where the activated endorser's money is locked -- deprecated
        //GetCCaddress(cp, myccaddr, mypk);                       // activated coins on cc address
        std::cerr << __func__ << "calling AddMarmaraInputs for activated addr=" << activated1of2addr << " needs activated amount to lock-in-loop=" << amountToLock << std::endl;
        if ((inputsum = AddMarmarainputs(IsActivatedOpret, mtx, pubkeys, activated1of2addr, amountToLock, MARMARA_VINS)) >= amountToLock) // add 1/n remainder from the locked fund
        {
            mtx.vin.push_back(CTxIn(requesttxid, 0, CScript()));  // spend the approval tx
            if (funcid == 'T')
                mtx.vin.push_back(CTxIn(batontxid, 0, CScript()));   // spend the baton
            if (funcid == 'I' || AddNormalinputs(mtx, mypk, txfee, 1) > 0)
            {
                mtx.vout.push_back(MakeCC1vout(EVAL_MARMARA, txfee, receiverpk));  // transfer the baton to the next receiver
                if (funcid == 'I')
                    mtx.vout.push_back(MakeCC1vout(EVAL_MARMARA, txfee, Marmarapk));

                // lock 1/N amount in loop
                char createtxidaddr[KOMODO_ADDRESS_BUFSIZE];
                CPubKey createtxidPk = CCtxidaddr(createtxidaddr, createtxid);

                CScript opret = MarmaraEncodeLoopOpret('K', createtxid, mypk, amount, matures, currency);
                vscript_t vopret;
                GetOpReturnData(opret, vopret);
                //std::vector< vscript_t > vData{ E_MARSHAL(ss << (uint8_t)EVAL_MARMARA << (uint8_t)'K' << vscript_t(mypk.begin(), mypk.end())) };   // add mypk to vout to identify who has locked coins in the credit loop
                std::vector< vscript_t > vData{ vopret };  // add mypk to cc vout to identify who locked the coins in the loop
                mtx.vout.push_back(MakeCC1of2vout(EVAL_MARMARA, amountToLock, Marmarapk, createtxidPk, &vData));
                std::cerr << __func__ << " sending to loop amount=" << amountToLock << " marked with mypk=" << HexStr(mypk) << std::endl;

                // return change to the my activated address:
                int64_t change = (inputsum - amountToLock);
                if (change > 0) {
                    int32_t height = komodo_nextheight();
                    if ((height & 1) != 0) // make height even as only such even height is considered for staking
                        height++;
                    CScript opret = MarmaraCoinbaseOpret('C', height, mypk);
                    vscript_t vopret;
                    GetOpReturnData(opret, vopret);
                    std::vector< vscript_t > vData{ vopret }; // add coinbase opret to ccvout for the change
                    mtx.vout.push_back(MakeCC1of2vout(EVAL_MARMARA, change, Marmarapk, mypk, &vData));  // adding MarmaraCoinbase cc vout 'opret' for change
                }

                if (endorsersNumber < 1 || RedistributeLockedRemainder(mtx, cp, creditloop, batontxid, amountToLock) >= 0)  // if there are issuers already then distribute and return amount / n value
                {
                    CC* activated1of2cond = MakeCCcond1of2(EVAL_MARMARA, Marmarapk, mypk);  // create vintx probe 1of2 cond, do not cc_free it!
                    CCAddVintxCond(cp, activated1of2cond);
                    cc_free(activated1of2cond);

                    rawtx = FinalizeCCTx(0, cp, mtx, mypk, txfee, MarmaraEncodeLoopOpret(funcid, createtxid, receiverpk, amount, matures, currency));

                    if (rawtx.size() == 0) {
                        errorstr = "couldnt finalize CCtx";
                        std::cerr << __func__ << " bad mtx=" << HexStr(E_MARSHAL(ss << mtx)) << std::endl;
                    }
                }
                else
                    errorstr = "could not return back locked in loop funds";
            }
            else
                errorstr = "dont have enough normal inputs for 2*txfee";
        }
        else
            errorstr = "dont have enough locked inputs for amount";
    }
    if (!errorstr.empty())
    {
        result.push_back(Pair("result", "error"));
        result.push_back(Pair("error", errorstr));
    }
    else
    {
        result.push_back(Pair("result", (char *)"success"));
        result.push_back(Pair("hex", rawtx));
        char sfuncid[2]; 
        sfuncid[0] = funcid, sfuncid[1] = 0;
        result.push_back(Pair("funcid", sfuncid));
        result.push_back(Pair("createtxid", createtxid.GetHex()));
        result.push_back(Pair("approvaltxid", requesttxid.GetHex()));
        if (funcid == 'T')
            result.push_back(Pair("batontxid", batontxid.GetHex()));
        result.push_back(Pair("receiverpk", HexStr(receiverpk)));
        result.push_back(Pair("amount", ValueFromAmount(amount)));
        result.push_back(Pair("matures", matures));
        result.push_back(Pair("currency", currency));
    }
    return(result);
}

UniValue MarmaraCreditloop(uint256 txid)
{
    UniValue result(UniValue::VOBJ), a(UniValue::VARR); 
    std::vector<uint256> creditloop; 
    uint256 batontxid, refcreatetxid, hashBlock; uint8_t funcid; 
    int32_t numerrs = 0, i, n, numvouts, matures, refmatures; int64_t amount, refamount; CPubKey pk; std::string currency, refcurrency; 
    CTransaction tx; 
    char normaladdr[KOMODO_ADDRESS_BUFSIZE], myCCaddr[KOMODO_ADDRESS_BUFSIZE], destaddr[KOMODO_ADDRESS_BUFSIZE], batonCCaddr[KOMODO_ADDRESS_BUFSIZE], sfuncid[2]; 
    struct CCcontract_info *cp, C;

    cp = CCinit(&C, EVAL_MARMARA);
    if ((n = MarmaraGetbatontxid(creditloop, batontxid, txid)) > 0)
    {
        if ( myGetTransaction(batontxid,tx,hashBlock) != 0 && (numvouts = tx.vout.size()) > 1 )
        {
            result.push_back(Pair("result", (char *)"success"));
            Getscriptaddress(normaladdr, CScript() << ParseHex(HexStr(Mypubkey())) << OP_CHECKSIG);
            result.push_back(Pair("myNormalAddress", normaladdr));
            GetCCaddress(cp, myCCaddr, Mypubkey());
            result.push_back(Pair("myCCaddress", myCCaddr));

            if ((funcid = MarmaraDecodeLoopOpret(tx.vout.back().scriptPubKey, refcreatetxid, pk, refamount, refmatures, refcurrency)) != 0)
            {
                sfuncid[0] = funcid, sfuncid[1] = 0;
                result.push_back(Pair("funcid", sfuncid));
                result.push_back(Pair("currency", refcurrency));

                if (funcid == 'S')
                {
                    refcreatetxid = creditloop[0];
                    result.push_back(Pair("settlement", batontxid.GetHex()));
                    result.push_back(Pair("createtxid", refcreatetxid.GetHex()));
                    result.push_back(Pair("remainder", ValueFromAmount(refamount)));
                    result.push_back(Pair("settled", refmatures));
                    result.push_back(Pair("pubkey", HexStr(pk)));
                    Getscriptaddress(normaladdr, CScript() << ParseHex(HexStr(pk)) << OP_CHECKSIG);
                    result.push_back(Pair("myNormalAddr", normaladdr));
                    result.push_back(Pair("collected", ValueFromAmount(tx.vout[0].nValue)));
                    Getscriptaddress(destaddr, tx.vout[0].scriptPubKey);
                    if (strcmp(normaladdr, destaddr) != 0)
                    {
                        result.push_back(Pair("destaddr", destaddr));
                        numerrs++;
                    }
                    refamount = -1;
                }
                else if (funcid == 'D')
                {
                    refcreatetxid = creditloop[0];
                    result.push_back(Pair("settlement", batontxid.GetHex()));
                    result.push_back(Pair("createtxid", refcreatetxid.GetHex()));
                    result.push_back(Pair("remainder", ValueFromAmount(refamount)));
                    result.push_back(Pair("settled", refmatures));
                    Getscriptaddress(destaddr, tx.vout[0].scriptPubKey);
                    result.push_back(Pair("txidaddr", destaddr));
                    if (tx.vout.size() > 1)
                        result.push_back(Pair("collected", ValueFromAmount(tx.vout[1].nValue)));
                }
                else
                {
                    result.push_back(Pair("batontxid", batontxid.GetHex()));
                    result.push_back(Pair("createtxid", refcreatetxid.GetHex()));
                    result.push_back(Pair("amount", ValueFromAmount(refamount)));
                    result.push_back(Pair("matures", refmatures));
                    if (refcreatetxid != creditloop[0])
                    {
                        fprintf(stderr, "%s invalid refcreatetxid, setting to creditloop[0]\n", __func__);
                        refcreatetxid = creditloop[0];
                        numerrs++;
                    }
                    result.push_back(Pair("batonpk", HexStr(pk)));
                    Getscriptaddress(normaladdr, CScript() << ParseHex(HexStr(pk)) << OP_CHECKSIG);
                    result.push_back(Pair("batonaddr", normaladdr));
                    GetCCaddress(cp, batonCCaddr, pk);  // baton address
                    result.push_back(Pair("batonCCaddr", batonCCaddr));
                    Getscriptaddress(normaladdr, tx.vout[0].scriptPubKey);
                    if (strcmp(normaladdr, batonCCaddr) != 0)  // TODO: how is this possible?
                    {
                        result.push_back(Pair("vout0address", normaladdr));
                        numerrs++;
                    }

                    if (strcmp(myCCaddr, /*normaladdr*/batonCCaddr) == 0) // TODO: impossible with normal addr
                        result.push_back(Pair("ismine", 1));
                    else
                        result.push_back(Pair("ismine", 0));
                }

                // add locked-in-loop amount:
                char lockInLoop1of2addr[KOMODO_ADDRESS_BUFSIZE], txidaddr[KOMODO_ADDRESS_BUFSIZE];
                CPubKey createtxidPk = CCtxidaddr(txidaddr, refcreatetxid);
                GetCCaddress1of2(cp, lockInLoop1of2addr, GetUnspendable(cp, NULL), createtxidPk);  // 1of2 lock-in-loop address 
                std::vector<CPubKey> pubkeys;
                CMutableTransaction mtx;
                int64_t amountLockedInLoop = AddMarmarainputs(IsLockInLoopOpret, mtx, pubkeys, lockInLoop1of2addr, 0, 0);
                result.push_back(Pair("LockedInLoopCCaddr", lockInLoop1of2addr));
                result.push_back(Pair("LockedInLoopAmount", ValueFromAmount(amountLockedInLoop)));  // should be 0 if settled

                for (i = 0; i < n; i++)
                {
                    if ( myGetTransaction(creditloop[i],tx,hashBlock) != 0 && (numvouts = tx.vout.size()) > 1 )
                    {
                        uint256 createtxid;
                        if ((funcid = MarmaraDecodeLoopOpret(tx.vout[numvouts - 1].scriptPubKey, createtxid, pk, amount, matures, currency)) != 0)
                        {
                            UniValue obj(UniValue::VOBJ);
                            obj.push_back(Pair("txid", creditloop[i].GetHex()));
                            sfuncid[0] = funcid, sfuncid[1] = 0;
                            obj.push_back(Pair("funcid", sfuncid));
                            if (funcid == 'R' && createtxid == zeroid)
                            {
                                createtxid = creditloop[i];
                                obj.push_back(Pair("issuerpk", HexStr(pk)));
                                Getscriptaddress(normaladdr, CScript() << ParseHex(HexStr(pk)) << OP_CHECKSIG);
                                obj.push_back(Pair("issueraddr", normaladdr));
                                GetCCaddress(cp, normaladdr, pk);
                                obj.push_back(Pair("issuerCCaddr", normaladdr));
                            }
                            else
                            {
                                obj.push_back(Pair("receiverpk", HexStr(pk)));
                                Getscriptaddress(normaladdr, CScript() << ParseHex(HexStr(pk)) << OP_CHECKSIG);
                                obj.push_back(Pair("receiveraddr", normaladdr));
                                GetCCaddress(cp, normaladdr, pk);
                                obj.push_back(Pair("receiverCCaddr", normaladdr));
                            }
                            Getscriptaddress(destaddr, tx.vout[0].scriptPubKey);
                            if (strcmp(destaddr, normaladdr) != 0)
                            {
                                obj.push_back(Pair("vout0address", destaddr));
                                numerrs++;
                            }
                            if (i == 0 && refamount < 0)
                            {
                                refamount = amount;
                                refmatures = matures;
                                result.push_back(Pair("amount", ValueFromAmount(refamount)));
                                result.push_back(Pair("matures", refmatures));
                            }
                            if (createtxid != refcreatetxid || amount != refamount || matures != refmatures || currency != refcurrency)
                            {
                                numerrs++;
                                obj.push_back(Pair("objerror", (char *)"mismatched createtxid or amount or matures or currency"));
                                obj.push_back(Pair("createtxid", createtxid.GetHex()));
                                obj.push_back(Pair("amount", ValueFromAmount(amount)));
                                obj.push_back(Pair("matures", matures));
                                obj.push_back(Pair("currency", currency));
                            }
                            a.push_back(obj);
                        }
                    }
                }
                result.push_back(Pair("n", n));
                result.push_back(Pair("numerrors", numerrs));
                result.push_back(Pair("creditloop", a));
            }
            else
            {
                result.push_back(Pair("result", (char *)"error"));
                result.push_back(Pair("error", (char *)"couldnt get batontxid opret"));
            }
        }
        else
        {
            result.push_back(Pair("result", (char *)"error"));
            result.push_back(Pair("error", (char *)"couldnt find batontxid"));
        }
    }
    else
    {
        result.push_back(Pair("result", (char *)"error"));
        result.push_back(Pair("error", (char *)"couldnt get creditloop"));
    }
    return(result);
}

// collect miner pool rewards (?)
UniValue MarmaraPoolPayout(int64_t txfee, int32_t firstheight, double perc, char *jsonstr) // [[pk0, shares0], [pk1, shares1], ...]
{
    CMutableTransaction mtx = CreateNewContextualCMutableTransaction(Params().GetConsensus(), komodo_nextheight());
    UniValue result(UniValue::VOBJ), a(UniValue::VARR); cJSON *item, *array; std::string rawtx; int32_t i, n; uint8_t buf[33]; CPubKey Marmarapk, pk, poolpk; int64_t payout, poolfee = 0, total, totalpayout = 0; double poolshares, share, shares = 0.; char *pkstr, *errorstr = 0; struct CCcontract_info *cp, C;
    poolpk = pubkey2pk(Mypubkey());
    if (txfee == 0)
        txfee = 10000;
    cp = CCinit(&C, EVAL_MARMARA);
    Marmarapk = GetUnspendable(cp, 0);
    if ((array = cJSON_Parse(jsonstr)) != 0 && (n = cJSON_GetArraySize(array)) > 0)
    {
        for (i = 0; i < n; i++)
        {
            item = jitem(array, i);
            if ((pkstr = jstr(jitem(item, 0), 0)) != 0 && strlen(pkstr) == 66)
                shares += jdouble(jitem(item, 1), 0);
            else
            {
                errorstr = (char *)"all items must be of the form [<pubkey>, <shares>]";
                break;
            }
        }
        if (errorstr == 0 && shares > SMALLVAL)
        {
            shares += shares * perc;
            if ((total = AddMarmaraCoinbases(cp, mtx, firstheight, poolpk, 60)) > 0)
            {
                for (i = 0; i < n; i++)
                {
                    item = jitem(array, i);
                    if ((share = jdouble(jitem(item, 1), 0)) > SMALLVAL)
                    {
                        payout = (share * (total - txfee)) / shares;
                        if (payout > 0)
                        {
                            if ((pkstr = jstr(jitem(item, 0), 0)) != 0 && strlen(pkstr) == 66)
                            {
                                UniValue x(UniValue::VOBJ);
                                totalpayout += payout;
                                decode_hex(buf, 33, pkstr);
                                mtx.vout.push_back(MakeCC1of2vout(EVAL_MARMARA, payout, Marmarapk, buf2pk(buf)));
                                x.push_back(Pair(pkstr, (double)payout / COIN));
                                a.push_back(x);
                            }
                        }
                    }
                }
                if (totalpayout > 0 && total > totalpayout - txfee)
                {
                    poolfee = (total - totalpayout - txfee);
                    mtx.vout.push_back(MakeCC1of2vout(EVAL_MARMARA, poolfee, Marmarapk, poolpk));
                }
                rawtx = FinalizeCCTx(0, cp, mtx, poolpk, txfee, MarmaraCoinbaseOpret('P', firstheight, poolpk));
                if (rawtx.size() == 0)
                    errorstr = (char *)"couldnt finalize CCtx";
            }
            else errorstr = (char *)"couldnt find any coinbases to payout";
        }
        else if (errorstr == 0)
            errorstr = (char *)"no valid shares submitted";
        free(array);
    }
    else errorstr = (char *)"couldnt parse poolshares jsonstr";
    if (rawtx.size() == 0 || errorstr != 0)
    {
        result.push_back(Pair("result", "error"));
        if (errorstr != 0)
            result.push_back(Pair("error", errorstr));
    }
    else
    {
        result.push_back(Pair("result", (char *)"success"));
        result.push_back(Pair("hex", rawtx));
        if (totalpayout > 0 && total > totalpayout - txfee)
        {
            result.push_back(Pair("firstheight", firstheight));
            result.push_back(Pair("lastheight", ((firstheight / MARMARA_GROUPSIZE) + 1) * MARMARA_GROUPSIZE - 1));
            result.push_back(Pair("total", ValueFromAmount(total)));
            result.push_back(Pair("totalpayout", ValueFromAmount(totalpayout)));
            result.push_back(Pair("totalshares", shares));
            result.push_back(Pair("poolfee", ValueFromAmount(poolfee)));
            result.push_back(Pair("perc", ValueFromAmount((int64_t)(100. * (double)poolfee / totalpayout * COIN))));
            result.push_back(Pair("payouts", a));
        }
    }
    return(result);
}

// get all tx, constrain by vout, issuances[] and closed[]

UniValue MarmaraInfo(CPubKey refpk, int32_t firstheight, int32_t lastheight, int64_t minamount, int64_t maxamount, std::string currency)
{
    CMutableTransaction mtx; std::vector<CPubKey> pubkeys;
    UniValue result(UniValue::VOBJ), a(UniValue::VARR), b(UniValue::VARR); int32_t i, n, matches; int64_t totalclosed = 0, totalamount = 0; std::vector<uint256> issuances, closed; 
    CPubKey Marmarapk; 
    char mynormaladdr[KOMODO_ADDRESS_BUFSIZE];
    char activated1of2addr[KOMODO_ADDRESS_BUFSIZE];
    char myccaddr[KOMODO_ADDRESS_BUFSIZE];

    struct CCcontract_info *cp, C;

    cp = CCinit(&C, EVAL_MARMARA);
    Marmarapk = GetUnspendable(cp, 0);
    result.push_back(Pair("result", "success"));
    
    Getscriptaddress(mynormaladdr, CScript() << ParseHex(HexStr(Mypubkey())) << OP_CHECKSIG);
    result.push_back(Pair("myNormalAddress", mynormaladdr));
    result.push_back(Pair("myNormalAmount", ValueFromAmount(CCaddress_balance(mynormaladdr, 0))));

    GetCCaddress1of2(cp, activated1of2addr, Marmarapk, Mypubkey());
    result.push_back(Pair("myCCActivatedAddress", activated1of2addr));
    result.push_back(Pair("myActivatedAmount", ValueFromAmount(AddMarmarainputs(IsActivatedOpret, mtx, pubkeys, activated1of2addr, 0, CC_MAXVINS)))); // changed MARMARA_VIN to CC_MAXVINS - we need actual amount
    result.push_back(Pair("myAmountOnActivatedAddress-old", ValueFromAmount(CCaddress_balance(activated1of2addr, 1))));

    GetCCaddress(cp, myccaddr, Mypubkey());
    result.push_back(Pair("myCCAddress", myccaddr));
    result.push_back(Pair("myCCBalance", ValueFromAmount(CCaddress_balance(myccaddr, 1))));


    // calc lock-in-loops amount for mypk:
    CAmount loopAmount = 0;
    CAmount totalLoopAmount = 0;
    char prevloopaddr[KOMODO_ADDRESS_BUFSIZE] = "";
    UniValue resultloops(UniValue::VARR);
    EnumMyLockedInLoop([&](char *loopaddr, const CTransaction & tx, int32_t nvout, CBlockIndex *pindex) // call enumerator with callback
    {
        //std::cerr << "lambda " << " loopaddr=" << loopaddr << " prevloopaddr=" << prevloopaddr << " loopAmount=" << loopAmount << std::endl;

        if (strcmp(prevloopaddr, loopaddr) != 0) {  // loop address changed
            if (prevloopaddr[0] != '\0') {  // prevloop was
                UniValue entry(UniValue::VOBJ);
                // if new loop then store amount for the prevloop
                entry.push_back(Pair("LoopAddress", prevloopaddr));
                entry.push_back(Pair("myAmountLockedInLoop", ValueFromAmount(loopAmount)));
                resultloops.push_back(entry);
                loopAmount = 0;  //reset for the next loop
            }
            strcpy(prevloopaddr, loopaddr);
        }
        loopAmount += tx.vout[nvout].nValue;
        totalLoopAmount += tx.vout[nvout].nValue;
    });
    if (prevloopaddr[0] != '\0') {   // last loop
        UniValue entry(UniValue::VOBJ);
        entry.push_back(Pair("LoopAddress", prevloopaddr));
        entry.push_back(Pair("myAmountLockedInLoop", ValueFromAmount(loopAmount)));
        resultloops.push_back(entry);
        //std::cerr << "lastloop " << " prevloopaddr=" << prevloopaddr << " loopAmount=" << loopAmount << std::endl;
    }
    result.push_back(Pair("Loops", resultloops));
    result.push_back(Pair("TotalLockedInLoop", ValueFromAmount(totalLoopAmount)));

    if (refpk.size() == 33)
        result.push_back(Pair("issuer", HexStr(refpk)));
    if (currency.size() == 0)
        currency = (char *)MARMARA_CURRENCY;
    if (firstheight <= lastheight)
        firstheight = 0, lastheight = (1 << 30);
    if (minamount <= maxamount)
        minamount = 0, maxamount = (1LL << 60);
    result.push_back(Pair("firstheight", firstheight));
    result.push_back(Pair("lastheight", lastheight));
    result.push_back(Pair("minamount", ValueFromAmount(minamount)));
    result.push_back(Pair("maxamount", ValueFromAmount(maxamount)));
    result.push_back(Pair("currency", currency));
    if ((n = MarmaraGetCreditloops(totalamount, issuances, totalclosed, closed, cp, firstheight, lastheight, minamount, maxamount, refpk, currency)) > 0)
    {
        result.push_back(Pair("n", n));
        matches = (int32_t)issuances.size();
        result.push_back(Pair("pending", matches));
        for (i = 0; i < matches; i++)
            a.push_back(issuances[i].GetHex());
        result.push_back(Pair("issuances", a));
        result.push_back(Pair("totalamount", ValueFromAmount(totalamount)));
        matches = (int32_t)closed.size();
        result.push_back(Pair("numclosed", matches));
        for (i = 0; i < matches; i++)
            b.push_back(closed[i].GetHex());
        result.push_back(Pair("closed", b));
        result.push_back(Pair("totalclosed", ValueFromAmount(totalclosed)));
    }
    return(result);
}
