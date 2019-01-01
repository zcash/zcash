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

#include "cc/eval.h"
#include "cc/utils.h"
#include "importcoin.h"
#include "crosschain.h"
#include "primitives/transaction.h"


/*
 * CC Eval method for import coin.
 *
 * This method should control every parameter of the ImportCoin transaction, since it has no signature
 * to protect it from malleability.
 
 ##### 0xffffffff is a special CCid for single chain/dual daemon imports
 */

extern std::string ASSETCHAINS_SELFIMPORT;
extern uint16_t ASSETCHAINS_CODAPORT,ASSETCHAINS_BEAMPORT;

int32_t GetSelfimportProof(TxProof &proof,CTransaction burnTx,uint256 hash) // find burnTx with hash from "other" daemon
{
    if ( ASSETCHAINS_SELFIMPORT == "BEAM" )
    {
        // confirm via ASSETCHAINS_BEAMPORT that burnTx/hash is a valid BEAM burn
            return(-1);
    }
    else if ( ASSETCHAINS_SELFIMPORT == "CODA" )
    {
        // confirm via ASSETCHAINS_CODAPORT that burnTx/hash is a valid CODA burn
            return(-1);
    }
    else if ( ASSETCHAINS_SELFIMPORT == "PUBKEY" )
    {
        // make sure vin0 is signed by ASSETCHAINS_OVERRIDE_PUBKEY33
            return(0);
    }
    else if ( ASSETCHAINS_SELFIMPORT == "GATEWAY" )
    {
        // external coin is the assetchains symbol in the burnTx OP_RETURN
            return(-1);
    }
    else return(-1);
    return(0);
}

// use proof from the above functions to validate the import

int32_t CheckBEAMimport(TxProof proof,CTransaction burnTx,std::vector<CTxOut> payouts)
{
    // check with dual-BEAM daemon via ASSETCHAINS_BEAMPORT for validity of burnTx
    return(-1);
}

int32_t CheckCODAimport(TxProof proof,CTransaction burnTx,std::vector<CTxOut> payouts)
{
    // check with dual-CODA daemon via ASSETCHAINS_CODAPORT for validity of burnTx
    return(-1);
}

int32_t CheckGATEWAYimport(std::string coin,TxProof proof,CTransaction burnTx,std::vector<CTxOut> payouts)
{
    // check for valid burn from external coin blockchain and if valid return(0);
    return(-1);
}

int32_t CheckPUBKEYimport(TxProof proof,CTransaction burnTx,std::vector<CTxOut> payouts)
{
    // if burnTx has ASSETCHAINS_PUBKEY vin, it is valid return(0);
    return(0);
    return(-1);
}

bool Eval::ImportCoin(const std::vector<uint8_t> params, const CTransaction &importTx, unsigned int nIn)
{
    if (importTx.vout.size() < 2)
        return Invalid("too-few-vouts");

    // params
    TxProof proof;
    CTransaction burnTx;
    std::vector<CTxOut> payouts;

    if (!UnmarshalImportTx(importTx, proof, burnTx, payouts))
        return Invalid("invalid-params");
    
    // Control all aspects of this transaction
    // It should not be at all malleable
    if (MakeImportCoinTransaction(proof, burnTx, payouts).GetHash() != importTx.GetHash())
        return Invalid("non-canonical");

    // burn params
    uint32_t targetCcid;
    std::string targetSymbol;
    uint256 payoutsHash;

    if (!UnmarshalBurnTx(burnTx, targetSymbol, &targetCcid, payoutsHash))
        return Invalid("invalid-burn-tx");

    // check burn amount
    {
        uint64_t burnAmount = burnTx.vout.back().nValue;
        if (burnAmount == 0)
            return Invalid("invalid-burn-amount");
        uint64_t totalOut = 0;
        for (int i=0; i<importTx.vout.size(); i++)
            totalOut += importTx.vout[i].nValue;
        if (totalOut > burnAmount)
            return Invalid("payout-too-high");
    }

    // Check burntx shows correct outputs hash
    if (payoutsHash != SerializeHash(payouts))
        return Invalid("wrong-payouts");

    if (targetCcid < KOMODO_FIRSTFUNGIBLEID)
        return Invalid("chain-not-fungible");
    
    // Check proof confirms existance of burnTx
    if ( targetCcid != 0xffffffff )
    {
        if (targetCcid != GetAssetchainsCC() || targetSymbol != GetAssetchainsSymbol())
            return Invalid("importcoin-wrong-chain");
        uint256 target = proof.second.Exec(burnTx.GetHash());
        if (!CheckMoMoM(proof.first, target))
            return Invalid("momom-check-fail");
    }
    else if ( ASSETCHAINS_SELFIMPORT == targetSymbol || ASSETCHAINS_SELFIMPORT == "GATEWAY" ) // various selfchain imports
    {
        if ( ASSETCHAINS_SELFIMPORT == "BEAM" )
        {
            if ( CheckBEAMimport(proof,burnTx,payouts) < 0 )
                return Invalid("BEAM-import-failure");
        }
        else if ( ASSETCHAINS_SELFIMPORT == "CODA" )
        {
            if ( CheckCODAimport(proof,burnTx,payouts) < 0 )
                return Invalid("CODA-import-failure");
        }
        else if ( ASSETCHAINS_SELFIMPORT == "PUBKEY" )
        {
            if ( CheckPUBKEYimport(proof,burnTx,payouts) < 0 )
                return Invalid("PUBKEY-import-failure");
        }
        else
        {
            if ( CheckGATEWAYimport(GetAssetchainsSymbol(),proof,burnTx,payouts) < 0 )
                return Invalid("GATEWAY-import-failure");
        }
    }
    return Valid();
}


