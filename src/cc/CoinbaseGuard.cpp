/********************************************************************
 * (C) 2018 Michael Toutonghi
 * 
 * Distributed under the MIT software license, see the accompanying
 * file COPYING or http://www.opensource.org/licenses/mit-license.php.
 * 
 * This crypto-condition eval solves the problem of nothing-at-stake
 * in a proof of stake consensus system.
 * 
 */

#include "CoinbaseGuard.h"
#include "script/script.h"
#include "main.h"

extern int32_t VERUS_MIN_STAKEAGE;

bool UnpackStakeOpRet(const CTransaction &stakeTx, std::vector<std::vector<unsigned char>> &vData)
{
    bool isValid = stakeTx.vout[stakeTx.vout.size() - 1].scriptPubKey.GetOpretData(vData);

    if (isValid && vData.size() == 1 && vData[0][0] > 0 && vData[0][0] < 4)
    {
        CScript data = CScript(vData[0].begin(), vData[0].end());
        vData.clear();

        uint32_t bytesTotal;
        CScript::const_iterator pc = data.begin();
        std::vector<unsigned char> vch = std::vector<unsigned char>();
        opcodetype op;
        bool moreData = true;

        for (bytesTotal = vch.size(); 
             bytesTotal <= nMaxDatacarrierBytes && !(isValid = (pc == data.end())) && (moreData = data.GetOp(pc, op, vch)) && (op > 0 && op <= OP_PUSHDATA4); 
             bytesTotal += vch.size())
        {
            vData.push_back(vch);
        }
        
        // if we ran out of data, we're ok
        if (isValid)

        if (isValid && (vData.size() >= CStakeParams::STAKE_MINPARAMS) && (vData.size() <= CStakeParams::STAKE_MAXPARAMS))
        {
            return true;
        }
    }

    return false;
}

CStakeParams::CStakeParams(std::vector<std::vector<unsigned char>> vData)
{
    // A stake OP_RETURN contains:
    // 1. source block height in little endian 32 bit
    // 2. target block height in little endian 32 bit
    // 3. 32 byte prev block hash
    // 4. alternate 20 byte pubkey hash, 33 byte pubkey, or not present to use same as stake destination

    srcHeight = 0;
    blkHeight = 0;
    if (vData[0].size() == 1 && 
        vData[0][0] == OPRETTYPE_STAKEPARAMS && vData[1].size() <= 4 && 
        vData[2].size() <= 4 && 
        vData[3].size() == sizeof(prevHash) &&
        (vData.size() == STAKE_MINPARAMS || (vData.size() == STAKE_MAXPARAMS && vData[4].size() == 33)))
    {
        for (auto ch : vData[1])
        {
            srcHeight = srcHeight << 8 | ch;
        }
        for (auto ch : vData[2])
        {
            blkHeight = blkHeight << 8 | ch;
        }

        prevHash = uint256(vData[3]);

        if (vData.size() == 4)
        {
            pk = CPubKey();
        }
        else if (vData[4].size() == 33)
        {
            pk = CPubKey(vData[4]);
            if (!pk.IsValid())
            {
                // invalidate
                srcHeight = 0;
            }
        }
        else
        {
            // invalidate
            srcHeight = 0;
        }
    }
}

// this validates everything, except the PoS eligibility and the actual stake spend. the only time it matters
// is to validate a properly formed stake transaction for either pre-check before PoS validity check, or to
// validate the stake transaction on a fork that will be used to spend a winning stake that cheated by being posted
// on two fork chains
bool ValidateStakeTransaction(const CTransaction &stakeTx, CStakeParams &stakeParams)
{
    std::vector<std::vector<unsigned char>> vData = std::vector<std::vector<unsigned char>>();

    // a valid stake transaction has one input and two outputs, one output is the monetary value and one is an op_ret with CStakeParams
    // stake output #1 must be P2PK or P2PKH, unless a delegate for the coinbase is specified

    bool isValid = false;;
    if (stakeTx.vin.size() == 1 && 
        stakeTx.vout.size() == 2 && 
        stakeTx.vout[0].nValue > 0 && 
        stakeTx.vout[1].scriptPubKey.IsOpReturn() && 
        UnpackStakeOpRet(stakeTx, vData))
    {
        stakeParams = CStakeParams(vData);
        if (stakeParams.IsValid())
        {
            // if we have gotten this far and are still valid, we need to validate everything else
            // even if the utxo is spent, this can succeed, as it only checks that is was ever valid
            CTransaction srcTx = CTransaction();
            uint256 blkHash = uint256();
            txnouttype txType;
            CBlockIndex *pindex;
            if (isValid && myGetTransaction(stakeTx.vin[0].prevout.hash, srcTx, blkHash))
            {
                isValid = false;
                if ((pindex = mapBlockIndex[blkHash]) != NULL)
                {
                    std::vector<std::vector<unsigned char>> vAddr = std::vector<std::vector<unsigned char>>();

                    if (stakeParams.srcHeight == pindex->GetHeight() && 
                        (stakeParams.blkHeight - stakeParams.srcHeight >= VERUS_MIN_STAKEAGE) &&
                        Solver(srcTx.vout[stakeTx.vin[0].prevout.n].scriptPubKey, txType, vAddr))
                    {
                        if (txType == TX_PUBKEY && !stakeParams.pk.IsValid())
                        {
                            stakeParams.pk = CPubKey(vAddr[0]);
                        }
                        if ((txType == TX_PUBKEY) || (txType == TX_PUBKEYHASH && stakeParams.pk.IsFullyValid()))
                        {
                            auto consensusBranchId = CurrentEpochBranchId(stakeParams.blkHeight, Params().GetConsensus());
                            isValid = VerifyScript(stakeTx.vin[0].scriptSig, 
                                                   srcTx.vout[stakeTx.vin[0].prevout.n].scriptPubKey, 
                                                   STANDARD_SCRIPT_VERIFY_FLAGS + SCRIPT_VERIFY_SIGPUSHONLY, 
                                                   BaseSignatureChecker(), 
                                                   consensusBranchId);
                        }
                    }
                }
            }
            else
            {
                isValid = false;
            }
        }
    }
    return isValid;
}

bool MakeGuardedOutput(CAmount value, CPubKey &dest, CTransaction &stakeTx, CTxOut &vout)
{
    CCcontract_info *cp, C;
    cp = CCinit(&C,EVAL_COINBASEGUARD);

    CPubKey ccAddress = CPubKey(ParseHex(cp->CChexstr));

    // return an output that is bound to the stake transaction and can be spent by presenting either a signed condition by the original 
    // destination address or a properly signed stake transaction of the same utxo on a fork
    vout = MakeCC1of2vout(EVAL_COINBASEGUARD, value, dest, ccAddress);

    // add parameters to scriptPubKey
    COptCCParams p = COptCCParams(COptCCParams::VERSION, EVAL_COINBASEGUARD, 1, 2);

    std::vector<unsigned char> a1, a2;
    CKeyID id1 = dest.GetID();
    CKeyID id2 = ccAddress.GetID();
    a1 = std::vector<unsigned char>(id1.begin(), id1.end());
    a2 = std::vector<unsigned char>(id2.begin(), id2.end());

    // version
    // utxo source hash
    // utxo source output
    // destination's pubkey
    CKeyID key = dest.GetID();
    vout.scriptPubKey << p.AsVector() << OP_DROP
                      << a1 << OP_DROP << a2 << OP_DROP
                      << std::vector<unsigned char>(stakeTx.vin[0].prevout.hash.begin(), stakeTx.vin[0].prevout.hash.end()) << OP_DROP
                      << stakeTx.vin[0].prevout.n << OP_DROP;

    return false;
}

// this creates a spend using a stake transaction
bool MakeGuardedSpend(CTxIn &vin, CPubKey &dest, CTransaction &pCheater)
{
    CCcontract_info *cp,C;

    cp = CCinit(&C,EVAL_COINBASEGUARD);
    CC cc;
    vin.scriptSig = CCPubKey(MakeCCcond1of2(EVAL_COINBASEGUARD, dest, CPubKey(ParseHex(cp->CChexstr))));
}

bool CoinbaseGuardValidate(struct CCcontract_info *cp, Eval* eval, const CTransaction &tx, uint32_t nIn)
{
    // This also supports a variable blockstomaturity option for backward feature compatibility
    // validate this spend of a transaction with it being past any applicable time lock and one of the following statements being true:
    //  1. the spend is signed by the original output destination's private key and normal payment requirements, spends as normal
    //  2. the spend is signed by the private key of the CoinbaseGuard contract and pushes a signed stake transaction
    //     with the same exact utxo source, a target block height of later than that of this tx that is also targeting a fork
    //     of the chain

    // first, check to see if the spending contract is signed by the default destination address
    // if so, success and we are done

    // get preConditions and parameters
    std::vector<std::vector<unsigned char>> preConditions = std::vector<std::vector<unsigned char>>();
    std::vector<std::vector<unsigned char>> params = std::vector<std::vector<unsigned char>>();

    if (GetCCParams(eval, tx, nIn, preConditions, params))
    {
        CC *cc = GetCryptoCondition(tx.vin[nIn].scriptSig);

        printf("CryptoCondition code %x\n", *cc->code);
        // check any applicable time lock
        // determine who signed
        // if from receiver's priv key, success
        // if from contract priv key:
        //   if data provided is valid stake spend of same utxo targeting same or later block height on a fork:
        //     return success
        //   endif
        // endif
        // return fail
        cc_free(cc);
    }
    return true;
}

UniValue CoinbaseGuardInfo()
{
    UniValue result(UniValue::VOBJ); char numstr[64];
    CMutableTransaction mtx;
    CPubKey pk; 

    CCcontract_info *cp,C;

    cp = CCinit(&C,EVAL_COINBASEGUARD);

    result.push_back(Pair("result","success"));
    result.push_back(Pair("name","CoinbaseGuard"));

    // all UTXOs to the contract address that are to any of the wallet addresses are to us
    // each is spendable as a normal transaction, but the spend may fail if it gets spent out
    // from under us
    pk = GetUnspendable(cp,0);
    return(result);
}

