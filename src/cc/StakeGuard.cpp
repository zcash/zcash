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

#include "StakeGuard.h"
#include "script/script.h"
#include "main.h"
#include "hash.h"

#include "streams.h"

extern int32_t VERUS_MIN_STAKEAGE;

bool IsData(opcodetype opcode)
{
    return (opcode >= 0 && opcode <= OP_PUSHDATA4) || (opcode >= OP_1 && opcode <= OP_16);
}

bool UnpackStakeOpRet(const CTransaction &stakeTx, std::vector<std::vector<unsigned char>> &vData)
{
    bool isValid = stakeTx.vout[stakeTx.vout.size() - 1].scriptPubKey.GetOpretData(vData);

    if (isValid && vData.size() == 1)
    {
        CScript data = CScript(vData[0].begin(), vData[0].end());
        vData.clear();

        uint32_t bytesTotal;
        CScript::const_iterator pc = data.begin();
        std::vector<unsigned char> vch = std::vector<unsigned char>();
        opcodetype op;
        bool moreData = true;

        for (bytesTotal = vch.size(); 
             bytesTotal <= nMaxDatacarrierBytes && !(isValid = (pc == data.end())) && (moreData = data.GetOp(pc, op, vch)) && IsData(op); 
             bytesTotal += vch.size())
        {
            if (op >= OP_1 && op <= OP_16)
            {
                vch.resize(1);
                vch[0] = (op - OP_1) + 1;
            }
            vData.push_back(vch);
        }
        
        // if we ran out of data, we're ok
        if (isValid && (vData.size() >= CStakeParams::STAKE_MINPARAMS) && (vData.size() <= CStakeParams::STAKE_MAXPARAMS))
        {
            return true;
        }
    }
    return false;
}

CStakeParams::CStakeParams(const std::vector<std::vector<unsigned char>> &vData)
{
    // A stake OP_RETURN contains:
    // 1. source block height in little endian 32 bit
    // 2. target block height in little endian 32 bit
    // 3. 32 byte prev block hash
    // 4. 33 byte pubkey, or not present to use same as stake destination

    srcHeight = 0;
    blkHeight = 0;
    if (vData[0].size() == 1 && 
        vData[0][0] == OPRETTYPE_STAKEPARAMS && vData[1].size() <= 4 && 
        vData[2].size() <= 4 && 
        vData[3].size() == sizeof(prevHash) &&
        (vData.size() == STAKE_MINPARAMS || (vData.size() == STAKE_MAXPARAMS && vData[4].size() == 33)))
    {
        for (int i = 0, size = vData[1].size(); i < size; i++)
        {
            srcHeight = srcHeight | vData[1][i] << (8 * i);
        }
        for (int i = 0, size = vData[2].size(); i < size; i++)
        {
            blkHeight = blkHeight | vData[2][i] << (8 * i);
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

bool GetStakeParams(const CTransaction &stakeTx, CStakeParams &stakeParams)
{
    std::vector<std::vector<unsigned char>> vData = std::vector<std::vector<unsigned char>>();

    //printf("opret stake script: %s\nvalue at scriptPubKey[0]: %x\n", stakeTx.vout[1].scriptPubKey.ToString().c_str(), stakeTx.vout[1].scriptPubKey[0]);

    if (stakeTx.vin.size() == 1 && 
        stakeTx.vout.size() == 2 && 
        stakeTx.vout[0].nValue > 0 && 
        stakeTx.vout[1].scriptPubKey.IsOpReturn() && 
        UnpackStakeOpRet(stakeTx, vData))
    {
        stakeParams = CStakeParams(vData);
        return stakeParams.IsValid();
    }
    return false;
}

// this validates the format of the stake transaction and, optionally, whether or not it is 
// properly signed to spend the source stake.
// it does not validate the relationship to a coinbase guard, PoS eligibility or the actual stake spend.
// the only time it matters
// is to validate a properly formed stake transaction for either pre-check before PoS validity check, or to
// validate the stake transaction on a fork that will be used to spend a winning stake that cheated by being posted
// on two fork chains
bool ValidateStakeTransaction(const CTransaction &stakeTx, CStakeParams &stakeParams, bool validateSig)
{
    std::vector<std::vector<unsigned char>> vData = std::vector<std::vector<unsigned char>>();

    // a valid stake transaction has one input and two outputs, one output is the monetary value and one is an op_ret with CStakeParams
    // stake output #1 must be P2PK or P2PKH, unless a delegate for the coinbase is specified
    if (GetStakeParams(stakeTx, stakeParams))
    {
        // if we have gotten this far and are still valid, we need to validate everything else
        // even if the utxo is spent, this can succeed, as it only checks that is was ever valid
        CTransaction srcTx = CTransaction();
        uint256 blkHash = uint256();
        txnouttype txType;
        CBlockIndex *pindex;
        if (myGetTransaction(stakeTx.vin[0].prevout.hash, srcTx, blkHash))
        {
            BlockMap::const_iterator it = mapBlockIndex.find(blkHash);
            if (it != mapBlockIndex.end() && (pindex = it->second) != NULL)
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

                        if (!validateSig || VerifyScript(stakeTx.vin[0].scriptSig, 
                                            srcTx.vout[stakeTx.vin[0].prevout.n].scriptPubKey, 
                                            MANDATORY_SCRIPT_VERIFY_FLAGS,
                                            TransactionSignatureChecker(&stakeTx, (uint32_t)0, srcTx.vout[stakeTx.vin[0].prevout.n].nValue),
                                            consensusBranchId))
                        {
                            return true;
                        }
                    }
                }
            }
        }
    }
    return false;
}

bool MakeGuardedOutput(CAmount value, CPubKey &dest, CTransaction &stakeTx, CTxOut &vout)
{
    CCcontract_info *cp, C;
    cp = CCinit(&C,EVAL_STAKEGUARD);

    CPubKey ccAddress = CPubKey(ParseHex(cp->CChexstr));

    // return an output that is bound to the stake transaction and can be spent by presenting either a signed condition by the original 
    // destination address or a properly signed stake transaction of the same utxo on a fork
    vout = MakeCC1of2vout(EVAL_STAKEGUARD, value, dest, ccAddress);

    std::vector<CPubKey> vPubKeys = std::vector<CPubKey>();
    vPubKeys.push_back(dest);
    vPubKeys.push_back(ccAddress);
    
    std::vector<std::vector<unsigned char>> vData = std::vector<std::vector<unsigned char>>();

    CVerusHashWriter hw = CVerusHashWriter(SER_GETHASH, PROTOCOL_VERSION);

    hw << stakeTx.vin[0].prevout.hash;
    hw << stakeTx.vin[0].prevout.n;

    uint256 utxo = hw.GetHash();
    vData.push_back(std::vector<unsigned char>(utxo.begin(), utxo.end()));

    CStakeParams p;
    if (GetStakeParams(stakeTx, p))
    {
        // prev block hash and height is here to make validation easy
        vData.push_back(std::vector<unsigned char>(p.prevHash.begin(), p.prevHash.end()));
        std::vector<unsigned char> height = std::vector<unsigned char>(4);
        for (int i = 0; i < 4; i++)
        {
            height[i] = (p.blkHeight >> (8 * i)) & 0xff;
        }
        vData.push_back(height);

        COptCCParams ccp = COptCCParams(COptCCParams::VERSION, EVAL_STAKEGUARD, 1, 2, vPubKeys, vData);

        vout.scriptPubKey << ccp.AsVector() << OP_DROP;
        return true;
    }
    return false;
}

// validates if a stake transaction is both valid and cheating, defined by:
// the same exact utxo source, a target block height of later than that of this tx that is also targeting a fork
// of the chain. we know the transaction is a coinbase
bool ValidateMatchingStake(const CTransaction &ccTx, uint32_t voutNum, const CTransaction &stakeTx, bool &cheating)
{
    // an invalid or non-matching stake transaction cannot cheat
    cheating = false;

    //printf("ValidateMatchingStake: ccTx.vin[0].prevout.hash: %s, ccTx.vin[0].prevout.n: %d\n", ccTx.vin[0].prevout.hash.GetHex().c_str(), ccTx.vin[0].prevout.n);

    if (ccTx.IsCoinBase())
    {
        CStakeParams p;
        if (ValidateStakeTransaction(stakeTx, p))
        {
            std::vector<std::vector<unsigned char>> vParams = std::vector<std::vector<unsigned char>>();
            CScript dummy;

            if (ccTx.vout[voutNum].scriptPubKey.IsPayToCryptoCondition(&dummy, vParams) && vParams.size() > 0)
            {
                COptCCParams ccp = COptCCParams(vParams[0]);
                if (ccp.IsValid() & ccp.vData.size() >= 3 && ccp.vData[2].size() <= 4)
                {
                    CVerusHashWriter hw = CVerusHashWriter(SER_GETHASH, PROTOCOL_VERSION);

                    hw << stakeTx.vin[0].prevout.hash;
                    hw << stakeTx.vin[0].prevout.n;
                    uint256 utxo = hw.GetHash();

                    uint32_t height = 0;
                    int i, dataLen = ccp.vData[2].size();
                    for (i = dataLen - 1; i >= 0; i--)
                    {
                        height = (height << 8) + ccp.vData[2][i];
                    }
                    // for debugging strange issue
                    // printf("iterator: %d, height: %d, datalen: %d\n", i, height, dataLen);

                    if (utxo == uint256(ccp.vData[0]))
                    {
                        if (p.prevHash != uint256(ccp.vData[1]) && p.blkHeight >= height)
                        {
                            cheating = true;
                            return true;
                        }
                        // if block height is equal and we are at the else, prevHash must have been equal
                        else if (p.blkHeight == height)
                        {
                            return true;                            
                        }
                    }
                }
            }
        }
    }
    return false;
}

// this attaches an opret to a mutable transaction that provides the necessary evidence of a signed, cheating stake transaction
bool MakeCheatEvidence(CMutableTransaction &mtx, const CTransaction &ccTx, uint32_t voutNum, const CTransaction &cheatTx)
{
    std::vector<unsigned char> vch;
    CDataStream s = CDataStream(SER_DISK, CLIENT_VERSION);
    bool isCheater = false;

    if (ValidateMatchingStake(ccTx, voutNum, cheatTx, isCheater) && isCheater)
    {
        CTxOut vOut = CTxOut();
        int64_t opretype_stakecheat = OPRETTYPE_STAKECHEAT;

        CScript vData = CScript();
        cheatTx.Serialize(s);
        vch = std::vector<unsigned char>(s.begin(), s.end());
        vData << opretype_stakecheat << vch;
        vch = std::vector<unsigned char>(vData.begin(), vData.end());
        vOut.scriptPubKey << OP_RETURN << vch;

        // printf("Script encoding inner:\n%s\nouter:\n%s\n", vData.ToString().c_str(), vOut.scriptPubKey.ToString().c_str());

        vOut.nValue = 0;
        mtx.vout.push_back(vOut);
    }
    return isCheater;
}

typedef struct ccFulfillmentCheck {
    std::vector<CPubKey> &vPK;
    std::vector<uint32_t> &vCount;
} ccFulfillmentCheck;

// to figure out which node is signed
int CCFulfillmentVisitor(CC *cc, struct CCVisitor visitor)
{
    //printf("cc_typeName: %s, cc_isFulfilled: %x, cc_isAnon: %x, cc_typeMask: %x, cc_condToJSONString:\n%s\n", 
    //       cc_typeName(cc), cc_isFulfilled(cc), cc_isAnon(cc), cc_typeMask(cc), cc_conditionToJSONString(cc));

    if (strcmp(cc_typeName(cc), "secp256k1-sha-256") == 0)
    {
        cJSON *json = cc_conditionToJSON(cc);
        if (json)
        {
            cJSON *pubKeyNode = json->child->next;
            if (strcmp(pubKeyNode->string, "publicKey") == 0)
            {
                ccFulfillmentCheck *pfc = (ccFulfillmentCheck *)(visitor.context);

                //printf("public key: %s\n", pubKeyNode->valuestring);
                CPubKey pubKey = CPubKey(ParseHex(pubKeyNode->valuestring));

                for (int i = 0; i < pfc->vPK.size(); i++)
                {
                    if (i < pfc->vCount.size() && (pfc->vPK[i] == pubKey))
                    {
                        pfc->vCount[i]++;
                    }
                }
            }
            cJSON_free(json);
        }
    }

    return 1;
}

int IsCCFulfilled(CC *cc, ccFulfillmentCheck *ctx)
{
    struct CCVisitor visitor = {&CCFulfillmentVisitor, NULL, 0, (void *)ctx};
    cc_visit(cc, visitor);

    //printf("count key 1: %d, count key 2: %d\n", ctx->vCount[0], ctx->vCount[1]);
    return ctx->vCount[0];
}

bool StakeGuardValidate(struct CCcontract_info *cp, Eval* eval, const CTransaction &tx, uint32_t nIn)
{
    // WARNING: this has not been tested combined with time locks
    // validate this spend of a transaction with it being past any applicable time lock and one of the following statements being true:
    //  1. the spend is signed by the original output destination's private key and normal payment requirements, spends as normal
    //  2. the spend is signed by the private key of the StakeGuard contract and pushes a signed stake transaction
    //     with the same exact utxo source, a target block height of later than or equal to this tx, and a different prevBlock hash

    // first, check to see if the spending contract is signed by the default destination address
    // if so, success and we are done

    // get preConditions and parameters
    std::vector<std::vector<unsigned char>> preConditions = std::vector<std::vector<unsigned char>>();
    std::vector<std::vector<unsigned char>> params = std::vector<std::vector<unsigned char>>();
    CTransaction txOut;

    bool signedByFirstKey = false;
    bool validCheat = false;

    CC *cc = GetCryptoCondition(tx.vin[nIn].scriptSig);

    if (cc)
    {
        COptCCParams ccp;
        signedByFirstKey = false;
        validCheat = false;

        // tx is the spending tx, the cc transaction comes back in txOut
        if (GetCCParams(eval, tx, nIn, txOut, preConditions, params))
        {
            if (preConditions.size() > 0)
            {
                ccp = COptCCParams(preConditions[0]);
            }

            if (ccp.IsValid() && ccp.m == 1 && ccp.n == 2 && ccp.vKeys.size() == 2)
            {
                std::vector<uint32_t> vc = {0, 0};
                ccFulfillmentCheck fc = {ccp.vKeys, vc};

                signedByFirstKey = (IsCCFulfilled(cc, &fc) != 0);

                if ((!signedByFirstKey && ccp.evalCode == EVAL_STAKEGUARD && ccp.vKeys.size() == 2 && ccp.version == COptCCParams::VERSION) &&
                    params.size() == 2 && params[0].size() > 0 && params[0][0] == OPRETTYPE_STAKECHEAT)
                {
                    CDataStream s = CDataStream(std::vector<unsigned char>(params[1].begin(), params[1].end()), SER_DISK, CLIENT_VERSION);
                    bool checkOK = false;
                    CTransaction cheatTx;
                    try
                    {
                        cheatTx.Unserialize(s);
                        checkOK = true;
                    }
                    catch (...)
                    {
                    }
                    if (checkOK && !ValidateMatchingStake(txOut, tx.vin[0].prevout.n, cheatTx, validCheat))
                    {
                        validCheat = false;
                    }
                }
            }
        }
        cc_free(cc);
    }
    if (!(signedByFirstKey || validCheat))
    {
        eval->Error("error reading coinbase or spending proof invalid\n");
        return false;
    }
    else return true;
}

bool IsStakeGuardInput(const CScript &scriptSig)
{
    printf("IsStakeGuardInput: not implemented");
    return false;
}

UniValue StakeGuardInfo()
{
    UniValue result(UniValue::VOBJ); char numstr[64];
    CMutableTransaction mtx;
    CPubKey pk; 

    CCcontract_info *cp,C;

    cp = CCinit(&C,EVAL_STAKEGUARD);

    result.push_back(Pair("result","success"));
    result.push_back(Pair("name","StakeGuard"));

    // all UTXOs to the contract address that are to any of the wallet addresses are to us
    // each is spendable as a normal transaction, but the spend may fail if it gets spent out
    // from under us
    pk = GetUnspendable(cp,0);
    return(result);
}

