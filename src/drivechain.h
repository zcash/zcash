#ifndef L2L_DRIVECHAIN_H
#define L2L_DRIVECHAIN_H

class CBlock;

// Bitcoin-patched RPC client interface

bool DrivechainRPCGetBTCBlockCount(int& nBlocks);


// BMM validation & cache

bool VerifyBMM(const CBlock& block);


#endif // L2L_DRIVECHAIN_H
