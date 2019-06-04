/*Descriptson and examples of COptCCParams class found in:
    script/standard.h/cpp 
    class COptCCParams
    
structure of data in vData payload attached to end of CCvout: 
    param 
    OP_1 
    param
    OP_2 ... etc until OP_16
    OP_PUSHDATA4 is the last OP code to tell things its at the end. 
    
    taken from standard.cpp line 22: COptCCParams::COptCCParams(std::vector<unsigned char> &vch)

EXAMPLE taken from Verus how to create scriptPubKey from COptCCParams class:
EXAMPLE taken from Verus how to decode scriptPubKey from COptCCParams class:
*/

bool MakeGuardedOutput(CAmount value, CPubKey &dest, CTransaction &stakeTx, CTxOut &vout)
{
    CCcontract_info *cp, C;
    cp = CCinit(&C,EVAL_STAKEGUARD);

    CPubKey ccAddress = CPubKey(ParseHex(cp->CChexstr));

    // return an output that is bound to the stake transaction and can be spent by presenting either a signed condition by the original 
    // destination address or a properly signed stake transaction of the same utxo on a fork
    vout = MakeCC1of2vout(EVAL_STAKEGUARD, value, dest, ccAddress);

    std::vector<CPubKey> vPubKeys = std::vector<CPubKey>();
    vPubKeys.push_back(dest);
    vPubKeys.push_back(ccAddress);
        
    std::vector<std::vector<unsigned char>> vData = std::vector<std::vector<unsigned char>>();

    CVerusHashWriter hw = CVerusHashWriter(SER_GETHASH, PROTOCOL_VERSION);

    hw << stakeTx.vin[0].prevout.hash;
    hw << stakeTx.vin[0].prevout.n;

    uint256 utxo = hw.GetHash();
    vData.push_back(std::vector<unsigned char>(utxo.begin(), utxo.end())); // Can we use any data here to construct vector? 

    CStakeParams p;
    if (GetStakeParams(stakeTx, p))
    {
        // prev block hash and height is here to make validation easy
        vData.push_back(std::vector<unsigned char>(p.prevHash.begin(), p.prevHash.end()));
        std::vector<unsigned char> height = std::vector<unsigned char>(4);
        for (int i = 0; i < 4; i++)
        {
            height[i] = (p.blkHeight >> (8 * i)) & 0xff;
        }
        vData.push_back(height);

        COptCCParams ccp = COptCCParams(COptCCParams::VERSION, EVAL_STAKEGUARD, 1, 2, vPubKeys, vData);

        vout.scriptPubKey << ccp.AsVector() << OP_DROP;
        return true;
    }
    return false;
}

bool ValidateMatchingStake(const CTransaction &ccTx, uint32_t voutNum, const CTransaction &stakeTx, bool &cheating)
{
    // an invalid or non-matching stake transaction cannot cheat
    cheating = false;

    //printf("ValidateMatchingStake: ccTx.vin[0].prevout.hash: %s, ccTx.vin[0].prevout.n: %d\n", ccTx.vin[0].prevout.hash.GetHex().c_str(), ccTx.vin[0].prevout.n);

    if (ccTx.IsCoinBase())
    {
        CStakeParams p;
        if (ValidateStakeTransaction(stakeTx, p))
        {
            std::vector<std::vector<unsigned char>> vParams = std::vector<std::vector<unsigned char>>();
            CScript dummy;

            if (ccTx.vout[voutNum].scriptPubKey.IsPayToCryptoCondition(&dummy, vParams) && vParams.size() > 0)
            {
                COptCCParams ccp = COptCCParams(vParams[0]);
                if (ccp.IsValid() & ccp.vData.size() >= 3 && ccp.vData[2].size() <= 4)
                {
                    CVerusHashWriter hw = CVerusHashWriter(SER_GETHASH, PROTOCOL_VERSION);

                    hw << stakeTx.vin[0].prevout.hash;
                    hw << stakeTx.vin[0].prevout.n;
                    uint256 utxo = hw.GetHash();

                    uint32_t height = 0;
                    int i, dataLen = ccp.vData[2].size();
                    for (i = dataLen - 1; i >= 0; i--)
                    {
                        height = (height << 8) + ccp.vData[2][i];
                    }
                    // for debugging strange issue
                    // printf("iterator: %d, height: %d, datalen: %d\n", i, height, dataLen);

                    if (utxo == uint256(ccp.vData[0]))
                    {
                        if (p.prevHash != uint256(ccp.vData[1]) && p.blkHeight >= height)
                        {
                            cheating = true;
                            return true;
                        }
                        // if block height is equal and we are at the else, prevHash must have been equal
                        else if (p.blkHeight == height)
                        {
                            return true;                            
                        }
                    }
                }
            }
        }
    }
    return false;
}
