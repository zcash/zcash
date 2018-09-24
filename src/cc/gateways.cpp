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

#include "CCGateways.h"

/*
 prevent duplicate bindtxid via mempool scan
 wait for notarization for oraclefeed and validation of gatewaysdeposit
 debug multisig and do partial signing
 validation
 
 string oracles
 */

/*
 Uses MofN CC's normal msig handling to create automated deposits -> token issuing. And partial signing by the selected pubkeys for releasing the funds. A user would be able to select which pubkeys to use to construct the automated deposit/redeem multisigs.
 
 the potential pubkeys to be used would be based on active oracle data providers with recent activity.
 
 bind asset <-> KMD gateway deposit address
 KMD deposit -> globally spendable marker utxo
 spend marker utxo and spend linked/locked asset to user's CC address
 
 redeem -> asset to global CC address with withdraw address -> gateway spendable marker utxo
 spend market utxo and withdraw from gateway deposit address
 
 rpc calls:
 GatewayList
 GatewayInfo bindtxid
 GatewayBind coin tokenid M N pubkey(s)
 external: deposit to depositaddr with claimpubkey
 GatewayDeposit coin tokenid external.deposittxid -> markertxid
 GatewayClaim coin tokenid external.deposittxid markertxid -> spend marker and deposit asset
 
 GatewayWithdraw coin tokenid withdrawaddr
 external: do withdraw to withdrawaddr and spend marker, support for partial signatures and autocomplete
 
 deposit addr can be 1 to MofN pubkeys
 1:1 gateway with native coin
 
 In order to create a new gateway it is necessary to follow some strict steps.
 1. create a token with the max possible supply that will be issued
 2. transfer 100% of them to the gateways CC's global pubkey's asset CC address. (yes it is a bit confusing)
 3. create an oracle with the identical name, ie. KMD and format must start with Ihh (height, blockhash, merkleroot)
 4. register a publisher and fund it with a subscribe. there will be a special client app that will automatically publish the merkleroots.
 5. Now a gatewaysbind can bind an external coin to an asset, along with the oracle for the merkleroots. the txid from the bind is used in most of the other gateways CC calls
 
 usage:
 ./c tokencreate KMD 1000000 KMD_equivalent_token_for_gatewaysCC
 a7398a8748354dd0a3f8d07d70e65294928ecc3674674bb2d9483011ccaa9a7a
 
 transfer to gateways pubkey: 03ea9c062b9652d8eff34879b504eda0717895d27597aaeb60347d65eed96ccb40 RDMqGyREkP1Gwub1Nr5Ye8a325LGZsWBCb
 ./c tokentransfer a7398a8748354dd0a3f8d07d70e65294928ecc3674674bb2d9483011ccaa9a7a 03ea9c062b9652d8eff34879b504eda0717895d27597aaeb60347d65eed96ccb40 100000000000000
 2206fc39c0f384ca79819eb491ddbf889642cbfe4d0796bb6a8010ed53064a56
 
 ./c oraclescreate KMD blockheaders Ihh
 1f1aefcca2bdea8196cfd77337fb21de22d200ddea977c2f9e8742c55829d808
 
 ./c oraclesregister 1f1aefcca2bdea8196cfd77337fb21de22d200ddea977c2f9e8742c55829d808 1000000
 83b59eac238cbe54616ee13b2fdde85a48ec869295eb04051671a1727c9eb402
 
 ./c oraclessubscribe 1f1aefcca2bdea8196cfd77337fb21de22d200ddea977c2f9e8742c55829d808 02ebc786cb83de8dc3922ab83c21f3f8a2f3216940c3bf9da43ce39e2a3a882c92 1000
 f9499d8bb04ffb511fcec4838d72e642ec832558824a2ce5aed87f1f686f8102
 
 gatewaysbind tokenid oracletxid coin tokensupply M N pubkey(s)
 ./c gatewaysbind a7398a8748354dd0a3f8d07d70e65294928ecc3674674bb2d9483011ccaa9a7a 1f1aefcca2bdea8196cfd77337fb21de22d200ddea977c2f9e8742c55829d808 KMD 100000000000000 1 1 02ebc786cb83de8dc3922ab83c21f3f8a2f3216940c3bf9da43ce39e2a3a882c92
 e6c99f79d4afb216aa8063658b4222edb773dd24bb0f8e91bd4ef341f3e47e5e
 
 ./c gatewaysinfo e6c99f79d4afb216aa8063658b4222edb773dd24bb0f8e91bd4ef341f3e47e5e
 {
 "result": "success",
 "name": "Gateways",
 "pubkey": "02ebc786cb83de8dc3922ab83c21f3f8a2f3216940c3bf9da43ce39e2a3a882c92",
 "coin": "KMD",
 "oracletxid": "1f1aefcca2bdea8196cfd77337fb21de22d200ddea977c2f9e8742c55829d808",
 "taddr": 0,
 "prefix": 60,
 "prefix2": 85,
 "deposit": "",
 "tokenid": "a7398a8748354dd0a3f8d07d70e65294928ecc3674674bb2d9483011ccaa9a7a",
 "totalsupply": "1000000.00000000",
 "remaining": "1000000.00000000",
 "issued": "0.00000000"
 }
 
 To make a gateway deposit, send the funds to the "deposit" address, along with any amount to the same pubkey address you want to get the assetized KMD to appear in.
 
 0223d114dededb04f253816d6ad0ce78dd08c617c94ce3c53bf50dc74a5157bef8 pubkey for RFpxgqff7FDHFuHa3jSX5NzqqWCcELz8ha
 ./komodo-cli z_sendmany "<funding addr>" '[{"address":"RFpxgqff7FDHFuHa3jSX5NzqqWCcELz8ha","amount":0.0001},{"address":"RHV2As4rox97BuE3LK96vMeNY8VsGRTmBj","amount":7.6999}]'
 bc41a00e429db741c3199f17546a48012fd3b7eea45dfc6bc2f5228278133009 height.1003776 merkle.90aedc2f19200afc9aca2e351438d011ebae8264a58469bf225883045f61917f
 
 ./komodo-cli gettxoutproof '["bc41a00e429db741c3199f17546a48012fd3b7eea45dfc6bc2f5228278133009"]'
 04000000232524ea04b54489eb222f8b3f566ed35504e3050488a63f0ab7b1c2030000007f91615f04835822bf6984a56482aeeb11d03814352eca9afc0a20192fdcae900000000000000000000000000000000000000000000000000000000000000000268e965ba608071d0800038e16762900000000000000000000000000000000000000000042008dd6fd4005016b4292b05df6dfd316c458c53e28fb2befb96e4870a40c6c04e733d75d8a7a18cce34fe326005efdc403bfa4644e30eeafdaeff34419edc591299e6cc5933cb2eeecbab5c4dfe97cd413b75215999a3dd02b540373581e81d8512bff1640590a6b4da4aaa9b8adc0102c38ca0022daed997b53ed192ba326e212fba5e505ce29e3ad149cef7f48d0e00948a1acd81731d84008760759211eb4abbc7b037939a7964182edb59cf9065357e864188ee5fc7316e8796963036bb99eeb9f06c95d64f78749ecec7181c12eb5d83a3b9b1c1e8a0aae9a20ce04a250b28216620bfc99bb81a6de4db80b93a5aea916de97c1a272e26644abdd683f19c5e3174a2e4513ed767d8f11a4c3074295f697839c5d9139676a813451cc7da38f68cbae5d990a79075f98903233ca04fe1b4b099e433585e5adcc45d41d54a9c648179297359c75950a5e574f13f70b728bbbf552770256315cd0a00139d6ab6934cb5ed70a4fc01a92611b096dd0028f17f4cc687b75f37dca530aa47a18321c50528dbd9272eabb3e13a87021a05918a6d2627e2caba6d7cf1a9f0b831ea3337b9a6af92746d83140078d60c72b6beacf91c9e68a34cee209e08670be1d17ff8d80b7a2285b1325461a2e33f2ee675593f1900e066a5d212615cd8da18749b0e684eee73edcc9031709be715b889c6d015cf4bd4ad5ab7e21bd3492c208930a54d353ef36a437f507ead38855633c1b88d060d9e4221ca8ce2f698e8a6ae0d41e9ace3cbd401f1e0f07650e9c126d4ef20278c8be6e85c7637513643f8d02d7ad64c09da11c16429d60e5160c345844b8158ece62794e8ad280d4e4664150e74978609ece431e51a9f9e1ce8aa49c16f36c7fd12b71acc42d893e18476b8b1e144a8175519612efc93e0aecc61f3b21212c958b0e2331d76aaa62faf11a58fe2bd91ab9ab01b906406c9bbc02df2a106e67182aae0a20b538bf19f09c57f9de5e198ba254580fb1b11e22ad526550093420cb7c68628d4c3ad329c8acc6e219093d277810ed016b6099b7e3781de412a22dacedaa2acf29e8062debcd85c7b9529a20b2782a2470763ac27cf89611a527d43ac89b8063ffb93b6ed993425194f8ee821a8493a563072c896f9584f95db28e3f2fc5fb4a6f3c39d615cd563641717cd50afb73ed3989cbf504b2043882993ce9575f56402534173b1396fbc13df80920b46788ae340ad5a91f25177cc74aa69024d76f56166199d2e4d50a053555256c4e3137ea1cee1130e916a88b6ee5cf2c85652fb8824d5dacfa485e3ef6190591ac0c2fcacc4fc7deb65aca4b0b89b76e35a46b0627e2e967cc63a5d606a984c8e63eabb98fde3e69114340ae524c974cb936e57690e98a7a74533f6f7d1d0496976496b54d14a8163efb32b70dfbb79d80a3022c4f53571c08bf044270565716b435084376714b224ab23e9817c05af8223723afc0577af5c8fc28f71036ca82528aaa4ca9bcd18a50e25d2a528f183d3a2074d968d170876d8dce434c5937261b55173ab87e03d5632ca0834fdc5387c15ab3a17d75c0f274004f289ff1bf7d14e97fdf4172eb49adfb418cc2f4794806ae7c0111c97df4d65d38679ec93fea3ef738ed565e8906a8fe1861cafe3938c772fedcfab40159938e06ef414fd299f2355c6d3369bc1bd3c4db64ce205f0a1b70a40030f505b736e28230de82e97776b5ee7b10708bb3020d28cec7a8e124549ec80c547ac4e7b52bf397c72bcfce30820554ab8fb4d1f73b209bc32a0e7e878843cdbf5f01222728ccea7e6ab7cb5e3fee3234f5b85d1985f91492f6ceaa6454a658dab5074f163ce26ed753137fa61c940679de13bd7b212cd3cf2b334f5201cecbc7473342bd7a239e09169bccd56d03000000037a9068df0625e548e71263c8361b4e904c998378f6b9e32729c3f19b10ad752e093013788222f5c26bfc5da4eeb7d32f01486a54179f19c341b79d420ea041bc8878d22fad4692b2d609c3cf190903874d3682a714c7483518c9392e07c25035010b
 
 ./komodo-cli getrawtransaction bc41a00e429db741c3199f17546a48012fd3b7eea45dfc6bc2f5228278133009
 010000000149964cdcd17fe9b1cae4d0f3b5f5db301d9b4f54099fdf4d34498df281757094010000006a4730440220594f3a630dd73c123f44621aa8bb9968ab86734833453dd479af6d79ae6f584202207bb5e35f13b337ccc8a88d9a006c8c5ddb016c0a6f4f2dc44357a8128623d85d01210223154bf53cd3a75e64d86697070d6437c8f0010a09c1df35b659e31ce3d79b5dffffffff0310270000000000001976a91447d2e323a14b0c3be08698aa46a9b91489b189d688ac701de52d000000001976a91459fdba29ea85c65ad90f6d38f7a6646476b26b1688acb0a86a00000000001976a914f9a9daf5519dae38b8b61d945f075da895df441d88ace18d965b
 
 gatewaysdeposit bindtxid height coin cointxid claimvout deposithex proof destpub amount
 ./komodo-cli -ac_name=AT5 gatewaysdeposit e6c99f79d4afb216aa8063658b4222edb773dd24bb0f8e91bd4ef341f3e47e5e 1003776 KMD bc41a00e429db741c3199f17546a48012fd3b7eea45dfc6bc2f5228278133009 0 010000000149964cdcd17fe9b1cae4d0f3b5f5db301d9b4f54099fdf4d34498df281757094010000006a4730440220594f3a630dd73c123f44621aa8bb9968ab86734833453dd479af6d79ae6f584202207bb5e35f13b337ccc8a88d9a006c8c5ddb016c0a6f4f2dc44357a8128623d85d01210223154bf53cd3a75e64d86697070d6437c8f0010a09c1df35b659e31ce3d79b5dffffffff0310270000000000001976a91447d2e323a14b0c3be08698aa46a9b91489b189d688ac701de52d000000001976a91459fdba29ea85c65ad90f6d38f7a6646476b26b1688acb0a86a00000000001976a914f9a9daf5519dae38b8b61d945f075da895df441d88ace18d965b 04000000232524ea04b54489eb222f8b3f566ed35504e3050488a63f0ab7b1c2030000007f91615f04835822bf6984a56482aeeb11d03814352eca9afc0a20192fdcae900000000000000000000000000000000000000000000000000000000000000000268e965ba608071d0800038e16762900000000000000000000000000000000000000000042008dd6fd4005016b4292b05df6dfd316c458c53e28fb2befb96e4870a40c6c04e733d75d8a7a18cce34fe326005efdc403bfa4644e30eeafdaeff34419edc591299e6cc5933cb2eeecbab5c4dfe97cd413b75215999a3dd02b540373581e81d8512bff1640590a6b4da4aaa9b8adc0102c38ca0022daed997b53ed192ba326e212fba5e505ce29e3ad149cef7f48d0e00948a1acd81731d84008760759211eb4abbc7b037939a7964182edb59cf9065357e864188ee5fc7316e8796963036bb99eeb9f06c95d64f78749ecec7181c12eb5d83a3b9b1c1e8a0aae9a20ce04a250b28216620bfc99bb81a6de4db80b93a5aea916de97c1a272e26644abdd683f19c5e3174a2e4513ed767d8f11a4c3074295f697839c5d9139676a813451cc7da38f68cbae5d990a79075f98903233ca04fe1b4b099e433585e5adcc45d41d54a9c648179297359c75950a5e574f13f70b728bbbf552770256315cd0a00139d6ab6934cb5ed70a4fc01a92611b096dd0028f17f4cc687b75f37dca530aa47a18321c50528dbd9272eabb3e13a87021a05918a6d2627e2caba6d7cf1a9f0b831ea3337b9a6af92746d83140078d60c72b6beacf91c9e68a34cee209e08670be1d17ff8d80b7a2285b1325461a2e33f2ee675593f1900e066a5d212615cd8da18749b0e684eee73edcc9031709be715b889c6d015cf4bd4ad5ab7e21bd3492c208930a54d353ef36a437f507ead38855633c1b88d060d9e4221ca8ce2f698e8a6ae0d41e9ace3cbd401f1e0f07650e9c126d4ef20278c8be6e85c7637513643f8d02d7ad64c09da11c16429d60e5160c345844b8158ece62794e8ad280d4e4664150e74978609ece431e51a9f9e1ce8aa49c16f36c7fd12b71acc42d893e18476b8b1e144a8175519612efc93e0aecc61f3b21212c958b0e2331d76aaa62faf11a58fe2bd91ab9ab01b906406c9bbc02df2a106e67182aae0a20b538bf19f09c57f9de5e198ba254580fb1b11e22ad526550093420cb7c68628d4c3ad329c8acc6e219093d277810ed016b6099b7e3781de412a22dacedaa2acf29e8062debcd85c7b9529a20b2782a2470763ac27cf89611a527d43ac89b8063ffb93b6ed993425194f8ee821a8493a563072c896f9584f95db28e3f2fc5fb4a6f3c39d615cd563641717cd50afb73ed3989cbf504b2043882993ce9575f56402534173b1396fbc13df80920b46788ae340ad5a91f25177cc74aa69024d76f56166199d2e4d50a053555256c4e3137ea1cee1130e916a88b6ee5cf2c85652fb8824d5dacfa485e3ef6190591ac0c2fcacc4fc7deb65aca4b0b89b76e35a46b0627e2e967cc63a5d606a984c8e63eabb98fde3e69114340ae524c974cb936e57690e98a7a74533f6f7d1d0496976496b54d14a8163efb32b70dfbb79d80a3022c4f53571c08bf044270565716b435084376714b224ab23e9817c05af8223723afc0577af5c8fc28f71036ca82528aaa4ca9bcd18a50e25d2a528f183d3a2074d968d170876d8dce434c5937261b55173ab87e03d5632ca0834fdc5387c15ab3a17d75c0f274004f289ff1bf7d14e97fdf4172eb49adfb418cc2f4794806ae7c0111c97df4d65d38679ec93fea3ef738ed565e8906a8fe1861cafe3938c772fedcfab40159938e06ef414fd299f2355c6d3369bc1bd3c4db64ce205f0a1b70a40030f505b736e28230de82e97776b5ee7b10708bb3020d28cec7a8e124549ec80c547ac4e7b52bf397c72bcfce30820554ab8fb4d1f73b209bc32a0e7e878843cdbf5f01222728ccea7e6ab7cb5e3fee3234f5b85d1985f91492f6ceaa6454a658dab5074f163ce26ed753137fa61c940679de13bd7b212cd3cf2b334f5201cecbc7473342bd7a239e09169bccd56d03000000037a9068df0625e548e71263c8361b4e904c998378f6b9e32729c3f19b10ad752e093013788222f5c26bfc5da4eeb7d32f01486a54179f19c341b79d420ea041bc8878d22fad4692b2d609c3cf190903874d3682a714c7483518c9392e07c25035010b 0223d114dededb04f253816d6ad0ce78dd08c617c94ce3c53bf50dc74a5157bef8 7.6999
 -> 9d80ea79a65aaa0d464f8b762356fa01047e16e9793505a22ca04559f81a6eb6
 
 to get the merkleroots onchain, from the multisig signers nodes run the oraclefeed program with acname oracletxid pubkey Ihh
 ./oraclefeed AT5 1f1aefcca2bdea8196cfd77337fb21de22d200ddea977c2f9e8742c55829d808 02ebc786cb83de8dc3922ab83c21f3f8a2f3216940c3bf9da43ce39e2a3a882c92 Ihh
 
 gatewaysclaim bindtxid coin deposittxid destpub amount
 ./c gatewaysclaim e6c99f79d4afb216aa8063658b4222edb773dd24bb0f8e91bd4ef341f3e47e5e KMD 9d80ea79a65aaa0d464f8b762356fa01047e16e9793505a22ca04559f81a6eb6 0223d114dededb04f253816d6ad0ce78dd08c617c94ce3c53bf50dc74a5157bef8 7.6999
 
 now the asset is in the pubkey's asset address!
 it can be used, traded freely and any node who has the asset can do a gatewayswithdraw
 
 gatewayswithdraw bindtxid coin withdrawpub amount
 ./c gatewayswithdraw e6c99f79d4afb216aa8063658b4222edb773dd24bb0f8e91bd4ef341f3e47e5e KMD 03b7621b44118017a16043f19b30cc8a4cfe068ac4e42417bae16ba460c80f3828 1
 ef3cc452da006eb2edda6b6ed3d3347664be51260f3e91f59ec44ec9701367f0
 
 Now there is a withdraw pending, so it needs to be processed by the signing nodes on the KMD side
 
 gatewayspending bindtxid coin
 gatewayspending will display all pending withdraws and if it is done on one of the msigpubkeys, then it will queue it for processing
 ./c gatewayspending e6c99f79d4afb216aa8063658b4222edb773dd24bb0f8e91bd4ef341f3e47e5e KMD
 
 
 Implementation Issues:
    When thinking about validation, it is clear that we cant use EVAL_ASSETS for the locked coins as there wont be any enforcement of the gateways locking. This means we need a way to transfer assets into gateways outputs and back. It seems a tokenconvert rpc will be needed and hopefully that will be enough to make it all work properly.
 
 Care must be taken so that tokens are not lost and can be converted back.
 
 This changes the usage to require tokenconvert before doing the bind and also tokenconvert before doing a withdraw. EVAL_GATEWAYS has evalcode of 241
 
 The gatewaysclaim automatically converts the deposit amount of tokens back to EVAL_ASSETS.
 
 */


int32_t GatewaysAddQueue(std::string coin,uint256 txid,CScript scriptPubKey,int64_t nValue)
{
    char destaddr[64],str[65];
    Getscriptaddress(destaddr,scriptPubKey);
    fprintf(stderr,"GatewaysAddQueue: %s %s %s %.8f\n",coin.c_str(),uint256_str(str,txid),destaddr,(double)nValue/COIN);
}

// start of consensus code

CScript EncodeGatewaysBindOpRet(uint8_t funcid,std::string coin,uint256 tokenid,int64_t totalsupply,uint256 oracletxid,uint8_t M,uint8_t N,std::vector<CPubKey> pubkeys,uint8_t taddr,uint8_t prefix,uint8_t prefix2)
{
    CScript opret; uint8_t evalcode = EVAL_GATEWAYS;
    opret << OP_RETURN << E_MARSHAL(ss << evalcode << funcid << coin << prefix << prefix2 << taddr << tokenid << totalsupply << M << N << pubkeys << oracletxid);
    return(opret);
}

CScript EncodeGatewaysOpRet(uint8_t funcid,std::string coin,uint256 bindtxid,std::vector<CPubKey> publishers,std::vector<uint256>txids,int32_t height,uint256 cointxid,std::string deposithex,std::vector<uint8_t>proof,CPubKey destpub,int64_t amount)
{
    CScript opret; uint8_t evalcode = EVAL_GATEWAYS;
    opret << OP_RETURN << E_MARSHAL(ss << evalcode << funcid << coin << bindtxid << publishers << txids << height << cointxid << deposithex << proof << destpub << amount);
    return(opret);
}

uint8_t DecodeGatewaysOpRet(const CScript &scriptPubKey,std::string &coin,uint256 &bindtxid,std::vector<CPubKey>&publishers,std::vector<uint256>&txids,int32_t &height,uint256 &cointxid,std::string &deposithex,std::vector<uint8_t> &proof,CPubKey &destpub,int64_t &amount)
{
    std::vector<uint8_t> vopret; uint8_t *script,e,f;
    GetOpReturnData(scriptPubKey, vopret);
    script = (uint8_t *)vopret.data();
    if ( vopret.size() > 2 && E_UNMARSHAL(vopret,ss >> e; ss >> f; ss >> coin; ss >> bindtxid; ss >> publishers; ss >> txids; ss >> height; ss >> cointxid; ss >> deposithex; ss >> proof; ss >> destpub; ss >> amount) != 0 )
    {
        return(f);
    }
    return(0);
}

uint8_t DecodeGatewaysBindOpRet(char *depositaddr,const CScript &scriptPubKey,std::string &coin,uint256 &tokenid,int64_t &totalsupply,uint256 &oracletxid,uint8_t &M,uint8_t &N,std::vector<CPubKey> &pubkeys,uint8_t &taddr,uint8_t &prefix,uint8_t &prefix2)
{
    std::vector<uint8_t> vopret; uint8_t *script,e,f;
    GetOpReturnData(scriptPubKey, vopret);
    script = (uint8_t *)vopret.data();
    depositaddr[0] = 0;
    if ( vopret.size() > 2 && E_UNMARSHAL(vopret,ss >> e; ss >> f; ss >> coin; ss >> prefix; ss >> prefix2; ss >> taddr; ss >> tokenid; ss >> totalsupply; ss >> M; ss >> N; ss >> pubkeys; ss >> oracletxid) != 0 )
    {
        if ( prefix == 60 )
        {
            if ( N > 1 )
                Getscriptaddress(depositaddr,GetScriptForMultisig(M,pubkeys));
            else Getscriptaddress(depositaddr,CScript() << ParseHex(HexStr(pubkeys[0])) << OP_CHECKSIG);
        }
        else
        {
            fprintf(stderr,"need to generate non-KMD addresses prefix.%d\n",prefix);
        }
        return(f);
    } else fprintf(stderr,"error decoding bind opret\n");
    return(0);
}

int64_t IsGatewaysvout(struct CCcontract_info *cp,const CTransaction& tx,int32_t v)
{
    char destaddr[64];
    if ( tx.vout[v].scriptPubKey.IsPayToCryptoCondition() != 0 )
    {
        if ( Getscriptaddress(destaddr,tx.vout[v].scriptPubKey) > 0 && strcmp(destaddr,cp->unspendableCCaddr) == 0 )
            return(tx.vout[v].nValue);
    }
    return(0);
}

bool GatewaysExactAmounts(struct CCcontract_info *cp,Eval* eval,const CTransaction &tx,int32_t minage,uint64_t txfee)
{
    static uint256 zerohash;
    CTransaction vinTx; uint256 hashBlock,activehash; int32_t i,numvins,numvouts; int64_t inputs=0,outputs=0,assetoshis;
    numvins = tx.vin.size();
    numvouts = tx.vout.size();
    for (i=0; i<numvins; i++)
    {
        //fprintf(stderr,"vini.%d\n",i);
        if ( (*cp->ismyvin)(tx.vin[i].scriptSig) != 0 )
        {
            //fprintf(stderr,"vini.%d check mempool\n",i);
            if ( eval->GetTxUnconfirmed(tx.vin[i].prevout.hash,vinTx,hashBlock) == 0 )
                return eval->Invalid("cant find vinTx");
            else
            {
                //fprintf(stderr,"vini.%d check hash and vout\n",i);
                if ( hashBlock == zerohash )
                    return eval->Invalid("cant Gateways from mempool");
                if ( (assetoshis= IsGatewaysvout(cp,vinTx,tx.vin[i].prevout.n)) != 0 )
                    inputs += assetoshis;
            }
        }
    }
    for (i=0; i<numvouts; i++)
    {
        //fprintf(stderr,"i.%d of numvouts.%d\n",i,numvouts);
        if ( (assetoshis= IsGatewaysvout(cp,tx,i)) != 0 )
            outputs += assetoshis;
    }
    if ( inputs != outputs+txfee )
    {
        fprintf(stderr,"inputs %llu vs outputs %llu\n",(long long)inputs,(long long)outputs);
        return eval->Invalid("mismatched inputs != outputs + txfee");
    }
    else return(true);
}

bool GatewaysValidate(struct CCcontract_info *cp,Eval *eval,const CTransaction &tx)
{
    int32_t numvins,numvouts,preventCCvins,preventCCvouts,i,numblocks; bool retval; uint256 txid; uint8_t hash[32]; char str[65],destaddr[64];
    std::vector<std::pair<CAddressIndexKey, CAmount> > txids;
    fprintf(stderr,"return true without gateways validation\n");
    return(true);
    numvins = tx.vin.size();
    numvouts = tx.vout.size();
    preventCCvins = preventCCvouts = -1;
    if ( numvouts < 1 )
        return eval->Invalid("no vouts");
    else
    {
        for (i=0; i<numvins; i++)
        {
            if ( IsCCInput(tx.vin[0].scriptSig) == 0 )
            {
                return eval->Invalid("illegal normal vini");
            }
        }
        //fprintf(stderr,"check amounts\n");
        if ( GatewaysExactAmounts(cp,eval,tx,1,10000) == false )
        {
            fprintf(stderr,"Gatewaysget invalid amount\n");
            return false;
        }
        else
        {
            txid = tx.GetHash();
            memcpy(hash,&txid,sizeof(hash));
            retval = PreventCC(eval,tx,preventCCvins,numvins,preventCCvouts,numvouts);
            if ( retval != 0 )
                fprintf(stderr,"Gatewaysget validated\n");
            else fprintf(stderr,"Gatewaysget invalid\n");
            return(retval);
        }
    }
}
// end of consensus code

// helper functions for rpc calls in rpcwallet.cpp

int64_t AddGatewaysInputs(struct CCcontract_info *cp,CMutableTransaction &mtx,CPubKey pk,uint256 refassetid,int64_t total,int32_t maxinputs)
{
    char coinaddr[64],destaddr[64]; int64_t nValue,price,totalinputs = 0; uint256 assetid,txid,hashBlock; std::vector<uint8_t> origpubkey; std::vector<uint8_t> vopret; CTransaction vintx; int32_t j,vout,n = 0; uint8_t evalcode,funcid;
    std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> > unspentOutputs;
    GetCCaddress(cp,coinaddr,pk);
    SetCCunspents(unspentOutputs,coinaddr);
    fprintf(stderr,"check %s for gateway inputs\n",coinaddr);
    for (std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> >::const_iterator it=unspentOutputs.begin(); it!=unspentOutputs.end(); it++)
    {
        txid = it->first.txhash;
        vout = (int32_t)it->first.index;
        for (j=0; j<mtx.vin.size(); j++)
            if ( txid == mtx.vin[j].prevout.hash && vout == mtx.vin[j].prevout.n )
                break;
        if ( j != mtx.vin.size() )
            continue;
        if ( GetTransaction(txid,vintx,hashBlock,false) != 0 )
        {
            Getscriptaddress(destaddr,vintx.vout[vout].scriptPubKey);
            //fprintf(stderr,"%s vout.%d %.8f %.8f\n",destaddr,vout,(double)vintx.vout[vout].nValue/COIN,(double)it->second.satoshis/COIN);
            if ( strcmp(destaddr,coinaddr) != 0 && strcmp(destaddr,cp->unspendableCCaddr) != 0 && strcmp(destaddr,cp->unspendableaddr2) != 0 )
                continue;
            GetOpReturnData(vintx.vout[vintx.vout.size()-1].scriptPubKey, vopret);
            if ( E_UNMARSHAL(vopret,ss >> evalcode; ss >> funcid; ss >> assetid) != 0 )
            {
                assetid = revuint256(assetid);
                //char str[65],str2[65]; fprintf(stderr,"vout.%d %d:%d (%c) check for refassetid.%s vs %s %.8f\n",vout,evalcode,cp->evalcode,funcid,uint256_str(str,refassetid),uint256_str(str2,assetid),(double)vintx.vout[vout].nValue/COIN);
                if ( assetid == refassetid && funcid == 't' && (nValue= vintx.vout[vout].nValue) > 0 && myIsutxo_spentinmempool(txid,vout) == 0 )
                {
                    //fprintf(stderr,"total %llu maxinputs.%d %.8f\n",(long long)total,maxinputs,(double)it->second.satoshis/COIN);
                    if ( total != 0 && maxinputs != 0 )
                        mtx.vin.push_back(CTxIn(txid,vout,CScript()));
                    //nValue = it->second.satoshis;
                    totalinputs += nValue;
                    n++;
                    if ( (total > 0 && totalinputs >= total) || (maxinputs > 0 && n >= maxinputs) )
                        break;
                }
            }
        }
    }
    return(totalinputs);
}

int32_t GatewaysBindExists(struct CCcontract_info *cp,CPubKey gatewayspk,uint256 reftokenid) // dont forget to check mempool!
{
    char markeraddr[64],depositaddr[64]; std::string coin; int32_t numvouts; int64_t totalsupply; uint256 tokenid,oracletxid,hashBlock; uint8_t M,N,taddr,prefix,prefix2; std::vector<CPubKey> pubkeys; CTransaction tx;
    std::vector<std::pair<CAddressIndexKey, CAmount> > addressIndex;
    _GetCCaddress(markeraddr,EVAL_GATEWAYS,gatewayspk);
    fprintf(stderr,"bind markeraddr.(%s) need to scan mempool also\n",markeraddr);
    SetCCtxids(addressIndex,markeraddr);
    for (std::vector<std::pair<CAddressIndexKey, CAmount> >::const_iterator it=addressIndex.begin(); it!=addressIndex.end(); it++)
    {
        if ( GetTransaction(it->first.txhash,tx,hashBlock,false) != 0 && (numvouts= tx.vout.size()) > 0 )
        {
            if ( DecodeGatewaysBindOpRet(depositaddr,tx.vout[numvouts-1].scriptPubKey,coin,tokenid,totalsupply,oracletxid,M,N,pubkeys,taddr,prefix,prefix2) == 'B' )
            {
                if ( tokenid == reftokenid )
                {
                    fprintf(stderr,"trying to bind an existing tokenid\n");
                    return(1);
                }
            }
        }
    }
    return(0);
}

static int32_t myIs_coinaddr_inmempoolvout(char *coinaddr)
{
    int32_t i,n; char destaddr[64];
    BOOST_FOREACH(const CTxMemPoolEntry &e,mempool.mapTx)
    {
        const CTransaction &tx = e.GetTx();
        if ( (n= tx.vout.size()) > 0 )
        {
            const uint256 &txid = tx.GetHash();
            for (i=0; i<n; i++)
            {
                Getscriptaddress(destaddr,tx.vout[i].scriptPubKey);
                if ( strcmp(destaddr,coinaddr) == 0 )
                {
                    fprintf(stderr,"found (%s) vout in mempool\n",coinaddr);
                    return(1);
                }
            }
        }
    }
    return(0);
}

int32_t GatewaysCointxidExists(struct CCcontract_info *cp,uint256 cointxid) // dont forget to check mempool!
{
    char txidaddr[64]; std::string coin; int32_t numvouts; uint256 hashBlock;
    std::vector<std::pair<CAddressIndexKey, CAmount> > addressIndex;
    CCtxidaddr(txidaddr,cointxid);
    SetCCtxids(addressIndex,txidaddr);
    for (std::vector<std::pair<CAddressIndexKey, CAmount> >::const_iterator it=addressIndex.begin(); it!=addressIndex.end(); it++)
    {
        return(-1);
    }
    return(myIs_coinaddr_inmempoolvout(txidaddr));
}

UniValue GatewaysInfo(uint256 bindtxid)
{
    UniValue result(UniValue::VOBJ),a(UniValue::VARR); std::string coin; char str[67],numstr[65],depositaddr[64],gatewaysassets[64]; uint8_t M,N; std::vector<CPubKey> pubkeys; uint8_t taddr,prefix,prefix2; uint256 tokenid,oracletxid,hashBlock; CTransaction tx; CMutableTransaction mtx; CPubKey Gatewayspk; struct CCcontract_info *cp,C; int32_t i; int64_t totalsupply,remaining;
    result.push_back(Pair("result","success"));
    result.push_back(Pair("name","Gateways"));
    cp = CCinit(&C,EVAL_GATEWAYS);
    Gatewayspk = GetUnspendable(cp,0);
    _GetCCaddress(gatewaysassets,EVAL_GATEWAYS,Gatewayspk);
    if ( GetTransaction(bindtxid,tx,hashBlock,false) != 0 )
    {
        depositaddr[0] = 0;
        if ( tx.vout.size() > 0 && DecodeGatewaysBindOpRet(depositaddr,tx.vout[tx.vout.size()-1].scriptPubKey,coin,tokenid,totalsupply,oracletxid,M,N,pubkeys,taddr,prefix,prefix2) != 0 && M <= N && N > 0 )
        {
            if ( N > 1 )
            {
                result.push_back(Pair("M",M));
                result.push_back(Pair("N",N));
                for (i=0; i<N; i++)
                    a.push_back(pubkey33_str(str,(uint8_t *)&pubkeys[i]));
                result.push_back(Pair("pubkeys",a));
            } else result.push_back(Pair("pubkey",pubkey33_str(str,(uint8_t *)&pubkeys[0])));
            result.push_back(Pair("coin",coin));
            result.push_back(Pair("oracletxid",uint256_str(str,oracletxid)));
            result.push_back(Pair("taddr",taddr));
            result.push_back(Pair("prefix",prefix));
            result.push_back(Pair("prefix2",prefix2));
            result.push_back(Pair("deposit",depositaddr));
            result.push_back(Pair("tokenid",uint256_str(str,tokenid)));
            sprintf(numstr,"%.8f",(double)totalsupply/COIN);
            result.push_back(Pair("totalsupply",numstr));
            remaining = CCaddress_balance(gatewaysassets);
            sprintf(numstr,"%.8f",(double)remaining/COIN);
            result.push_back(Pair("remaining",numstr));
            sprintf(numstr,"%.8f",(double)(totalsupply - remaining)/COIN);
            result.push_back(Pair("issued",numstr));
        }
    }
    return(result);
}

UniValue GatewaysList()
{
    UniValue result(UniValue::VARR); std::vector<std::pair<CAddressIndexKey, CAmount> > addressIndex; struct CCcontract_info *cp,C; uint256 txid,hashBlock,oracletxid,tokenid; CTransaction vintx; std::string coin; int64_t totalsupply; char str[65],depositaddr[64]; uint8_t M,N,taddr,prefix,prefix2; std::vector<CPubKey> pubkeys;
    cp = CCinit(&C,EVAL_GATEWAYS);
    SetCCtxids(addressIndex,cp->unspendableCCaddr);
    for (std::vector<std::pair<CAddressIndexKey, CAmount> >::const_iterator it=addressIndex.begin(); it!=addressIndex.end(); it++)
    {
        txid = it->first.txhash;
        if ( GetTransaction(txid,vintx,hashBlock,false) != 0 )
        {
            if ( vintx.vout.size() > 0 && DecodeGatewaysBindOpRet(depositaddr,vintx.vout[vintx.vout.size()-1].scriptPubKey,coin,tokenid,totalsupply,oracletxid,M,N,pubkeys,taddr,prefix,prefix2) != 0 )
            {
                result.push_back(uint256_str(str,txid));
            }
        }
    }
    return(result);
}

std::string GatewaysBind(uint64_t txfee,std::string coin,uint256 tokenid,int64_t totalsupply,uint256 oracletxid,uint8_t M,uint8_t N,std::vector<CPubKey> pubkeys)
{
    CMutableTransaction mtx; CTransaction oracletx; uint8_t taddr,prefix,prefix2; CPubKey mypk,gatewayspk; CScript opret; uint256 hashBlock; struct CCcontract_info *cp,C; std::string name,description,format; int32_t i,numvouts; int64_t fullsupply; char destaddr[64],coinaddr[64],str[65],*fstr;
    cp = CCinit(&C,EVAL_GATEWAYS);
    if ( strcmp((char *)"KMD",coin.c_str()) == 0 )
    {
        taddr = 0;
        prefix = 60;
        prefix2 = 85;
    }
    else
    {
        fprintf(stderr,"set taddr, prefix, prefix2 for %s\n",coin.c_str());
        taddr = 0;
        prefix = 60;
        prefix2 = 85;
    }
    if ( N == 0 || N > 15 || M > N )
    {
        fprintf(stderr,"illegal M.%d or N.%d\n",M,N);
        return("");
    }
    if ( pubkeys.size() != N )
    {
        fprintf(stderr,"M.%d N.%d but pubkeys[%d]\n",M,N,(int32_t)pubkeys.size());
        return("");
    }
    for (i=0; i<N; i++)
    {
        Getscriptaddress(coinaddr,CScript() << ParseHex(HexStr(pubkeys[i])) << OP_CHECKSIG);
        if ( CCaddress_balance(coinaddr) == 0 )
        {
            fprintf(stderr,"M.%d N.%d but pubkeys[%d] has no balance\n",M,N,i);
            return("");
        }
    }
    if ( txfee == 0 )
        txfee = 10000;
    mypk = pubkey2pk(Mypubkey());
    gatewayspk = GetUnspendable(cp,0);
    if ( _GetCCaddress(destaddr,EVAL_GATEWAYS,gatewayspk) == 0 )
    {
        fprintf(stderr,"Gateway bind.%s (%s) cant create globaladdr\n",coin.c_str(),uint256_str(str,tokenid));
        return("");
    }
    if ( (fullsupply= CCfullsupply(tokenid)) != totalsupply )
    {
        fprintf(stderr,"Gateway bind.%s (%s) globaladdr.%s totalsupply %.8f != fullsupply %.8f\n",coin.c_str(),uint256_str(str,tokenid),cp->unspendableCCaddr,(double)totalsupply/COIN,(double)fullsupply/COIN);
        return("");
    }
    if ( CCtoken_balance(destaddr,tokenid) != totalsupply )
    {
        fprintf(stderr,"Gateway bind.%s (%s) destaddr.%s globaladdr.%s token balance %.8f != %.8f\n",coin.c_str(),uint256_str(str,tokenid),destaddr,cp->unspendableCCaddr,(double)CCtoken_balance(destaddr,tokenid)/COIN,(double)totalsupply/COIN);
        return("");
    }
    if ( GetTransaction(oracletxid,oracletx,hashBlock,false) == 0 || (numvouts= oracletx.vout.size()) <= 0 )
    {
        fprintf(stderr,"cant find oracletxid %s\n",uint256_str(str,oracletxid));
        return("");
    }
    if ( DecodeOraclesCreateOpRet(oracletx.vout[numvouts-1].scriptPubKey,name,description,format) != 'C' )
    {
        fprintf(stderr,"mismatched oracle name %s != %s\n",name.c_str(),coin.c_str());
        return("");
    }
    if ( (fstr= (char *)format.c_str()) == 0 || strncmp(fstr,"Ihh",3) != 0 )
    {
        fprintf(stderr,"illegal format (%s) != (%s)\n",fstr,(char *)"Ihh");
        return("");
    }
    if ( GatewaysBindExists(cp,gatewayspk,tokenid) != 0 ) // dont forget to check mempool!
    {
        fprintf(stderr,"Gateway bind.%s (%s) already exists\n",coin.c_str(),uint256_str(str,tokenid));
        return("");
    }
    if ( AddNormalinputs(mtx,mypk,2*txfee,60) > 0 )
    {
        mtx.vout.push_back(MakeCC1vout(cp->evalcode,txfee,gatewayspk));
        return(FinalizeCCTx(0,cp,mtx,mypk,txfee,EncodeGatewaysBindOpRet('B',coin,tokenid,totalsupply,oracletxid,M,N,pubkeys,taddr,prefix,prefix2)));
    }
    fprintf(stderr,"cant find enough inputs\n");
    return("");
}

uint256 GatewaysReverseScan(uint256 &txid,int32_t height,uint256 reforacletxid,uint256 batontxid)
{
    CTransaction tx; uint256 hash,mhash,bhash,hashBlock,oracletxid; int32_t len,len2,numvouts; int64_t val,merkleht; CPubKey pk; std::vector<uint8_t>data;
    txid = zeroid;
    char str[65];
    //fprintf(stderr,"start reverse scan %s\n",uint256_str(str,batontxid));
    while ( GetTransaction(batontxid,tx,hashBlock,false) != 0 && (numvouts= tx.vout.size()) > 0 )
    {
        //fprintf(stderr,"check %s\n",uint256_str(str,batontxid));
        if ( DecodeOraclesData(tx.vout[numvouts-1].scriptPubKey,oracletxid,bhash,pk,data) == 'D' && oracletxid == reforacletxid )
        {
            //fprintf(stderr,"decoded %s\n",uint256_str(str,batontxid));
            if ( oracle_format(&hash,&merkleht,0,'I',(uint8_t *)data.data(),0,(int32_t)data.size()) == sizeof(int32_t) && merkleht == height )
            {
                len = oracle_format(&hash,&val,0,'h',(uint8_t *)data.data(),sizeof(int32_t),(int32_t)data.size());
                len2 = oracle_format(&mhash,&val,0,'h',(uint8_t *)data.data(),(int32_t)(sizeof(int32_t)+sizeof(uint256)),(int32_t)data.size());
                char str2[65]; fprintf(stderr,"found merkleht.%d len.%d len2.%d %s %s\n",(int32_t)merkleht,len,len2,uint256_str(str,hash),uint256_str(str2,mhash));
                if ( len == sizeof(hash)+sizeof(int32_t) && len2 == 2*sizeof(mhash)+sizeof(int32_t) && mhash != zeroid )
                {
                    txid = batontxid;
                    //fprintf(stderr,"set txid\n");
                    return(mhash);
                }
                else
                {
                    //fprintf(stderr,"missing hash\n");
                    return(zeroid);
                }
            } //else fprintf(stderr,"height.%d vs search ht.%d\n",(int32_t)merkleht,(int32_t)height);
            batontxid = bhash;
            //fprintf(stderr,"new hash %s\n",uint256_str(str,batontxid));
        } else break;
    }
    fprintf(stderr,"end of loop\n");
    return(zeroid);
}

/* Get the block merkle root for a proof
 * IN: proofData
 * OUT: merkle root
 * OUT: transaction IDS
 */
uint256 BitcoinGetProofMerkleRoot(const std::vector<uint8_t> &proofData, std::vector<uint256> &txids)
{
    CMerkleBlock merkleBlock;
    if (!E_UNMARSHAL(proofData, ss >> merkleBlock))
        return uint256();
    return merkleBlock.txn.ExtractMatches(txids);
}

int64_t GatewaysVerify(char *refdepositaddr,uint256 oracletxid,int32_t claimvout,std::string refcoin,uint256 cointxid,const std::string deposithex,std::vector<uint8_t>proof,uint256 merkleroot,CPubKey destpub)
{
    std::vector<uint256> txids; uint256 proofroot,hashBlock,txid = zeroid; CTransaction tx; std::string name,description,format; char destaddr[64],destpubaddr[64],claimaddr[64],str[65],str2[65]; int32_t i,numvouts; int64_t nValue = 0;
    if ( GetTransaction(oracletxid,tx,hashBlock,false) == 0 || (numvouts= tx.vout.size()) <= 0 )
    {
        fprintf(stderr,"GatewaysVerify cant find oracletxid %s\n",uint256_str(str,oracletxid));
        return(0);
    }
    if ( DecodeOraclesCreateOpRet(tx.vout[numvouts-1].scriptPubKey,name,description,format) != 'C' || name != refcoin )
    {
        fprintf(stderr,"GatewaysVerify mismatched oracle name %s != %s\n",name.c_str(),refcoin.c_str());
        return(0);
    }
    proofroot = BitcoinGetProofMerkleRoot(proof,txids);
    if ( proofroot != merkleroot )
    {
        fprintf(stderr,"GatewaysVerify mismatched merkleroot %s != %s\n",uint256_str(str,proofroot),uint256_str(str2,merkleroot));
        return(0);
    }
    if ( DecodeHexTx(tx,deposithex) != 0 )
    {
        Getscriptaddress(claimaddr,tx.vout[claimvout].scriptPubKey);
        Getscriptaddress(destpubaddr,CScript() << ParseHex(HexStr(destpub)) << OP_CHECKSIG);
        if ( strcmp(claimaddr,destpubaddr) == 0 )
        {
            for (i=0; i<numvouts; i++)
            {
                Getscriptaddress(destaddr,tx.vout[i].scriptPubKey);
                if ( strcmp(refdepositaddr,destaddr) == 0 )
                {
                    txid = tx.GetHash();
                    nValue = tx.vout[i].nValue;
                    break;
                }
            }
        } else fprintf(stderr,"claimaddr.(%s) != destpubaddr.(%s)\n",claimaddr,destpubaddr);
    }
    if ( txid == cointxid )
    {
        fprintf(stderr,"verified proof for cointxid in merkleroot\n");
        return(nValue);
    } else fprintf(stderr,"(%s) != (%s) or txid %s mismatch.%d or script mismatch\n",refdepositaddr,destaddr,uint256_str(str,txid),txid != cointxid);
    return(0);
}

int64_t GatewaysDepositval(CTransaction tx,CPubKey mypk)
{
    int32_t numvouts,height; int64_t amount; std::string coin,deposithex; std::vector<CPubKey> publishers; std::vector<uint256>txids; uint256 bindtxid,cointxid; std::vector<uint8_t> proof; CPubKey claimpubkey;
    if ( (numvouts= tx.vout.size()) > 0 )
    {
        if ( DecodeGatewaysOpRet(tx.vout[numvouts-1].scriptPubKey,coin,bindtxid,publishers,txids,height,cointxid,deposithex,proof,claimpubkey,amount) == 'D' && claimpubkey == mypk )
        {
            // coin, bindtxid, publishers
            fprintf(stderr,"need to validate deposittxid more\n");
            return(amount);
        }
    }
    return(0);
}

std::string GatewaysDeposit(uint64_t txfee,uint256 bindtxid,int32_t height,std::string refcoin,uint256 cointxid,int32_t claimvout,std::string deposithex,std::vector<uint8_t>proof,CPubKey destpub,int64_t amount)
{
    CMutableTransaction mtx; CTransaction bindtx; CPubKey mypk,gatewayspk; uint256 oracletxid,merkleroot,mhash,hashBlock,tokenid,txid; int64_t totalsupply; int32_t i,m,n,numvouts; uint8_t M,N,taddr,prefix,prefix2; std::string coin; struct CCcontract_info *cp,C; std::vector<CPubKey> pubkeys,publishers; std::vector<uint256>txids; char str[67],depositaddr[64],txidaddr[64];
    cp = CCinit(&C,EVAL_GATEWAYS);
    if ( txfee == 0 )
        txfee = 10000;
    mypk = pubkey2pk(Mypubkey());
    gatewayspk = GetUnspendable(cp,0);
    //fprintf(stderr,"GatewaysDeposit ht.%d %s %.8f numpks.%d\n",height,refcoin.c_str(),(double)amount/COIN,(int32_t)pubkeys.size());
    if ( GetTransaction(bindtxid,bindtx,hashBlock,false) == 0 || (numvouts= bindtx.vout.size()) <= 0 )
    {
        fprintf(stderr,"cant find bindtxid %s\n",uint256_str(str,bindtxid));
        return("");
    }
    if ( DecodeGatewaysBindOpRet(depositaddr,bindtx.vout[numvouts-1].scriptPubKey,coin,tokenid,totalsupply,oracletxid,M,N,pubkeys,taddr,prefix,prefix2) != 'B' || refcoin != coin )
    {
        fprintf(stderr,"invalid bindtxid %s coin.%s\n",uint256_str(str,bindtxid),coin.c_str());
        return("");
    }
    n = (int32_t)pubkeys.size();
    merkleroot = zeroid;
    for (i=m=0; i<n; i++)
    {
        fprintf(stderr,"pubkeys[%d] %s\n",i,pubkey33_str(str,(uint8_t *)&pubkeys[i]));
        if ( (mhash= GatewaysReverseScan(txid,height,oracletxid,OraclesBatontxid(oracletxid,pubkeys[i]))) != zeroid )
        {
            if ( merkleroot == zeroid )
                merkleroot = mhash, m = 1;
            else if ( mhash == merkleroot )
                m++;
            publishers.push_back(pubkeys[i]);
            txids.push_back(txid);
        }
    }
    fprintf(stderr,"cointxid.%s m.%d of n.%d\n",uint256_str(str,cointxid),m,n);
    if ( merkleroot == zeroid || m < n/2 )
    {
        //uint256 tmp;
        //decode_hex((uint8_t *)&tmp,32,(char *)"90aedc2f19200afc9aca2e351438d011ebae8264a58469bf225883045f61917f");
        //merkleroot = revuint256(tmp);
        fprintf(stderr,"couldnt find merkleroot for ht.%d %s oracle.%s m.%d vs n.%d\n",height,coin.c_str(),uint256_str(str,oracletxid),m,n);
        return("");
    }
    if ( GatewaysCointxidExists(cp,cointxid) != 0 )
    {
        fprintf(stderr,"cointxid.%s already exists\n",uint256_str(str,cointxid));
        return("");
    }
    if ( GatewaysVerify(depositaddr,oracletxid,claimvout,coin,cointxid,deposithex,proof,merkleroot,destpub) != amount )
    {
        fprintf(stderr,"deposittxid didnt validate\n");
        return("");
    }
    if ( AddNormalinputs(mtx,mypk,3*txfee,60) > 0 )
    {
        mtx.vout.push_back(MakeCC1vout(cp->evalcode,txfee,mypk));
        mtx.vout.push_back(CTxOut(txfee,CScript() << ParseHex(HexStr(CCtxidaddr(txidaddr,cointxid))) << OP_CHECKSIG));
        return(FinalizeCCTx(0,cp,mtx,mypk,txfee,EncodeGatewaysOpRet('D',coin,bindtxid,publishers,txids,height,cointxid,deposithex,proof,destpub,amount)));
    }
    fprintf(stderr,"cant find enough inputs\n");
    return("");
}

std::string GatewaysClaim(uint64_t txfee,uint256 bindtxid,std::string refcoin,uint256 deposittxid,CPubKey destpub,int64_t amount)
{
    CMutableTransaction mtx; CTransaction tx; CPubKey mypk,gatewayspk; struct CCcontract_info *cp,C; uint8_t M,N,taddr,prefix,prefix2; std::string coin; std::vector<CPubKey> msigpubkeys; int64_t totalsupply,depositamount,inputs,CCchange=0; int32_t numvouts; uint256 hashBlock,assetid,oracletxid; char str[65],depositaddr[64],coinaddr[64];
    cp = CCinit(&C,EVAL_GATEWAYS);
    if ( txfee == 0 )
        txfee = 10000;
    mypk = pubkey2pk(Mypubkey());
    gatewayspk = GetUnspendable(cp,0);
    if ( GetTransaction(bindtxid,tx,hashBlock,false) == 0 || (numvouts= tx.vout.size()) <= 0 )
    {
        fprintf(stderr,"cant find bindtxid %s\n",uint256_str(str,bindtxid));
        return("");
    }
    if ( DecodeGatewaysBindOpRet(depositaddr,tx.vout[numvouts-1].scriptPubKey,coin,assetid,totalsupply,oracletxid,M,N,msigpubkeys,taddr,prefix,prefix2) != 'B' || coin != refcoin )
    {
        fprintf(stderr,"invalid bindtxid %s coin.%s\n",uint256_str(str,bindtxid),coin.c_str());
        return("");
    }
    if ( GetTransaction(deposittxid,tx,hashBlock,false) == 0 )
    {
        fprintf(stderr,"cant find deposittxid %s\n",uint256_str(str,bindtxid));
        return("");
    }
    if ( (depositamount= GatewaysDepositval(tx,mypk)) != amount )
    {
        fprintf(stderr,"invalid Gateways deposittxid %s %.8f != %.8f\n",uint256_str(str,deposittxid),(double)depositamount/COIN,(double)amount/COIN);
        return("");
    }
    //fprintf(stderr,"depositaddr.(%s) vs %s\n",depositaddr,cp->unspendableaddr2);
    if ( AddNormalinputs(mtx,mypk,txfee,1) > 0 )
    {
        if ( (inputs= AddGatewaysInputs(cp,mtx,gatewayspk,assetid,amount,60)) > 0 )
        {
            if ( inputs > amount )
                CCchange = (inputs - amount);
            mtx.vin.push_back(CTxIn(deposittxid,0,CScript())); // triggers EVAL_GATEWAYS validation
            mtx.vout.push_back(MakeCC1vout(EVAL_ASSETS,amount,mypk)); // transfer back to normal token
            if ( CCchange != 0 )
                mtx.vout.push_back(MakeCC1vout(EVAL_GATEWAYS,CCchange,gatewayspk));
            return(FinalizeCCTx(0,cp,mtx,mypk,txfee,EncodeAssetOpRet('t',assetid,zeroid,0,Mypubkey())));
        }
    }
    fprintf(stderr,"cant find enough inputs or mismatched total\n");
    return("");
}

std::string GatewaysWithdraw(uint64_t txfee,uint256 bindtxid,std::string refcoin,std::vector<uint8_t> withdrawpub,int64_t amount)
{
    CMutableTransaction mtx; CTransaction tx; CPubKey mypk,gatewayspk; struct CCcontract_info *cp,C; uint256 assetid,hashBlock,oracletxid; int32_t numvouts; int64_t totalsupply,inputs,CCchange=0; uint8_t M,N,taddr,prefix,prefix2; std::string coin; std::vector<CPubKey> msigpubkeys; char depositaddr[64],str[65],coinaddr[64];
    cp = CCinit(&C,EVAL_GATEWAYS);
    if ( txfee == 0 )
        txfee = 10000;
    mypk = pubkey2pk(Mypubkey());
    gatewayspk = GetUnspendable(cp,0);
    if ( GetTransaction(bindtxid,tx,hashBlock,false) == 0 || (numvouts= tx.vout.size()) <= 0 )
    {
        fprintf(stderr,"cant find bindtxid %s\n",uint256_str(str,bindtxid));
        return("");
    }
    if ( DecodeGatewaysBindOpRet(depositaddr,tx.vout[numvouts-1].scriptPubKey,coin,assetid,totalsupply,oracletxid,M,N,msigpubkeys,taddr,prefix,prefix2) != 'B' || coin != refcoin )
    {
        fprintf(stderr,"invalid bindtxid %s coin.%s\n",uint256_str(str,bindtxid),coin.c_str());
        return("");
    }
    if ( AddNormalinputs(mtx,mypk,3*txfee,3) > 0 )
    {
        if ( (inputs= AddGatewaysInputs(cp,mtx,mypk,assetid,amount,60)) > 0 )
        {
            if ( inputs > amount )
                CCchange = (inputs - amount);
            mtx.vout.push_back(MakeCC1vout(EVAL_GATEWAYS,amount,gatewayspk));
            mtx.vout.push_back(CTxOut(txfee,CScript() << withdrawpub << OP_CHECKSIG));
            mtx.vout.push_back(CTxOut(txfee,CScript() << ParseHex(HexStr(gatewayspk)) << OP_CHECKSIG));
            if ( CCchange != 0 )
                mtx.vout.push_back(MakeCC1vout(EVAL_GATEWAYS,CCchange,mypk));
            return(FinalizeCCTx(0,cp,mtx,mypk,txfee,EncodeAssetOpRet('t',assetid,zeroid,0,Mypubkey())));
        }
    }
    fprintf(stderr,"cant find enough inputs or mismatched total\n");
    return("");
}

std::string GatewaysMarkdone(uint64_t txfee,uint256 withdrawtxid,std::string refcoin,uint256 cointxid)
{
    CMutableTransaction mtx; CScript opret; CPubKey mypk; struct CCcontract_info *cp,C;
    cp = CCinit(&C,EVAL_GATEWAYS);
    if ( txfee == 0 )
        txfee = 5000;
    mypk = pubkey2pk(Mypubkey());
    mtx.vin.push_back(CTxIn(withdrawtxid,2,CScript()));
    mtx.vout.push_back(CTxOut(5000,CScript() << ParseHex(HexStr(mypk)) << OP_CHECKSIG));
    opret << OP_RETURN << E_MARSHAL(ss << cp->evalcode << 'M' << cointxid << refcoin);
    return(FinalizeCCTx(0,cp,mtx,mypk,txfee,opret));
}

UniValue GatewaysPendingWithdraws(uint256 bindtxid,std::string refcoin)
{
    UniValue result(UniValue::VOBJ),pending(UniValue::VARR),obj(UniValue::VOBJ); CTransaction tx; std::string coin; CPubKey mypk,gatewayspk; std::vector<CPubKey> msigpubkeys; uint256 hashBlock,assetid,txid,oracletxid; uint8_t M,N,taddr,prefix,prefix2; char depositaddr[64],withmarker[64],coinaddr[64],destaddr[64],str[65],withaddr[64],numstr[32],txidaddr[64],signeraddr[64]; int32_t i,n,numvouts,vout,numqueued,queueflag; int64_t totalsupply; struct CCcontract_info *cp,C;
    std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> > unspentOutputs;
    cp = CCinit(&C,EVAL_GATEWAYS);
    mypk = pubkey2pk(Mypubkey());
    gatewayspk = GetUnspendable(cp,0);
    _GetCCaddress(coinaddr,EVAL_GATEWAYS,gatewayspk);
    if ( GetTransaction(bindtxid,tx,hashBlock,false) == 0 || (numvouts= tx.vout.size()) <= 0 )
    {
        fprintf(stderr,"cant find bindtxid %s\n",uint256_str(str,bindtxid));
        return(result);
    }
    if ( DecodeGatewaysBindOpRet(depositaddr,tx.vout[numvouts-1].scriptPubKey,coin,assetid,totalsupply,oracletxid,M,N,msigpubkeys,taddr,prefix,prefix2) != 'B' || coin != refcoin )
    {
        fprintf(stderr,"invalid bindtxid %s coin.%s\n",uint256_str(str,bindtxid),coin.c_str());
        return(result);
    }
    n = msigpubkeys.size();
    queueflag = 0;
    for (i=0; i<n; i++)
        if ( msigpubkeys[i] == mypk )
        {
            queueflag = 1;
            break;
        }
    Getscriptaddress(withmarker,CScript() << ParseHex(HexStr(gatewayspk)) << OP_CHECKSIG);
    SetCCunspents(unspentOutputs,withmarker);
    numqueued = 0;
    for (std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> >::const_iterator it=unspentOutputs.begin(); it!=unspentOutputs.end(); it++)
    {
        txid = it->first.txhash;
        vout = (int32_t)it->first.index;
        if ( GetTransaction(txid,tx,hashBlock,false) != 0 )
        {
            Getscriptaddress(destaddr,tx.vout[0].scriptPubKey);
            Getscriptaddress(withaddr,tx.vout[1].scriptPubKey);
            if ( strcmp(destaddr,coinaddr) == 0 )
            {
                obj.push_back(Pair("txid",uint256_str(str,txid)));
                CCtxidaddr(txidaddr,txid);
                obj.push_back(Pair("txidaddr",txidaddr));
                obj.push_back(Pair("withdrawaddr",withaddr));
                sprintf(numstr,"%.8f",(double)tx.vout[0].nValue/COIN);
                obj.push_back(Pair("amount",numstr));
                if ( queueflag != 0 )
                {
                    obj.push_back(Pair("depositaddr",depositaddr));
                    Getscriptaddress(signeraddr,CScript() << ParseHex(HexStr(mypk)) << OP_CHECKSIG);
                    obj.push_back(Pair("signeraddr",signeraddr));
                }
                pending.push_back(obj);
            }
        }
    }
    result.push_back(Pair("coin",refcoin));
    result.push_back(Pair("pending",pending));
    result.push_back(Pair("queueflag",queueflag));
    return(result);
}

std::string GatewaysMultisig(uint64_t txfee,std::string refcoin,uint256 bindtxid,uint256 withdrawtxid,char *txidaddr)
{
    UniValue result(UniValue::VOBJ); std::string coin,hex; char str[67],numstr[65],depositaddr[64],gatewaysassets[64]; uint8_t M,N; std::vector<CPubKey> pubkeys; uint8_t taddr,prefix,prefix2; uint256 tokenid,oracletxid,hashBlock; CTransaction tx; CPubKey Gatewayspk,mypk; struct CCcontract_info *cp,C; int32_t i,n,complete,partialtx; int64_t totalsupply;
    cp = CCinit(&C,EVAL_GATEWAYS);
    if ( txfee == 0 )
        txfee = 10000;
    complete = partialtx = 0;
    mypk = pubkey2pk(Mypubkey());
    Gatewayspk = GetUnspendable(cp,0);
    _GetCCaddress(gatewaysassets,EVAL_GATEWAYS,Gatewayspk);
    if ( GetTransaction(bindtxid,tx,hashBlock,false) != 0 )
    {
        depositaddr[0] = 0;
        if ( tx.vout.size() > 0 && DecodeGatewaysBindOpRet(depositaddr,tx.vout[tx.vout.size()-1].scriptPubKey,coin,tokenid,totalsupply,oracletxid,M,N,pubkeys,taddr,prefix,prefix2) != 0 && M <= N && N > 1 && coin == refcoin )
        {
            // need a decentralized way to add signatures to msig tx
            n = pubkeys.size();
            for (i=0; i<n; i++)
                if ( mypk == pubkeys[i] )
                    break;
            if ( i != n )
            {
                hex = "";
            } else fprintf(stderr,"not one of the multisig signers\n");
        }
    }
    return(hex);
}
