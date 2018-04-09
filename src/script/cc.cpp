#include "cryptoconditions/include/cryptoconditions.h"
#include "script/cc.h"


bool IsCryptoConditionsEnabled()
{
    return 0 != ASSETCHAINS_CC;
}


bool IsSupportedCryptoCondition(const CC *cond)
{
    int mask = cc_typeMask(cond);

    if (mask & ~CCEnabledTypes) return false;

    // Also require that the condition have at least one signable node
    if (!(mask & CCSigningNodes)) return false;

    return true;
}


bool IsSignedCryptoCondition(const CC *cond)
{
    if (!cc_isFulfilled(cond)) return false;
    if (1 << cc_typeId(cond) & CCSigningNodes) return true;
    if (cc_typeId(cond) == CC_Threshold)
        for (int i=0; i<cond->size; i++)
            if (IsSignedCryptoCondition(cond->subconditions[i])) return true;
    return false;
}


static unsigned char* CopyPubKey(CPubKey pkIn)
{
    unsigned char* pk = (unsigned char*) malloc(33);
    memcpy(pk, pkIn.begin(), 33);  // TODO: compressed?
    return pk;
}


CC* CCNewThreshold(int t, std::vector<CC*> v)
{
    CC *cond = cc_new(CC_Threshold);
    cond->threshold = t;
    cond->size = v.size();
    cond->subconditions = (CC**) calloc(v.size(), sizeof(CC*));
    memcpy(cond->subconditions, v.data(), v.size() * sizeof(CC*));
    return cond;
}


CC* CCNewSecp256k1(CPubKey k)
{
    CC *cond = cc_new(CC_Secp256k1);
    cond->publicKey = CopyPubKey(k);
    return cond;
}


CC* CCNewEval(std::vector<unsigned char> code)
{
    CC *cond = cc_new(CC_Eval);
    cond->code = (unsigned char*) malloc(code.size());
    memcpy(cond->code, code.data(), code.size());
    cond->codeLength = code.size();
    return cond;
}


CScript CCPubKey(const CC *cond)
{
    unsigned char buf[1000];
    size_t len = cc_conditionBinary(cond, buf);
    return CScript() << std::vector<unsigned char>(buf, buf+len) << OP_CHECKCRYPTOCONDITION;
}


CScript CCSig(const CC *cond)
{
    unsigned char buf[10000];
    size_t len = cc_fulfillmentBinary(cond, buf, 10000);
    auto ffill = std::vector<unsigned char>(buf, buf+len);
    ffill.push_back(1);  // SIGHASH_ALL
    return CScript() << ffill;
}


std::string CCShowStructure(CC *cond)
{
    std::string out;
    if (cc_isAnon(cond)) {
        out = "A" + std::to_string(cc_typeId(cond));
    }
    else if (cc_typeId(cond) == CC_Threshold) {
        out += "(" + std::to_string(cond->threshold) + " of ";
        for (int i=0; i<cond->size; i++) {
            out += CCShowStructure(cond->subconditions[i]);
            if (i < cond->size - 1) out += ",";
        }
        out += ")";
    }
    else {
        out = std::to_string(cc_typeId(cond));
    }
    return out;
}


CC* CCPrune(CC *cond)
{
    std::vector<unsigned char> ffillBin;
    GetPushData(CCSig(cond), ffillBin);
    return cc_readFulfillmentBinary(ffillBin.data(), ffillBin.size()-1);
}


bool GetPushData(const CScript &sig, std::vector<unsigned char> &data)
{
    opcodetype opcode;
    auto pc = sig.begin();
    if (sig.GetOp(pc, opcode, data)) return opcode > OP_0 && opcode <= OP_PUSHDATA4;
    return false;
}


bool GetOpReturnData(const CScript &sig, std::vector<unsigned char> &data)
{
    auto pc = sig.begin();
    opcodetype opcode;
    if (sig.GetOp2(pc, opcode, NULL))
        if (opcode == OP_RETURN)
            if (sig.GetOp(pc, opcode, data))
                return opcode > OP_0 && opcode <= OP_PUSHDATA4;
    return false;
}
