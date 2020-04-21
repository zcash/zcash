#ifndef HEIR_VALIDATE_V0_H
#define HEIR_VALIDATE_V0_H

#include "../CCinclude.h"
//#include "CCHeir_v0.h"

namespace heirv0 {

#define IS_CHARINSTR(c, str) (std::string(str).find((char)(c)) != std::string::npos)

// makes coin initial tx opret
vscript_t EncodeHeirCreateOpRet(uint8_t funcid, CPubKey ownerPubkey, CPubKey heirPubkey, int64_t inactivityTimeSec, std::string heirName, std::string memo);
vscript_t EncodeHeirOpRet(uint8_t funcid,  uint256 fundingtxid, uint8_t isHeirSpendingBegan);

uint256 FindLatestFundingTx(uint256 fundingtxid, uint256 &tokenid, CScript& opRetScript, uint8_t &isHeirSpendingBegan);
uint256 _FindLatestFundingTx(uint256 fundingtxid, uint8_t& funcId, uint256 &tokenid, CPubKey& ownerPubkey, CPubKey& heirPubkey, int64_t& inactivityTime, std::string& heirName, std::string& memo, CScript& fundingOpretScript, uint8_t &hasHeirSpendingBegun);

uint8_t DecodeHeirEitherOpRet(CScript scriptPubKey, uint256 &tokenid, CPubKey& ownerPubkey, CPubKey& heirPubkey, int64_t& inactivityTime, std::string& heirName, std::string& memo, bool noLogging = false);
uint8_t DecodeHeirEitherOpRet(CScript scriptPubKey, uint256 &tokenid, uint256 &fundingTxidInOpret, uint8_t &hasHeirSpendingBegun, bool noLogging = false);
uint8_t _DecodeHeirEitherOpRet(CScript scriptPubKey, uint256 &tokenid, CPubKey& ownerPubkey, CPubKey& heirPubkey, int64_t& inactivityTime, std::string& heirName, std::string& memo, uint256& fundingTxidInOpret, uint8_t &hasHeirSpendingBegun, bool noLogging);

inline static bool isMyFuncId(uint8_t funcid) { return IS_CHARINSTR(funcid, "FAC"); }
inline static bool isSpendingTx(uint8_t funcid) { return (funcid == 'C'); }

};

#endif
