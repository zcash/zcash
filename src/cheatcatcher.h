/********************************************************************
 * (C) 2018 Michael Toutonghi
 * 
 * Distributed under the MIT software license, see the accompanying
 * file COPYING or http://www.opensource.org/licenses/mit-license.php.
 * 
 * This supports code to catch nothing at stake cheaters who stake
 * on multiple forks.
 * 
 */

#ifndef CHEATCATCHER_H
#define CHEATCATCHER_H

#include <vector>

#include "streams.h"
#include "primitives/transaction.h"
#include "script/script.h"
#include "uint256.h"

#include <vector>
#include <map>

class CTxHolder
{
    public:
        uint256 utxo;
        uint32_t height;
        CTransaction tx;
        CTxHolder(const CTransaction &_tx, uint32_t _height) : height(_height), tx(_tx) {
            CVerusHashWriter hw = CVerusHashWriter(SER_GETHASH, PROTOCOL_VERSION);

            hw << tx.vin[0].prevout.hash;
            hw << tx.vin[0].prevout.n;
            utxo = hw.GetHash();
        }

        CTxHolder& operator=(const CTxHolder& txh)
        {
            utxo = txh.utxo;
            height = txh.height;
            tx = txh.tx;
        }
};


class CCheatList
{
    private:
        std::multimap<const int32_t, CTxHolder> orderedCheatCandidates;
        std::multimap<const uint256, CTxHolder *> indexedCheatCandidates;
        CCriticalSection cs_cheat;

    public:
        CCheatList() {}

        // prune all transactions in the list below height
        uint32_t Prune(uint32_t height);

        // check to see if a transaction that could be a cheat for the passed transaction is in our list
        bool IsCheatInList(const CTransaction &tx, CTransaction *pcheatTx);

        // check to see if there are cheat candidates of the same or greater block height in list
        bool IsHeightOrGreaterInList(uint32_t height);

        // add a potential cheat transaction to the list. we do this for all stake transactions from orphaned stakes
        bool Add(const CTxHolder &txh);

        // remove a transaction from the the list
        void Remove(const CTxHolder &txh);
};


extern CCheatList cheatList;
extern boost::optional<libzcash::SaplingPaymentAddress> cheatCatcher;

#endif