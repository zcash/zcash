#ifndef BITCOIN_GTEST_UTILS_H
#define BITCOIN_GTEST_UTILS_H

#include "random.h"
#include "util/system.h"
#include "key_io.h"

int GenZero(int n);
int GenMax(int n);
void LoadProofParameters();
void LoadGlobalWallet();
void UnloadGlobalWallet();

template<typename Tree> void AppendRandomLeaf(Tree &tree);

#endif
