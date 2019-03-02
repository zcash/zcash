#ifndef HEIR_VALIDATE_H
#define HEIR_VALIDATE_H

#include "CCinclude.h"
#include "CCHeir.h"

#define IS_CHARINSTR(c, str) (std::string(str).find((char)(c)) != std::string::npos)

// makes coin initial tx opret
vscript_t EncodeHeirCreateOpRet(uint8_t funcid, CPubKey ownerPubkey, CPubKey heirPubkey, int64_t inactivityTimeSec, std::string heirName, std::string memo);
vscript_t EncodeHeirOpRet(uint8_t funcid,  uint256 fundingtxid, uint8_t isHeirSpendingBegan);

uint256 FindLatestFundingTx(uint256 fundingtxid, uint256 &tokenid, CScript& opRetScript, uint8_t &isHeirSpendingBegan);
//uint8_t DecodeHeirOpRet(CScript scriptPubKey, uint256& fundingtxid, uint8_t &isHeirSpendingBegan, bool noLogging = false);
//uint8_t DecodeHeirOpRet(CScript scriptPubKey, CPubKey& ownerPubkey, CPubKey& heirPubkey, int64_t& inactivityTime, std::string& heirName,  bool noLogging = false);
uint8_t DecodeHeirEitherOpRet(CScript scriptPubKey, uint256 &tokenid, CPubKey& ownerPubkey, CPubKey& heirPubkey, int64_t& inactivityTime, std::string& heirName, std::string& memo, bool noLogging = false);
uint8_t DecodeHeirEitherOpRet(CScript scriptPubKey, uint256 &tokenid, uint256 &fundingTxidInOpret, uint8_t &hasHeirSpendingBegun, bool noLogging = false);

inline static bool isMyFuncId(uint8_t funcid) { return IS_CHARINSTR(funcid, "FAC"); }
inline static bool isSpendingTx(uint8_t funcid) { return (funcid == 'C'); }

// helper class to allow polymorphic behaviour for HeirXXX() functions in case of coins
class CoinHelper {
public:
    
    static uint8_t getMyEval() { return EVAL_HEIR; }
    static int64_t addOwnerInputs(uint256 dummyid, CMutableTransaction& mtx, CPubKey ownerPubkey, int64_t total, int32_t maxinputs) {
        return AddNormalinputs(mtx, ownerPubkey, total, maxinputs);
    }
    
    static CScript makeCreateOpRet(uint256 dummyid, std::vector<CPubKey> dummyPubkeys, CPubKey ownerPubkey, CPubKey heirPubkey, int64_t inactivityTimeSec, std::string heirName, std::string memo) {
        return CScript() << OP_RETURN << EncodeHeirCreateOpRet((uint8_t)'F', ownerPubkey, heirPubkey, inactivityTimeSec, heirName, memo);
    }
    static CScript makeAddOpRet(uint256 dummyid, std::vector<CPubKey> dummyPubkeys, uint256 fundingtxid, uint8_t isHeirSpendingBegan) {
        return CScript() << OP_RETURN << EncodeHeirOpRet((uint8_t)'A', fundingtxid, isHeirSpendingBegan);
    }
    static CScript makeClaimOpRet(uint256 dummyid, std::vector<CPubKey> dummyPubkeys, uint256 fundingtxid, uint8_t isHeirSpendingBegan) {
        return CScript() << OP_RETURN << EncodeHeirOpRet((uint8_t)'C', fundingtxid, isHeirSpendingBegan);
    }
    static CTxOut make1of2Vout(int64_t amount, CPubKey ownerPubkey, CPubKey heirPubkey) {
        return MakeCC1of2vout(EVAL_HEIR, amount, ownerPubkey, heirPubkey);
    }
    static CTxOut makeUserVout(int64_t amount, CPubKey myPubkey) {
        return CTxOut(amount, CScript() << ParseHex(HexStr(myPubkey)) << OP_CHECKSIG);
    }
    /*	static CTxOut makeClaimerVout(int64_t amount, CPubKey myPubkey) {
     return CTxOut(amount, CScript() << ParseHex(HexStr(myPubkey)) << OP_CHECKSIG);
     } */
    static bool GetCoinsOrTokensCCaddress1of2(char *coinaddr, CPubKey ownerPubkey, CPubKey heirPubkey) {
        struct CCcontract_info *cpHeir, heirC;
        cpHeir = CCinit(&heirC, EVAL_HEIR);
        return GetCCaddress1of2(cpHeir, coinaddr, ownerPubkey, heirPubkey);
    }
    static void CCaddrCoinsOrTokens1of2set(struct CCcontract_info *cp, CPubKey ownerPubkey, CPubKey heirPubkey, char *coinaddr)
    {
        uint8_t mypriv[32];
        Myprivkey(mypriv);
        CCaddr1of2set(cp, ownerPubkey, heirPubkey,mypriv, coinaddr);
    }
};

// helper class to allow polymorphic behaviour for HeirXXX() functions in case of tokens
class TokenHelper {
public:
    static uint8_t getMyEval() { return EVAL_TOKENS; }
    static int64_t addOwnerInputs(uint256 tokenid, CMutableTransaction& mtx, CPubKey ownerPubkey, int64_t total, int32_t maxinputs) {
        struct CCcontract_info *cpHeir, heirC;
        cpHeir = CCinit(&heirC, EVAL_TOKENS);
        return AddTokenCCInputs(cpHeir, mtx, ownerPubkey, tokenid, total, maxinputs);
    }
    
    static CScript makeCreateOpRet(uint256 tokenid, std::vector<CPubKey> voutTokenPubkeys, CPubKey ownerPubkey, CPubKey heirPubkey, int64_t inactivityTimeSec, std::string heirName, std::string memo) {
        return EncodeTokenOpRet(tokenid, voutTokenPubkeys,
                                std::make_pair(OPRETID_HEIRDATA, EncodeHeirCreateOpRet((uint8_t)'F', ownerPubkey, heirPubkey, inactivityTimeSec, heirName, memo)));
    }
    static CScript makeAddOpRet(uint256 tokenid, std::vector<CPubKey> voutTokenPubkeys, uint256 fundingtxid, uint8_t isHeirSpendingBegan) {
        return EncodeTokenOpRet(tokenid, voutTokenPubkeys,
                                std::make_pair(OPRETID_HEIRDATA, EncodeHeirOpRet((uint8_t)'A', fundingtxid, isHeirSpendingBegan)));
    }
    static CScript makeClaimOpRet(uint256 tokenid, std::vector<CPubKey> voutTokenPubkeys, uint256 fundingtxid, uint8_t isHeirSpendingBegan) {
        return EncodeTokenOpRet(tokenid, voutTokenPubkeys,
                                std::make_pair(OPRETID_HEIRDATA, EncodeHeirOpRet((uint8_t)'C', fundingtxid, isHeirSpendingBegan)));
    }
    
    static CTxOut make1of2Vout(int64_t amount, CPubKey ownerPubkey, CPubKey heirPubkey) {
        return MakeTokensCC1of2vout(EVAL_HEIR, amount, ownerPubkey, heirPubkey);
    }
    static CTxOut makeUserVout(int64_t amount, CPubKey myPubkey) {
        return MakeCC1vout(EVAL_TOKENS, amount, myPubkey);  // yes EVAL_TOKENS
    }
    /*	static CTxOut makeClaimerVout(int64_t amount, CPubKey myPubkey) {
     return MakeCC1vout(EVAL_TOKENS, amount, myPubkey); // yes EVAL_TOKENS
     } */
    static bool GetCoinsOrTokensCCaddress1of2(char *coinaddr, CPubKey ownerPubkey, CPubKey heirPubkey) {
        struct CCcontract_info *cpHeir, heirC;
        cpHeir = CCinit(&heirC, EVAL_HEIR);
        return GetTokensCCaddress1of2(cpHeir, coinaddr, ownerPubkey, heirPubkey);
    }
    
    static void CCaddrCoinsOrTokens1of2set(struct CCcontract_info *cp, CPubKey ownerPubkey, CPubKey heirPubkey, char *coinaddr) {
        
        CCaddrTokens1of2set(cp, ownerPubkey, heirPubkey, coinaddr);
    }
};


/**
 * Small framework for vins and vouts validation implementing a variation of 'chain of responsibility' pattern:
 * It consists of two classes CInputValidationPlan and COutputValidationPlan which both are configured with an array of vectors of validators
 * (These validators are derived from the class CValidatorBase).
 *
 * A example of a validator may verify for a vout if its public key corresponds to the public key which is stored in opreturn.
 * Or, vin validator may check if this vin depicts correctly to the CC contract's address.
 *
 * For validating vins CInputValidator additionally is provided with an instance of a class derived from the CInputIdentifierBase class.
 * this identifier class allows to select identical vins (for example, normal vins or cc input vins) and apply validators from the corresponding vector to it.
 * Note: CInputValidator treats that at least one identified vin should be present, otherwise it returns eval->invalid() and false.
 *
 * For validating vouts COutputValidator is configured for each vector of validators with the vout index to which these validators are applied
 * (see constructors of both CInputValidator and COutputValidator)
 */

/**
 * base class for all validators
 */
class CValidatorBase
{
public:
    CValidatorBase(CCcontract_info* cp) : m_cp(cp) {}
    virtual bool isVinValidator() const = 0;
    virtual bool validateVin(CTxIn vin, std::vector<CTxOut> prevVout, int32_t prevN, std::string& message) const = 0;
    virtual bool validateVout(CTxOut vout, int32_t vout_n, std::string& message) const = 0;
    
protected:
    CCcontract_info * m_cp;
};

/**
 * Base class for classes which identify vins as normal or cc inputs
 */
class CInputIdentifierBase
{
public:
    CInputIdentifierBase(CCcontract_info* cp) : m_cp(cp) {}
    virtual std::string inputName() const = 0;
    virtual bool identifyInput(CTxIn vin) const = 0;
protected:
    CCcontract_info * m_cp;
};

/**
 * Encapsulates an array containing rows of validators
 * Each row is a vector of validators (zero is possible) for validating vins or prev tx's vouts
 * this validation plan is used for validating tx inputs
 */
template <typename TValidatorBase>
class CInputValidationPlan
{
    using ValidatorsRow = std::vector<TValidatorBase*>;
    
public:
    
    // Pushes a row of validators for validating a vin or vout
    // @param CInputIdentifierBase* pointer to class-identifier which determines several identical adjacent vins (like in schema "vin.0+: normal inputs")
    // @param pargs parameter pack of zero or more pointer to validator objects
    // Why pointers? because we store the base class in validators' row and then call its virtual functions
    template <typename TValidatorBaseX, typename... ARGS>
    void pushValidators(CInputIdentifierBase *identifier, ARGS*... pargs) // validators row passed as variadic arguments CValidatorX *val1, CValidatorY *val2 ...
    {
        ValidatorsRow vValidators({ (TValidatorBase*)pargs... });
        m_arrayValidators.push_back(std::make_pair(identifier, vValidators));
    }
    
    // validate tx inputs and corresponding prev tx vouts
    bool validate(const CTransaction& tx, Eval* eval)
    {
        std::string message = "<empty>";
        //std::cerr << "CInputValidationPlan::validate() starting vins validation..." << std::endl;
        
        int32_t ival = 0;
        int32_t iv = 0;
        int32_t numv = tx.vin.size();
        int32_t numValidators = m_arrayValidators.size();
        
        // run over vins:
        while (iv < numv && ival < numValidators) {
            
            int32_t identifiedCount = 0;
            CInputIdentifierBase *identifier = m_arrayValidators[ival].first;
            //					check if this is 'our' input:
            while (iv < numv && identifier->identifyInput(tx.vin[iv])) {
                
                // get prev tx:
                CTransaction prevTx, *pPrevTxOrNull = NULL;
                uint256 hashBlock;
                
                if (!eval->GetTxUnconfirmed(tx.vin[iv].prevout.hash, prevTx, hashBlock)) {
                    std::ostringstream stream;
                    stream << "can't find vinTx for vin=" << iv << ".";
                    return eval->Invalid(stream.str().c_str());
                }
                pPrevTxOrNull = &prevTx;  // TODO: get prev tx only if it required (i.e. if vout validators are present)
                
                // exec 'validators' from validator row of ival index, for tx.vin[iv]
                if (!execValidatorsInRow(&tx, pPrevTxOrNull, iv, ival, message)) {
                    std::ostringstream stream;
                    stream << "invalid tx vin[" << iv << "]:" << message;
                    return eval->Invalid(stream.str().c_str());				// ... if not, return 'invalid'
                }
                
                identifiedCount++;		// how many vins we identified
                iv++;					// advance to the next vin
            }
            
            // CInputValidationPlan treats that there must be at least one identified vin for configured validators' row
            // like in 'vin.0: normal input'
            if (identifiedCount == 0) {
                std::ostringstream stream;
                stream << "can't find required vins for " << identifier->inputName() << ".";
                return eval->Invalid(stream.str().c_str());
            }
            
            ival++;   // advance to the next validator row
            // and it will try the same vin with the new CInputIdentifierBase and validators row
        }
        
        // validation is successful if all validators have been used (i.e. ival = numValidators)
        if (ival < numValidators) {
            std::cerr << "CInputValidationPlan::validate() incorrect tx" << " ival=" << ival << " numValidators=" << numValidators << std::endl;
            return eval->Invalid("incorrect tx structure: not all required vins are present.");
        }
        
        //std::cerr << "CInputValidationPlan::validate() returns with true" << std::endl;
        return true;
    }
    
private:
    // Executes validators from the requested row of validators (selected by iValidators) for selected vin or vout (selected by iv)
    bool execValidatorsInRow(const CTransaction* pTx, const CTransaction* pPrevTx, int32_t iv, int32_t ival, std::string& refMessage) const
    {
        // check boundaries:
        if (ival < 0 || ival >= m_arrayValidators.size()) {
            std::cerr << "CInputValidationPlan::execValidatorsInRow() internal error: incorrect param ival=" << ival << " size=" << m_arrayValidators.size();
            refMessage = "internal error: incorrect param ival index";
            return false;
        }
        
        if (iv < 0 || iv >= pTx->vin.size()) {
            std::cerr << "CInputValidationPlan::execValidatorsInRow() internal error: incorrect param iv=" << iv << " size=" << m_arrayValidators.size();
            refMessage = "internal error: incorrect param iv index";
            return false;
        }
        
        // get requested row of validators:
        ValidatorsRow vValidators = m_arrayValidators[ival].second;
        
        //std::cerr << "CInputValidationPlan::execValidatorsInRow() calling validators" << " for vin iv=" << iv << " ival=" << ival << std::endl;
        
        for (auto v : vValidators) {
            bool result;
            
            if (v->isVinValidator())
                // validate this vin and previous vout:
                result = v->validateVin(pTx->vin[iv], pPrevTx->vout, pTx->vin[iv].prevout.n, refMessage);
            else
                // if it is vout validator pass the previous tx vout:
                result = v->validateVout( pPrevTx->vout[pTx->vin[iv].prevout.n], pTx->vin[iv].prevout.n, refMessage);
            if (!result) {
                return result;
            }
        }
        return true; // validation OK
    }
    
    
private:
    //std::map<CInputIdentifierBase*, ValidatorsRow> m_arrayValidators;
    std::vector< std::pair<CInputIdentifierBase*, ValidatorsRow> > m_arrayValidators;
    
};


/**
 * Encapsulates an array containing rows of validators
 * Each row is a vector of validators (zero is possible) for validating vouts
 * this validation plan is used for validating tx outputs
 */
template <typename TValidatorBase>
class COutputValidationPlan
{
    using ValidatorsRow = std::vector<TValidatorBase*>;
    
public:
    // Pushes a row of validators for validating a vout
    // @param ivout index to vout to validate
    // @param pargs parameter pack of zero or more pointer to validator objects
    // Why pointers? because we store base class and call its virtual functions
    
    template <typename TValidatorBaseX, typename... ARGS>
    void pushValidators(int32_t ivout, ARGS*... pargs) // validators row passed as variadic arguments CValidatorX *val1, CValidatorY *val2 ...
    {
        ValidatorsRow vValidators({ (TValidatorBase*)pargs... });
        m_arrayValidators.push_back(std::make_pair(ivout, vValidators));
    }
    
    // validate tx outputs
    bool validate(const CTransaction& tx, Eval* eval)
    {
        std::string message = "<empty>";
        //std::cerr << "COutputValidationPlan::validateOutputs() starting vouts validation..." << std::endl;
        
        int32_t ival = 0;
        int32_t numVouts = tx.vout.size();
        int32_t numValidators = m_arrayValidators.size();
        
        // run over vouts:
        while (ival < numValidators) {
            
            int32_t ivout = m_arrayValidators[ival].first;
            if (ivout >= numVouts) {
                std::cerr << "COutputValidationPlan::validate() incorrect tx" << "for ival=" << ival << " in tx.vout no such ivout=" << ivout << std::endl;
                return eval->Invalid("incorrect tx structure: not all required vouts are present.");
            }
            else
            {
                // exec 'validators' from validator row of ival index, for tx.vout[ivout]
                if (!execValidatorsInRow(&tx, ivout, ival, message)) {
                    std::ostringstream stream;
                    stream << "invalid tx vout[" << ivout << "]:" << message;
                    return eval->Invalid(stream.str().c_str());				// ... if not, return 'invalid'
                }
            }
            ival++;					// advance to the next vout
        }
        //std::cerr << "COutputValidationPlan::validate() returns with true" << std::endl;
        return true;
    }
    
private:
    // Executes validators from the requested row of validators (selected by iValidators) for selected vin or vout (selected by iv)
    bool execValidatorsInRow(const CTransaction* pTx, int32_t iv, int32_t ival, std::string& refMessage) const
    {
        // check boundaries:
        if (ival < 0 || ival >= m_arrayValidators.size()) {
            std::cerr << "COutputValidationPlan::execValidatorsInRow() internal error: incorrect param ival=" << ival << " size=" << m_arrayValidators.size();
            refMessage = "internal error: incorrect param ival index";
            return false;
        }
        
        if (iv < 0 || iv >= pTx->vout.size()) {
            std::cerr << "COutputValidationPlan::execValidatorsInRow() internal error: incorrect param iv=" << iv << " size=" << m_arrayValidators.size();
            refMessage = "internal error: incorrect param iv index";
            return false;
        }
        
        // get requested row of validators:
        ValidatorsRow vValidators = m_arrayValidators[ival].second;
        
        //std::cerr << "COutputValidationPlan::execRow() calling validators" << " for vout iv=" << iv << " ival=" << ival << std::endl;
        
        for (auto v : vValidators) {
            
            if (!v->isVinValidator())	{
                // if this is a 'in' validation plan then pass the previous tx vout:
                bool result = v->validateVout(pTx->vout[iv], iv, refMessage);
                if (!result)
                    return result;
            }
        }
        return true; // validation OK
    }
    
private:
    //std::map<int32_t, ValidatorsRow> m_mapValidators;
    std::vector< std::pair<int32_t, ValidatorsRow> > m_arrayValidators;
    
};

class CNormalInputIdentifier : CInputIdentifierBase  {
public:
    CNormalInputIdentifier(CCcontract_info* cp) : CInputIdentifierBase(cp) {}
    virtual std::string inputName() const { return std::string("normal input"); }
    virtual bool identifyInput(CTxIn vin) const {
        return !IsCCInput(vin.scriptSig);
    }
};

class CCCInputIdentifier : CInputIdentifierBase {
public:
    CCCInputIdentifier(CCcontract_info* cp) : CInputIdentifierBase(cp) {}
    virtual std::string inputName() const { return std::string("CC input"); }
    virtual bool identifyInput(CTxIn vin) const {
        return IsCCInput(vin.scriptSig);
    }
};


/**
 * Validates 1of2address for vout (may be used for either this or prev tx)
 */
template <class Helper> class CCC1of2AddressValidator : CValidatorBase
{
public:
    CCC1of2AddressValidator(CCcontract_info* cp, CScript opRetScript, std::string customMessage = "") :
    m_fundingOpretScript(opRetScript), m_customMessage(customMessage), CValidatorBase(cp) {}
    
    virtual bool isVinValidator() const { return false; }
    virtual bool validateVout(CTxOut vout, int32_t vout_n, std::string& message) const
    {
        //std::cerr << "CCC1of2AddressValidator::validateVout() entered" << std::endl;
        CPubKey ownerPubkey, heirPubkey;
        int64_t inactivityTime;
        std::string heirName, memo;
        uint256 tokenid;
        
        uint8_t funcId = DecodeHeirEitherOpRet(m_fundingOpretScript, tokenid, ownerPubkey, heirPubkey, inactivityTime, heirName, memo, true);
        if (funcId == 0) {
            message = m_customMessage + std::string(" invalid opreturn format");
            std::cerr << "CCC1of2AddressValidator::validateVout() exits with false: " << message << std::endl;
            return false;
        }
        
        char shouldBeAddr[65], ccAddr[65];
        
        //GetCCaddress1of2(m_cp, shouldBeAddr, ownerPubkey, heirPubkey);
        Helper::GetCoinsOrTokensCCaddress1of2(shouldBeAddr, ownerPubkey, heirPubkey);
        
        if (vout.scriptPubKey.IsPayToCryptoCondition()) {
            if (Getscriptaddress(ccAddr, vout.scriptPubKey) && strcmp(shouldBeAddr, ccAddr) == 0) {
                //std::cerr << "CCC1of2AddressValidator::validateVout() exits with true" << std::endl;
                return true;
            }
            else {
                message = m_customMessage + std::string(" incorrect heir funding address: incorrect pubkey(s)");
            }
        }
        else {
            message = m_customMessage + std::string(" incorrect heir funding address: not a 1of2addr");
        }
        
        std::cerr << "CCC1of2AddressValidator::validateVout() exits with false: " << message << std::endl;
        return false;
    }
    virtual bool validateVin(CTxIn vin, std::vector<CTxOut> prevVout, int32_t prevN, std::string& message) const { return false; }
    
private:
    CScript m_fundingOpretScript;
    std::string m_customMessage;
};


/**
 * Validates if this is vout to owner or heir from opret (funding or change)
 */
template <class Helper> class CMyPubkeyVoutValidator : CValidatorBase
{
public:
    CMyPubkeyVoutValidator(CCcontract_info* cp, CScript opRetScript, bool checkNormals)
    : m_fundingOpretScript(opRetScript), m_checkNormals(checkNormals), CValidatorBase(cp) {	}
    
    virtual bool isVinValidator() const { return false; }
    virtual bool validateVout(CTxOut vout, int32_t vout_n, std::string& message) const
    {
        //std::cerr << "CMyPubkeyVoutValidator::validateVout() entered" << std::endl;
        
        CPubKey ownerPubkey, heirPubkey;
        int64_t inactivityTime;
        std::string heirName, memo;
        uint256 tokenid;
        
        ///std::cerr << "CMyPubkeyVoutValidator::validateVout() m_opRetScript=" << m_opRetScript.ToString() << std::endl;
        
        // get both pubkeys:
        uint8_t funcId = DecodeHeirEitherOpRet(m_fundingOpretScript, tokenid, ownerPubkey, heirPubkey, inactivityTime, heirName, memo, true);
        if (funcId == 0) {
            message = std::string("invalid opreturn format");
            return false;
        }
        
        CScript ownerScript;
        CScript heirScript;
        if (m_checkNormals)  {  //not used, incorrect check, too strict
            ownerScript = CoinHelper::makeUserVout(vout.nValue, ownerPubkey).scriptPubKey;
            heirScript = CoinHelper::makeUserVout(vout.nValue, heirPubkey).scriptPubKey;
            std::cerr << "CMyPubkeyVoutValidator::validateVout() vout.scriptPubKey=" << vout.scriptPubKey.ToString() << " makeUserVout(coin,owner)=" << CoinHelper::makeUserVout(vout.nValue, ownerPubkey).scriptPubKey.ToString() << " makeUserVout(coin,heir)=" << CoinHelper::makeUserVout(vout.nValue, heirPubkey).scriptPubKey.ToString() << std::endl;
        }
        else  {
            ownerScript = Helper::makeUserVout(vout.nValue, ownerPubkey).scriptPubKey;
            heirScript = Helper::makeUserVout(vout.nValue, heirPubkey).scriptPubKey;
            std::cerr << "CMyPubkeyVoutValidator::validateVout() vout.scriptPubKey=" << vout.scriptPubKey.ToString() << " makeUserVout(owner)=" << Helper::makeUserVout(vout.nValue, ownerPubkey).scriptPubKey.ToString() << " makeUserVout(heir)=" << Helper::makeUserVout(vout.nValue, heirPubkey).scriptPubKey.ToString() << std::endl;
        }
        
        // recreate scriptPubKey for owner and heir and compare it with that of the vout to check:
        if (vout.scriptPubKey == ownerScript ||	vout.scriptPubKey == heirScript) {
            // this is vout to owner or heir addr:
            //std::cerr << "CMyPubkeyVoutValidator::validateVout() exits with true" << std::endl;
            return true;
        }
        
        std::cerr << "CMyPubkeyVoutValidator::validateVout() exits with false (not the owner's or heir's addresses)" << std::endl;
        message = std::string("invalid pubkey");
        return false;
    }
    virtual bool validateVin(CTxIn vin, std::vector<CTxOut> prevVout, int32_t prevN, std::string& message) const { return true; }
    
private:
    CScript m_fundingOpretScript;
    //uint256 m_lasttxid;
    bool m_checkNormals;
};

/**
 * Check if the user is the heir and the heir is allowed to spend (duration > inactivityTime)
 */
template <class Helper> class CHeirSpendValidator : CValidatorBase
{
public:
    CHeirSpendValidator(CCcontract_info* cp, CScript opRetScript, uint256 latesttxid, uint8_t isHeirSpendingBegan)
    : m_fundingOpretScript(opRetScript), m_latesttxid(latesttxid), m_isHeirSpendingBegan(isHeirSpendingBegan), CValidatorBase(cp) {}
    
    virtual bool isVinValidator() const { return false; }
    virtual bool validateVout(CTxOut vout, int32_t vout_n, std::string& message) const
    {
        //std::cerr << "CHeirSpendValidator::validateVout() entered" << std::endl;
        
        CPubKey ownerPubkey, heirPubkey;
        int64_t inactivityTime;
        std::string heirName, memo;
        uint256 tokenid;
        
        // get heir pubkey:
        uint8_t funcId = DecodeHeirEitherOpRet(m_fundingOpretScript, tokenid, ownerPubkey, heirPubkey, inactivityTime, heirName, memo, true);
        if (funcId == 0) {
            message = std::string("invalid opreturn format");
            return false;
        }
        
        int32_t numblocks;
        int64_t durationSec = CCduration(numblocks, m_latesttxid);
        
        // recreate scriptPubKey for heir and compare it with that of the vout:
        if (vout.scriptPubKey == Helper::makeUserVout(vout.nValue, heirPubkey).scriptPubKey) {
            // this is the heir is trying to spend
            if (!m_isHeirSpendingBegan && durationSec <= inactivityTime) {
                message = "heir is not allowed yet to spend funds";
                std::cerr << "CHeirSpendValidator::validateVout() heir is not allowed yet to spend funds" << std::endl;
                return false;
            }
            else {
                // heir is allowed to spend
                return true;
            }
        }
        
        //std::cerr << "CHeirSpendValidator::validateVout() exits with true" << std::endl;
        
        // this is not heir:
        return true;
    }
    virtual bool validateVin(CTxIn vin, std::vector<CTxOut> prevVout, int32_t prevN, std::string& message) const { return true; }
    
private:
    CScript m_fundingOpretScript;
    uint256 m_latesttxid;
    uint8_t m_isHeirSpendingBegan;
};

/**
 * Validates this opreturn and compares it with the opreturn from the previous tx
 */
template <class Helper> class COpRetValidator : CValidatorBase
{
public:
    COpRetValidator(CCcontract_info* cp, CScript opret)
    : m_fundingOpretScript(opret), CValidatorBase(cp) {}
    
    virtual bool isVinValidator() const { return false; }
    virtual bool validateVout(CTxOut vout, int32_t vout_n, std::string& message) const
    {
        //std::cerr << "COpRetValidator::validateVout() entered" << std::endl;
        
        uint256 fundingTxidInOpret = zeroid, dummyTxid, tokenid = zeroid, initialTokenid = zeroid;
        uint8_t dummyIsHeirSpendingBegan;
        
        uint8_t funcId = DecodeHeirEitherOpRet(vout.scriptPubKey, tokenid, fundingTxidInOpret, dummyIsHeirSpendingBegan, true);
        if (funcId == 0) {
            message = std::string("invalid opreturn format");
            return false;
        }
        
        uint8_t initialFuncId = DecodeHeirEitherOpRet(m_fundingOpretScript, initialTokenid, dummyTxid, dummyIsHeirSpendingBegan, true);
        if (initialFuncId == 0) {
            message = std::string("invalid initial tx opreturn format");
            return false;
        }
        
        // validation rules:
        if (!isMyFuncId(funcId)) {
            message = std::string("invalid funcid in opret");
            return false;
        }
        
        if (typeid(Helper) == typeid(TokenHelper)) {
            if (tokenid != initialTokenid) {
                message = std::string("invalid tokenid in opret");
                return false;
            }
        }
        
        //std::cerr << "COpRetValidator::validateVout() exits with true" << std::endl;
        return true;
    }
    virtual bool validateVin(CTxIn vin, std::vector<CTxOut> prevVout, int32_t prevN, std::string& message) const { return true; }
    
private:
    CScript m_fundingOpretScript;
};


/**
 * marker spending prevention validator, 
 * returns false if for tx with funcid=F vout.1 is being tried to spend 
 */
template <class Helper> class CMarkerValidator : CValidatorBase
{
public:
	CMarkerValidator(CCcontract_info* cp)
		: CValidatorBase(cp) {	}

	virtual bool isVinValidator() const { return true; }  // this is vin validator
	virtual bool validateVout(CTxOut vout, int32_t vout_n, std::string& message) const { return true; }
	virtual bool validateVin(CTxIn vin, std::vector<CTxOut> prevVout, int32_t prevN, std::string& message) const { 

		uint256 fundingTxidInOpret = zeroid, dummyTxid, tokenid = zeroid, initialTokenid = zeroid;
		uint8_t dummyIsHeirSpendingBegan;

		//std::cerr << "CMarkerValidator::validateVin() prevVout.size()=" << prevVout.size() << " prevN=" << prevN << std::endl;
		if (prevVout.size() > 0) {

			// get funcId for prev tx:
			uint8_t funcId = DecodeHeirEitherOpRet(prevVout[prevVout.size()-1].scriptPubKey, tokenid, fundingTxidInOpret, dummyIsHeirSpendingBegan, true);

			//std::cerr << "CMarkerValidator::validateVin() funcId=" << (funcId?funcId:' ') << std::endl;

			if (funcId == 'F' && prevN == 1) { // do not allow to spend 'F' marker's vout
				message = std::string("spending marker not allowed");
				return false;
			}
		}
		//std::cerr << "CMarkerValidator::validateVin() exits with true" << std::endl;
		return true; 
	}
};

/**
 * empty validator always returns true
 */
template <class Helper> class CNullValidator : CValidatorBase
{
public:
    CNullValidator(CCcontract_info* cp)
    : CValidatorBase(cp) {	}
    
    virtual bool isVinValidator() const { return false; }
    virtual bool validateVout(CTxOut vout, int32_t vout_n, std::string& message) const { return true; }
    virtual bool validateVin(CTxIn vin, std::vector<CTxOut> prevVout, int32_t prevN, std::string& message) const { return true; }
};


#endif
