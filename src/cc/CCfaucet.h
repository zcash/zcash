/******************************************************************************
 * Copyright © 2014-2018 The SuperNET Developers.                             *
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


#ifndef CC_FAUCET_H
#define CC_FAUCET_H

#include "CCinclude.h"

extern const char *FaucetCCaddr = "RGKRjeTBw4LYFotSDLT6RWzMHbhXri6BG6" ;//"RFYE2yL3KknWdHK6uNhvWacYsCUtwzjY3u";
extern char FaucetCChexstr[67] = { "02adf84e0e075cf90868bd4e3d34a03420e034719649c41f371fc70d8e33aa2702" };

// CCcustom
bool IsFaucetInput(CScript const& scriptSig);
CC *MakeFaucetCond(CPubKey pk);


#endif
