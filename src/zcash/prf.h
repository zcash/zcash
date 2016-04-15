/*
Zcash uses SHA256Compress as a PRF for various components
within the zkSNARK circuit.
*/

#ifndef _PRF_H_
#define _PRF_H_

#include "uint256.h"

uint256 PRF_addr_a_pk(const uint256& a_sk);
uint256 PRF_addr_sk_enc(const uint256& a_sk);
uint256 PRF_nf(const uint256& a_sk, const uint256& rho);
uint256 PRF_pk(const uint256& a_sk, size_t i0, const uint256& h_sig);
uint256 PRF_rho(const uint256& phi, size_t i0, const uint256& h_sig);

#endif // _PRF_H_
