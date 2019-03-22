
CScript games_opret(uint8_t funcid,CPubKey pk)
{
    CScript opret; uint8_t evalcode = EVAL_GAMES;
    opret << OP_RETURN << E_MARSHAL(ss << evalcode << funcid << pk);
    return(opret);
}

uint8_t games_opretdecode(CPubKey &pk,CScript scriptPubKey)
{
    std::vector<uint8_t> vopret; uint8_t e,f;
    GetOpReturnData(scriptPubKey,vopret);
    if ( vopret.size() > 2 && E_UNMARSHAL(vopret,ss >> e; ss >> f; ss >> pk) != 0 && e == EVAL_GAMES )
    {
        return(f);
    }
    return(0);
}

UniValue games_rawtxresult(UniValue &result,std::string rawtx,int32_t broadcastflag)
{
    CTransaction tx;
    if ( rawtx.size() > 0 )
    {
        result.push_back(Pair("hex",rawtx));
        if ( DecodeHexTx(tx,rawtx) != 0 )
        {
            if ( broadcastflag != 0 && myAddtomempool(tx) != 0 )
                RelayTransaction(tx);
            result.push_back(Pair("txid",tx.GetHash().ToString()));
            result.push_back(Pair("result","success"));
        } else result.push_back(Pair("error","decode hex"));
    } else result.push_back(Pair("error","couldnt finalize CCtx"));
    return(result);
}

UniValue games_rng(uint64_t txfee,struct CCcontract_info *cp,cJSON *params)
{
    UniValue result(UniValue::VOBJ); int32_t i,invertflag=0,n,playerid=0; uint16_t seeds[4]; uint64_t seed,initseed; bits256 hash;
    if ( params != 0 && ((n= cJSON_GetArraySize(params)) == 2 || n == 3) )
    {
        hash = jbits256(jitem(params,0),0);
        seed = jdouble(jitem(params,1),0);
        if ( n == 3 )
        {
            playerid = juint(jitem(params,2),0);
            if ( playerid >= 0x100 )
            {
                result.push_back(Pair("result","error"));
                result.push_back(Pair("error","playerid too big"));
                return(result);
            }
        }
        if ( seed == 0 )
        {
            playerid++;
            if ( playerid == 0x100 )
            {
                invertflag = 1;
                playerid--;
            }
            for (i=0; i<8; i++)
            {
                if ( ((1 << i) & playerid) != 0 )
                    seed ^= hash.uints[i];
                if ( invertflag != 0 )
                    seed ^= (uint64_t)-1;
            }
        }
        initseed = seed;
        for (i=0; i<4; i++)
        {
            seeds[i] = (seed >> (i*16));
            seeds[i] = (seeds[i]*11109 + 13849);
        }
        seed = ((uint64_t)seeds[3] << 48) | ((uint64_t)seeds[2] << 24) | ((uint64_t)seeds[1] << 16) | seeds[0];
        result.push_back(Pair("playerid",(int64_t)(playerid - 1 + invertflag)));
        result.push_back(Pair("initseed",initseed));
        result.push_back(Pair("seed",seed));
        result.push_back(Pair("result","success"));
    }
    else
    {
        result.push_back(Pair("result","error"));
        result.push_back(Pair("error","not enough params"));
    }
    return(result);
}

// send yourself 1 coin to your CC address using normal utxo from your -pubkey

UniValue games_func1(uint64_t txfee,struct CCcontract_info *cp,cJSON *params)
{
    CMutableTransaction mtx = CreateNewContextualCMutableTransaction(Params().GetConsensus(), komodo_nextheight()); std::string rawtx;
    UniValue result(UniValue::VOBJ); CPubKey mypk; int64_t amount = COIN; int32_t broadcastflag=0;
    if ( txfee == 0 )
        txfee = GAMES_TXFEE;
    mypk = pubkey2pk(Mypubkey());
    if ( AddNormalinputs(mtx,mypk,COIN+txfee,64) >= COIN+txfee ) // add utxo to mtx
    {
        mtx.vout.push_back(MakeCC1vout(cp->evalcode,amount,mypk)); // make vout0
        // add opreturn, change is automatically added and tx is properly signed
        rawtx = FinalizeCCTx(0,cp,mtx,mypk,txfee,games_opret('1',mypk));
        return(games_rawtxresult(result,rawtx,broadcastflag));
    }
    return(result);
}

bool games_validate(struct CCcontract_info *cp,int32_t height,Eval *eval,const CTransaction tx)
{
    char expectedaddress[64]; CPubKey pk;
    if ( tx.vout.size() != 2 ) // make sure the tx only has 2 outputs
        return eval->Invalid("invalid number of vouts");
    else if ( games_opretdecode(pk,tx.vout[1].scriptPubKey) != '1' ) // verify has opreturn
        return eval->Invalid("invalid opreturn");
    GetCCaddress(cp,expectedaddress,pk);
    if ( IsCClibvout(cp,tx,0,expectedaddress) == COIN ) // make sure amount and destination matches
        return(true);
    else return eval->Invalid("invalid vout0 amount");
}


