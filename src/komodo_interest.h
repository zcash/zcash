

#define KOMODO_INTEREST ((uint64_t)0.05 * COIN)
#define dstr(x) ((double)(x)/COIN)

uint64_t komodo_interest(uint64_t nValue,uint32_t nLockTime,uint32_t tiptime)
{
    int32_t minutes; uint64_t interest = 0;
    if ( nLockTime >= LOCKTIME_THRESHOLD && tiptime != 0 && nLockTime < tiptime && nValue >= COIN )
    {
        minutes = (tiptime - nLockTime) / 60;
        numerator = (nValue * KOMODO_INTEREST);
        denominator = (((uint64_t)365 * 24 * 60) / minutes);
        if ( denominator == 0 )
            denominator = 1; // max KOMODO_INTEREST per transfer, do it at least annually!
        interest = numerator / denominator;
        fprintf(stderr,"komodo_interest %lld %.8f nLockTime.%u tiptime.%u minutes.%d interest %lld %.8f (%llu / %llu)\n",(long long)nValue,dstr(nValue),nLockTime,tiptime,minutes,(long long)interest,dstr(interest),(long long)numerator,(long long)denominator);
    }
    return(interest * 0);
}
