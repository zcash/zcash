/******************************************************************************
 * Copyright © 2014-2019 The SuperNET Developers.                             *
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



// creategame, register (inventory + baton + buyin), progress (events + statehash + [compr state]?), claimwin
// create game buyin, newbie flag, 10 blocks registration seed is starting blockhash!
// inheritance of items across games!
// binding tokens to specific items to allow for built in market
// pubkey token inventory creates items can be used for a specific campaign
// player wins buyins + ingame gold -> ROGUE + ingame items -> tokens via 1 vout per item to be spent into a token opreturn

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

int32_t rogue_replay(uint64_t seed);
int rogue(int argc, char **argv, char **envp);

int main(int argc, char **argv, char **envp)
{
    uint64_t seed; FILE *fp = 0;
    if ( argc > 1 && (fp=fopen(argv[1],"rb")) == 0 )
    {
        seed = atol(argv[1]);
        fprintf(stderr,"replay %llu\n",(long long)seed);
        return(rogue_replay(seed));
    }
    else
    {
        if ( fp != 0 )
            fclose(fp);
        return(rogue(argc,argv,envp));
    }
}


