/******************************************************************************
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
#include "key_io.h"

 /*
  Marmara CC is for the MARMARA project

  'B' initial data for credit loop
  vins normal
  vout0 request to senderpk (issuer)

  'R' request for credit issuance 
  vins normal
  vout0 request to senderpk (endorser)

  'I' check issuance
  vin0 request from 'R'
  vins1+ normal
  vout0 baton to 1st receiverpk
  vout1 marker to Marmara so all issuances can be tracked (spent when loop is closed)

  'T' check transfer to endorser
  vin0 request from 'R'
  vin1 baton from 'I'/'T'
  vins2+ normal
  vout0 baton to next receiverpk (following the unspent baton back to original is the credit loop)

  'S' check settlement
  vin0 'I' marker
  vin1 baton
  vins CC utxos from credit loop

  'D' default/partial payment in the settlement
  //TODO: should we implement several partial settlements in case of too many vins?

  'A' activated funds

  'C' marmara coinbase

  'L' lock in loop funds

  'K' pubkey in cc vout opret who locked his funds in loop 

 */

const uint8_t MARMARA_OPRET_VERSION = 1;
const int32_t MARMARA_MARKER_VOUT = 1;
const int32_t MARMARA_BATON_VOUT = 0;
const int32_t MARMARA_REQUEST_VOUT = 0;
const int32_t MARMARA_OPENCLOSE_VOUT = 3;

const bool CHECK_ONLY_CCOPRET = true;

// credit loop data from different tx oprets
struct CreditLoopOpret {
    bool hasCreateOpret;
    bool hasIssuanceOpret;
    bool hasSettlementOpret;

    uint8_t lastfuncid;

    uint8_t autoSettlement;
    uint8_t autoInsurance;

    // create tx data:
    CAmount amount;  // loop amount
    int32_t matures; // check maturing height
    std::string currency;  // currently MARMARA

    // issuer data:
    int32_t disputeExpiresHeight;
    uint8_t escrowOn;
    CAmount blockageAmount;

    // last issuer/endorser/receiver data:
    uint256 createtxid;
    CPubKey pk;             // always the last pk in opret
    int32_t avalCount;      // only for issuer/endorser

    // settlement data:
    CAmount remaining;

    // init default values:
    CreditLoopOpret() {
        hasCreateOpret = false;
        hasIssuanceOpret = false;
        hasSettlementOpret = false;

        lastfuncid = 0;

        amount = 0LL;
        matures = 0;
        autoSettlement = 1;
        autoInsurance = 1;

        createtxid = zeroid;
        disputeExpiresHeight = 0;
        avalCount = 0;
        escrowOn = false;
        blockageAmount = 0LL;

        remaining = 0L;
    }
};


// helper functions for rpc calls

/* see similar funcs below
int64_t IsMarmaravout(struct CCcontract_info *cp, const CTransaction& tx, int32_t v)
{
    char destaddr[KOMODO_ADDRESS_BUFSIZE];
    if (tx.vout[v].scriptPubKey.IsPayToCryptoCondition() != 0)
    {
        if (Getscriptaddress(destaddr, tx.vout[v].scriptPubKey) && strcmp(destaddr, cp->unspendableCCaddr) == 0)
            return(tx.vout[v].nValue);
    }
    return(0);
}*/

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

uint8_t MarmaraDecodeCoinbaseOpret(const CScript scriptPubKey, CPubKey &pk, int32_t &height, int32_t &unlockht)
{
    vscript_t vopret;
    GetOpReturnData(scriptPubKey, vopret);

    /* if (0)
    {
    int32_t i;

    std::cerr  << " ";
    for (i = 0; i < vopret.size(); i++)
    fprintf(stderr, "%02x", script[i]);
    fprintf(stderr, " <- opret\n");
    } */

    if (vopret.size() >= 3)
    {
        uint8_t evalcode, funcid, version;
        uint8_t *script = (uint8_t *)vopret.data();

        if (script[0] == EVAL_MARMARA)
        {
            if (script[1] == 'C' || script[1] == 'P' || script[1] == 'A')
            {
                if (script[2] == MARMARA_OPRET_VERSION)
                {
                    if (E_UNMARSHAL(vopret, ss >> evalcode; ss >> funcid; ss >> version; ss >> pk; ss >> height; ss >> unlockht) != 0)
                    {
                        return(script[1]);
                    }
                    else
                        LOGSTREAMFN("marmara", CCLOG_ERROR, stream << "opret unmarshal error for funcid=" << (char)script[1] << std::endl);
                }
                else
                    LOGSTREAMFN("marmara", CCLOG_DEBUG3, stream << "incorrect marmara activated or coinbase opret version=" << (char)script[2] << std::endl);
            }
            else
                LOGSTREAMFN("marmara", CCLOG_DEBUG3, stream << "not marmara activated or coinbase funcid=" << (char)script[1] << std::endl);
        }
        else
            LOGSTREAMFN("marmara", CCLOG_ERROR, stream << "not marmara opret, evalcode=" << (int)script[0] << std::endl);
    }
    else
        LOGSTREAMFN("marmara", CCLOG_ERROR, stream << "bad marmara opret, vopret.size()=" << vopret.size() << std::endl);
    return(0);
}

CScript EncodeMarmaraCoinbaseOpRet(uint8_t funcid, CPubKey pk, int32_t ht)
{
    CScript opret;
    int32_t unlockht;
    uint8_t evalcode = EVAL_MARMARA;
    uint8_t version = MARMARA_OPRET_VERSION;

    unlockht = MarmaraUnlockht(ht);
    opret << OP_RETURN << E_MARSHAL(ss << evalcode << funcid << version << pk << ht << unlockht);
    /*   if (0)
    {
    vscript_t vopret; uint8_t *script, i;
    GetOpReturnData(opret, vopret);
    script = (uint8_t *)vopret.data();
    {
    std::cerr  << " ";
    for (i = 0; i < vopret.size(); i++)
    fprintf(stderr, "%02x", script[i]);
    fprintf(stderr, " <- gen opret.%c\n", funcid);
    }
    } */
    return(opret);
}


// encode lock-in-loop tx opret functions:

CScript MarmaraEncodeLoopCreateOpret(CPubKey senderpk, int64_t amount, int32_t matures, std::string currency)
{
    CScript opret;
    uint8_t evalcode = EVAL_MARMARA;
    uint8_t funcid = 'B'; // create tx (initial request tx)
    uint8_t version = MARMARA_OPRET_VERSION;

    opret << OP_RETURN << E_MARSHAL(ss << evalcode << funcid << version << senderpk << amount << matures << currency);
    return(opret);
}

CScript MarmaraEncodeLoopIssuerOpret(uint256 createtxid, CPubKey receiverpk, uint8_t autoSettlement, uint8_t autoInsurance, int32_t avalCount, int32_t disputeExpiresHeight, uint8_t escrowOn, CAmount blockageAmount)
{
    CScript opret;
    uint8_t evalcode = EVAL_MARMARA;
    uint8_t funcid = 'I'; // issuance tx
    uint8_t version = MARMARA_OPRET_VERSION;

    opret << OP_RETURN << E_MARSHAL(ss << evalcode << funcid << version << createtxid << receiverpk << autoSettlement << autoInsurance << avalCount << disputeExpiresHeight << escrowOn << blockageAmount);
    return(opret);
}

CScript MarmaraEncodeLoopRequestOpret(uint256 createtxid, CPubKey senderpk)
{
    CScript opret;
    uint8_t evalcode = EVAL_MARMARA;
    uint8_t funcid = 'R'; // request tx
    uint8_t version = MARMARA_OPRET_VERSION;

    opret << OP_RETURN << E_MARSHAL(ss << evalcode << funcid << version << createtxid << senderpk);
    return(opret);
}

CScript MarmaraEncodeLoopTransferOpret(uint256 createtxid, CPubKey receiverpk, int32_t avalCount)
{
    CScript opret;
    uint8_t evalcode = EVAL_MARMARA;
    uint8_t funcid = 'T'; // transfer tx
    uint8_t version = MARMARA_OPRET_VERSION;

    opret << OP_RETURN << E_MARSHAL(ss << evalcode << funcid << version << createtxid << receiverpk << avalCount);
    return(opret);
}

CScript MarmaraEncodeLoopCCVoutOpret(uint256 createtxid, CPubKey senderpk)
{
    CScript opret;
    uint8_t evalcode = EVAL_MARMARA;
    uint8_t funcid = 'K'; // opret in cc 1of2 lock-in-loop vout
    uint8_t version = MARMARA_OPRET_VERSION;

    opret << OP_RETURN << E_MARSHAL(ss << evalcode << funcid << version << createtxid << senderpk);
    return(opret);
}

CScript MarmaraEncodeLoopSettlementOpret(bool isSuccess, uint256 createtxid, CPubKey pk, CAmount remaining)
{
    CScript opret;
    uint8_t evalcode = EVAL_MARMARA;
    uint8_t funcid = isSuccess ? 'S' : 'D';
    uint8_t version = MARMARA_OPRET_VERSION;

    opret << OP_RETURN << E_MARSHAL(ss << evalcode << funcid << version << createtxid << pk << remaining);
    return(opret);
}

// decode different lock-in-loop oprets, update the loopData
uint8_t MarmaraDecodeLoopOpret(const CScript scriptPubKey, struct CreditLoopOpret &loopData)
{
    vscript_t vopret;

    GetOpReturnData(scriptPubKey, vopret);
    if (vopret.size() >= 3)
    {
        uint8_t evalcode = vopret.begin()[0];
        uint8_t funcid = vopret.begin()[1];
        uint8_t version = vopret.begin()[2];

        if (evalcode == EVAL_MARMARA)   // check limits
        {
            if (version == MARMARA_OPRET_VERSION)
            {
                if (funcid == 'B') {  // createtx
                    if (E_UNMARSHAL(vopret, ss >> evalcode; ss >> loopData.lastfuncid; ss >> version; ss >> loopData.pk; ss >> loopData.amount; ss >> loopData.matures; ss >> loopData.currency)) {
                        loopData.hasCreateOpret = true;
                        return loopData.lastfuncid;
                    }
                }
                else if (funcid == 'I') {
                    if (E_UNMARSHAL(vopret, ss >> evalcode; ss >> loopData.lastfuncid; ss >> version; ss >> loopData.createtxid; ss >> loopData.pk; ss >> loopData.autoSettlement; ss >> loopData.autoInsurance; ss >> loopData.avalCount >> loopData.disputeExpiresHeight >> loopData.escrowOn >> loopData.blockageAmount)) {
                        loopData.hasIssuanceOpret = true;
                        return loopData.lastfuncid;
                    }
                }
                else if (funcid == 'R') {
                    if (E_UNMARSHAL(vopret, ss >> evalcode; ss >> loopData.lastfuncid; ss >> version; ss >> loopData.createtxid; ss >> loopData.pk)) {
                        return funcid;
                    }
                }
                else if (funcid == 'T') {
                    if (E_UNMARSHAL(vopret, ss >> evalcode; ss >> loopData.lastfuncid; ss >> version; ss >> loopData.createtxid; ss >> loopData.pk; ss >> loopData.avalCount)) {
                        return funcid;
                    }
                }
                else if (funcid == 'K') {
                    if (E_UNMARSHAL(vopret, ss >> evalcode; ss >> loopData.lastfuncid; ss >> version; ss >> loopData.createtxid; ss >> loopData.pk)) {
                        return funcid;
                    }
                }
                else if (funcid == 'S' || funcid == 'D') {
                    if (E_UNMARSHAL(vopret, ss >> evalcode; ss >> loopData.lastfuncid; ss >> version; ss >> loopData.createtxid; ss >> loopData.pk >> loopData.remaining)) {
                        loopData.hasSettlementOpret = true;
                        return funcid;
                    }
                }
                // get here from any E_UNMARSHAL error:
                LOGSTREAMFN("marmara", CCLOG_DEBUG2, stream << "cannot parse loop opret: not my funcid=" << (int)funcid << " or bad opret format=" << HexStr(vopret) << std::endl);
            }
            else
                LOGSTREAMFN("marmara", CCLOG_ERROR, stream << "unsupported opret version=" << (int)version << std::endl);
        }
        else
            LOGSTREAMFN("marmara", CCLOG_ERROR, stream << "not marmara opret, evalcode=" << (int)evalcode << std::endl);
    }
    else
        LOGSTREAMFN("marmara", CCLOG_DEBUG3, stream << "opret too small=" << HexStr(vopret) << std::endl);

    return(0);
}

static CTxOut MakeMarmaraCC1of2voutOpret(CAmount amount, const CPubKey &pk2, const CScript &opret)
{
    vscript_t vopret;
    struct CCcontract_info *cp, C;
    cp = CCinit(&C, EVAL_MARMARA);
    CPubKey Marmarapk = GetUnspendable(cp, 0);

    GetOpReturnData(opret, vopret);
    std::vector< vscript_t > vData{ vopret };    // add mypk to vout to identify who has locked coins in the credit loop
    return MakeCC1of2vout(EVAL_MARMARA, amount, Marmarapk, pk2, &vData);
}

static bool GetCCOpReturnData(const CScript &spk, CScript &opret)
{
    CScript dummy;
    std::vector< vscript_t > vParams;

    // get cc opret
    if (spk.IsPayToCryptoCondition(&dummy, vParams))
    {
        if (vParams.size() > 0)
        {
            COptCCParams p = COptCCParams(vParams[0]);
            if (p.vData.size() > 0)
            {
                opret << OP_RETURN << p.vData[0]; // reconstruct opret 
                return true;
            }
        }
    }
    return false;
}

// add mined coins
int64_t AddMarmaraCoinbases(struct CCcontract_info *cp, CMutableTransaction &mtx, int32_t firstheight, CPubKey poolpk, int32_t maxinputs)
{
    char coinaddr[KOMODO_ADDRESS_BUFSIZE];
    int64_t nValue, totalinputs = 0;
    uint256 txid, hashBlock;
    CTransaction vintx;
    int32_t unlockht, ht, vout, unlocks, n = 0;
    std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> > unspentOutputs;

    CPubKey Marmarapk = GetUnspendable(cp, NULL);
    GetCCaddress1of2(cp, coinaddr, Marmarapk, poolpk);
    SetCCunspents(unspentOutputs, coinaddr, true);
    unlocks = MarmaraUnlockht(firstheight);

    LOGSTREAMFN("marmara", CCLOG_DEBUG2, stream << " check coinaddr=" << coinaddr << std::endl);
    for (std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> >::const_iterator it = unspentOutputs.begin(); it != unspentOutputs.end(); it++)
    {
        txid = it->first.txhash;
        vout = (int32_t)it->first.index;
        LOGSTREAMFN("marmara", CCLOG_DEBUG2, stream << " txid=" << txid.GetHex() << " vout=" << vout << std::endl);
        if (myGetTransaction(txid, vintx, hashBlock) != 0)
        {
            if (vintx.IsCoinBase() != 0 && vintx.vout.size() == 2 && vintx.vout[1].nValue == 0)
            {
                CPubKey pk;
                if (MarmaraDecodeCoinbaseOpret(vintx.vout[1].scriptPubKey, pk, ht, unlockht) == 'C' && unlockht == unlocks && pk == poolpk && ht >= firstheight)
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
                    else
                        LOGSTREAMFN("marmara", CCLOG_ERROR, stream << "tx in mempool or vout not positive, nValue=" << nValue << std::endl);
                }
                else
                    LOGSTREAMFN("marmara", CCLOG_ERROR, stream << "decode error unlockht=" << unlockht << " vs unlocks=" << unlocks << " is-pool-pk=" << (pk == poolpk) << std::endl);
            }
            else
                LOGSTREAMFN("marmara", CCLOG_ERROR, stream << "not coinbase" << std::endl);
        }
        else
            LOGSTREAMFN("marmara", CCLOG_ERROR, stream << "error getting tx=" << txid.GetHex() << std::endl);
    }
    return(totalinputs);
}


// Classes to check opret by calling CheckOpret member func for two cases:
// 1) the opret in cc vout data is checked first and considered primary
// 2) if it is not required to check only cc opret, the opret in the last vout is checked second and considered secondary
// returns the opret and pubkey from the opret

class CMarmaraOpretChecker {
public:
    bool checkOnlyCC;
    virtual bool CheckOpret(const CScript &spk, CPubKey &opretpk) const = 0;
};

// checks if opret for activated coins, returns pk from opret
class CActivatedOpretChecker : public CMarmaraOpretChecker
{
public:
    CActivatedOpretChecker() { checkOnlyCC = false; }   // TODO: isn't only the cc opret allowed now???
    bool CheckOpret(const CScript &spk, CPubKey &opretpk) const
    {
        uint8_t funcid;
        int32_t ht, unlockht;

        return (funcid = MarmaraDecodeCoinbaseOpret(spk, opretpk, ht, unlockht)) == 'C' || funcid == 'P' || funcid == 'A';
    }
};

// checks if opret for lock-in-loop coins, returns pk from opret
class CLockInLoopOpretChecker : public CMarmaraOpretChecker
{
public:
    CLockInLoopOpretChecker() { checkOnlyCC = false; }
    CLockInLoopOpretChecker(bool whatToCheck) { checkOnlyCC = whatToCheck; }
    bool CheckOpret(const CScript &spk, CPubKey &opretpk) const
    {
        struct CreditLoopOpret loopData;

        uint8_t funcid = MarmaraDecodeLoopOpret(spk, loopData);
        if (funcid != 0) {
            opretpk = loopData.pk;
            return true;
        }
        return false;
    }
};

// checks either of two options for tx:
// tx has cc vin for evalcode
static bool tx_has_my_cc_vin(struct CCcontract_info *cp, const CTransaction &tx)
{
    for (auto const &vin : tx.vin)
        if (cp->ismyvin(vin.scriptSig))
            return true;

    return false;
}

// check if this is a activated vout:
static bool activated_vout_matches_pk_in_opret(const CTransaction &tx, int32_t nvout, const CScript &opret)
{
    CPubKey pk;
    int32_t h, unlockh;

    MarmaraDecodeCoinbaseOpret(opret, pk, h, unlockh);
    if (tx.vout[nvout] == MakeMarmaraCC1of2voutOpret(tx.vout[nvout].nValue, pk, opret))
        return true;
    else
        return false;
}

// check if this is a LCL vout:
static bool vout_matches_createtxid_in_opret(const CTransaction &tx, int32_t nvout, const CScript &opret)
{
    struct CreditLoopOpret loopData;
    MarmaraDecodeLoopOpret(opret, loopData);

    CPubKey createtxidPk = CCtxidaddr_tweak(NULL, loopData.createtxid);

    if (tx.vout[nvout] == MakeMarmaraCC1of2voutOpret(tx.vout[nvout].nValue, createtxidPk, opret))
        return true;
    else
        return false;
}


// calls checker first for the cc vout opret then for the last vout opret
static bool get_either_opret(CMarmaraOpretChecker *opretChecker, const CTransaction &tx, int32_t nvout, CScript &opretOut, CPubKey &opretpk)
{
    CScript opret;
    bool isccopret = false, opretok = false;

    if (!opretChecker)
        return false;

    // first check cc opret
    if (GetCCOpReturnData(tx.vout[nvout].scriptPubKey, opret))
    {
        LOGSTREAMFN("marmara", CCLOG_DEBUG3, stream << "ccopret=" << opret.ToString() << std::endl);
        if (opretChecker->CheckOpret(opret, opretpk))
        {
            isccopret = true;
            opretok = true;
            opretOut = opret;
        }
    }

    // then check opret in the last vout:
    if (!opretChecker->checkOnlyCC && !opretok)   // if needed opret was not found in cc vout then check opret in the back of vouts
    {
        if (nvout < tx.vout.size()-1) {   // there might be opret in the back
            opret = tx.vout.back().scriptPubKey;
            if (opretChecker->CheckOpret(opret, opretpk))
            {
                isccopret = false;
                opretok = true;
                opretOut = opret;
            }
        }
    }

    // print opret evalcode and funcid for debug logging:
    vscript_t vprintopret;
    uint8_t funcid = 0, evalcode = 0;
    if (GetOpReturnData(opret, vprintopret) && vprintopret.size() >= 2)
    {
        evalcode = vprintopret.begin()[0];
        funcid = vprintopret.begin()[1];
    }
    LOGSTREAMFN("marmara", CCLOG_DEBUG3, stream << " opret eval=" << (int)evalcode << " funcid=" << (char)(funcid ? funcid : ' ') << " isccopret=" << isccopret << std::endl);
    return opretok;
}

// checks if tx vout is valid activated coins:
// - activated opret is okay
// - vin txns are funded from marmara cc inputs (this means they were validated while added to the chain) 
// - or vin txns are self-funded from normal inputs
// returns the pubkey from the opret
bool IsMarmaraActivatedVout(const CTransaction &tx, int32_t nvout, CPubKey &pk_in_opret)
{
    CActivatedOpretChecker activatedOpretChecker;
    CScript opret;

    if (nvout < 0 || nvout >= tx.vout.size())
        return false;

    // this check considers 2 cases:
    // first if opret is in the cc vout data
    // second if opret is in the last vout
    if (get_either_opret(&activatedOpretChecker, tx, nvout, opret, pk_in_opret))
    {
        // check opret pk matches vout
        if (activated_vout_matches_pk_in_opret(tx, nvout, opret))
        {
            struct CCcontract_info *cp, C;
            cp = CCinit(&C, EVAL_MARMARA);

            // if opret is okay
            // check that vintxns have cc inputs
            // or only self-funded from normals activated coins are allowed

            if (!tx_has_my_cc_vin(cp, tx) && TotalPubkeyNormalInputs(tx, pk_in_opret) == 0)
            {
                LOGSTREAMFN("marmara", CCLOG_DEBUG1, stream << "vintx=" << tx.GetHash().GetHex() << " has no marmara cc inputs or self-funding normal inputs" << std::endl);
                return false;
            }

            // vout is okay
            return true;
        }
        else
        {
            LOGSTREAMFN("marmara", CCLOG_DEBUG1, stream << "tx=" << tx.GetHash().GetHex() << " pubkey in opreturn does not match vout" << std::endl);
            return false;
        }
    }
    return false;
}

/*bool IsMarmaraActivatedVout(const CTransaction &tx, int32_t nvout, CPubKey &pk_in_opret)
{
    CActivatedOpretChecker activatedOpretChecker;
    CScript opret;

    if (nvout < 0 || nvout >= tx.vout.size())
        return false;

    // this check considers 2 cases:
    // first if opret is in the cc vout data
    // second if opret is in the last vout
    if (CheckEitherOpRet(&activatedOpretChecker, tx, nvout, opret, pk_in_opret))
    {
        // check opret pk matches vout
        if (vout_matches_pk_in_opret(tx, nvout, opret))
        {
            struct CCcontract_info *cp, C;
            cp = CCinit(&C, EVAL_MARMARA);

            // if opret is okay
            // check that vintxns have cc inputs
            // or only self-funded from normals activated coins are allowed
            for (auto const &vin : tx.vin)
            {
                if (IsCCInput(vin.scriptSig))
                {
                    if (cp->ismyvin(vin.scriptSig))
                    {
                        CTransaction vintx;
                        uint256 hashBlock;
                        if (myGetTransaction(vin.prevout.hash, vintx, hashBlock) && !hashBlock.IsNull()) // not in mempool
                        {
                            CScript vintxOpret;
                            CPubKey pk_in_vintx_opret;

                            if (CheckEitherOpRet(&activatedOpretChecker, vintx, vin.prevout.n, vintxOpret, pk_in_vintx_opret))
                            {
                                if (tx_has_my_cc_vin(cp, vintx) || TotalPubkeyNormalInputs(vintx, pk_in_opret) > 0)
                                {
                                    // check opret pk matches vintx vout
                                    if (!vout_matches_pk_in_opret(tx, nvout, opret))
                                    {
                                        LOGSTREAMFN("marmara", CCLOG_INFO, stream << "vintx=" << vin.prevout.hash.GetHex() << " opreturn does not match vout (skipping this vout)" << std::endl);
                                        return false;
                                    }
                                }
                                else
                                {
                                    LOGSTREAMFN("marmara", CCLOG_INFO, stream << "vintx=" << vin.prevout.hash.GetHex() << " has no marmara cc inputs or not self-funded (skipping this vout)" << std::endl);
                                    return false;
                                }
                            }
                            else
                            {
                                LOGSTREAMFN("marmara", CCLOG_INFO, stream << "vintx=" << vin.prevout.hash.GetHex() << " has incorrect opreturn (skipping this vout)" << std::endl);
                                return false;
                            }
                        }
                        else
                        {
                            LOGSTREAMFN("marmara", CCLOG_DEBUG1, stream << "could not load vintx=" << vin.prevout.hash.GetHex() << " (vintx could be still in mempool)" << std::endl);
                            return false;
                        }
                    }
                    else
                    {
                        LOGSTREAMFN("marmara", CCLOG_INFO, stream << "vins from other cc modules are not allowed, txid=" << tx.GetHash().GetHex() << " vin.nSequence=" << vin.nSequence << " (skipping this vout)" << std::endl);
                        return false;
                    }
                }
            }
        }
        else
        {
            LOGSTREAMFN("marmara", CCLOG_INFO, stream << "tx=" << tx.GetHash().GetHex() << " pubkey in opreturn does not match vout (skipping this vout)" << std::endl);
            return false;
        }
    }
    return true;
}  */



// checks if tx vout is valid locked-in-loop coins
// - activated opret is okay
// - vin txns are funded from marmara cc inputs (this means they were validated while added to the chain)
// returns the pubkey from the opret

bool IsMarmaraLockedInLoopVout(const CTransaction &tx, int32_t nvout, CPubKey &pk_in_opret)
{
    CLockInLoopOpretChecker lclOpretChecker;
    CScript opret;
    struct CCcontract_info *cp, C;
    cp = CCinit(&C, EVAL_MARMARA);
    CPubKey Marmarapk = GetUnspendable(cp, NULL);

    if (nvout < 0 || nvout >= tx.vout.size())
        return false;

    // this check considers 2 cases:
    // first if opret is in the cc vout data
    // second if opret is in the last vout
    if (get_either_opret(&lclOpretChecker, tx, nvout, opret, pk_in_opret))
    {
        // check opret pk matches vout
        if (vout_matches_createtxid_in_opret(tx, nvout, opret))
        {
            struct CCcontract_info *cp, C;
            cp = CCinit(&C, EVAL_MARMARA);

            // if opret is okay
            // check that vintxns have cc inputs
            if (!tx_has_my_cc_vin(cp, tx))
            {
                LOGSTREAMFN("marmara", CCLOG_DEBUG1, stream << "vintx=" << tx.GetHash().GetHex() << " has no marmara cc inputs" << std::endl);
                return false;
            }
            // vout is okay
            return true;
        }
        else
        {
            LOGSTREAMFN("marmara", CCLOG_DEBUG1, stream << "tx=" << tx.GetHash().GetHex() << " pubkey in opreturn does not match vout" << std::endl);
            return false;
        }
    }
    return false;
}

// add activated or locked-in-loop coins from 1of2 address 
// for lock-in-loop mypk not checked, so all locked-in-loop utxos for an address are added:
template <typename IsMarmaraVoutT>
int64_t AddMarmaraCCInputs(IsMarmaraVoutT IsMarmaraVout, CMutableTransaction &mtx, std::vector<CPubKey> &pubkeys, const char *unspentaddr, CAmount amount, int32_t maxinputs)
{
    CAmount totalinputs = 0, totaladded = 0;
    
    if (maxinputs > CC_MAXVINS)
        maxinputs = CC_MAXVINS;

    // threshold not used any more
    /*if (maxinputs > 0)
    threshold = total / maxinputs;
    else
    threshold = total;*/

    std::vector<CC_utxo> utxos;
    std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> > unspentOutputs;
    SetCCunspents(unspentOutputs, (char*)unspentaddr, true);

    if (amount != 0 && unspentOutputs.size() > 0)  // if amount == 0 only calc total
    {
        utxos.reserve(unspentOutputs.size());
        if (utxos.capacity() == 0)
        {
            LOGSTREAMFN("marmara", CCLOG_ERROR, stream << "not enough memory to load utxos" << std::endl);
            return -1;
        }
    }

    LOGSTREAMFN("marmara", CCLOG_DEBUG2, stream << "adding utxos from addr=" << unspentaddr << " total=" << amount << std::endl);

    // add all utxos from cc addr:
    for (std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> >::const_iterator it = unspentOutputs.begin(); it != unspentOutputs.end(); it++)
    {
        uint256 txid = it->first.txhash;
        int32_t nvout = (int32_t)it->first.index;
        uint256 hashBlock;
        CTransaction tx;

        //TODO: decide on threshold usage, could be side effects like 'insufficient funds' error
        //if (it->second.satoshis < threshold)
        //    continue;

        // check if vin might be already added to mtx:
        if (std::find_if(mtx.vin.begin(), mtx.vin.end(), [&](CTxIn v) {return (v.prevout.hash == txid && v.prevout.n == nvout); }) != mtx.vin.end()) {
            LOGSTREAMFN("marmara", CCLOG_DEBUG2, stream << "skipping already added txid=" << txid.GetHex() << " nvout=" << nvout << " satoshis=" << it->second.satoshis << std::endl);
            continue;
        }

        bool isSpentInMempool = false;
        if (myGetTransaction(txid, tx, hashBlock) && 
            tx.vout.size() > 0 &&
            tx.vout[nvout].scriptPubKey.IsPayToCryptoCondition() &&
            !(isSpentInMempool = myIsutxo_spentinmempool(ignoretxid, ignorevin, txid, nvout)))
        {
            CPubKey opretpk;
            //CScript opret;
            std::vector< vscript_t > vParams;
            //bool isccopret = false, opretok = false;

            // picks up either activated or LCL vouts
            if (IsMarmaraVout(tx, nvout, opretpk))      //if (CheckEitherOpRet(opretChecker, tx, nvout, opret, senderpk))
            {
                char utxoaddr[KOMODO_ADDRESS_BUFSIZE];

                Getscriptaddress(utxoaddr, tx.vout[nvout].scriptPubKey);
                if (strcmp(unspentaddr, utxoaddr) == 0)  // check if the real vout address matches the index address (as another key could be used in the addressindex)
                {
                    LOGSTREAMFN("marmara", CCLOG_DEBUG2, stream << "found good vintx for addr=" << unspentaddr << " txid=" << txid.GetHex() << " nvout=" << nvout << " satoshis=" << it->second.satoshis << std::endl);

                    if (amount != 0)
                    {
                        CC_utxo ccutxo{ txid, it->second.satoshis, nvout };
                        utxos.push_back(ccutxo);
                        pubkeys.push_back(opretpk); // add endorsers pubkeys
                    }
                    totalinputs += it->second.satoshis;
                }
                else
                    LOGSTREAMFN("marmara", CCLOG_ERROR, stream << "incorrect index addr=" << unspentaddr << " vs utxoaddr=" << utxoaddr << " txid=" << txid.GetHex() << std::endl);
            }
            else
                LOGSTREAMFN("marmara", CCLOG_INFO, stream << "addr=" << unspentaddr << " txid=" << txid.GetHex() << " nvout=" << nvout << " IsMarmaraVout returned false, skipping vout" << std::endl);
        }
        else
            LOGSTREAMFN("marmara", CCLOG_DEBUG2, stream << "skipping txid=" << txid.GetHex() << " nvout=" << nvout << " satoshis=" << it->second.satoshis  << " isSpentInMempool=" << isSpentInMempool << std::endl);
    }

    LOGSTREAMFN("marmara", CCLOG_DEBUG2, stream << "for addr=" << unspentaddr << " found total=" << totalinputs << std::endl);
    if (amount == 0)
        return totalinputs;

    // add best selected utxos:
    CAmount remains = amount;
    while (utxos.size() > 0)
    {
        int64_t below = 0, above = 0;
        int32_t abovei = -1, belowi = -1, ind = -1;

        if (CC_vinselect(&abovei, &above, &belowi, &below, utxos.data(), utxos.size(), remains) < 0)
        {
            LOGSTREAMFN("marmara", CCLOG_ERROR, stream << "error finding unspent" << " remains=" << remains << " amount=" << amount << " utxos.size()=" << utxos.size() << std::endl);
            return(0);
        }
        if (abovei >= 0) // best is 'above'
            ind = abovei;
        else if (belowi >= 0)  // second try is 'below'
            ind = belowi;
        else
        {
            LOGSTREAMFN("marmara", CCLOG_ERROR, stream << "error finding unspent" << " remains=" << remains << " amount=" << amount << " abovei=" << abovei << " belowi=" << belowi << " ind=" << " utxos.size()=" << utxos.size() << ind << std::endl);
            return(0);
        }

        mtx.vin.push_back(CTxIn(utxos[ind].txid, utxos[ind].vout, CScript()));
        totaladded += utxos[ind].nValue;
        remains -= utxos[ind].nValue;

        // remove used utxo[ind]:
        utxos[ind] = utxos.back();
        utxos.pop_back();

        if (totaladded >= amount) // found the requested amount
            break;
        if (mtx.vin.size() >= maxinputs)  // reached maxinputs
            break;
    }

//    if (totaladded < amount)  // why do we need this?
//        return 0;

    return totaladded;
}



// finds the creation txid from the loop tx opret or 
// return itself if it is the request tx
static int32_t get_create_txid(uint256 &createtxid, uint256 txid)
{
    CTransaction tx; 
    uint256 hashBlock; 
  
    createtxid = zeroid;
    if (myGetTransaction(txid, tx, hashBlock) != 0 && !hashBlock.IsNull() && tx.vout.size() > 1)  // might be called from validation code, so non-locking version
    {
        uint8_t funcid;
        struct CreditLoopOpret loopData;

        if ((funcid = MarmaraDecodeLoopOpret(tx.vout.back().scriptPubKey, loopData)) == 'I' || funcid == 'T' || funcid == 'R' ) {
            createtxid = loopData.createtxid;
            LOGSTREAMFN("marmara", CCLOG_DEBUG2, stream  << "found for funcid=" << (char)funcid << " createtxid=" << createtxid.GetHex() << std::endl);
            return(0);
        }
        else if (funcid == 'B')
        {
            if (createtxid == zeroid)  // TODO: maybe this is not needed 
                createtxid = txid;
            LOGSTREAMFN("marmara", CCLOG_DEBUG2, stream  << "found for funcid=" << (char)funcid << " createtxid=" << createtxid.GetHex() << std::endl);
            return(0);
        }
    }
    return(-1);
}

// starting from any baton txid, finds the latest yet unspent batontxid 
// adds createtxid 'B' in creditloop vector (only if there are other txns in the loop)
// finds all the baton txids starting from the createtx (1+ in creditloop vector), apart from the latest baton txid
// returns the number of txns marked with the baton
int32_t MarmaraGetbatontxid(std::vector<uint256> &creditloop, uint256 &batontxid, uint256 txid)
{
    uint256 createtxid, spenttxid; 
    int64_t value; 
    int32_t vini, height, n = 0;
    const int32_t NO_MEMPOOL = 1;
    const int32_t DO_LOCK = 1;
    
    batontxid = zeroid;
    if (get_create_txid(createtxid, txid) == 0) // retrieve the initial creation txid
    {
        txid = createtxid;
        //fprintf(stderr,"%s txid.%s -> createtxid %s\n", logFuncName, txid.GetHex().c_str(),createtxid.GetHex().c_str());

        while (CCgetspenttxid(spenttxid, vini, height, txid, MARMARA_BATON_VOUT) == 0)  // while the current baton is spent
        {
            creditloop.push_back(txid);
            //fprintf(stderr,"%d: %s\n",n,txid.GetHex().c_str());
            n++;
            if ((value = CCgettxout(spenttxid, MARMARA_BATON_VOUT, NO_MEMPOOL, DO_LOCK)) == 10000)  //check if the baton value is unspent yet - this is the last baton
            {
                batontxid = spenttxid;
                //fprintf(stderr,"%s got baton %s %.8f\n", logFuncName, batontxid.GetHex().c_str(),(double)value/COIN);
                return n;
            }
            else if (value > 0)
            {
                batontxid = spenttxid;
                LOGSTREAMFN("marmara", CCLOG_ERROR, stream  << "n=" << n << " found and will use false baton=" << batontxid.GetHex() << " vout=" << MARMARA_BATON_VOUT << "value=" << value << std::endl);
                return n;
            }
            // TODO: get funcid (and check?)
            txid = spenttxid;
        }

        if (n == 0) 
            return 0;   // empty loop
        else
            return -1;  //bad loop
    }
    return -1;
}

static int32_t get_settlement_txid(uint256 &settletxid, uint256 issuetxid)
{
    int32_t vini, height;

    if (CCgetspenttxid(settletxid, vini, height, issuetxid, MARMARA_OPENCLOSE_VOUT) == 0)
    {
        return 0;
    }
    return -1;
}

// load the create tx and adds data from its opret to loopData safely, with no overriding
static int32_t get_loop_creation_data(uint256 createtxid, struct CreditLoopOpret &loopData)
{
    CTransaction tx;
    uint256 hashBlock;

    if (myGetTransaction(createtxid, tx, hashBlock) != 0 && !hashBlock.IsNull() && tx.vout.size() > 1)  // might be called from validation code, so non-locking version
    {
        uint8_t funcid;
        vscript_t vopret;

        // first check if this is really createtx to prevent override loopData with other tx type data:
        if (GetOpReturnData(tx.vout.back().scriptPubKey, vopret) && vopret.size() >= 2 && vopret.begin()[0] == EVAL_MARMARA && vopret.begin()[1] == 'B')  
        {
            if ((funcid = MarmaraDecodeLoopOpret(tx.vout.back().scriptPubKey, loopData)) == 'B') {
                return(0); //0 is okay
            }
        }
    }
    return(-1);
}

// consensus code:

// check total loop amount in tx and redistributed back amount:
static bool check_lcl_redistribution(const CTransaction &tx, uint256 requesttxid, std::string &errorStr)
{
    std::vector<uint256> creditloop;
    uint256 batontxid, createtxid;
    struct CreditLoopOpret creationLoopData;
    struct CreditLoopOpret currentLoopData;
    int32_t n_endorsers = 0;

    if ((n_endorsers = MarmaraGetbatontxid(creditloop, batontxid, requesttxid)) < 0) {
        errorStr = "could not get credit loop";
        return false;
    }

    createtxid = creditloop.empty() ? requesttxid : creditloop[0];
    if (get_loop_creation_data(createtxid, creationLoopData) < 0)
    {
        errorStr = "could not get credit loop creation data";
        return false;
    }

    // get opret data
    if (tx.vout.size() == 0 || MarmaraDecodeLoopOpret(tx.vout.back().scriptPubKey, currentLoopData) == 0)
    {
        errorStr = "no opreturn found in the last vout";
        return false;
    }

    CAmount lclAmount = 0L;
    std::set<CPubKey> prevEndorserPks;
    for (int32_t i = 0; i < tx.vout.size() - 1; i ++)  // except the last vout opret
    {
        if (tx.vout[i].scriptPubKey.IsPayToCryptoCondition())
        {
            CScript opret;
            CreditLoopOpret voutLoopData;

            if (GetCCOpReturnData(tx.vout[i].scriptPubKey, opret) && MarmaraDecodeLoopOpret(opret, voutLoopData) == 'K')
            {
                CPubKey createtxidPk = CCtxidaddr_tweak(NULL, createtxid);
                if (tx.vout[i] != MakeMarmaraCC1of2voutOpret(tx.vout[i].nValue, createtxidPk, opret))
                {
                    errorStr = "'K' cc output incorrect";
                    return false;
                }
                lclAmount = +tx.vout[i].nValue;
                prevEndorserPks.insert(voutLoopData.pk);
            }
            /* for issue tx no 'K' vouts:
            else
            {
                errorStr = "no 'K' funcid found in cc opreturn";
                return false;
            } */
        }
    }

    // check loop amount:
    if (creationLoopData.amount != lclAmount) 
    {
        errorStr = "tx LCL amount invalid";
        return false;
    }

    if (n_endorsers != prevEndorserPks.size() - 1)
    {
        errorStr = "invalid endorsers size to pay back 1/N";
        return false;
    }

    // calc total redistributed amount to endorsers' normal outputs:
    CAmount redistributedAmount = 0L;
    for (auto const &v : tx.vout)
    {
        if (!v.scriptPubKey.IsPayToCryptoCondition())
        {
            // check if a normal matches to any endorser pubkey
            for (auto & pk : prevEndorserPks) {
                if (v == CTxOut(v.nValue, CScript() << ParseHex(HexStr(pk)) << OP_CHECKSIG)) {
                    prevEndorserPks.erase(pk);
                    redistributedAmount += v.nValue;
                }
            }
        }
    }
    // only one new endorser should remain without back payment to a normal output
    if (prevEndorserPks.size() != 1)
    {
        LOGSTREAMFN("marmara", CCLOG_ERROR, stream << "invalid redistribution to normals: remained endorserPks.size()=" << prevEndorserPks.size() << std::endl);
        errorStr = "tx redistribution amount to normals invalid";
        return false;
    }

    // check that redistributed amount == (N-1)/N * loop amount
    if (lclAmount - redistributedAmount != lclAmount / n_endorsers)
    {
        LOGSTREAMFN("marmara", CCLOG_ERROR, stream << "invalid redistribution to normals: lclAmount=" << lclAmount << " redistributedAmount =" << redistributedAmount << " n_endorsers=" << n_endorsers << " (lclAmount - redistributedAmount)=" << (lclAmount - redistributedAmount) << " (lclAmount / n_endorsers)=" << (lclAmount / n_endorsers) << std::endl);
        return false;
    }

    return true;
}

// check request or create tx 
static bool check_request_tx(uint256 requesttxid, CPubKey receiverpk, uint8_t issueFuncId, std::string &errorstr)
{
    struct CCcontract_info *cp, C;
    cp = CCinit(&C, EVAL_MARMARA);
    CPubKey Marmarapk = GetUnspendable(cp, NULL);

    // make sure less than maxlength (?)

    uint256 createtxid;
    struct CreditLoopOpret loopData;
    CTransaction requesttx;
    uint256 hashBlock;
    uint8_t funcid = 0;

    if (requesttxid.IsNull())
        errorstr = "requesttxid can't be empty";
    else if (get_create_txid(createtxid, requesttxid) < 0)
        errorstr = "can't get createtxid from requesttxid (request tx could be in mempool)";
    // check requested cheque params:
    else if (get_loop_creation_data(createtxid, loopData) < 0)
        errorstr = "cannot get loop creation data";
    else if (!myGetTransaction(requesttxid, requesttx, hashBlock))
        errorstr = "cannot get request transaction";
    // TODO: do we need here to check the request tx in mempool?
    else if (hashBlock.IsNull())    /*is in mempool?*/
        errorstr = "request transaction still in mempool";
    else if (requesttx.vout.size() < 1 || (funcid = MarmaraDecodeLoopOpret(requesttx.vout.back().scriptPubKey, loopData)) == 0)
        errorstr = "cannot decode request tx opreturn data";
    else if (TotalPubkeyNormalInputs(requesttx, receiverpk) == 0)     // extract and check the receiver pubkey
        errorstr = "receiver pubkey does not match signer of request tx";
    else if (TotalPubkeyNormalInputs(requesttx, loopData.pk) > 0)     // extract and check the receiver pubkey
        errorstr = "sender pk signed request tx, cannot request credit from self";
    else if (loopData.matures <= chainActive.LastTip()->GetHeight())
        errorstr = "credit loop must mature in the future";

    else {
        if (issueFuncId == 'I' && funcid != 'B')
            errorstr = "not a create tx";
        if (issueFuncId == 'T' && funcid != 'R')
            errorstr = "not a request tx";
    }
    
    if (!errorstr.empty())
        return false;
    else
        return true;
}

static bool check_issue_tx(const CTransaction &tx, std::string &errorstr)
{
    struct CreditLoopOpret loopData;
    struct CCcontract_info *cp, C;
    cp = CCinit(&C, EVAL_MARMARA);

    if (tx.vout.size() == 0) {
        errorstr = "no vouts";
        return false;
    }

    MarmaraDecodeLoopOpret(tx.vout.back().scriptPubKey, loopData);
    if (loopData.lastfuncid != 'I' && loopData.lastfuncid != 'T') {
        errorstr = "not an issue or transfer tx";
        return false;
    }

    // find request tx, it is in the first or second cc input from the end:
    std::list<int32_t> nbatonvins;
    for (int i = tx.vin.size() - 1; i >= 0; i --)
    {
        if (IsCCInput(tx.vin[i].scriptSig))
        {
            if (cp->ismyvin(tx.vin[i].scriptSig))
            {
                CTransaction vintx;
                uint256 hashBlock;

                if (myGetTransaction(tx.vin[i].prevout.hash, vintx, hashBlock) /*&& !hashBlock.IsNull()*/)
                {
                    CPubKey pk_in_opret;
                    if (!IsMarmaraActivatedVout(vintx, tx.vin[i].prevout.n, pk_in_opret))   // if vin not added by AddMarmaraCCInputs
                    {
                        nbatonvins.push_back(i);                                            // this is probably baton or request tx
                    }
                }
                else
                {
                    errorstr = "can't get vintx for vin=" + std::to_string(i);
                    return false;
                }
            }
            else
            {
                errorstr = "tx cannot have non-marmara cc vins";
                return false;
            }
        }
    }

    if (nbatonvins.size() == 0)
    {
        errorstr = "no request tx vin";
        return false;
    }
    int32_t requesttx_i = nbatonvins.front();
    nbatonvins.pop_front();
    if (!check_request_tx(tx.vin[requesttx_i].prevout.hash, loopData.pk, loopData.lastfuncid, errorstr))
        return false;

    if (loopData.lastfuncid == 'T')
    {
        CTransaction vintx;
        uint256 hashBlock;

        if (nbatonvins.size() == 0)
        {
            errorstr = "no baton vin";
            return false;
        }
        int32_t baton_i = nbatonvins.front();
        nbatonvins.pop_front();

        // TODO: check that the baton tx is a cc tx 
        if (myGetTransaction(tx.vin[baton_i].prevout.hash, vintx, hashBlock) /*&& !hashBlock.IsNull()*/)
        {
            if (!tx_has_my_cc_vin(cp, vintx)) {
                errorstr = "no marmara cc vins in baton tx";
                return false;
            }
        }
    }

    if (nbatonvins.size() != 0)  // no other vins should present
    {
        errorstr = "unknown cc vin(s)";
        return false;
    }

    //if (loopData.lastfuncid == 'T')  // TODO: maybe for issue tx it could work too
    //{

    // check LCL fund redistribution and vouts in transfer tx
    if (!check_lcl_redistribution(tx, tx.vin[requesttx_i].prevout.hash, errorstr))
        return false;
    //}

    // check issue tx vouts...
    // ...checked in check_lcl_redistribution

    return true;
}

//#define HAS_FUNCID(v, funcid) (std::find((v).begin(), (v).end(), funcid) != (v).end())
#define FUNCIDS_TO_STRING(funcids) [](const std::set<uint8_t> &s) { std::string r; for (auto const &e : s) r += e; return r; }(funcids)

bool MarmaraValidate(struct CCcontract_info *cp, Eval* eval, const CTransaction &tx, uint32_t nIn)
{
    if (!ASSETCHAINS_MARMARA)
        return eval->Invalid("-ac_marmara must be set for marmara CC");

    if (tx.vout.size() < 1)
        return eval->Invalid("no vouts");

    CPubKey Marmarapk = GetUnspendable(cp, 0);
    std::string validationError;
    std::set<uint8_t> funcIds;

    for (int32_t i = 0; i < tx.vout.size(); i++)
    {
        CPubKey opretpk;
        CScript opret;
        CActivatedOpretChecker activatedChecker;
        CLockInLoopOpretChecker lockinloopChecker;

        // temp simple check for opret presence
        if (get_either_opret(&activatedChecker, tx, i, opret, opretpk)) 
        {
            CPubKey pk;
            int32_t h, uh;
            uint8_t funcid = MarmaraDecodeCoinbaseOpret(opret, pk, h, uh);
            funcIds.insert(funcid);
        }
        else if (get_either_opret(&lockinloopChecker, tx, i, opret, opretpk))
        {
            struct CreditLoopOpret loopData;
            MarmaraDecodeLoopOpret(opret, loopData);
            funcIds.insert(loopData.lastfuncid);
        }
    }

    if (funcIds.empty())
        return eval->Error("invalid or no opreturns");

    if (funcIds == std::set<uint8_t>{'P'})
    {
        int32_t ht, unlockht, vht, vunlockht;
        CPubKey pk, vpk;
        uint8_t funcid = MarmaraDecodeCoinbaseOpret(tx.vout.back().scriptPubKey, pk, ht, unlockht);

        for (int32_t i = 0; i < tx.vin.size(); i++)
        {
            if ((*cp->ismyvin)(tx.vin[i].scriptSig) != 0)
            {
                CTransaction vinTx;
                uint256 hashBlock;

                if (eval->GetTxUnconfirmed(tx.vin[i].prevout.hash, vinTx, hashBlock) == 0)
                    return eval->Error("cant find vinTx");
                else
                {
                    if (vinTx.IsCoinBase() == 0)
                        return eval->Error("noncoinbase input");
                    else if (vinTx.vout.size() != 2)
                        return eval->Error("coinbase doesnt have 2 vouts");
                    uint8_t vfuncid = MarmaraDecodeCoinbaseOpret(vinTx.vout[1].scriptPubKey, vpk, vht, vunlockht);
                    if (vfuncid != 'C' || vpk != pk || vunlockht != unlockht)
                        return eval->Error("mismatched opreturn");
                }
            }
        }
        return(true);
    }
    else if (funcIds == std::set<uint8_t>{'L'}) // locked in loop funds 
    {
        // TODO: check this, seems error() is better than invalid():
        return eval->Error("unexpected tx funcid 'L'");   // this tx should have no cc inputs
    }
    else if (funcIds == std::set<uint8_t>{'B'}) // create credit loop
    {
        return eval->Error("unexpected tx funcid 'B'");   // this tx should have no cc inputs
    }
    else if (funcIds == std::set<uint8_t>{'R'}) // receive -> agree to receive 'I' from pk, amount, currency, due ht
    {
        return eval->Error("unexpected tx funcid 'R'");   // tx have no cc inputs
    }
    else if (funcIds == std::set<uint8_t>{'I'} || funcIds == std::set<uint8_t>{'I', 'K'} || funcIds == std::set<uint8_t>{'A', 'I', 'K'}) // issue -> issue currency to pk with due mature height
    {
        if (!check_issue_tx(tx, validationError))
            return eval->Error("invalid issue tx: " + validationError);   // tx have no cc inputs
        else
            return true;
    }
    else if (funcIds == std::set<uint8_t>{'T'} || funcIds == std::set<uint8_t>{'T', 'K'} || funcIds == std::set<uint8_t>{'A', 'T', 'K'}) // transfer -> given 'R' transfer 'I' or 'T' to the pk of 'R'
    {
        if (!check_issue_tx(tx, validationError))
            return eval->Error("invalid transfer tx: " + validationError);   // tx have no cc inputs
        else
            return true;
    }
    else if (funcIds == std::set<uint8_t>{'S'}) // settlement -> automatically spend issuers locked funds, given 'I'
    {
        // TODO: validate settlement tx
        return(true);
    }
    else if (funcIds == std::set<uint8_t>{'D'}) // insufficient settlement
    {
        return(true);
    }
    else if (funcIds == std::set<uint8_t>{'C'}) // coinbase 
    {
        return(true);
    }
    else if (funcIds == std::set<uint8_t>{'K'}) // pk in lock-in-loop
    {
        return(true);
    }
    else if (funcIds == std::set<uint8_t>{'A'}) // activated
    {
        return(true);
    }
    else if (funcIds == std::set<uint8_t>{'O'}) // released
    {
        return(true);
    }
    // staking only for locked utxo

    LOGSTREAMFN("marmara", CCLOG_ERROR, stream << " validation error for txid=" << tx.GetHash().GetHex() << " tx has bad funcids=" << FUNCIDS_TO_STRING(funcIds) << std::endl);
    return eval->Error("fall through error");
}
// end of consensus code



// set marmara coinbase opret for even blocks
// this is also activated coins opret
CScript MarmaraCoinbaseOpret(uint8_t funcid, int32_t height, CPubKey pk)
{
    uint8_t *ptr;
    //fprintf(stderr,"height.%d pksize.%d\n",height,(int32_t)pk.size());
    if (height > 0 && (height & 1) == 0 && pk.size() == CPubKey::COMPRESSED_PUBLIC_KEY_SIZE)
        return(EncodeMarmaraCoinbaseOpRet(funcid, pk, height));
    else
        return(CScript());
}

// returns scriptPubKey with 1of2 addr for coinbase tx where coins will go in createNewBlock in miner.cpp 
CScript Marmara_scriptPubKey(int32_t nHeight, CPubKey minerpk)
{
    if (nHeight > 0 && (nHeight & 1) == 0)
    {
        char coinaddr[KOMODO_ADDRESS_BUFSIZE];
        CScript opret = MarmaraCoinbaseOpret('C', nHeight, minerpk);
        CTxOut ccvout; 
       
        if (minerpk.size() != CPubKey::COMPRESSED_PUBLIC_KEY_SIZE)
        {
            LOGSTREAMFN("marmara", CCLOG_ERROR, stream << "bad minerpk=" << HexStr(minerpk) << std::endl);
            return CScript();
        }

        // use special rewards pubkey for testing 
        std::string marmara_test_pubkey_param = GetArg("-marmara-test-pubkey", "");
        if (!marmara_test_pubkey_param.empty()) {
            minerpk = pubkey2pk(ParseHex(marmara_test_pubkey_param));
        }

        // set initial amount to zero, it will be overriden by miner's code
        ccvout = MakeMarmaraCC1of2voutOpret(0, minerpk, opret);  // add cc opret to coinbase
        Getscriptaddress(coinaddr, ccvout.scriptPubKey);

        //LOGSTREAMFN("marmara", CCLOG_DEBUG1, stream << "for activated rewards using pk=" << HexStr(minerpk) << " height=" << nHeight << " 1of2addr=" << coinaddr << std::endl);
        return(ccvout.scriptPubKey);
    }
    else
    {
        //LOGSTREAMFN("marmara", CCLOG_DEBUG1, stream << "not even ht, returning normal scriptPubKey" << std::endl);
        return CScript() << ParseHex(HexStr(minerpk)) << OP_CHECKSIG;
    }
}

// creates coinbase transaction: adds marmara opreturn 
// now actually does nothing as opret is already in the cc vout - see Marmara_scriptPubKey()
void MarmaraCreateCoinbase(CMutableTransaction &txNew, int32_t nHeight, CPubKey minerpk)
{
    char checkaddr[KOMODO_ADDRESS_BUFSIZE];

    if (nHeight > 0 && (nHeight & 1) == 0)
    {
        Getscriptaddress(checkaddr, txNew.vout[0].scriptPubKey);
        LOGSTREAMFN("marmara", CCLOG_DEBUG1, stream << "set mining coinbase=" << checkaddr << std::endl);  // uncommented

        /* do nothing as opret is already in ccvout
        txNew.vout.resize(2);
        txNew.vout[1].nValue = 0;
        txNew.vout[1].scriptPubKey = MarmaraCoinbaseOpret('C', nHeight, minerpk);
        */
    }
}

// half of the blocks (with even heights) should be mined as activated (to some unlock height)
// validates opreturn for even blocks
int32_t MarmaraValidateCoinbase(int32_t height, CTransaction tx, std::string &errmsg)
{ 
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

        std::cerr  << " ";
        for (ht = 0; ht < sizeof(histo) / sizeof(*histo); ht++)
            fprintf(stderr, "%d ", histo[ht]);
        fprintf(stderr, "<- unlock histogram[%d] by days locked\n", (int32_t)(sizeof(histo) / sizeof(*histo)));
    } */

    if ((height & 1) != 0) // odd block - no marmara opret
    {
        return(0);  // TODO: do we need to check here that really no marmara coinbase opret for odd blocks?
    }
    else //even block - check for cc vout & opret
    {
        int32_t ht, unlockht; 
        CTxOut ccvout;
        struct CCcontract_info *cp, C;
        cp = CCinit(&C, EVAL_MARMARA);
        CPubKey Marmarapk = GetUnspendable(cp, NULL);

        if (tx.vout.size() >= 1 && tx.vout.size() <= 2) // NOTE: both cc and last vout oprets are supported in coinbases
        {
            CScript opret; 
            CPubKey dummypk, opretpk;
            CActivatedOpretChecker activatedChecker;

            if (!get_either_opret(&activatedChecker, tx, 0, opret, dummypk))
            {
                LOGSTREAMFN("marmara", CCLOG_ERROR, stream << "can't find coinbase opret" << std::endl);  
                errmsg = "marmara cc bad coinbase opreturn";
                return -1;
            }

            if (MarmaraDecodeCoinbaseOpret(opret, opretpk, ht, unlockht) == 'C')
            {
                if (ht == height && MarmaraUnlockht(height) == unlockht)
                {
                    std::vector< vscript_t > vParams;
                    CScript ccvoutCoinbase;

                    ccvout = MakeCC1of2vout(EVAL_MARMARA, 0, Marmarapk, opretpk);   // TODO: check again if pk matches the address
                    tx.vout[0].scriptPubKey.IsPayToCryptoCondition(&ccvoutCoinbase, vParams);
                    if (ccvout.scriptPubKey == ccvoutCoinbase)
                        return(0);

                    char addr0[KOMODO_ADDRESS_BUFSIZE], addr1[KOMODO_ADDRESS_BUFSIZE];
                    Getscriptaddress(addr0, ccvout.scriptPubKey);
                    Getscriptaddress(addr1, tx.vout[0].scriptPubKey);
                    LOGSTREAMFN("marmara", CCLOG_ERROR, stream << " ht=" << height << " mismatched CCvout scriptPubKey=" << addr0 << " vs tx.vout[0].scriptPubKey=" << addr1 << " opretpk.size=" << opretpk.size() << " opretpk=" << HexStr(opretpk) << std::endl);
                }
                else
                    LOGSTREAMFN("marmara", CCLOG_ERROR, stream << " ht=" << height << " MarmaraUnlockht=" << MarmaraUnlockht(height) << " vs opret's ht=" << ht << " unlock=" << unlockht << std::endl);
            }
            else
                LOGSTREAMFN("marmara", CCLOG_ERROR, stream << " ht=" << height << " error decoding coinbase opret" << std::endl);
        }
        else
            LOGSTREAMFN("marmara", CCLOG_ERROR, stream << " ht=" << height << " incorrect vout size for marmara coinbase" << std::endl);

        errmsg = "marmara cc constrains even height blocks to pay 100%% to CC in vout0 with opreturn";
        return(-1);
    }
}

// check marmara stake tx
// stake tx should have one cc vout and optional opret (in this case it is the cc opret)
// stake tx points to staking utxo in vintx
// stake tx vout[0].scriptPubKey equals the referred staking utxo scriptPubKey 
// and opret equals to the opret in the last vout or to the ccopret in the referred staking tx
// see komodo_staked() where stake tx is created
int32_t MarmaraPoScheck(char *destaddr, CScript inOpret, CTransaction staketx, int32_t height)  
// note: the opret is fetched in komodo_txtime from cc opret or the last vout. 
// And that opret was added to stake tx by MarmaraSignature()
{
    uint8_t funcid; 
    char pkInOpretAddr[KOMODO_ADDRESS_BUFSIZE];
    const int32_t MARMARA_STAKE_TX_OK = 1;
    const int32_t MARMARA_NOT_STAKE_TX = 0;

    LOGSTREAMFN("marmara", CCLOG_DEBUG2, stream  << "staketxid=" << staketx.GetHash().ToString() << " numvins=" << staketx.vin.size() << " numvouts=" << staketx.vout.size() << " vout[0].nValue="  << staketx.vout[0].nValue << " inOpret.size=" << inOpret.size() << std::endl);
    //old code: if (staketx.vout.size() == 2 && inOpret == staketx.vout[1].scriptPubKey)

    //check stake tx:
    /*bool checkStakeTxVout = false;
    if (strcmp(ASSETCHAINS_SYMBOL, "MARMARAXY5") == 0 && height < 2058)
        checkStakeTxVout = (staketx.vout.size() == 2); // old blocks stake txns have last vout opret 
    else
        checkStakeTxVout = (staketx.vout.size() == 1); // stake txns have cc vout opret */

    if (staketx.vout.size() == 1 && staketx.vout[0].scriptPubKey.IsPayToCryptoCondition())
    {
        CScript opret;
        struct CCcontract_info *cp, C;
        cp = CCinit(&C, EVAL_MARMARA);
        CPubKey Marmarapk = GetUnspendable(cp, 0);
        CPubKey opretpk;
        CActivatedOpretChecker activatedChecker;
        CLockInLoopOpretChecker lockinloopChecker;

        if (get_either_opret(&activatedChecker, staketx, 0, opret, opretpk))
        {
            if (inOpret != opret)
            {
                LOGSTREAMFN("marmara", CCLOG_ERROR, stream << "found activated opret not equal to vintx opret, opret=" << opret.ToString() << std::endl);
                return MARMARA_NOT_STAKE_TX;
            }

            //int32_t height, unlockht;
            //funcid = DecodeMarmaraCoinbaseOpRet(opret, senderpk, height, unlockht);
            GetCCaddress1of2(cp, pkInOpretAddr, Marmarapk, opretpk);
            if (strcmp(destaddr, pkInOpretAddr) != 0)
            {
                LOGSTREAMFN("marmara", CCLOG_ERROR, stream << "found bad activated opret" << " destaddr=" << destaddr << " not equal to 1of2 addr for pk in opret=" << pkInOpretAddr << std::endl);
                return MARMARA_NOT_STAKE_TX;
            }
            LOGSTREAMFN("marmara", CCLOG_INFO, stream << "found good activated opret" << " destaddr=" << destaddr << std::endl);
            return MARMARA_STAKE_TX_OK;
        }
        else if (get_either_opret(&lockinloopChecker, staketx, 0, opret, opretpk))
        {
            if (inOpret != opret)
            {
                LOGSTREAMFN("marmara", CCLOG_ERROR, stream << "found bad lock-in-loop opret not equal to vintx opret, opret=" << opret.ToString() << std::endl);
                return MARMARA_NOT_STAKE_TX;
            }

            struct CreditLoopOpret loopData;
            MarmaraDecodeLoopOpret(opret, loopData);
            CPubKey createtxidPk = CCtxidaddr_tweak(NULL, loopData.createtxid);
            GetCCaddress1of2(cp, pkInOpretAddr, Marmarapk, createtxidPk);

            if (strcmp(destaddr, pkInOpretAddr) != 0)
            {
                LOGSTREAMFN("marmara", CCLOG_ERROR, stream << "found bad locked-in-loop opret" << " destaddr=" << destaddr << " not equal to 1of2 addr for pk in opret=" << pkInOpretAddr << std::endl);
                return MARMARA_NOT_STAKE_TX;
            }
            LOGSTREAMFN("marmara", CCLOG_INFO, stream << "found good locked-in-loop opret" << " destaddr=" << destaddr << std::endl);
            return MARMARA_STAKE_TX_OK;
        }
    }
    
    LOGSTREAMFN("marmara", CCLOG_DEBUG1, stream << "incorrect stake tx vout num" << " stake txid=" << staketx.GetHash().GetHex() << " inOpret=" << inOpret.ToString() << std::endl);
    return MARMARA_NOT_STAKE_TX;
}

#define MAKE_ACTIVATED_WALLET_DATA(key, pk, addr, segid, amount) std::make_tuple(key, pk, addr, segid, amount)

#define ACTIVATED_WALLET_DATA_KEY(d) std::get<0>(d)
#define ACTIVATED_WALLET_DATA_PK(d) std::get<1>(d)
#define ACTIVATED_WALLET_DATA_ADDR(d) std::get<2>(d)
#define ACTIVATED_WALLET_DATA_SEGID(d) std::get<3>(d)
#define ACTIVATED_WALLET_DATA_AMOUNT(d) std::get<4>(d)

typedef std::tuple<CKey, CPubKey, std::string, uint32_t, CAmount> tACTIVATED_WALLET_DATA;
typedef std::vector<tACTIVATED_WALLET_DATA> vACTIVATED_WALLET_DATA;

// enum activated 1of2 addr in the wallet:
static void EnumWalletActivatedAddresses(CWallet *pwalletMain, vACTIVATED_WALLET_DATA &activated)
{
    struct CCcontract_info *cp, C;
    cp = CCinit(&C, EVAL_MARMARA);
    CPubKey marmarapk = GetUnspendable(cp, 0);

    std::set<CKeyID> setKeyIds;
    pwalletMain->GetKeys(setKeyIds);
    for (auto keyid : setKeyIds)
    {
        //std::cerr << "key=" << keyid.ToString()  << std::endl;
        CPubKey pk;
        if (pwalletMain->GetPubKey(keyid, pk))
        {
            CKey key;
            pwalletMain->GetKey(keyid, key);

            CMutableTransaction mtx;
            std::vector<CPubKey> pubkeys;
            char activated1of2addr[KOMODO_ADDRESS_BUFSIZE];
            GetCCaddress1of2(cp, activated1of2addr, marmarapk, pk);
            CAmount amount = AddMarmaraCCInputs(IsMarmaraActivatedVout, mtx, pubkeys, activated1of2addr, 0, CC_MAXVINS);
            if (amount > 0)
            {
                uint32_t segid = komodo_segid32(activated1of2addr) & 0x3f;
                tACTIVATED_WALLET_DATA tuple = MAKE_ACTIVATED_WALLET_DATA(key, pk, std::string(activated1of2addr), segid, amount);
                activated.push_back(tuple);
            }
            memset(&key, '\0', sizeof(key));
        }
        else
            LOGSTREAMFN("marmara", CCLOG_ERROR, stream << "can't get pubkey from the wallet for keyid=" << keyid.ToString() << std::endl);
    }
}

// enumerates activated cc vouts in the wallet or on mypk if wallet is not available
// calls a callback allowing to do something with the utxos (add to staking utxo array)
// TODO: maybe better to use AddMarmaraCCInputs with a callback for unification...
template <class T>
static void EnumMyActivated(T func)
{
    std::vector<std::string> activatedAddresses;
    if (pwalletMain)
    {
        const CKeyStore& keystore = *pwalletMain;
        LOCK2(cs_main, pwalletMain->cs_wallet);
        vACTIVATED_WALLET_DATA activated;
        EnumWalletActivatedAddresses(pwalletMain, activated);
        for (auto a : activated)
            activatedAddresses.push_back(ACTIVATED_WALLET_DATA_ADDR(a));
    }
    else
    {
        // should not be here as it can't be PoS without a wallet
        LOGSTREAMFN("marmara", CCLOG_ERROR, stream << "wallet not available" << std::endl);
    }

    for (auto addr : activatedAddresses)
    {
        // add activated coins:
        std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> > activatedOutputs;
        SetCCunspents(activatedOutputs, (char*)addr.c_str(), true);

        // add my activated coins:
        LOGSTREAMFN("marmara", CCLOG_DEBUG3, stream << "checking activatedaddr=" << addr << std::endl);
        for (std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> >::const_iterator it = activatedOutputs.begin(); it != activatedOutputs.end(); it++)
        {
            CTransaction tx; uint256 hashBlock;
            CBlockIndex *pindex;

            uint256 txid = it->first.txhash;
            int32_t nvout = (int32_t)it->first.index;
            CAmount nValue;

            if ((nValue = it->second.satoshis) < COIN)   // skip small values
                continue;

            LOGSTREAMFN("marmara", CCLOG_DEBUG3, stream << "check tx on activatedaddr with txid=" << txid.GetHex() << " vout=" << nvout << std::endl);

            if (myGetTransaction(txid, tx, hashBlock) && (pindex = komodo_getblockindex(hashBlock)) != 0 && myIsutxo_spentinmempool(ignoretxid, ignorevin, txid, nvout) == 0)
            {
                char utxoaddr[KOMODO_ADDRESS_BUFSIZE] = "";

                Getscriptaddress(utxoaddr, tx.vout[nvout].scriptPubKey);
                if (strcmp(addr.c_str(), utxoaddr) == 0)  // check if actual vout address matches the address in the index
                                                          // because a key from vSolution[1] could appear in the addressindex and it does not match the address.
                                                          // This is fixed in this marmara branch but this fix is for discussion
                {
                    CScript opret;
                    CPubKey senderpk;
                    CActivatedOpretChecker activatedChecker;

                    if (get_either_opret(&activatedChecker, tx, nvout, opret, senderpk))
                    {
                        // call callback function:
                        func(addr.c_str(), tx, nvout, pindex);
                        LOGSTREAMFN("marmara", CCLOG_DEBUG3, stream << "found my activated 1of2 addr txid=" << txid.GetHex() << " vout=" << nvout << std::endl);
                    }
                    else
                        LOGSTREAMFN("marmara", CCLOG_ERROR, stream << "skipped activated 1of2 addr txid=" << txid.GetHex() << " vout=" << nvout << " cant decode opret or not mypk" << std::endl);
                }
                else
                    LOGSTREAMFN("marmara", CCLOG_ERROR, stream << "skipped activated 1of2 addr txid=" << txid.GetHex() << " vout=" << nvout << " uxto addr not matched index" << std::endl);
            }
        }
    }
}

// enumerates mypk locked in loop cc vouts
// calls a callback allowing to do something with the utxos (add to staking utxo array)
// TODO: maybe better to use AddMarmaraCCInputs with a callback for unification...
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
    LOGSTREAMFN("marmara", CCLOG_DEBUG3, stream  << "checking markeraddr=" << markeraddr << std::endl);
    for (std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> >::const_iterator it = markerOutputs.begin(); it != markerOutputs.end(); it++)
    {
        CTransaction isssuancetx;
        uint256 hashBlock;
        uint256 txid = it->first.txhash;
        int32_t nvout = (int32_t)it->first.index;

        LOGSTREAMFN("marmara", CCLOG_DEBUG3, stream  << "checking tx on markeraddr txid=" << txid.GetHex() << " vout=" << nvout << std::endl);
        if (nvout == MARMARA_MARKER_VOUT)
        {
            if (myGetTransaction(txid, isssuancetx, hashBlock) && !hashBlock.IsNull())
            {
                if (!isssuancetx.IsCoinBase() && isssuancetx.vout.size() > 2 && isssuancetx.vout.back().nValue == 0)
                {
                    struct CreditLoopOpret loopData;
                    // get createtxid from the issuance tx
                    if (MarmaraDecodeLoopOpret(isssuancetx.vout.back().scriptPubKey, loopData) == 'I')
                    {
                        char loopaddr[KOMODO_ADDRESS_BUFSIZE];
                        std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> > loopOutputs;
                        CPubKey createtxidPk = CCtxidaddr_tweak(NULL, loopData.createtxid);

                        // enum unspents in the loop
                        GetCCaddress1of2(cp, loopaddr, Marmarapk, createtxidPk);
                        SetCCunspents(loopOutputs, loopaddr, true);

                        // enum all locked-in-loop addresses:
                        LOGSTREAMFN("marmara", CCLOG_DEBUG3, stream << "checking on loopaddr=" << loopaddr << std::endl);
                        for (std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> >::const_iterator it = loopOutputs.begin(); it != loopOutputs.end(); it++)
                        {
                            CTransaction looptx;
                            uint256 hashBlock;
                            CBlockIndex *pindex;
                            uint256 txid = it->first.txhash;
                            int32_t nvout = (int32_t)it->first.index;

                            LOGSTREAMFN("marmara", CCLOG_DEBUG3, stream << "checking tx on loopaddr txid=" << txid.GetHex() << " vout=" << nvout << std::endl);

                            if (myGetTransaction(txid, looptx, hashBlock) && (pindex = komodo_getblockindex(hashBlock)) != 0 && myIsutxo_spentinmempool(ignoretxid, ignorevin, txid, nvout) == 0)  
                            {
                                /* lock-in-loop cant be mined */                   /* now it could be cc opret, not necessary OP_RETURN vout in the back */
                                if (!looptx.IsCoinBase() && looptx.vout.size() > 0 /* && looptx.vout.back().nValue == 0 */)
                                {
                                    char utxoaddr[KOMODO_ADDRESS_BUFSIZE] = "";

                                    Getscriptaddress(utxoaddr, looptx.vout[nvout].scriptPubKey);

                                    // NOTE: This is checking if the real spk address matches the index address 
                                    // because other keys from the vout.spk could be used in the addressindex)
                                    // spk structure (keys): hashed-cc, pubkey, ccopret
                                    // For the marmara branch I disabled getting other keys except the first in ExtractDestination but this is debatable
                                    if (strcmp(loopaddr, utxoaddr) == 0)  
                                    {
                                        CScript opret;
                                        CPubKey pk_in_opret;

                                        // get pk from cc opret or last vout opret
                                        // use pk only from cc opret (which marks vout with owner), do not use the last vout opret if no cc opret somehow
                                        CLockInLoopOpretChecker lockinloopChecker(CHECK_ONLY_CCOPRET);
                                        if (get_either_opret(&lockinloopChecker, looptx, nvout, opret, pk_in_opret))
                                        {
                                            if (mypk == pk_in_opret)   // check mypk in opret
                                            {
                                                // call callback func:
                                                func(loopaddr, looptx, nvout, pindex);
                                                LOGSTREAMFN("marmara", CCLOG_DEBUG3, stream << "found my lock-in-loop 1of2 addr txid=" << txid.GetHex() << " vout=" << nvout << std::endl);
                                            }
                                            else
                                                LOGSTREAMFN("marmara", CCLOG_DEBUG2, stream << "skipped lock-in-loop 1of2 addr txid=" << txid.GetHex() << " vout=" << nvout << " not mypk" << std::endl);
                                        }
                                        else
                                            LOGSTREAMFN("marmara", CCLOG_ERROR, stream << "skipped lock-in-loop 1of2 addr txid=" << txid.GetHex() << " vout=" << nvout << " cant decode opret" << std::endl);
                                    }
                                    else
                                        LOGSTREAMFN("marmara", CCLOG_ERROR, stream << "skipped lock-in-loop 1of2 addr txid=" << txid.GetHex() << " vout=" << nvout << " uxto addr not matched index" << std::endl);
                                }
                            }
                        }
                    }
                }
            }
            else
                LOGSTREAMFN("marmara", CCLOG_ERROR, stream << "error getting issuance tx=" << txid.GetHex() << ", is in mempool=" << hashBlock.IsNull() << std::endl);
        }
    }
}


// add marmara special UTXO from activated and lock-in-loop addresses for staking
// called from PoS code
struct komodo_staking *MarmaraGetStakingUtxos(struct komodo_staking *array, int32_t *numkp, int32_t *maxkp, uint8_t *hashbuf)
{
    const char *logFName = __func__;

    // add activated utxos for mypk:
    //std::cerr  << " entered" << std::endl;
    EnumMyActivated(
        [&](const char *activatedaddr, const CTransaction & tx, int32_t nvout, CBlockIndex *pindex) 
        {
            array = komodo_addutxo(array, numkp, maxkp, (uint32_t)pindex->nTime, (uint64_t)tx.vout[nvout].nValue, tx.GetHash(), nvout, (char*)activatedaddr, hashbuf, tx.vout[nvout].scriptPubKey);
            LOGSTREAM("marmara", CCLOG_DEBUG1, stream << logFName << " " << "added uxto for staking activated 1of2 addr txid=" << tx.GetHash().GetHex() << " vout=" << nvout << std::endl);
        }
    );

    // add lock-in-loops utxos for mypk:
    EnumMyLockedInLoop(
        [&](const char *loopaddr, const CTransaction & tx, int32_t nvout, CBlockIndex *pindex)
        {
            array = komodo_addutxo(array, numkp, maxkp, (uint32_t)pindex->nTime, (uint64_t)tx.vout[nvout].nValue, tx.GetHash(), nvout, (char*)loopaddr, hashbuf, tx.vout[nvout].scriptPubKey);
            LOGSTREAM("marmara", CCLOG_DEBUG1, stream << logFName << " " << "added uxto for staking lock-in-loop 1of2addr txid=" << tx.GetHash().GetHex() << " vout=" << nvout << std::endl);
        }
    );
   
    return array;
}

// returns stake preferences for activated and locked utxos
int32_t MarmaraGetStakeMultiplier(const CTransaction & tx, int32_t nvout)
{
    CScript opret;
    CPubKey mypk = pubkey2pk(Mypubkey());
    CPubKey opretpk;
    CActivatedOpretChecker activatedChecker;
    CLockInLoopOpretChecker lockinloopChecker;

    if (nvout >= 0 && nvout < tx.vout.size()) // check boundary
    {
        LOGSTREAMFN("marmara", CCLOG_DEBUG1, stream << "check txid=" << tx.GetHash().GetHex() << std::endl);

        if (get_either_opret(&lockinloopChecker, tx, nvout, opret, opretpk) /*&& mypk == opretpk - not for validation */)   // check if opret is lock-in-loop and cc vout is mypk
        {
            LOGSTREAMFN("marmara", CCLOG_DEBUG1, stream << "check lock in loop opret okay, pk=" << HexStr(opretpk) << std::endl);
            if (tx.vout[nvout].scriptPubKey.IsPayToCryptoCondition()) 
            {
                LOGSTREAMFN("marmara", CCLOG_DEBUG1, stream << "check iscc  okay" << std::endl);

                struct CreditLoopOpret loopData;
                if (MarmaraDecodeLoopOpret(opret, loopData) != 0)     
                {
                    LOGSTREAMFN("marmara", CCLOG_DEBUG1, stream << "decode loop opret okay" << std::endl);

                    struct CCcontract_info *cp, C;
                    cp = CCinit(&C, EVAL_MARMARA);
                    CPubKey Marmarapk = GetUnspendable(cp, NULL);

                    // get LCL address
                    char lockInLoop1of2addr[KOMODO_ADDRESS_BUFSIZE];
                    CPubKey createtxidPk = CCtxidaddr_tweak(NULL, loopData.createtxid);
                    GetCCaddress1of2(cp, lockInLoop1of2addr, Marmarapk, createtxidPk);

                    // get vout address
                    char ccvoutaddr[KOMODO_ADDRESS_BUFSIZE];
                    Getscriptaddress(ccvoutaddr, tx.vout[nvout].scriptPubKey);

                    LOGSTREAMFN("marmara", CCLOG_DEBUG1, stream << "ccvoutaddr=" << ccvoutaddr << " lockInLoop1of2addr=" << lockInLoop1of2addr << std::endl);

                    if (strcmp(lockInLoop1of2addr, ccvoutaddr) == 0)  // check vout address is lock-in-loop address
                    {
                        LOGSTREAMFN("marmara", CCLOG_DEBUG2, stream  << "utxo picked for stake x100 as lock-in-loop" << " txid=" << tx.GetHash().GetHex() << " nvout=" << nvout << std::endl);
                        return 3; // 3x multiplier for lock-in-loop
                    }
                }
            }
        }
        else if (get_either_opret(&activatedChecker, tx, nvout, opret, opretpk))   // check if this is activated opret 
        {
            if (tx.vout[nvout].scriptPubKey.IsPayToCryptoCondition())    
            {    
                struct CCcontract_info *cp, C;
                cp = CCinit(&C, EVAL_MARMARA);
                CPubKey Marmarapk = GetUnspendable(cp, NULL);

                char activated1of2addr[KOMODO_ADDRESS_BUFSIZE];
                char ccvoutaddr[KOMODO_ADDRESS_BUFSIZE];
                GetCCaddress1of2(cp, activated1of2addr, Marmarapk, opretpk/* mypk*/);
                Getscriptaddress(ccvoutaddr, tx.vout[nvout].scriptPubKey);

                LOGSTREAMFN("marmara", CCLOG_DEBUG1, stream << "ccvoutaddr=" << ccvoutaddr << " activated1of2addr=" << activated1of2addr << std::endl);

                if (strcmp(activated1of2addr, ccvoutaddr) == 0)   // check vout address is my activated address
                {
                    LOGSTREAMFN("marmara", CCLOG_DEBUG2, stream  << "utxo picked for stake x1 as activated" << " txid=" << tx.GetHash().GetHex() << " nvout=" << nvout << std::endl);
                    return 1;  // 1x multiplier for activated
                }
            }
        }
    }

    LOGSTREAMFN("marmara", CCLOG_DEBUG1, stream << "utxo not recognized for stake" << " txid=" << tx.GetHash().GetHex() << " nvout=" << nvout << std::endl);
    return 1;  //default multiplier 1x
}



// make activated by locking the amount on the max block height
UniValue MarmaraLock(int64_t txfee, int64_t amount)
{
    CMutableTransaction tmpmtx, mtx = CreateNewContextualCMutableTransaction(Params().GetConsensus(), komodo_nextheight());
    UniValue result(UniValue::VOBJ);
    struct CCcontract_info *cp, C;
    CPubKey Marmarapk, mypk, pk;
    //int32_t unlockht, /*refunlockht,*/ nvout, ht, numvouts;
    int64_t nValue, val, inputsum = 0, remains, change = 0;
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
    {
        inputsum = AddNormalinputs2(mtx, val + txfee, MARMARA_VINS);  //added '+txfee' because if 'inputsum' exactly was equal to 'val' we'd exit from insufficient funds 
        /* do not need this as threshold removed from Addnormalinputs
        if (inputsum < val + txfee) {
            // if added inputs are insufficient
            // try to add value and txfee separately: 
            mtx.vin.clear();
            inputsum = AddNormalinputs2(mtx, val, CC_MAXVINS / 2);
            inputsum += AddNormalinputs2(mtx, txfee, 5);
        }*/
    }
    //fprintf(stderr,"%s added normal inputs=%.8f required val+txfee=%.8f\n", logFuncName, (double)inputsum/COIN,(double)(val+txfee)/COIN);

    CScript opret = MarmaraCoinbaseOpret('A', height, mypk);
    // lock the amount on 1of2 address:
    mtx.vout.push_back(MakeMarmaraCC1of2voutOpret(amount, mypk, opret)); //add coinbase opret

    if (inputsum < amount + txfee)  // if not enough normal inputs for collateral
    {
        //refunlockht = MarmaraUnlockht(height);  // randomized 

        result.push_back(Pair("normalfunds", ValueFromAmount(inputsum)));
        result.push_back(Pair("height", static_cast<int64_t>(height)));
        //result.push_back(Pair("unlockht", refunlockht));

        // fund remainder to add:
        remains = (amount + txfee) - inputsum;

        std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> > unspentOutputs;
        GetCCaddress1of2(cp, activated1of2addr, Marmarapk, mypk);
        SetCCunspents(unspentOutputs, activated1of2addr, true);
        //threshold = remains / (MARMARA_VINS + 1);
        uint8_t mypriv[32];
        Myprivkey(mypriv);
        CCaddr1of2set(cp, Marmarapk, mypk, mypriv, activated1of2addr);
        memset(mypriv,0,sizeof(mypriv));
    }
    if (inputsum >= amount + txfee)
    {
        if (inputsum > amount + txfee)
        {
            change = (inputsum - amount);
            mtx.vout.push_back(CTxOut(change, CScript() << ParseHex(HexStr(mypk)) << OP_CHECKSIG));
        }
        rawtx = FinalizeCCTx(0, cp, mtx, mypk, txfee, CScript()/*opret moved to cc vout*/);
        if (rawtx.size() == 0)
        {
            errorstr = "couldnt finalize CCtx";
        }
        else
        {
            result.push_back(Pair("result", "success"));
            result.push_back(Pair("hex", rawtx));
            return(result);
        }
    }
    else
        errorstr = (char *)"insufficient funds";
    result.push_back(Pair("result", "error"));
    result.push_back(Pair("error", errorstr));
    return(result);
}

// add stake tx opret, finalize and sign stake tx on activated or lock-in-loop 1of2 addr
// (note: utxosig bufsize = 512 is checked)
int32_t MarmaraSignature(uint8_t *utxosig, CMutableTransaction &mtx)
{
    uint256 txid, hashBlock; 
    CTransaction vintx; 
    int64_t txfee = 10000;

    // compatibility rules:

    // for marmara testers chain 
    bool lastVoutOpretDiscontinued = true;
    if (strcmp(ASSETCHAINS_SYMBOL, "MCL0") == 0)
    {
        CBlockIndex *tipindex = chainActive.Tip();
        if (tipindex)
        {
            if (tipindex->GetHeight() + 1 < 2000)
            {
                lastVoutOpretDiscontinued = false;
            }
        }
    }
    // end of compatibility rules

    if (myGetTransaction(mtx.vin[0].prevout.hash, vintx, hashBlock) && vintx.vout.size() > 0 /*was >1, but if ccopret could be only 1 vout*/ && mtx.vin[0].prevout.n < vintx.vout.size())
    {
        CScript finalOpret, vintxOpret;
        struct CCcontract_info *cp, C;
        cp = CCinit(&C, EVAL_MARMARA);
        uint8_t marmarapriv[32];
        CPubKey Marmarapk = GetUnspendable(cp, marmarapriv);

        CPubKey mypk = pubkey2pk(Mypubkey());  // for txfee only
        CPubKey opretpk;
        CActivatedOpretChecker activatedChecker;
        CLockInLoopOpretChecker lockinloopChecker;

        if (get_either_opret(&activatedChecker, vintx, mtx.vin[0].prevout.n, vintxOpret, opretpk))  // note: opret should be ONLY in vintx ccvout
        {
            // sign activated staked utxo

            // decode utxo 1of2 address
            char activated1of2addr[KOMODO_ADDRESS_BUFSIZE];
            CKeyID keyid = opretpk.GetID();
            CKey privkey;

            if (!pwalletMain || !pwalletMain->GetKey(keyid, privkey))
            {
                LOGSTREAMFN("marmara", CCLOG_ERROR, stream << "can't find user privkey or wallet not available" << std::endl);
                return 0;
            }

            //LOGSTREAMFN("marmara", CCLOG_DEBUG1, stream << "vintx opretpk=" << HexStr(opretpk) << " keyId=" << keyid.GetHex() << " privkey=" << HexStr(privkey) << std::endl);
            //CPubKey pk0 = pubkey2pk(ParseHex("03f8c7b24729101443500bcb26171a65ab070e1b424bfd8c1830b0ba42d9491703"));
            //LOGSTREAMFN("marmara", CCLOG_DEBUG1, stream << "for 03f8 pk keyId=" << pk0.GetID().GetHex() << std::endl);

            // this is for transition period to cc-vout opret in stake txns
            // if vintx has the last-vout opret then move it to cc-vout opret
            // check if cc vout opret exists in mtx
            CScript opret;
            bool hasccopret = false;
            if (GetCCOpReturnData(mtx.vout[0].scriptPubKey, opret))
            {
                LOGSTREAMFN("marmara", CCLOG_DEBUG2, stream << "compatibility code: in mtx found ccopret=" << opret.ToString() << std::endl);
                if (activatedChecker.CheckOpret(opret, opretpk))
                {
                    hasccopret = true;
                }
            }
            // if mtx does not have cc opret then add it
            if (!hasccopret && lastVoutOpretDiscontinued)
            {
                // add cc opret to stake tx:
                LOGSTREAMFN("marmara", CCLOG_DEBUG2, stream << "compatibility code added ccopret to mtx" << std::endl);
                mtx.vout[0] = MakeMarmaraCC1of2voutOpret(mtx.vout[0].nValue, opretpk, vintxOpret);
            }

            Getscriptaddress(activated1of2addr, mtx.vout[0].scriptPubKey);

            LOGSTREAMFN("marmara", CCLOG_DEBUG1, stream << "found activated opret in staking vintx" << std::endl);

            CC *probeCond = MakeCCcond1of2(EVAL_MARMARA, Marmarapk, opretpk);
            // use privkey for user's pubkey from the wallet instead of the global pk:
            CCAddVintxCond(cp, probeCond, /*marmarapriv*/privkey.begin());    //add probe condition to sign vintx 1of2 utxo
            cc_free(probeCond);

            if (lastVoutOpretDiscontinued)
                finalOpret = CScript();  //empty for activated
            else
                finalOpret = vintxOpret; // last-vout opret continues to be used until some height

            memset(&privkey, '\0', sizeof(privkey));

        }
        else if (get_either_opret(&lockinloopChecker, vintx, mtx.vin[0].prevout.n, vintxOpret, opretpk))   // note: opret could be in vintx ccvout
        {
            // sign lock-in-loop utxo

            struct CreditLoopOpret loopData;
            MarmaraDecodeLoopOpret(vintxOpret, loopData);

            CPubKey createtxidPk = CCtxidaddr_tweak(NULL, loopData.createtxid);

            LOGSTREAMFN("marmara", CCLOG_DEBUG1, stream << "found locked-in-loop opret in staking vintx" << std::endl);

            CC *probeCond = MakeCCcond1of2(EVAL_MARMARA, Marmarapk, createtxidPk);
            CCAddVintxCond(cp, probeCond, marmarapriv); //add probe condition to sign vintx 1of2 utxo
            cc_free(probeCond);

            if (lastVoutOpretDiscontinued)
                finalOpret = CScript();  // empty last vout opret
            else
                finalOpret = vintxOpret; // last-vout opret continues to be used until some height
        }

        // note: opreturn for stake tx is taken from the staking utxo (ccvout or back):
        std::string rawtx = FinalizeCCTx(0, cp, mtx, mypk, txfee, finalOpret);  // opret for LCL or empty for activated
        if (rawtx.size() > 0)
        {
            int32_t siglen = mtx.vin[0].scriptSig.size();
            uint8_t *scriptptr = &mtx.vin[0].scriptSig[0];

            if (siglen > 512) {   // check sig buffer limit
                LOGSTREAMFN("marmara", CCLOG_ERROR, stream << "scriptSig length is more than utxosig bufsize, truncated! siglen=" << siglen << std::endl);
                siglen = 512;
            }

            std::ostringstream debstream;
            for (int32_t i = 0; i < siglen; i++)   {
                utxosig[i] = scriptptr[i];
                debstream << std::hex << (int)scriptptr[i];
            }
            LOGSTREAMFN("marmara", CCLOG_DEBUG2, stream << "scriptSig=" << debstream.str() << " got signed rawtx=" << rawtx << " siglen=" << siglen << std::endl);
            return(siglen);
        }
        else
            LOGSTREAMFN("marmara", CCLOG_ERROR, stream << "cannot sign marmara staked tx, bad mtx=" << HexStr(E_MARSHAL(ss << mtx)) << " opretpk=" << HexStr(opretpk) << std::endl);
    }
    else 
        LOGSTREAMFN("marmara", CCLOG_ERROR, stream << "cannot get vintx for staked tx" << std::endl);
    return(0);
}

// jl777: decide on what unlockht settlement change should have -> from utxo making change

UniValue MarmaraSettlement(int64_t txfee, uint256 refbatontxid, CTransaction &settlementTx)
{
    UniValue result(UniValue::VOBJ);
    std::vector<uint256> creditloop;
    uint256 batontxid;
    int32_t numerrs = 0, numDebtors;
    int64_t inputsum;
    std::string rawtx;
    char loop1of2addr[KOMODO_ADDRESS_BUFSIZE], myCCaddr[KOMODO_ADDRESS_BUFSIZE], destaddr[KOMODO_ADDRESS_BUFSIZE], batonCCaddr[KOMODO_ADDRESS_BUFSIZE];
    struct CCcontract_info *cp, C;

    if (txfee == 0)
        txfee = 10000;

    cp = CCinit(&C, EVAL_MARMARA);
    CPubKey minerpk = pubkey2pk(Mypubkey());
    uint8_t marmarapriv[32];
    CPubKey Marmarapk = GetUnspendable(cp, marmarapriv);
    
    int64_t change = 0;
    int32_t height = chainActive.LastTip()->GetHeight();
    if ((numDebtors = MarmaraGetbatontxid(creditloop, batontxid, refbatontxid)) > 0)
    {
        CTransaction batontx;
        uint256 hashBlock;
        struct CreditLoopOpret loopData;

        if( get_loop_creation_data(creditloop[0], loopData) == 0 )
        {
            if (myGetTransaction(batontxid, batontx, hashBlock) && !hashBlock.IsNull() && batontx.vout.size() > 1)
            {
                uint8_t funcid;

                if ((funcid = MarmaraDecodeLoopOpret(batontx.vout.back().scriptPubKey, loopData)) != 0) // update loopData with the baton opret
                {
                    CMutableTransaction mtx = CreateNewContextualCMutableTransaction(Params().GetConsensus(), komodo_nextheight());

                    if (loopData.createtxid != creditloop[0])
                    {
                        result.push_back(Pair("result", "error"));
                        result.push_back(Pair("error", "invalid opret createtxid, should be set to creditloop[0]")); //TODO: note change
                        return(result);
                    }
                    else if (chainActive.LastTip()->GetHeight() < loopData.matures)
                    {
                        LOGSTREAMFN("marmara", CCLOG_INFO, stream << "loop doesnt mature for another " << loopData.matures - chainActive.LastTip()->GetHeight() << " blocks" << std::endl);
                        result.push_back(Pair("result", "error"));
                        result.push_back(Pair("error", "cant settle immature creditloop"));
                        return(result);
                    }
                    else if ((loopData.matures & 1) == 0)
                    {
                        // discontinued:
                        //result.push_back(Pair("result", "error"));
                        //result.push_back(Pair("error", "cant automatic settle even maturity heights"));
                        //return(result);
                    }
                    else if (numDebtors < 1)
                    {
                        result.push_back(Pair("result", "error"));
                        result.push_back(Pair("error", "creditloop too short"));
                        return(result);
                    }
                    GetCCaddress(cp, myCCaddr, Mypubkey());
                    Getscriptaddress(batonCCaddr, batontx.vout[0].scriptPubKey);

                    // allow any miner to settle, do not check mypk:
                    //if (strcmp(myCCaddr, batonCCaddr) == 0) // if mypk user owns the baton
                    //{
                    std::vector<CPubKey> pubkeys;
                    uint256 issuetxid;

                    // note: can't spend the baton any more as settlement could be done by any miner
                    // spend the marker on marmara global pk
                    if (numDebtors > 1)
                        issuetxid = creditloop[1];
                    else
                        issuetxid = batontxid;
                    mtx.vin.push_back(CTxIn(issuetxid, MARMARA_OPENCLOSE_VOUT, CScript())); // spend vout2 marker - close the loop

                    // add tx fee from mypubkey
                    if (AddNormalinputs2(mtx, txfee, 4) < txfee) {  // TODO: in the previous code txfee was taken from 1of2 address
                        result.push_back(Pair("result", "error"));
                        result.push_back(Pair("error", "cant add normal inputs for txfee"));
                        return(result);
                    }

                    char lockInLoop1of2addr[KOMODO_ADDRESS_BUFSIZE];
                    CPubKey createtxidPk = CCtxidaddr_tweak(NULL, loopData.createtxid);
                    GetCCaddress1of2(cp, lockInLoop1of2addr, Marmarapk, createtxidPk);  // 1of2 lock-in-loop address

                    CC *lockInLoop1of2cond = MakeCCcond1of2(EVAL_MARMARA, Marmarapk, createtxidPk);
                    CCAddVintxCond(cp, lockInLoop1of2cond, marmarapriv); //add probe condition to spend from the lock-in-loop address
                    cc_free(lockInLoop1of2cond);

                    LOGSTREAMFN("marmara", CCLOG_DEBUG1, stream << "calling AddMarmaraCCInputs for lock-in-loop addr=" << lockInLoop1of2addr << " adding amount=" << loopData.amount << std::endl);
                    if ((inputsum = AddMarmaraCCInputs(IsMarmaraLockedInLoopVout, mtx, pubkeys, lockInLoop1of2addr, loopData.amount, MARMARA_VINS)) >= loopData.amount)
                    {
                        change = (inputsum - loopData.amount);
                        mtx.vout.push_back(CTxOut(loopData.amount, CScript() << ParseHex(HexStr(loopData.pk)) << OP_CHECKSIG));   // locked-in-loop money is released to mypk doing the settlement
                        if (change > txfee) {
                            LOGSTREAMFN("marmara", CCLOG_ERROR, stream << "error: change not null=" << change << ", sent back to lock-in-loop addr=" << lockInLoop1of2addr << std::endl);
                            mtx.vout.push_back(MakeCC1of2vout(EVAL_MARMARA, change, Marmarapk, createtxidPk));
                        }
                        rawtx = FinalizeCCTx(0, cp, mtx, minerpk, txfee, MarmaraEncodeLoopSettlementOpret(true, loopData.createtxid, loopData.pk, 0));
                        if (rawtx.empty()) {
                            result.push_back(Pair("result", "error"));
                            result.push_back(Pair("error", "could not finalize CC Tx"));
                            LOGSTREAMFN("marmara", CCLOG_ERROR, stream << "bad settlement mtx=" << HexStr(E_MARSHAL(ss << mtx)) << std::endl);
                        }
                        else {
                            result.push_back(Pair("result", "success"));
                            result.push_back(Pair("hex", rawtx));
                            settlementTx = mtx;
                        }
                        return(result);
                    }

                    if (inputsum < loopData.amount)
                    {
                        int64_t remaining = loopData.amount - inputsum;
                        mtx.vout.push_back(CTxOut(txfee, CScript() << ParseHex(HexStr(CCtxidaddr_tweak(NULL, loopData.createtxid))) << OP_CHECKSIG)); // failure marker

                        // TODO: seems this was supposed that txfee should been taken from 1of2 address?
                        //if (refamount - remaining > 3 * txfee)
                        //    mtx.vout.push_back(CTxOut(refamount - remaining - 2 * txfee, CScript() << ParseHex(HexStr(mypk)) << OP_CHECKSIG));
                        mtx.vout.push_back(CTxOut(loopData.amount - remaining - txfee, CScript() << ParseHex(HexStr(loopData.pk)) << OP_CHECKSIG));

                        rawtx = FinalizeCCTx(0, cp, mtx, minerpk, txfee, MarmaraEncodeLoopSettlementOpret(false, loopData.createtxid, loopData.pk, -remaining));  //some remainder left
                        if (rawtx.empty()) {
                            result.push_back(Pair("result", "error"));
                            result.push_back(Pair("error", "couldnt finalize CCtx"));
                            LOGSTREAMFN("marmara", CCLOG_DEBUG1, stream << "FinalizeCCTx bad mtx=" << HexStr(E_MARSHAL(ss << mtx)) << std::endl);
                        }
                        else {
                            result.push_back(Pair("result", "error"));
                            result.push_back(Pair("error", "insufficient funds"));
                            result.push_back(Pair("hex", rawtx));
                            result.push_back(Pair("remaining", ValueFromAmount(remaining)));
                        }
                    }
                    else
                    {
                        // jl777: maybe fund a txfee to report no funds avail
                        result.push_back(Pair("result", "error"));
                        result.push_back(Pair("error", "no funds available at all"));
                    }
                    //}
                    /*else
                    {
                        result.push_back(Pair("result", "error"));
                        result.push_back(Pair("error", "this node does not have the baton"));
                        result.push_back(Pair("myCCaddr", myCCaddr));
                        result.push_back(Pair("batonCCaddr", batonCCaddr));
                    }*/
                }
                else
                {
                    result.push_back(Pair("result", "error"));
                    result.push_back(Pair("error", "couldnt get batontxid opret"));
                }
            }
            else
            {
                result.push_back(Pair("result", "error"));
                result.push_back(Pair("error", "couldnt find batontxid"));
            }
        }
        else
        {
            result.push_back(Pair("result", "error"));
            result.push_back(Pair("error", "couldnt get credit loop creation data"));
        }
    }
    else
    {
        result.push_back(Pair("result", "error"));
        result.push_back(Pair("error", "couldnt get creditloop for the baton"));
    }
    return(result);
}

// enums credit loops (for the pk or all if null pk passed)
// returns pending and closed txids (only for MARMARA_MARKER_VOUT)
// for pending loops calls 'callback' with params batontxid and mature height (or -1 if the loop is closed)
template <class T>
static int32_t enum_credit_loops(int32_t nVoutMarker, int64_t &totalopen, std::vector<uint256> &issuances, int64_t &totalclosed, std::vector<uint256> &closed, struct CCcontract_info *cp, int32_t firstheight, int32_t lastheight, int64_t minamount, int64_t maxamount, CPubKey refpk, std::string refcurrency, T callback/*void (*callback)(uint256 batontxid, int32_t matures)*/)
{
    char marmaraaddr[KOMODO_ADDRESS_BUFSIZE]; 
    int32_t n = 0; 
    std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> > unspentOutputs;
    CPubKey Marmarapk = GetUnspendable(cp, 0);
    GetCCaddress(cp, marmaraaddr, Marmarapk);
    SetCCunspents(unspentOutputs, marmaraaddr, true);

    // do all txid, conditional on spent/unspent
    LOGSTREAMFN("marmara", CCLOG_DEBUG2, stream  << "check on marmara addr=" << marmaraaddr << std::endl);
    for (std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> >::const_iterator it = unspentOutputs.begin(); it != unspentOutputs.end(); it++)
    {
        CTransaction issuancetx;
        uint256 hashBlock;
        uint256 issuancetxid = it->first.txhash;
        int32_t vout = (int32_t)it->first.index;

        LOGSTREAMFN("marmara", CCLOG_DEBUG2, stream << "checking tx as marker on marmara addr txid=" << issuancetxid.GetHex() << " vout=" << vout << std::endl);
        // enum creditloop markers:
        if (vout == nVoutMarker)
        {
            if (myGetTransaction(issuancetxid, issuancetx, hashBlock) && !hashBlock.IsNull())  
            {
                if (!issuancetx.IsCoinBase() && issuancetx.vout.size() > 2 && issuancetx.vout.back().nValue == 0 /*has opreturn?*/)
                {
                    struct CreditLoopOpret loopData;
                    if (MarmaraDecodeLoopOpret(issuancetx.vout.back().scriptPubKey, loopData) == 'I')
                    {
                        if (get_loop_creation_data(loopData.createtxid, loopData) >= 0)
                        {
                            LOGSTREAMFN("marmara", CCLOG_DEBUG2, stream << "found issuance tx txid=" << issuancetxid.GetHex() << std::endl);
                            n++;
                            assert(!loopData.currency.empty());
                            assert(loopData.pk.size() != 0);
                            if (loopData.currency == refcurrency && loopData.matures >= firstheight && loopData.matures <= lastheight && loopData.amount >= minamount && loopData.amount <= maxamount && (refpk.size() == 0 || loopData.pk == refpk))
                            {
                                std::vector<uint256> creditloop;
                                uint256 settletxid, batontxid;
                                LOGSTREAMFN("marmara", CCLOG_DEBUG2, stream << "issuance tx is filtered, txid=" << issuancetxid.GetHex() << std::endl);

                                if (get_settlement_txid(settletxid, issuancetxid) == 0)
                                {
                                    CTransaction settletx;
                                    uint256 hashBlock;
                                    uint8_t funcid;

                                    LOGSTREAMFN("marmara", CCLOG_DEBUG2, stream << "found settle tx for issueancetxid=" << issuancetxid.GetHex() << std::endl);

                                    if (myGetTransaction(settletxid, settletx, hashBlock) && !hashBlock.IsNull() && settletx.vout.size() > 1 &&
                                        (funcid = MarmaraDecodeLoopOpret(settletx.vout.back().scriptPubKey, loopData)) != 0)
                                    {
                                        closed.push_back(issuancetxid);
                                        totalclosed += loopData.amount;
                                    }
                                    else 
                                        LOGSTREAMFN("marmara", CCLOG_INFO, stream << "could not get or decode settletx=" << settletxid.GetHex() << " (tx could be in mempool)" << std::endl);
                                }
                                else if (MarmaraGetbatontxid(creditloop, batontxid, issuancetxid) > 0)
                                {
                                    CTransaction batontx;
                                    uint256 hashBlock;
                                    uint8_t funcid;

                                    LOGSTREAMFN("marmara", CCLOG_DEBUG2, stream << "found baton tx for issueancetxid=" << issuancetxid.GetHex() << std::endl);

                                    if (myGetTransaction(batontxid, batontx, hashBlock) && !hashBlock.IsNull() && batontx.vout.size() > 1 &&
                                        (funcid = MarmaraDecodeLoopOpret(batontx.vout.back().scriptPubKey, loopData)) != 0)
                                    {
                                        issuances.push_back(issuancetxid);
                                        totalopen += loopData.amount;
                                        callback(batontxid, loopData.matures);
                                    }
                                    else
                                        LOGSTREAMFN("marmara", CCLOG_INFO, stream << "could not get or decode batontx=" << batontxid.GetHex() << " (baton could be in mempool)" << std::endl);
                                }
                                else
                                    LOGSTREAMFN("marmara", CCLOG_INFO, stream << "error finding baton for issuance txid=" << issuancetxid.GetHex() << " (tx could be in mempool)" << std::endl);
                            }
                        }
                        else
                            LOGSTREAMFN("marmara", CCLOG_ERROR, stream << "error load create tx for createtxid=" << loopData.createtxid.GetHex() << std::endl);
                    }
                    else
                        LOGSTREAMFN("marmara", CCLOG_ERROR, stream << "incorrect funcid for issuancetxid=" << issuancetxid.GetHex() << std::endl);
                }
            }
            else
                LOGSTREAMFN("marmara", CCLOG_ERROR, stream << "cant get tx on marmara marker addr (is in mempool=" << hashBlock.IsNull() << ") txid=" << issuancetxid.GetHex() << std::endl);
        }
    }
    return(n);
}

// adds to the passed vector the settlement transactions for all matured loops 
// called by the miner
// note that several or even all transactions might not fit into the current block, in this case they will be added on the next new block creation
// TODO: provide reserved space in the created block for at least some settlement transactions
void MarmaraRunAutoSettlement(int32_t height, std::vector<CTransaction> & settlementTransactions)
{
    int64_t totalopen, totalclosed;
    std::vector<uint256> issuances, closed;
    struct CCcontract_info *cp, C;
    cp = CCinit(&C, EVAL_MARMARA);
    std::string funcname = __func__;

    int32_t firstheight = 0, lastheight = (1 << 30);
    int64_t minamount = 0, maxamount = (1LL << 60);

    LOGSTREAMFN("marmara", CCLOG_DEBUG1, stream << "starting enum open batons" << std::endl);

    enum_credit_loops(MARMARA_OPENCLOSE_VOUT, totalopen, issuances, totalclosed, closed, cp, firstheight, lastheight, minamount, maxamount, CPubKey(), MARMARA_CURRENCY, [&](uint256 batontxid, int32_t matures) 
    {
        CTransaction settlementtx;
        //TODO: temp UniValue result legacy code, change to remove UniValue

        if (chainActive.LastTip()->GetHeight() >= matures)   //check height if matured 
        {
            LOGSTREAM("marmara", CCLOG_DEBUG1, stream << funcname << " " << "miner calling settlement for batontxid=" << batontxid.GetHex() << std::endl);

            UniValue result = MarmaraSettlement(0, batontxid, settlementtx);
            if (result["result"].getValStr() == "success") {
                LOGSTREAM("marmara", CCLOG_INFO, stream << funcname << " " << "miner trying to add to block settlement tx=" << settlementtx.GetHash().GetHex() <<  ", for batontxid=" << batontxid.GetHex() << std::endl);
                settlementTransactions.push_back(settlementtx);
            }
            else {
                LOGSTREAM("marmara", CCLOG_ERROR, stream << funcname << " " << "error=" << result["error"].getValStr() << " in settlement for batontxid=" << batontxid.GetHex() << std::endl);
            }
        }
    });
}

// create request tx for issuing or transfer baton (cheque) 
// the first call makes the credit loop creation tx
// txid of returned tx is requesttxid
UniValue MarmaraReceive(int64_t txfee, CPubKey senderpk, int64_t amount, std::string currency, int32_t matures, int32_t avalcount, uint256 batontxid, bool automaticflag)
{
    CMutableTransaction mtx = CreateNewContextualCMutableTransaction(Params().GetConsensus(), komodo_nextheight());
    UniValue result(UniValue::VOBJ); 
    struct CCcontract_info *cp, C; 
    int64_t requestFee; 
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

    if (batontxid == zeroid) 
    {
        // first time checking parameters
        if (currency != MARMARA_CURRENCY)
            errorstr = "for now, only MARMARA loops are supported";
        else if (amount <= txfee)
            errorstr = "amount must be for more than txfee";
        else if (matures <= chainActive.LastTip()->GetHeight())
            errorstr = "it must mature in the future";
        else if (mypk == senderpk)
            errorstr = "cannot request credit from self";
    }
    else
    {
        if (get_create_txid(createtxid, batontxid) < 0)
            errorstr = "cant get createtxid from batontxid";
    }

    if (createtxid != zeroid) 
    {
        // check original cheque params:
        CTransaction looptx;
        uint256 hashBlock;
        struct CreditLoopOpret loopData;

        if (get_loop_creation_data(createtxid, loopData) < 0)
            errorstr = "cannot get loop creation data";
        else if (!myGetTransaction(batontxid, looptx, hashBlock) ||
            hashBlock.IsNull() ||  // not in mempool
            looptx.vout.size() < 1 ||
            MarmaraDecodeLoopOpret(looptx.vout.back().scriptPubKey, loopData) == 0)
        {
            LOGSTREAMFN("marmara", CCLOG_INFO, stream << "cant get looptx.GetHash()=" << looptx.GetHash().GetHex() << " looptx.vout.size()=" << looptx.vout.size() << std::endl);
            errorstr = "cant load previous loop tx or tx in mempool or cant decode tx opreturn data";
        }
        else if (senderpk != loopData.pk)
            errorstr = "current baton holder does not match the requested sender pk";
        else if (loopData.matures <= chainActive.LastTip()->GetHeight())
            errorstr = "credit loop must mature in the future";

    }

    if (errorstr == NULL)
    {
        if (batontxid != zeroid)
            requestFee = txfee;
        else 
            requestFee = 2 * txfee;  // marker value 2 * txfee for easy identification
        if (AddNormalinputs(mtx, mypk, requestFee + txfee, 1) > 0)
        {
            mtx.vout.push_back(MakeCC1vout(EVAL_MARMARA, requestFee, senderpk));

            CScript opret;
            if (batontxid.IsNull())
                opret = MarmaraEncodeLoopCreateOpret(senderpk, amount, matures, currency);
            else
                opret = MarmaraEncodeLoopRequestOpret(createtxid, senderpk);

            rawtx = FinalizeCCTx(0, cp, mtx, mypk, txfee, opret);
            if (rawtx.size() == 0)
                errorstr = "couldnt finalize CCtx";
        }
        else 
            errorstr = "dont have enough normal inputs for requestfee and txfee";
    }
    if (rawtx.size() == 0 || errorstr != 0)
    {
        result.push_back(Pair("result", "error"));
        if (errorstr != 0)
            result.push_back(Pair("error", errorstr));
    }
    else
    {
        result.push_back(Pair("result", "success"));
        result.push_back(Pair("hex", rawtx));
        result.push_back(Pair("funcid", "R"));
        result.push_back(Pair("createtxid", createtxid.GetHex()));
        if (batontxid != zeroid)
            result.push_back(Pair("batontxid", batontxid.GetHex()));
        result.push_back(Pair("senderpk", HexStr(senderpk)));
        if (batontxid == zeroid) {
            result.push_back(Pair("amount", ValueFromAmount(amount)));
            result.push_back(Pair("matures", static_cast<int64_t>(matures)));
            result.push_back(Pair("currency", currency));
        }
    }
    return(result);
}


static int32_t redistribute_lcl_remainder(CMutableTransaction &mtx, struct CCcontract_info *cp, const std::vector<uint256> &creditloop, uint256 batontxid, CAmount amountToDistribute)
{
    CPubKey Marmarapk; 
    int32_t endorsersNumber = creditloop.size(); // number of endorsers, 0 is createtxid, last is holder
    CAmount inputsum, change;
    std::vector <CPubKey> endorserPubkeys;
    CTransaction createtx;
    uint256 hashBlock, dummytxid;
    uint256 createtxid = creditloop[0];
    struct CreditLoopOpret loopData;

    uint8_t marmarapriv[32];
    Marmarapk = GetUnspendable(cp, marmarapriv);

    if (endorsersNumber < 1)  // nobody to return to
        return 0;

    if (myGetTransaction(createtxid, createtx, hashBlock) && createtx.vout.size() > 1 &&
        MarmaraDecodeLoopOpret(createtx.vout.back().scriptPubKey, loopData) != 0)  // get amount value
    {
        char lockInLoop1of2addr[KOMODO_ADDRESS_BUFSIZE];
        CPubKey createtxidPk = CCtxidaddr_tweak(NULL, createtxid);
        GetCCaddress1of2(cp, lockInLoop1of2addr, Marmarapk, createtxidPk);  // 1of2 lock-in-loop address 

        // add locked-in-loop utxos:
        LOGSTREAMFN("marmara", CCLOG_DEBUG2, stream  << "calling AddMarmaraCCInputs for lock-in-loop addr=" << lockInLoop1of2addr << " adding as possible as amount=" << loopData.amount << std::endl);
        if ((inputsum = AddMarmaraCCInputs(IsMarmaraLockedInLoopVout, mtx, endorserPubkeys, lockInLoop1of2addr, loopData.amount, MARMARA_VINS)) >= loopData.amount / endorsersNumber) 
        {
            if (mtx.vin.size() >= CC_MAXVINS) {// total vin number limit
                LOGSTREAMFN("marmara", CCLOG_ERROR, stream  << "too many vins!" << std::endl);
                return -1;
            }

            if (endorserPubkeys.size() != endorsersNumber) {
                LOGSTREAMFN("marmara", CCLOG_ERROR, stream << " internal error not matched endorserPubkeys.size()=" << endorserPubkeys.size() << " endorsersNumber=" << endorsersNumber << " line=" << __LINE__ << std::endl);
                return -1;
            }

            CAmount amountReturned = 0;
            CAmount amountToPk = amountToDistribute / endorsersNumber;

            //for (int32_t i = 1; i < creditloop.size() + 1; i ++)  //iterate through all issuers/endorsers, skip i=0 which is 1st receiver tx, n + 1 is batontxid
            for (auto endorserPk : endorserPubkeys)
            {
                mtx.vout.push_back(CTxOut(amountToPk, CScript() << ParseHex(HexStr(endorserPk)) << OP_CHECKSIG));  // coins returned to each previous issuer normal output
                amountReturned += amountToPk;
                LOGSTREAMFN("marmara", CCLOG_DEBUG1, stream << " sending normal amount=" << amountToPk << " to pk=" << HexStr(endorserPk) << std::endl);
            }
            change = (inputsum - amountReturned);

            // return change to the lock-in-loop fund, distribute for pubkeys:
            if (change > 0) 
            {
                /* uncomment if the same check above is removed
                if (endorserPubkeys.size() != endorsersNumber) {
                    LOGSTREAMFN("marmara", CCLOG_ERROR, stream  << " internal error not matched endorsersPubkeys.size()=" << endorserPubkeys.size() << " endorsersNumber=" << endorsersNumber << " line=" << __LINE__ << std::endl);
                    return -1;
                } */
                for (auto pk : endorserPubkeys) 
                {
                    // each LCL utxo is marked with the pubkey who owns this part of the loop amount
                    // So for staking only those LCL utxo are picked up that are marked with the current node's pubkey
                    CScript opret = MarmaraEncodeLoopCCVoutOpret(createtxid, pk);   // add mypk to vout to identify who has locked coins in the credit loop
                    mtx.vout.push_back(MakeMarmaraCC1of2voutOpret(change / endorserPubkeys.size(), createtxidPk, opret));  // TODO: losing remainder?

                    LOGSTREAMFN("marmara", CCLOG_DEBUG1, stream  << "distributing to loop change/pubkeys.size()=" << change / endorserPubkeys.size() << " cc opret pk=" << HexStr(pk) << std::endl);
                }
            }

            CC *lockInLoop1of2cond = MakeCCcond1of2(EVAL_MARMARA, Marmarapk, createtxidPk);  
            CCAddVintxCond(cp, lockInLoop1of2cond, marmarapriv); //add probe condition to spend from the lock-in-loop address
            cc_free(lockInLoop1of2cond);

        }
        else  {
            LOGSTREAMFN("marmara", CCLOG_ERROR, stream  << "couldnt get locked-in-loop amount to return to endorsers" << std::endl);
            return -1;
        }
    }
    else {
        LOGSTREAMFN("marmara", CCLOG_ERROR, stream  << "could not load createtx" << std::endl);
        return -1;
    }
    return 0;
}


// issue or transfer coins to the next receiver
UniValue MarmaraIssue(int64_t txfee, uint8_t funcid, CPubKey receiverpk, const struct IssuerEndorserOptParams &optParams, uint256 requesttxid, uint256 batontxid)
{
    CMutableTransaction mtx = CreateNewContextualCMutableTransaction(Params().GetConsensus(), komodo_nextheight());
    UniValue result(UniValue::VOBJ); 
    std::string rawtx; 
    std::string errorStr;
    uint256 createtxid;

    struct CCcontract_info *cp, C;
    cp = CCinit(&C, EVAL_MARMARA);

    if (txfee == 0)
        txfee = 10000;

    // make sure less than maxlength (?)

    CPubKey Marmarapk = GetUnspendable(cp, NULL);
    CPubKey mypk = pubkey2pk(Mypubkey());
    
    /* else 
    if (requesttxid.IsNull())
        errorstr = "requesttxid can't be empty";
    
    if (errorstr.empty())
    {
        // check requested cheque params:
        CTransaction requesttx;
        uint256 hashBlock;
        uint8_t funcid = 0;

        if( get_loop_creation_data(createtxid, loopData) < 0 )
            errorstr = "cannot get loop creation data";
        else if( !myGetTransaction(requesttxid, requesttx, hashBlock) )
            errorstr = "cannot get request transaction or tx still in mempool or cannot decode request tx opreturn data";
            // TODO: do we need here to check the request tx in mempool?
        else if( hashBlock.IsNull() )   // is in mempool?
            errorstr = "request transaction still in mempool";
        else if( requesttx.vout.size() < 1 || (funcid = MarmaraDecodeLoopOpret(requesttx.vout.back().scriptPubKey, loopData)) == 0 ) 
            errorstr = "cannot decode request tx opreturn data";
        else if( funcid != 'B' && funcid != 'R' )
            errorstr = "baton is not a request tx";
        else if( mypk != loopData.pk ) 
            errorstr = "mypk does not match the requested sender pk";   // check it here;  seems we dont need this check as other pubkeys can't spend the baton
        else if (TotalPubkeyNormalInputs(requesttx, receiverpk) == 0)     // extract and check the receiver pubkey
            errorstr = "receiver pk does not match request tx";
        else if( loopData.matures <= chainActive.LastTip()->GetHeight() )
            errorstr = "credit loop must mature in the future";
    }
    if (errorstr.empty()) */

    if (mypk == receiverpk)
        errorStr = "cannot send baton to self";  // check it here
    else if (get_create_txid(createtxid, requesttxid) < 0)
        errorStr = "can't get createtxid from requesttxid (request tx could be in mempool)";
    else if (check_request_tx(requesttxid, receiverpk, funcid, errorStr))
    {
        struct CreditLoopOpret loopData;

        if (get_loop_creation_data(createtxid, loopData) >= 0)
        {
            uint256 dummytxid;
            std::vector<uint256> creditloop;
            int32_t endorsersNumber = MarmaraGetbatontxid(creditloop, dummytxid, requesttxid);

            if (endorsersNumber >= 0 && endorsersNumber < MARMARA_MAXENDORSERS)
            {
                char activated1of2addr[KOMODO_ADDRESS_BUFSIZE];
                int64_t inputsum;
                std::vector<CPubKey> pubkeys;
                int64_t amountToLock = (endorsersNumber > 0 ? loopData.amount / (endorsersNumber + 1) : loopData.amount);  // include new endorser

                GetCCaddress1of2(cp, activated1of2addr, Marmarapk, mypk);  // 1of2 address where the activated endorser's money is locked

                LOGSTREAMFN("marmara", CCLOG_DEBUG2, stream << "calling AddMarmaraCCInputs for activated addr=" << activated1of2addr << " needs activated amount to lock-in-loop=" << amountToLock << std::endl);
                if ((inputsum = AddMarmaraCCInputs(IsMarmaraActivatedVout, mtx, pubkeys, activated1of2addr, amountToLock, MARMARA_VINS)) >= amountToLock) // add 1/n remainder from the locked fund
                {
                    mtx.vin.push_back(CTxIn(requesttxid, MARMARA_REQUEST_VOUT, CScript()));  // spend the request tx baton (2*txfee for first-time request, 1*txfee for second-time and more)
                    if (funcid == 'T')
                        mtx.vin.push_back(CTxIn(batontxid, MARMARA_BATON_VOUT, CScript()));   // spend the previous issue/transfer tx baton (1*txfee)

                    if (funcid == 'T' || AddNormalinputs(mtx, mypk, 2 * txfee, 4) > 0)  // add two more txfee for marmaraissue
                    {
                        mtx.vout.push_back(MakeCC1vout(EVAL_MARMARA, txfee, receiverpk));  // vout0 is transfer of the baton to the next receiver
                        if (funcid == 'I')
                            mtx.vout.push_back(MakeCC1vout(EVAL_MARMARA, txfee, Marmarapk));  // vout1 is marker in issuance tx to list all loops

                        // lock 1/N amount in loop
                        CPubKey createtxidPk = CCtxidaddr_tweak(NULL, createtxid);

                        // add cc lock-in-loop opret 
                        // mark opret with my pk to indicate whose vout it is (to add it as mypk staking utxo) 
                        CScript opret = MarmaraEncodeLoopCCVoutOpret(createtxid, mypk);
                        // add cc opret with mypk to cc vout 
                        mtx.vout.push_back(MakeMarmaraCC1of2voutOpret(amountToLock, createtxidPk, opret)); //vout2 issued amount

                        if (funcid == 'I')
                            mtx.vout.push_back(MakeCC1vout(EVAL_MARMARA, txfee, Marmarapk));  // vout3 is open/close marker in issuance tx

                        LOGSTREAMFN("marmara", CCLOG_DEBUG1, stream << "sending to loop amount=" << amountToLock << " marked with mypk=" << HexStr(mypk) << std::endl);

                        // return change to mypk activated address:
                        int64_t change = (inputsum - amountToLock);
                        if (change > 0)
                        {
                            int32_t height = komodo_nextheight();
                            if ((height & 1) != 0) // make height even as only even height is considered for staking (TODO: strange)
                                height++;
                            CScript opret = MarmaraCoinbaseOpret('A', height, mypk);
                            // add coinbase opret to ccvout for the change
                            mtx.vout.push_back(MakeMarmaraCC1of2voutOpret(change, mypk, opret));  // adding MarmaraCoinbase cc vout 'opret' for change
                        }

                        if (endorsersNumber < 1 || redistribute_lcl_remainder(mtx, cp, creditloop, batontxid, amountToLock) >= 0)  // if there are issuers already then distribute and return amount / n value
                        {
                            CC* activated1of2cond = MakeCCcond1of2(EVAL_MARMARA, Marmarapk, mypk);  // create vintx probe 1of2 cond
                            CCAddVintxCond(cp, activated1of2cond);      // add the probe to cp, it is copied and we can cc_free it
                            cc_free(activated1of2cond);

                            CScript opret;
                            if (funcid == 'I')
                                opret = MarmaraEncodeLoopIssuerOpret(createtxid, receiverpk, optParams.autoSettlement, optParams.autoInsurance, optParams.avalCount, optParams.disputeExpiresOffset, optParams.escrowOn, optParams.blockageAmount);
                            else
                                opret = MarmaraEncodeLoopTransferOpret(createtxid, receiverpk, optParams.avalCount);

                            rawtx = FinalizeCCTx(0, cp, mtx, mypk, txfee, opret);

                            if (rawtx.size() == 0) {
                                errorStr = "couldnt finalize tx";
                                LOGSTREAMFN("marmara", CCLOG_ERROR, stream << "couldnt finalize, bad mtx=" << HexStr(E_MARSHAL(ss << mtx)) << std::endl);
                            }
                        }
                        else
                            errorStr = "could not return locked in loop funds to endorsers";
                    }
                    else
                        errorStr = "dont have enough normal inputs for txfee";
                }
                else
                    errorStr = "dont have enough locked inputs for amount";
            }
            else
            {
                if (endorsersNumber >= MARMARA_MAXENDORSERS)
                    errorStr = "too many endorsers";
                else
                    errorStr = "incorrect requesttxid";

            }
        }
        else
        {
            errorStr = "cannot get loop creation data";
        }
    }
    if (!errorStr.empty())
    {
        result.push_back(Pair("result", "error"));
        result.push_back(Pair("error", errorStr));
    }
    else
    {
        result.push_back(Pair("result", "success"));
        result.push_back(Pair("hex", rawtx));
        char sfuncid[2]; 
        sfuncid[0] = funcid;
        sfuncid[1] = '\0';
        result.push_back(Pair("funcid", sfuncid));
        result.push_back(Pair("createtxid", createtxid.GetHex()));
        result.push_back(Pair("requesttxid", requesttxid.GetHex()));
        if (funcid == 'T')
            result.push_back(Pair("batontxid", batontxid.GetHex()));
        result.push_back(Pair("receiverpk", HexStr(receiverpk)));
//        result.push_back(Pair("amount", ValueFromAmount(amount)));
//        result.push_back(Pair("matures", static_cast<int64_t>(matures)));
//        result.push_back(Pair("currency", currency));
    }
    return(result);
}

UniValue MarmaraCreditloop(uint256 txid)
{
    UniValue result(UniValue::VOBJ), a(UniValue::VARR); 
    std::vector<uint256> creditloop; 
    uint256 batontxid, hashBlock; uint8_t funcid; 
    int32_t numerrs = 0, i, n; 
    CTransaction lasttx; 
    struct CCcontract_info *cp, C;
    struct CreditLoopOpret loopData;
    bool isSettledOk = false;

    cp = CCinit(&C, EVAL_MARMARA);
    if ((n = MarmaraGetbatontxid(creditloop, batontxid, txid)) > 0)
    {
        if (get_loop_creation_data(creditloop[0], loopData) == 0)
        {
            uint256 issuetxid, settletxid, lasttxid;
            int32_t vini, height;

            if (n > 1)
                issuetxid = creditloop[1];
            else
                issuetxid = batontxid;

            std::vector<uint256> looptxids (creditloop.begin(), creditloop.end());

            if (get_settlement_txid(settletxid, issuetxid) == 0)
            {
                // loop is closed - last tx is the settle tx
                lasttxid = settletxid;
                looptxids.push_back(batontxid);  // add baton to to add its info to the result too
            }
            else
            {
                // loop is not closed - last tx is the baton
                lasttxid = batontxid;
            }

            // add last tx info
            if (myGetTransaction(lasttxid, lasttx, hashBlock) && lasttx.vout.size() > 1)
            {
                char normaladdr[KOMODO_ADDRESS_BUFSIZE], myCCaddr[KOMODO_ADDRESS_BUFSIZE], vout0addr[KOMODO_ADDRESS_BUFSIZE], batonCCaddr[KOMODO_ADDRESS_BUFSIZE];

                result.push_back(Pair("result", "success"));
                Getscriptaddress(normaladdr, CScript() << ParseHex(HexStr(Mypubkey())) << OP_CHECKSIG);
                result.push_back(Pair("myNormalAddress", normaladdr));
                GetCCaddress(cp, myCCaddr, Mypubkey());
                result.push_back(Pair("myCCaddress", myCCaddr));

                if ((funcid = MarmaraDecodeLoopOpret(lasttx.vout.back().scriptPubKey, loopData)) != 0)
                {
                    std::string sfuncid(1, (char)funcid);
                    result.push_back(Pair("funcid", sfuncid));
                    result.push_back(Pair("currency", loopData.currency));

                    if (loopData.createtxid != creditloop[0])
                    {
                        LOGSTREAMFN("marmara", CCLOG_ERROR, stream << "invalid loopData.createtxid for creditloop[0]=" << creditloop[0].GetHex() << " " << std::endl);
                        result.push_back(Pair("incorrect-createtxid-in-baton-opret", loopData.createtxid.GetHex()));
                        numerrs++;
                    }

                    if (funcid == 'S') //settled okay
                    {
                        //refcreatetxid = creditloop[0];
                        result.push_back(Pair("settlement", batontxid.GetHex()));
                        result.push_back(Pair("createtxid", creditloop[0].GetHex()));
                        result.push_back(Pair("remainder", ValueFromAmount(loopData.remaining)));
                        result.push_back(Pair("settled", static_cast<int64_t>(loopData.matures)));
                        result.push_back(Pair("pubkey", HexStr(loopData.pk)));
                        Getscriptaddress(normaladdr, CScript() << ParseHex(HexStr(loopData.pk)) << OP_CHECKSIG);
                        result.push_back(Pair("settledToNormalAddress", normaladdr));
                        result.push_back(Pair("collected", ValueFromAmount(lasttx.vout[0].nValue)));
                        Getscriptaddress(vout0addr, lasttx.vout[0].scriptPubKey);
                        if (strcmp(normaladdr, vout0addr) != 0)
                        {
                            result.push_back(Pair("incorrect-vout0-address-not-matched-pk-in-opret", vout0addr));
                            numerrs++;
                        }
                        isSettledOk = true;
                    }
                    else if (funcid == 'D')  //settled partially
                    {
                        //refcreatetxid = creditloop[0];
                        result.push_back(Pair("settlement", batontxid.GetHex()));
                        result.push_back(Pair("createtxid", creditloop[0].GetHex()));
                        result.push_back(Pair("remainder", ValueFromAmount(loopData.remaining)));
                        result.push_back(Pair("settled", static_cast<int64_t>(loopData.matures)));
                        Getscriptaddress(vout0addr, lasttx.vout[0].scriptPubKey);
                        result.push_back(Pair("txidaddr", vout0addr));  //TODO: why 'txidaddr'?
                        if (lasttx.vout.size() > 1)
                            result.push_back(Pair("collected", ValueFromAmount(lasttx.vout[1].nValue)));
                    }
                    else
                    {
                        result.push_back(Pair("batontxid", batontxid.GetHex()));
                        result.push_back(Pair("createtxid", creditloop[0].GetHex()));
                        result.push_back(Pair("amount", ValueFromAmount(loopData.amount)));
                        result.push_back(Pair("matures", static_cast<int64_t>(loopData.matures)));
                        result.push_back(Pair("batonpk", HexStr(loopData.pk)));
                        Getscriptaddress(normaladdr, CScript() << ParseHex(HexStr(loopData.pk)) << OP_CHECKSIG);
                        result.push_back(Pair("batonaddr", normaladdr));
                        GetCCaddress(cp, batonCCaddr, loopData.pk);  // baton address
                        result.push_back(Pair("batonCCaddr", batonCCaddr));
                        Getscriptaddress(vout0addr, lasttx.vout[0].scriptPubKey);
                        if (strcmp(vout0addr, batonCCaddr) != 0)  // TODO: how is this possible?
                        {
                            result.push_back(Pair("incorrect-vout0-address-not-matched-baton-address", normaladdr));
                            numerrs++;
                        }

                        if (strcmp(myCCaddr, /*normaladdr*/batonCCaddr) == 0) // TODO: impossible with normal addr
                            result.push_back(Pair("ismine", static_cast<int64_t>(1)));
                        else
                            result.push_back(Pair("ismine", static_cast<int64_t>(0)));
                    }
                }
                else
                {
                    result.push_back(Pair("result", "error"));
                    result.push_back(Pair("error", "couldnt decode last tx opret"));
                    return result;
                }
            }
            else
            {
                result.push_back(Pair("result", "error"));
                result.push_back(Pair("error", "couldnt load last tx or incorrect last tx"));
                return result;
            }
            
            // add locked-in-loop amount:
            char lockInLoop1of2addr[KOMODO_ADDRESS_BUFSIZE];
            CPubKey createtxidPk = CCtxidaddr_tweak(NULL, creditloop[0]);
            GetCCaddress1of2(cp, lockInLoop1of2addr, GetUnspendable(cp, NULL), createtxidPk);  // 1of2 lock-in-loop address 
            std::vector<CPubKey> pubkeys;
            CMutableTransaction mtx;

            int64_t amountLockedInLoop = AddMarmaraCCInputs(IsMarmaraLockedInLoopVout, mtx, pubkeys, lockInLoop1of2addr, 0, 0);
            result.push_back(Pair("LockedInLoopCCaddr", lockInLoop1of2addr));
            result.push_back(Pair("LockedInLoopAmount", ValueFromAmount(amountLockedInLoop)));  // should be 0 if 

            // add credit loop data:
            for (i = 0; i < looptxids.size(); i++)
            {
                if (myGetTransaction(looptxids[i], lasttx, hashBlock) != 0 && lasttx.vout.size() > 1)
                {
                    //uint256 createtxid = zeroid;
                    if ((funcid = MarmaraDecodeLoopOpret(lasttx.vout.back().scriptPubKey, loopData)) != 0)
                    {
                        char normaladdr[KOMODO_ADDRESS_BUFSIZE], ccaddr[KOMODO_ADDRESS_BUFSIZE], vout0addr[KOMODO_ADDRESS_BUFSIZE];

                        UniValue obj(UniValue::VOBJ);
                        obj.push_back(Pair("txid", looptxids[i].GetHex()));
                        std::string sfuncid(1, (char)funcid);
                        obj.push_back(Pair("funcid", sfuncid));
                        if (funcid == 'R' || funcid == 'B')
                        {
                            //createtxid = looptxids[i];
                            obj.push_back(Pair("issuerpk", HexStr(loopData.pk)));
                            Getscriptaddress(normaladdr, CScript() << ParseHex(HexStr(loopData.pk)) << OP_CHECKSIG);
                            obj.push_back(Pair("issuerNormalAddress", normaladdr));
                            GetCCaddress(cp, ccaddr, loopData.pk);
                            obj.push_back(Pair("issuerCCAddress", ccaddr));
                        }
                        else
                        {
                            obj.push_back(Pair("receiverpk", HexStr(loopData.pk)));
                            Getscriptaddress(normaladdr, CScript() << ParseHex(HexStr(loopData.pk)) << OP_CHECKSIG);
                            obj.push_back(Pair("receiverNormalAddress", normaladdr));
                            GetCCaddress(cp, ccaddr, loopData.pk);
                            obj.push_back(Pair("receiverCCAddress", ccaddr));
                        }
                        Getscriptaddress(vout0addr, lasttx.vout[0].scriptPubKey);
                        if (strcmp(vout0addr, normaladdr) != 0)
                        {
                            obj.push_back(Pair("incorrect-vout0address", vout0addr));
                            numerrs++;
                        }
                        if (i == 0 && isSettledOk)  // why isSettledOk checked?..
                        {
                            result.push_back(Pair("amount", ValueFromAmount(loopData.amount)));
                            result.push_back(Pair("matures", static_cast<int64_t>(loopData.matures)));
                        }
                        /* not relevant now as we do not copy params to new oprets
                        if (createtxid != refcreatetxid || amount != refamount || matures != refmatures || currency != refcurrency)
                        {
                        numerrs++;
                        obj.push_back(Pair("objerror", (char *)"mismatched createtxid or amount or matures or currency"));
                        obj.push_back(Pair("createtxid", createtxid.GetHex()));
                        obj.push_back(Pair("amount", ValueFromAmount(amount)));
                        obj.push_back(Pair("matures", static_cast<int64_t>(matures)));
                        obj.push_back(Pair("currency", currency));
                        } */
                        a.push_back(obj);
                    }
                }
            }
            result.push_back(Pair("n", static_cast<int64_t>(n)));
            result.push_back(Pair("numerrors", static_cast<int64_t>(numerrs)));
            result.push_back(Pair("creditloop", a));

        }
        else
        {
            result.push_back(Pair("result", "error"));
            result.push_back(Pair("error", "couldnt get loop creation data"));
        }
    }
    else if (n == 0)
    {
        // output info of createtx if only createtx exists
        if (get_loop_creation_data(txid, loopData) == 0)
        {
            std::string sfuncid(1, (char)loopData.lastfuncid);
            result.push_back(Pair("funcid", sfuncid));
            result.push_back(Pair("currency", loopData.currency));
            result.push_back(Pair("amount", ValueFromAmount(loopData.amount)));
            result.push_back(Pair("matures", static_cast<int64_t>(loopData.matures)));
            result.push_back(Pair("issuerpk", HexStr(loopData.pk)));
            result.push_back(Pair("createtxid", txid.GetHex()));
        }
        else
        {
            result.push_back(Pair("result", "error"));
            result.push_back(Pair("error", "couldnt get loop creation data"));
        }
    }
    else
    {
        result.push_back(Pair("result", "error"));
        result.push_back(Pair("error", "couldnt get creditloop"));
    }
    return(result);
}

// collect miner pool rewards (?)
UniValue MarmaraPoolPayout(int64_t txfee, int32_t firstheight, double perc, char *jsonstr) // [[pk0, shares0], [pk1, shares1], ...]
{
    CMutableTransaction mtx = CreateNewContextualCMutableTransaction(Params().GetConsensus(), komodo_nextheight());
    UniValue result(UniValue::VOBJ), a(UniValue::VARR); 
    cJSON *item, *array; std::string rawtx; 
    int32_t i, n; 
    uint8_t buf[CPubKey::COMPRESSED_PUBLIC_KEY_SIZE]; 
    CPubKey Marmarapk, pk, poolpk; 
    int64_t payout, poolfee = 0, total, totalpayout = 0; 
    double poolshares, share, shares = 0.; 
    char *pkstr;
    const char *errorstr = 0;
    struct CCcontract_info *cp, C;

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
            if ((pkstr = jstr(jitem(item, 0), 0)) != 0 && strlen(pkstr) == (2 * CPubKey::COMPRESSED_PUBLIC_KEY_SIZE))
                shares += jdouble(jitem(item, 1), 0);
            else
            {
                errorstr = "all items must be of the form [<pubkey>, <shares>]";
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
                            if ((pkstr = jstr(jitem(item, 0), 0)) != 0 && strlen(pkstr) == (2 * CPubKey::COMPRESSED_PUBLIC_KEY_SIZE))
                            {
                                UniValue x(UniValue::VOBJ);
                                totalpayout += payout;
                                decode_hex(buf, CPubKey::COMPRESSED_PUBLIC_KEY_SIZE, pkstr);
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
                    errorstr = "couldnt finalize CCtx";
            }
            else errorstr = "couldnt find any coinbases to payout";
        }
        else if (errorstr == 0)
            errorstr = "no valid shares submitted";
        free(array);
    }
    else errorstr = "couldnt parse poolshares jsonstr";
    if (rawtx.size() == 0 || errorstr != 0)
    {
        result.push_back(Pair("result", "error"));
        if (errorstr != 0)
            result.push_back(Pair("error", errorstr));
    }
    else
    {
        result.push_back(Pair("result", "success"));
        result.push_back(Pair("hex", rawtx));
        if (totalpayout > 0 && total > totalpayout - txfee)
        {
            result.push_back(Pair("firstheight", static_cast<int64_t>(firstheight)));
            result.push_back(Pair("lastheight", static_cast<int64_t>(((firstheight / MARMARA_GROUPSIZE) + 1) * MARMARA_GROUPSIZE - 1)));
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
    CMutableTransaction mtx; 
    std::vector<CPubKey> pubkeys;
    UniValue result(UniValue::VOBJ), a(UniValue::VARR), b(UniValue::VARR); 
    int32_t n; int64_t totalclosed = 0, totalamount = 0; 
    std::vector<uint256> issuances, closed; 
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
    result.push_back(Pair("myActivatedAmount", ValueFromAmount(AddMarmaraCCInputs(IsMarmaraActivatedVout, mtx, pubkeys, activated1of2addr, 0, MARMARA_VINS)))); 
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
        if (strcmp(prevloopaddr, loopaddr) != 0)   // loop address changed
        {
            if (prevloopaddr[0] != '\0')   // prevloop was
            {
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

    if (refpk.size() == CPubKey::COMPRESSED_PUBLIC_KEY_SIZE)
        result.push_back(Pair("issuer", HexStr(refpk)));
    if (currency.size() == 0)
        currency = (char *)MARMARA_CURRENCY;
    if (firstheight <= lastheight)
        firstheight = 0, lastheight = (1 << 30);
    if (minamount <= maxamount)
        minamount = 0, maxamount = (1LL << 60);
    result.push_back(Pair("firstheight", static_cast<int64_t>(firstheight)));
    result.push_back(Pair("lastheight", static_cast<int64_t>(lastheight)));
    result.push_back(Pair("minamount", ValueFromAmount(minamount)));
    result.push_back(Pair("maxamount", ValueFromAmount(maxamount)));
    result.push_back(Pair("currency", currency));
    if ((n = enum_credit_loops(MARMARA_MARKER_VOUT, totalamount, issuances, totalclosed, closed, cp, firstheight, lastheight, minamount, maxamount, refpk, currency, [](uint256, int32_t) {/*do nothing*/})) > 0)
    {
        result.push_back(Pair("n", static_cast<int64_t>(n)));
        result.push_back(Pair("numpending", static_cast<int64_t>(issuances.size())));
        for (int32_t i = 0; i < issuances.size(); i++)
            a.push_back(issuances[i].GetHex());
        result.push_back(Pair("issuances", a));
        result.push_back(Pair("totalamount", ValueFromAmount(totalamount)));
        result.push_back(Pair("numclosed", static_cast<int64_t>(closed.size())));
        for (int32_t i = 0; i < closed.size(); i++)
            b.push_back(closed[i].GetHex());
        result.push_back(Pair("closed", b));
        result.push_back(Pair("totalclosed", ValueFromAmount(totalclosed)));
    }
    return(result);
}

// generate a new activated address and return its segid
UniValue MarmaraNewActivatedAddress(CPubKey pk)
{
    UniValue ret(UniValue::VOBJ);
    char activated1of2addr[KOMODO_ADDRESS_BUFSIZE];
    struct CCcontract_info *cp, C;

    cp = CCinit(&C, EVAL_MARMARA);
    CPubKey marmarapk = GetUnspendable(cp, 0);
    
    GetCCaddress1of2(cp, activated1of2addr, marmarapk, pk);
    CKeyID keyID = pk.GetID();
    std::string addr = EncodeDestination(keyID);

    ret.push_back(Pair("pubkey", HexStr(pk.begin(), pk.end())));
    ret.push_back(Pair("normaladdress", addr));
    ret.push_back(Pair("activated1of2address", activated1of2addr));
    ret.push_back(Pair("segid", (int32_t)(komodo_segid32(activated1of2addr) & 0x3f)));
    return ret;
}

void OS_randombytes(unsigned char *x, long xlen);

// generate 64 activated addresses and split utxos on them
std::string MarmaraLock64(CWallet *pwalletMain, CAmount amount, int32_t nutxos)
{
    CMutableTransaction mtx = CreateNewContextualCMutableTransaction(Params().GetConsensus(), komodo_nextheight());
    char activated1of2addr[KOMODO_ADDRESS_BUFSIZE];
    const CAmount txfee = 10000;

    struct CCcontract_info *cp, C;
    cp = CCinit(&C, EVAL_MARMARA);
    CPubKey marmarapk = GetUnspendable(cp, 0);
    CPubKey mypk = pubkey2pk(Mypubkey());

    int32_t height = komodo_nextheight();
    // as opret creation function MarmaraCoinbaseOpret creates opret only for even blocks - adjust this base height to even value
    if ((height & 1) != 0)
        height++;

    // TODO: check that the wallet has already segid pubkeys    
    vACTIVATED_WALLET_DATA activated;
    EnumWalletActivatedAddresses(pwalletMain, activated);
    if (activated.size() >= 64)
    {
        CCerror = "wallet already has 64 activated split addresses. Use a clean wallet with enough inputs on my-pubkey in it";
        return std::string();
    }

    std::map<uint32_t, std::pair<CKey, CPubKey>> segidKeys;

    // add mypubkey
    char myactivated1of2addr[KOMODO_ADDRESS_BUFSIZE];
    GetCCaddress1of2(cp, myactivated1of2addr, marmarapk, mypk);
    uint32_t segid = komodo_segid32(myactivated1of2addr) & 0x3f;
    if (segidKeys.find(segid) == segidKeys.end())
    {
        // add myprivkey key
        uint8_t mypriv32[32];
        Myprivkey(mypriv32);
        CKey mykey;
        mykey.Set(&mypriv32[0], &mypriv32[32], true);
        segidKeys[segid] = std::make_pair(mykey, mypk);
    }

    while (segidKeys.size() < 64)  // until we do not generate keys for all 64 segids
    {
        uint8_t priv32[32];
        // generate random priv key
#ifndef __WIN32
        OS_randombytes(priv32, sizeof(priv32));
#else
        randombytes_buf(priv32, sizeof(priv32));
#endif
        CKey key;
        key.Set(&priv32[0], &priv32[32], true);
        CPubKey pubkey = key.GetPubKey();
        CKeyID vchAddress = pubkey.GetID();

        // get 1of2 address segid
        char activated1of2addr[KOMODO_ADDRESS_BUFSIZE];
        GetCCaddress1of2(cp, activated1of2addr, marmarapk, pubkey);
        uint32_t segid = komodo_segid32(activated1of2addr) & 0x3f;
        if (segidKeys.find(segid) == segidKeys.end())
        {
            // add segid's keys
            segidKeys[segid] = std::make_pair(key, pubkey);
        }
    }

    //std::cerr << "amount / 64LL / (CAmount)nutxos=" << (amount / 64LL / (CAmount)nutxos) << " 100LL * txfee=" << 100LL * txfee << std::endl;

    if (AddNormalinputs(mtx, mypk, amount + txfee, CC_MAXVINS) > 0)
    {
        // create tx with 64 * nutxo vouts:
        for (auto &keyPair : segidKeys)
        {
            for (int32_t i = 0; i < nutxos; i++)
            {
                if (amount / 64LL / (CAmount)nutxos < 100LL * txfee)
                {
                    CCerror = "amount too low";
                    return std::string();
                }
                // lock the amount on 1of2 address:
                CPubKey segidpk = keyPair.second.second;

                // add ccopret
                CScript opret = MarmaraCoinbaseOpret('A', height, segidpk);
                // add marmara opret segpk to each cc vout 
                mtx.vout.push_back(MakeMarmaraCC1of2voutOpret(amount / 64 / nutxos, segidpk, opret));
            }
        }
        std::string hextx = FinalizeCCTx(0, cp, mtx, mypk, txfee, CScript());
        if (hextx.empty())
        {
            CCerror = "could not finalize tx";
            return std::string();
        }

        // if tx okay save keys:
        pwalletMain->MarkDirty();
        std::string strLabel = "";
        for (auto &keyPair : segidKeys)
        {
            CKey key = keyPair.second.first;
            CPubKey pubkey = keyPair.second.second;
            CKeyID vchAddress = pubkey.GetID();

            pwalletMain->SetAddressBook(vchAddress, strLabel, "receive");

            // Don't throw error in case a key is already there
            if (pwalletMain->HaveKey(vchAddress)) {
                LOGSTREAMFN("marmara", CCLOG_INFO, stream << "key already in the wallet" << std::endl);
            }
            else {
                pwalletMain->mapKeyMetadata[vchAddress].nCreateTime = 1;
                if (!pwalletMain->AddKeyPubKey(key, pubkey))
                {
                    CCerror = "Error adding key to wallet";
                    return std::string();
                }
                LOGSTREAMFN("marmara", CCLOG_DEBUG1, stream << "key added to wallet addr=" << EncodeDestination(vchAddress) << std::endl);
            }
        }

        // whenever a key is imported, we need to scan the whole chain
        pwalletMain->nTimeFirstKey = 1; // 0 would be considered 'no value'
        return hextx;

    }
    else {
        CCerror = "not enough normal inputs or too many input utxos";
        return std::string();
    }
}

// list activated addresses in the wallet
UniValue MarmaraListActivatedAddresses(CWallet *pwalletMain)
{
    UniValue ret(UniValue::VOBJ);
    UniValue retarray(UniValue::VARR);

    vACTIVATED_WALLET_DATA activated;
    EnumWalletActivatedAddresses(pwalletMain, activated);
    for (auto a : activated)
    {
        UniValue elem(UniValue::VOBJ);
        std::string sActivated1of2addr = ACTIVATED_WALLET_DATA_ADDR(a);
        uint32_t segid = ACTIVATED_WALLET_DATA_SEGID(a);
        CAmount amount = ACTIVATED_WALLET_DATA_AMOUNT(a);

        elem.push_back(std::make_pair("activatedaddress", sActivated1of2addr));
        elem.push_back(std::make_pair("segid", (int32_t)segid));
        elem.push_back(std::make_pair("amount", ValueFromAmount(amount)));
        retarray.push_back(elem);
    }
    ret.push_back(Pair("WalletActivatedAddresses", retarray));
    return ret;
}

// release activated coins from 64 segids to normal address
std::string MarmaraReleaseActivatedCoins(CWallet *pwalletMain, const std::string &destaddr)
{
    CMutableTransaction mtx = CreateNewContextualCMutableTransaction(Params().GetConsensus(), komodo_nextheight());
    const CAmount txfee = 10000;

    struct CCcontract_info *cp, C;
    cp = CCinit(&C, EVAL_MARMARA);
    CPubKey mypk = pubkey2pk(Mypubkey());
    CPubKey marmarapk = GetUnspendable(cp, NULL);

    vACTIVATED_WALLET_DATA activated;
    EnumWalletActivatedAddresses(pwalletMain, activated);
    if (activated.size() == 0)
    {
        CCerror = "no activated coins in the wallet (size==0)";
        return std::string();
    }

    int32_t maxvins = 128;

    if (AddNormalinputs(mtx, mypk, txfee, 5) > 0)
    {
        CAmount total = 0LL;
        for (auto a : activated)
        {
            char activated1of2addr[KOMODO_ADDRESS_BUFSIZE];
            CKey key = ACTIVATED_WALLET_DATA_KEY(a);
            CPubKey pk = ACTIVATED_WALLET_DATA_PK(a);

            // skip mypubkey
            if (pk != mypk)
            {
                GetCCaddress1of2(cp, activated1of2addr, marmarapk, pk);

                CC *probeCond = MakeCCcond1of2(EVAL_MARMARA, marmarapk, pk);  //add probe condition
                CCAddVintxCond(cp, probeCond, key.begin());
                cc_free(probeCond);

                std::vector<CPubKey> pubkeys;
                CAmount amount = AddMarmaraCCInputs(IsMarmaraActivatedVout, mtx, pubkeys, activated1of2addr, 0, maxvins - mtx.vin.size());  // if total == 0 AddMarmaraCCInputs just calcs but does not adds vins
                if (amount > 0)
                {
                    amount = AddMarmaraCCInputs(IsMarmaraActivatedVout, mtx, pubkeys, activated1of2addr, amount, maxvins - mtx.vin.size());
                    total += amount;
                }
            }
        }

        if (total == 0)
        {
            CCerror = "no activated coins in the wallet (total==0)";
            return std::string();
        }
        CTxDestination dest = DecodeDestination(destaddr.c_str());
        mtx.vout.push_back(CTxOut(total, GetScriptForDestination(dest)));  // where to send activated coins from normal 

        int32_t height = komodo_nextheight();
        // as opret creation function MarmaraCoinbaseOpret creates opret only for even blocks - adjust this base height to even value
        if ((height & 1) != 0)
            height++;
        CScript opret = MarmaraCoinbaseOpret('O', height, mypk); // dummy opret

        std::string hextx = FinalizeCCTx(0, cp, mtx, mypk, txfee, opret);
        if (hextx.empty())
        {
            CCerror = "could not finalize tx";
            return std::string();
        }
        else
            return hextx;
    }
    else
    {
        CCerror = "insufficient normals for tx fee";
        return std::string();
    }
}


// unlock activated coins from mypk to normal address
std::string MarmaraUnlockActivatedCoins(CAmount amount)
{
    CMutableTransaction mtx = CreateNewContextualCMutableTransaction(Params().GetConsensus(), komodo_nextheight());
    const CAmount txfee = 10000;

    struct CCcontract_info *cp, C;
    cp = CCinit(&C, EVAL_MARMARA);
    CPubKey mypk = pubkey2pk(Mypubkey());
    CPubKey marmarapk = GetUnspendable(cp, NULL);
    int32_t maxvins = 128;

    if (AddNormalinputs(mtx, mypk, txfee, 5) > 0)
    {
        char activated1of2addr[KOMODO_ADDRESS_BUFSIZE];
        CActivatedOpretChecker activatedChecker;
        GetCCaddress1of2(cp, activated1of2addr, marmarapk, mypk);

        CC *probeCond = MakeCCcond1of2(EVAL_MARMARA, marmarapk, mypk);  //add probe condition
        CCAddVintxCond(cp, probeCond, NULL);

        std::vector<CPubKey> pubkeys;
        if (amount == 0)
            amount = AddMarmaraCCInputs(IsMarmaraActivatedVout, mtx, pubkeys, activated1of2addr, 0, maxvins);  // calc available
        amount = AddMarmaraCCInputs(IsMarmaraActivatedVout, mtx, pubkeys, activated1of2addr, amount, maxvins);
        if (amount == 0) {
            CCerror = "no activated inputs added";
            return std::string();
        }

        mtx.vout.push_back(CTxOut(amount, CScript() << Mypubkey() << OP_CHECKSIG));  // where to send activated coins from normal 
        LOGSTREAMFN("marmara", CCLOG_INFO, stream << "added amount=" << amount << std::endl);

        int32_t height = komodo_nextheight();
        // as opret creation function MarmaraCoinbaseOpret creates opret only for even blocks - adjust this base height to even value
        if ((height & 1) != 0)
            height++;
        CScript opret = MarmaraCoinbaseOpret('O', height, mypk); // dummy opret with release funcid

        std::string hextx = FinalizeCCTx(0, cp, mtx, mypk, txfee, opret);
        cc_free(probeCond);
        if (hextx.empty())
        {
            CCerror = "could not finalize tx";
            return std::string();
        }
        else
            return hextx;
    }
    else
    {
        CCerror = "insufficient normals for tx fee";
        return std::string();
    }
}

// collects PoS statistics
UniValue MarmaraPoSStat(int32_t beginHeight, int32_t endHeight)
{
    UniValue result(UniValue::VOBJ);
    UniValue array(UniValue::VARR);
    UniValue error(UniValue::VOBJ);

    /* tuple params:  is boosted, coinbase normal addr, normal total, coinbase cc addr, cc total, txcount, segid */
    typedef std::tuple<bool, std::string, int64_t, std::string, int64_t, int32_t, uint32_t> TStatElem;
    std::map<std::string, TStatElem> mapStat;

    if (beginHeight == 0)
        beginHeight = 1;
    if (endHeight == 0)
        endHeight = chainActive.Height();


    for(int32_t i = beginHeight; i <= endHeight; i ++) 
    {

        int8_t segid = komodo_segid(0, i);
        if (segid >= 0)
        {
            CBlockIndex *pblockindex = chainActive[i];
            CBlock block;

            if (fHavePruned && !(pblockindex->nStatus & BLOCK_HAVE_DATA) && pblockindex->nTx > 0) {
                error.push_back(Pair("result", "error"));
                error.push_back(Pair("error", std::string("Block not available (pruned data), h=") + std::to_string(i)));
                return error;
            }

            if (!ReadBlockFromDisk(block, pblockindex, 1)) {
                error.push_back(Pair("result", "error"));
                error.push_back(Pair("error", std::string("Can't read block from disk, h=") + std::to_string(i)));
                return error;
            }

            if (block.vtx.size() >= 2)
            {
                CTransaction coinbase = block.vtx[0];
                CTransaction stakeTx = block.vtx.back(), vintx;
                uint256 hashBlock;
                vscript_t vopret;

                // check vin.size and vout.size, do not do this yet for diagnosis
                // if (stakeTx.vin.size() == 1 && stakeTx.vout.size() == 1 || stakeTx.vout.size() == 2 && GetOpReturnData(stakeTx.vout.back().scriptPubKey, vopret) /*opret with merkle*/)
                // {
                if (myGetTransaction(stakeTx.vin[0].prevout.hash, vintx, hashBlock))
                {
                    //char vintxaddr[KOMODO_ADDRESS_BUFSIZE];
                    char staketxaddr[KOMODO_ADDRESS_BUFSIZE];
                    //Getscriptaddress(vintxaddr, vintx.vout[0].scriptPubKey);
                    Getscriptaddress(staketxaddr, stakeTx.vout[0].scriptPubKey);

                    //if (strcmp(vintxaddr, staketxaddr) == 0)
                    //{
                    CScript opret;
                    CPubKey dummypk, opretpk;
                    CActivatedOpretChecker activatedChecker;
                    CLockInLoopOpretChecker lockinloopChecker;
                    bool isBoosted;

                    if (get_either_opret(&activatedChecker, stakeTx, 0, opret, dummypk))
                    {
                        // staked is activated coins:
                        isBoosted = false;
                    }
                    else if (get_either_opret(&lockinloopChecker, stakeTx, 0, opret, dummypk))
                    {
                        // stakes is lockinloop coins
                        isBoosted = true;
                    }
                    else
                    {
                        error.push_back(Pair("result", "error"));
                        error.push_back(Pair("error", std::string("Stake transaction not recognized, h=") + std::to_string(i)));
                        return error;
                    }

                    LOGSTREAMFN("marmara", CCLOG_DEBUG1, stream << "h=" << i << " stake txid=" << stakeTx.GetHash().GetHex() << " vout.size()=" << stakeTx.vout.size() << " isBoosted=" << isBoosted << std::endl);

                    char stakeaddr[KOMODO_ADDRESS_BUFSIZE];
                    char coinbaseaddr[KOMODO_ADDRESS_BUFSIZE];
                    Getscriptaddress(stakeaddr, stakeTx.vout[0].scriptPubKey);
                    Getscriptaddress(coinbaseaddr, coinbase.vout[0].scriptPubKey);
                    std::string p1;
                    int64_t p2;
                    std::string p3;
                    int64_t p4;

                    TStatElem elem = mapStat[std::string(stakeaddr)];

                    if (!coinbase.vout[0].scriptPubKey.IsPayToCryptoCondition())
                    {
                        if (!std::get<1>(elem).empty() && std::get<1>(elem) != std::string(coinbaseaddr))
                        {
                            LOGSTREAMFN("marmara", CCLOG_ERROR, stream << "coinbase normal addr changed, storedaddr=" << std::get<1>(elem) << " curr addr=" << coinbaseaddr << " h=" << i << std::endl);
                        }
                        p1 = std::string(coinbaseaddr);
                        p2 = std::get<2>(elem) + coinbase.vout[0].nValue;
                        p3 = std::get<3>(elem);
                        p4 = std::get<4>(elem);
                        //LOGSTREAMFN("marmara", CCLOG_DEBUG1, stream << "coinbase normal addr=" << coinbaseaddr << " h=" << i << std::endl);
                    }
                    else
                    {
                        if (!std::get<3>(elem).empty() && std::get<3>(elem) != std::string(coinbaseaddr))
                        {
                            LOGSTREAMFN("marmara", CCLOG_ERROR, stream << "coinbase normal addr changed, storedaddr=" << std::get<3>(elem) << " curr addr=" << coinbaseaddr << " h=" << i << std::endl);
                        }
                        p1 = std::get<1>(elem);
                        p2 = std::get<2>(elem);
                        p3 = std::string(coinbaseaddr);
                        p4 = std::get<4>(elem) + coinbase.vout[0].nValue;
                    }
                    uint32_t segid = komodo_segid32(staketxaddr) & 0x3f;
                    mapStat[std::string(stakeaddr)] = std::make_tuple(isBoosted, p1, p2, p3, p4, std::get<5>(elem) + 1, segid);
                }
                //}
                //}
            }
            else
                LOGSTREAMFN("marmara", CCLOG_ERROR, stream << "not a pos block" << " h=" << i << std::endl);
        }
    }

    for (const auto &eStat : mapStat)
    {
        UniValue elem(UniValue::VOBJ);

        elem.push_back(Pair("StakeTxAddress", eStat.first));
        elem.push_back(Pair("IsBoosted", std::get<0>(eStat.second)));
        elem.push_back(Pair("CoinbaseNormalAddress", std::get<1>(eStat.second)));
        elem.push_back(Pair("CoinbaseNormalAmount", std::get<2>(eStat.second)));
        elem.push_back(Pair("CoinbaseActivatedAddress", std::get<3>(eStat.second)));
        elem.push_back(Pair("CoinbaseActivatedAmount", std::get<4>(eStat.second)));
        elem.push_back(Pair("StakeTxCount", std::get<5>(eStat.second)));
        elem.push_back(Pair("segid", static_cast<uint64_t>(std::get<6>(eStat.second))));
        array.push_back(elem);
    }

    result.push_back(Pair("result", "success"));
    result.push_back(Pair("BeginHeight", beginHeight));
    result.push_back(Pair("EndHeight", endHeight));
    result.push_back(Pair("StakingStat", array));
    return result;
}
