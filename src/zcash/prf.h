/*
Zcash uses SHA256Compress as a PRF for various components
within the zkSNARK circuit.
*/

#ifndef _PRF_H_
#define _PRF_H_

#include "uint256.h"

uint256 PRF(bool a, bool b, bool c, bool d,
            uint256 x,
            uint256 y);

template<unsigned char T>
uint256 PRF_addr(uint256 a_sk)
{
    uint256 y;
    *(y.begin()) = T;

    return PRF(0, 0, 0, 0, a_sk, y);
}

#endif // _PRF_H_
