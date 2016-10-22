uint32_t komodo_txtime(uint256 hash)
{
    CTransaction tx;
    uint256 hashBlock;
    if (!GetTransaction(hash, tx, hashBlock, true))
    {
        //printf("null GetTransaction\n");
        return(tx.nLockTime);
    }
    /*if (!hashBlock.IsNull()) {
        BlockMap::iterator mi = mapBlockIndex.find(hashBlock);
        if (mi != mapBlockIndex.end() && (*mi).second)
        {
            CBlockIndex* pindex = (*mi).second;
            if (chainActive.Contains(pindex))
                return(pindex->GetBlockTime());
        }
        //printf("cant find in iterator\n");
    }*/
    //printf("null hashBlock\n");
    return(0);
}

uint64_t komodo_interest(uint64_t nValue,uint32_t nLockTime,uint32_t tiptime)
{
    int32_t minutes; uint64_t interest = 0;
    if ( tiptime == 0 )
        tiptime = chainActive.Tip()->nTime;
    if ( nLockTime >= LOCKTIME_THRESHOLD && tiptime != 0 && nLockTime < tiptime && nValue >= COIN )
    {
        minutes = (tiptime - nLockTime) / 60;
        interest = (nValue * KOMODO_INTEREST) / (((uint64_t)365 * 100000000 * 24 * 60) / minutes);
        fprintf(stderr,"komodo_interest %lld %.8f nLockTime.%u tiptime.%u minutes.%d interest %lld %.8f\n",(long long)nValue,(double)nValue/100000000.,nLockTime,tiptime,minutes,(long long)interest,(double)interest/100000000);
    }
    return(interest * 0);
}
