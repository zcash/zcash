/*
 simple stub custom cc
 
 Just update the functions in this file, then from ~/komodo/src/cc
 
 ../komodo-cli -ac_name=CUSTOM stop
 ./makecustom
 ../komodod -ac_name=CUSTOM -ac_cclib=custom -ac_cc=2 ...
 
 The above will rebuild komodod and get it running again
 */

CScript custom_opret(uint8_t funcid,CPubKey pk)
{
    CScript opret; uint8_t evalcode = EVAL_CUSTOM;
    opret << OP_RETURN << E_MARSHAL(ss << evalcode << funcid << pk);
    return(opret);
}

uint8_t custom_opretdecode(CPubKey &pk,CScript scriptPubKey)
{
    std::vector<uint8_t> vopret; uint8_t e,f;
    GetOpReturnData(scriptPubKey,vopret);
    if ( vopret.size() > 2 && E_UNMARSHAL(vopret,ss >> e; ss >> f; ss >> pk) != 0 && e == EVAL_CUSTOM )
    {
        return(f);
    }
    return(0);
}

UniValue custom_rawtxresult(UniValue &result,std::string rawtx,int32_t broadcastflag)
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

UniValue custom_func0(uint64_t txfee,struct CCcontract_info *cp,cJSON *params)
{
    UniValue result(UniValue::VOBJ);
    result.push_back(Pair("result","success"));
    result.push_back(Pair("message","just an example of an information returning rpc"));
    return(result);
}

// send yourself 1 coin to your CC address using normal utxo from your -pubkey

UniValue custom_func1(uint64_t txfee,struct CCcontract_info *cp,cJSON *params)
{
    CMutableTransaction mtx = CreateNewContextualCMutableTransaction(Params().GetConsensus(), komodo_nextheight()); std::string rawtx;
    UniValue result(UniValue::VOBJ); CPubKey mypk; int64_t amount = COIN; int32_t broadcastflag=0;
    if ( txfee == 0 )
        txfee = CUSTOM_TXFEE;
    mypk = pubkey2pk(Mypubkey());
    if ( AddNormalinputs(mtx,mypk,COIN+txfee,64) >= COIN+txfee ) // add utxo to mtx
    {
        mtx.vout.push_back(MakeCC1vout(cp->evalcode,amount,mypk)); // make vout0
        // add opreturn, change is automatically added and tx is properly signed
        rawtx = FinalizeCCTx(0,cp,mtx,mypk,txfee,custom_opret('1',mypk));
        return(custom_rawtxresult(result,rawtx,broadcastflag));
    }
    return(result);
}

bool custom_validate(struct CCcontract_info *cp,int32_t height,Eval *eval,const CTransaction tx)
{
    char expectedaddress[64]; CPubKey pk;
    if ( tx.vout.size() != 2 ) // make sure the tx only has 2 outputs
        return eval->Invalid("invalid number of vouts");
    else if ( custom_opretdecode(pk,tx.vout[1].scriptPubKey) != '1' ) // verify has opreturn
        return eval->Invalid("invalid opreturn");
    GetCCaddress(cp,expectedaddress,pk);
    if ( IsCClibvout(cp,tx,0,expectedaddress) == COIN ) // make sure amount and destination matches
        return(true);
    else return eval->Invalid("invalid vout0 amount");
}


