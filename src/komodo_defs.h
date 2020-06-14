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

#ifndef KOMODO_DEFS_H
#define KOMODO_DEFS_H
#include "arith_uint256.h"
#include "chain.h"
#include "komodo_nk.h"

#define KOMODO_EARLYTXID_HEIGHT 100
//#define ADAPTIVEPOW_CHANGETO_DEFAULTON 1572480000
#define ASSETCHAINS_MINHEIGHT 128
#define ASSETCHAINS_MAX_ERAS 7
#define KOMODO_ELECTION_GAP 2000
#define ROUNDROBIN_DELAY 61
#define KOMODO_ASSETCHAIN_MAXLEN 65
#define KOMODO_LIMITED_NETWORKSIZE 4
#define IGUANA_MAXSCRIPTSIZE 10001
#define KOMODO_MAXMEMPOOLTIME 3600 // affects consensus
#define CRYPTO777_PUBSECPSTR "020e46e79a2a8d12b9b5d12c7a91adb4e454edfae43c0a0cb805427d2ac7613fd9"
#define KOMODO_FIRSTFUNGIBLEID 100
#define KOMODO_SAPLING_ACTIVATION 1544832000 // Dec 15th, 2018
#define KOMODO_SAPLING_DEADLINE 1550188800 // Feb 15th, 2019
#define ASSETCHAINS_STAKED_BLOCK_FUTURE_MAX 57
#define ASSETCHAINS_STAKED_BLOCK_FUTURE_HALF 27
#define ASSETCHAINS_STAKED_MIN_POW_DIFF 536900000 // 537000000 537300000
#define _COINBASE_MATURITY 100

// KMD Notary Seasons 
// 1: May 1st 2018 1530921600
// 2: July 15th 2019 1563148800 -> estimated height 1444000
// 3: 3rd season ending isnt known, so use very far times in future.
    // 1751328000 = dummy timestamp, 1 July 2025!
    // 7113400 = 5x current KMD blockheight. 
// to add 4th season, change NUM_KMD_SEASONS to 4, and add timestamp and height of activation to these arrays. 

#define NUM_KMD_SEASONS 5
#define NUM_KMD_NOTARIES 64

extern const uint32_t nStakedDecemberHardforkTimestamp; //December 2019 hardfork
extern const int32_t nDecemberHardforkHeight;   //December 2019 hardfork

extern const uint32_t nS4Timestamp; //dPoW Season 4 2020 hardfork
extern const int32_t nS4HardforkHeight;   //dPoW Season 4 2020 hardfork

static const uint32_t KMD_SEASON_TIMESTAMPS[NUM_KMD_SEASONS] = {1525132800, 1563148800, nStakedDecemberHardforkTimestamp, nS4Timestamp, 1751328000};
static const int32_t KMD_SEASON_HEIGHTS[NUM_KMD_SEASONS] = {814000, 1444000, nDecemberHardforkHeight, nS4HardforkHeight, 7113400};

// Era array of pubkeys. Add extra seasons to bottom as requried, after adding appropriate info above. 
static const char *notaries_elected[NUM_KMD_SEASONS][NUM_KMD_NOTARIES][2] =
{
    {
        { "0_jl777_testA", "03b7621b44118017a16043f19b30cc8a4cfe068ac4e42417bae16ba460c80f3828" },
        { "0_jl777_testB", "02ebfc784a4ba768aad88d44d1045d240d47b26e248cafaf1c5169a42d7a61d344" },
        { "0_kolo_testA", "0287aa4b73988ba26cf6565d815786caf0d2c4af704d7883d163ee89cd9977edec" },
        { "artik_AR", "029acf1dcd9f5ff9c455f8bb717d4ae0c703e089d16cf8424619c491dff5994c90" },
        { "artik_EU", "03f54b2c24f82632e3cdebe4568ba0acf487a80f8a89779173cdb78f74514847ce" },
        { "artik_NA", "0224e31f93eff0cc30eaf0b2389fbc591085c0e122c4d11862c1729d090106c842" },
        { "artik_SH", "02bdd8840a34486f38305f311c0e2ae73e84046f6e9c3dd3571e32e58339d20937" },
        { "badass_EU", "0209d48554768dd8dada988b98aca23405057ac4b5b46838a9378b95c3e79b9b9e" },
        { "badass_NA", "02afa1a9f948e1634a29dc718d218e9d150c531cfa852843a1643a02184a63c1a7" },
        { "badass_SH", "026b49dd3923b78a592c1b475f208e23698d3f085c4c3b4906a59faf659fd9530b" },
        { "crackers_EU", "03bc819982d3c6feb801ec3b720425b017d9b6ee9a40746b84422cbbf929dc73c3" }, // 10
        { "crackers_NA", "03205049103113d48c7c7af811b4c8f194dafc43a50d5313e61a22900fc1805b45" },
        { "crackers_SH", "02be28310e6312d1dd44651fd96f6a44ccc269a321f907502aae81d246fabdb03e" },
        { "durerus_EU", "02bcbd287670bdca2c31e5d50130adb5dea1b53198f18abeec7211825f47485d57" },
        { "etszombi_AR", "031c79168d15edabf17d9ec99531ea9baa20039d0cdc14d9525863b83341b210e9" },
        { "etszombi_EU", "0281b1ad28d238a2b217e0af123ce020b79e91b9b10ad65a7917216eda6fe64bf7" }, // 15
        { "etszombi_SH", "025d7a193c0757f7437fad3431f027e7b5ed6c925b77daba52a8755d24bf682dde" },
        { "farl4web_EU", "0300ecf9121cccf14cf9423e2adb5d98ce0c4e251721fa345dec2e03abeffbab3f" },
        { "farl4web_SH", "0396bb5ed3c57aa1221d7775ae0ff751e4c7dc9be220d0917fa8bbdf670586c030" },
        { "fullmoon_AR", "0254b1d64840ce9ff6bec9dd10e33beb92af5f7cee628f999cb6bc0fea833347cc" },
        { "fullmoon_NA", "031fb362323b06e165231c887836a8faadb96eda88a79ca434e28b3520b47d235b" }, // 20
        { "fullmoon_SH", "030e12b42ec33a80e12e570b6c8274ce664565b5c3da106859e96a7208b93afd0d" },
        { "grewal_NA", "03adc0834c203d172bce814df7c7a5e13dc603105e6b0adabc942d0421aefd2132" },
        { "grewal_SH", "03212a73f5d38a675ee3cdc6e82542a96c38c3d1c79d25a1ed2e42fcf6a8be4e68" },
        { "indenodes_AR", "02ec0fa5a40f47fd4a38ea5c89e375ad0b6ddf4807c99733c9c3dc15fb978ee147" },
        { "indenodes_EU", "0221387ff95c44cb52b86552e3ec118a3c311ca65b75bf807c6c07eaeb1be8303c" },
        { "indenodes_NA", "02698c6f1c9e43b66e82dbb163e8df0e5a2f62f3a7a882ca387d82f86e0b3fa988" },
        { "indenodes_SH", "0334e6e1ec8285c4b85bd6dae67e17d67d1f20e7328efad17ce6fd24ae97cdd65e" },
        { "jeezy_EU", "023cb3e593fb85c5659688528e9a4f1c4c7f19206edc7e517d20f794ba686fd6d6" },
        { "jsgalt_NA", "027b3fb6fede798cd17c30dbfb7baf9332b3f8b1c7c513f443070874c410232446" },
        { "karasugoi_NA", "02a348b03b9c1a8eac1b56f85c402b041c9bce918833f2ea16d13452309052a982" }, // 30
        { "kashifali_EU", "033777c52a0190f261c6f66bd0e2bb299d30f012dcb8bfff384103211edb8bb207" },
        { "kolo_AR", "03016d19344c45341e023b72f9fb6e6152fdcfe105f3b4f50b82a4790ff54e9dc6" },
        { "kolo_SH", "02aa24064500756d9b0959b44d5325f2391d8e95c6127e109184937152c384e185" },
        { "metaphilibert_AR", "02adad675fae12b25fdd0f57250b0caf7f795c43f346153a31fe3e72e7db1d6ac6" },
        { "movecrypto_AR", "022783d94518e4dc77cbdf1a97915b29f427d7bc15ea867900a76665d3112be6f3" },
        { "movecrypto_EU", "021ab53bc6cf2c46b8a5456759f9d608966eff87384c2b52c0ac4cc8dd51e9cc42" },
        { "movecrypto_NA", "02efb12f4d78f44b0542d1c60146738e4d5506d27ec98a469142c5c84b29de0a80" },
        { "movecrypto_SH", "031f9739a3ebd6037a967ce1582cde66e79ea9a0551c54731c59c6b80f635bc859" },
        { "muros_AR", "022d77402fd7179335da39479c829be73428b0ef33fb360a4de6890f37c2aa005e" },
        { "noashh_AR", "029d93ef78197dc93892d2a30e5a54865f41e0ca3ab7eb8e3dcbc59c8756b6e355" }, // 40
        { "noashh_EU", "02061c6278b91fd4ac5cab4401100ffa3b2d5a277e8f71db23401cc071b3665546" },
        { "noashh_NA", "033c073366152b6b01535e15dd966a3a8039169584d06e27d92a69889b720d44e1" },
        { "nxtswe_EU", "032fb104e5eaa704a38a52c126af8f67e870d70f82977e5b2f093d5c1c21ae5899" },
        { "polycryptoblog_NA", "02708dcda7c45fb54b78469673c2587bfdd126e381654819c4c23df0e00b679622" },
        { "pondsea_AR", "032e1c213787312099158f2d74a89e8240a991d162d4ce8017d8504d1d7004f735" },
        { "pondsea_EU", "0225aa6f6f19e543180b31153d9e6d55d41bc7ec2ba191fd29f19a2f973544e29d" },
        { "pondsea_NA", "031bcfdbb62268e2ff8dfffeb9ddff7fe95fca46778c77eebff9c3829dfa1bb411" },
        { "pondsea_SH", "02209073bc0943451498de57f802650311b1f12aa6deffcd893da198a544c04f36" },
        { "popcornbag_AR", "02761f106fb34fbfc5ddcc0c0aa831ed98e462a908550b280a1f7bd32c060c6fa3" },
        { "popcornbag_NA", "03c6085c7fdfff70988fda9b197371f1caf8397f1729a844790e421ee07b3a93e8" }, // 50
        { "ptytrader_NA", "0328c61467148b207400b23875234f8a825cce65b9c4c9b664f47410b8b8e3c222" },
        { "ptytrader_SH", "0250c93c492d8d5a6b565b90c22bee07c2d8701d6118c6267e99a4efd3c7748fa4" },
        { "rnr_AR", "029bdb08f931c0e98c2c4ba4ef45c8e33a34168cb2e6bf953cef335c359d77bfcd" },
        { "rnr_EU", "03f5c08dadffa0ffcafb8dd7ffc38c22887bd02702a6c9ac3440deddcf2837692b" },
        { "rnr_NA", "02e17c5f8c3c80f584ed343b8dcfa6d710dfef0889ec1e7728ce45ce559347c58c" },
        { "rnr_SH", "037536fb9bdfed10251f71543fb42679e7c52308bcd12146b2568b9a818d8b8377" },
        { "titomane_AR", "03cda6ca5c2d02db201488a54a548dbfc10533bdc275d5ea11928e8d6ab33c2185" },
        { "titomane_EU", "02e41feded94f0cc59f55f82f3c2c005d41da024e9a805b41105207ef89aa4bfbd" },
        { "titomane_SH", "035f49d7a308dd9a209e894321f010d21b7793461b0c89d6d9231a3fe5f68d9960" },
        { "vanbreuk_EU", "024f3cad7601d2399c131fd070e797d9cd8533868685ddbe515daa53c2e26004c3" }, // 60
        { "xrobesx_NA", "03f0cc6d142d14a40937f12dbd99dbd9021328f45759e26f1877f2a838876709e1" },
        { "xxspot1_XX", "02ef445a392fcaf3ad4176a5da7f43580e8056594e003eba6559a713711a27f955" },
        { "xxspot2_XX", "03d85b221ea72ebcd25373e7961f4983d12add66a92f899deaf07bab1d8b6f5573" }
    },
    {
        {"0dev1_jl777", "03b7621b44118017a16043f19b30cc8a4cfe068ac4e42417bae16ba460c80f3828" },
        {"0dev2_kolo", "030f34af4b908fb8eb2099accb56b8d157d49f6cfb691baa80fdd34f385efed961" },
        {"0dev3_kolo", "025af9d2b2a05338478159e9ac84543968fd18c45fd9307866b56f33898653b014" },
        {"0dev4_decker", "028eea44a09674dda00d88ffd199a09c9b75ba9782382cc8f1e97c0fd565fe5707" },
        {"a-team_SH", "03b59ad322b17cb94080dc8e6dc10a0a865de6d47c16fb5b1a0b5f77f9507f3cce" },
        {"artik_AR", "029acf1dcd9f5ff9c455f8bb717d4ae0c703e089d16cf8424619c491dff5994c90" },
        {"artik_EU", "03f54b2c24f82632e3cdebe4568ba0acf487a80f8a89779173cdb78f74514847ce" },
        {"artik_NA", "0224e31f93eff0cc30eaf0b2389fbc591085c0e122c4d11862c1729d090106c842" },
        {"artik_SH", "02bdd8840a34486f38305f311c0e2ae73e84046f6e9c3dd3571e32e58339d20937" },
        {"badass_EU", "0209d48554768dd8dada988b98aca23405057ac4b5b46838a9378b95c3e79b9b9e" },
        {"badass_NA", "02afa1a9f948e1634a29dc718d218e9d150c531cfa852843a1643a02184a63c1a7" }, // 10
        {"batman_AR", "033ecb640ec5852f42be24c3bf33ca123fb32ced134bed6aa2ba249cf31b0f2563" },
        {"batman_SH", "02ca5898931181d0b8aafc75ef56fce9c43656c0b6c9f64306e7c8542f6207018c" },
        {"ca333_EU", "03fc87b8c804f12a6bd18efd43b0ba2828e4e38834f6b44c0bfee19f966a12ba99" },
        {"chainmakers_EU", "02f3b08938a7f8d2609d567aebc4989eeded6e2e880c058fdf092c5da82c3bc5ee" },
        {"chainmakers_NA", "0276c6d1c65abc64c8559710b8aff4b9e33787072d3dda4ec9a47b30da0725f57a" },
        {"chainstrike_SH", "0370bcf10575d8fb0291afad7bf3a76929734f888228bc49e35c5c49b336002153" },
        {"cipi_AR", "02c4f89a5b382750836cb787880d30e23502265054e1c327a5bfce67116d757ce8" },
        {"cipi_NA", "02858904a2a1a0b44df4c937b65ee1f5b66186ab87a751858cf270dee1d5031f18" },
        {"crackers_EU", "03bc819982d3c6feb801ec3b720425b017d9b6ee9a40746b84422cbbf929dc73c3" },
        {"crackers_NA", "03205049103113d48c7c7af811b4c8f194dafc43a50d5313e61a22900fc1805b45" }, // 20
        {"dwy_EU", "0259c646288580221fdf0e92dbeecaee214504fdc8bbdf4a3019d6ec18b7540424" },
        {"emmanux_SH", "033f316114d950497fc1d9348f03770cd420f14f662ab2db6172df44c389a2667a" },
        {"etszombi_EU", "0281b1ad28d238a2b217e0af123ce020b79e91b9b10ad65a7917216eda6fe64bf7" },
        {"fullmoon_AR", "03380314c4f42fa854df8c471618751879f9e8f0ff5dbabda2bd77d0f96cb35676" },
        {"fullmoon_NA", "030216211d8e2a48bae9e5d7eb3a42ca2b7aae8770979a791f883869aea2fa6eef" },
        {"fullmoon_SH", "03f34282fa57ecc7aba8afaf66c30099b5601e98dcbfd0d8a58c86c20d8b692c64" },
        {"goldenman_EU", "02d6f13a8f745921cdb811e32237bb98950af1a5952be7b3d429abd9152f8e388d" },
        {"indenodes_AR", "02ec0fa5a40f47fd4a38ea5c89e375ad0b6ddf4807c99733c9c3dc15fb978ee147" },
        {"indenodes_EU", "0221387ff95c44cb52b86552e3ec118a3c311ca65b75bf807c6c07eaeb1be8303c" },
        {"indenodes_NA", "02698c6f1c9e43b66e82dbb163e8df0e5a2f62f3a7a882ca387d82f86e0b3fa988" }, // 30
        {"indenodes_SH", "0334e6e1ec8285c4b85bd6dae67e17d67d1f20e7328efad17ce6fd24ae97cdd65e" },
        {"jackson_AR", "038ff7cfe34cb13b524e0941d5cf710beca2ffb7e05ddf15ced7d4f14fbb0a6f69" },
        {"jeezy_EU", "023cb3e593fb85c5659688528e9a4f1c4c7f19206edc7e517d20f794ba686fd6d6" },
        {"karasugoi_NA", "02a348b03b9c1a8eac1b56f85c402b041c9bce918833f2ea16d13452309052a982" },
        {"komodoninja_EU", "038e567b99806b200b267b27bbca2abf6a3e8576406df5f872e3b38d30843cd5ba" },
        {"komodoninja_SH", "033178586896915e8456ebf407b1915351a617f46984001790f0cce3d6f3ada5c2" },
        {"komodopioneers_SH", "033ace50aedf8df70035b962a805431363a61cc4e69d99d90726a2d48fb195f68c" },
        {"libscott_SH", "03301a8248d41bc5dc926088a8cf31b65e2daf49eed7eb26af4fb03aae19682b95" },
        {"lukechilds_AR", "031aa66313ee024bbee8c17915cf7d105656d0ace5b4a43a3ab5eae1e14ec02696" },
        {"madmax_AR", "03891555b4a4393d655bf76f0ad0fb74e5159a615b6925907678edc2aac5e06a75" }, // 40
        {"meshbits_AR", "02957fd48ae6cb361b8a28cdb1b8ccf5067ff68eb1f90cba7df5f7934ed8eb4b2c" },
        {"meshbits_SH", "025c6e94877515dfd7b05682b9cc2fe4a49e076efe291e54fcec3add78183c1edb" },
        {"metaphilibert_AR", "02adad675fae12b25fdd0f57250b0caf7f795c43f346153a31fe3e72e7db1d6ac6" },
        {"metaphilibert_SH", "0284af1a5ef01503e6316a2ca4abf8423a794e9fc17ac6846f042b6f4adedc3309" },
        {"patchkez_SH", "0296270f394140640f8fa15684fc11255371abb6b9f253416ea2734e34607799c4" },
        {"pbca26_NA", "0276aca53a058556c485bbb60bdc54b600efe402a8b97f0341a7c04803ce204cb5" },
        {"peer2cloud_AR", "034e5563cb885999ae1530bd66fab728e580016629e8377579493b386bf6cebb15" },
        {"peer2cloud_SH", "03396ac453b3f23e20f30d4793c5b8ab6ded6993242df4f09fd91eb9a4f8aede84" },
        {"polycryptoblog_NA", "02708dcda7c45fb54b78469673c2587bfdd126e381654819c4c23df0e00b679622" },
        {"hyper_AR", "020f2f984d522051bd5247b61b080b4374a7ab389d959408313e8062acad3266b4" }, // 50
        {"hyper_EU", "03d00cf9ceace209c59fb013e112a786ad583d7de5ca45b1e0df3b4023bb14bf51" },
        {"hyper_SH", "0383d0b37f59f4ee5e3e98a47e461c861d49d0d90c80e9e16f7e63686a2dc071f3" },
        {"hyper_NA", "03d91c43230336c0d4b769c9c940145a8c53168bf62e34d1bccd7f6cfc7e5592de" },
        {"popcornbag_AR", "02761f106fb34fbfc5ddcc0c0aa831ed98e462a908550b280a1f7bd32c060c6fa3" },
        {"popcornbag_NA", "03c6085c7fdfff70988fda9b197371f1caf8397f1729a844790e421ee07b3a93e8" },
        {"alien_AR", "0348d9b1fc6acf81290405580f525ee49b4749ed4637b51a28b18caa26543b20f0" },
        {"alien_EU", "020aab8308d4df375a846a9e3b1c7e99597b90497efa021d50bcf1bbba23246527" },
        {"thegaltmines_NA", "031bea28bec98b6380958a493a703ddc3353d7b05eb452109a773eefd15a32e421" },
        {"titomane_AR", "029d19215440d8cb9cc6c6b7a4744ae7fb9fb18d986e371b06aeb34b64845f9325" },
        {"titomane_EU", "0360b4805d885ff596f94312eed3e4e17cb56aa8077c6dd78d905f8de89da9499f" }, // 60
        {"titomane_SH", "03573713c5b20c1e682a2e8c0f8437625b3530f278e705af9b6614de29277a435b" },
        {"webworker01_NA", "03bb7d005e052779b1586f071834c5facbb83470094cff5112f0072b64989f97d7" },
        {"xrobesx_NA", "03f0cc6d142d14a40937f12dbd99dbd9021328f45759e26f1877f2a838876709e1" },
    },
    {
        {"madmax_NA", "0237e0d3268cebfa235958808db1efc20cc43b31100813b1f3e15cc5aa647ad2c3" }, // 0
        {"alright_AR", "020566fe2fb3874258b2d3cf1809a5d650e0edc7ba746fa5eec72750c5188c9cc9" },
        {"strob_NA", "0206f7a2e972d9dfef1c424c731503a0a27de1ba7a15a91a362dc7ec0d0fb47685" },
        {"dwy_EU", "021c7cf1f10c4dc39d13451123707ab780a741feedab6ac449766affe37515a29e" },
        {"phm87_SH", "021773a38db1bc3ede7f28142f901a161c7b7737875edbb40082a201c55dcf0add" },
        {"chainmakers_NA", "02285d813c30c0bf7eefdab1ff0a8ad08a07a0d26d8b95b3943ce814ac8e24d885" },
        {"indenodes_EU", "0221387ff95c44cb52b86552e3ec118a3c311ca65b75bf807c6c07eaeb1be8303c" },
        {"blackjok3r_SH", "021eac26dbad256cbb6f74d41b10763183ee07fb609dbd03480dd50634170547cc" },
        {"chainmakers_EU", "03fdf5a3fce8db7dee89724e706059c32e5aa3f233a6b6cc256fea337f05e3dbf7" },
        {"titomane_AR", "023e3aa9834c46971ff3e7cb86a200ec9c8074a9566a3ea85d400d5739662ee989" },
        {"fullmoon_SH", "023b7252968ea8a955cd63b9e57dee45a74f2d7ba23b4e0595572138ad1fb42d21" }, // 10
        {"indenodes_NA", "02698c6f1c9e43b66e82dbb163e8df0e5a2f62f3a7a882ca387d82f86e0b3fa988" },
        {"chmex_EU", "0281304ebbcc39e4f09fda85f4232dd8dacd668e20e5fc11fba6b985186c90086e" },
        {"metaphilibert_SH", "0284af1a5ef01503e6316a2ca4abf8423a794e9fc17ac6846f042b6f4adedc3309" },
        {"ca333_DEV", "02856843af2d9457b5b1c907068bef6077ea0904cc8bd4df1ced013f64bf267958" },
        {"cipi_NA", "02858904a2a1a0b44df4c937b65ee1f5b66186ab87a751858cf270dee1d5031f18" },
        {"pungocloud_SH", "024dfc76fa1f19b892be9d06e985d0c411e60dbbeb36bd100af9892a39555018f6" },
        {"voskcoin_EU", "034190b1c062a04124ad15b0fa56dfdf34aa06c164c7163b6aec0d654e5f118afb" },
        {"decker_DEV", "028eea44a09674dda00d88ffd199a09c9b75ba9782382cc8f1e97c0fd565fe5707" },
        {"cryptoeconomy_EU", "0290ab4937e85246e048552df3e9a66cba2c1602db76e03763e16c671e750145d1" },
        {"etszombi_EU", "0293ea48d8841af7a419a24d9da11c34b39127ef041f847651bae6ab14dcd1f6b4" },  // 20
        {"karasugoi_NA", "02a348b03b9c1a8eac1b56f85c402b041c9bce918833f2ea16d13452309052a982" },
        {"pirate_AR", "03e29c90354815a750db8ea9cb3c1b9550911bb205f83d0355a061ac47c4cf2fde" },
        {"metaphilibert_AR", "02adad675fae12b25fdd0f57250b0caf7f795c43f346153a31fe3e72e7db1d6ac6" },
        {"zatjum_SH", "02d6b0c89cacd58a0af038139a9a90c9e02cd1e33803a1f15fceabea1f7e9c263a" },
        {"madmax_AR", "03c5941fe49d673c094bc8e9bb1a95766b4670c88be76d576e915daf2c30a454d3" },
        {"lukechilds_NA", "03f1051e62c2d280212481c62fe52aab0a5b23c95de5b8e9ad5f80d8af4277a64b" },
        {"cipi_AR", "02c4f89a5b382750836cb787880d30e23502265054e1c327a5bfce67116d757ce8" },
        {"tonyl_AR", "02cc8bc862f2b65ad4f99d5f68d3011c138bf517acdc8d4261166b0be8f64189e1" },
        {"infotech_DEV", "0345ad4ab5254782479f6322c369cec77a7535d2f2162d103d666917d5e4f30c4c" },
        {"fullmoon_NA", "032c716701fe3a6a3f90a97b9d874a9d6eedb066419209eed7060b0cc6b710c60b" },  // 30
        {"etszombi_AR", "02e55e104aa94f70cde68165d7df3e162d4410c76afd4643b161dea044aa6d06ce" },
        {"node-9_EU", "0372e5b51e86e2392bb15039bac0c8f975b852b45028a5e43b324c294e9f12e411" },
        {"phba2061_EU", "03f6bd15dba7e986f0c976ea19d8a9093cb7c989d499f1708a0386c5c5659e6c4e" },
        {"indenodes_AR", "02ec0fa5a40f47fd4a38ea5c89e375ad0b6ddf4807c99733c9c3dc15fb978ee147" },
        {"and1-89_EU", "02736cbf8d7b50835afd50a319f162dd4beffe65f2b1dc6b90e64b32c8e7849ddd" },
        {"komodopioneers_SH", "032a238a5747777da7e819cfa3c859f3677a2daf14e4dce50916fc65d00ad9c52a" },
        {"komodopioneers_EU", "036d02425916444fff8cc7203fcbfc155c956dda5ceb647505836bef59885b6866" },
        {"d0ct0r_NA", "0303725d8525b6f969122faf04152653eb4bf34e10de92182263321769c334bf58" },
        {"kolo_DEV", "02849e12199dcc27ba09c3902686d2ad0adcbfcee9d67520e9abbdda045ba83227" },
        {"peer2cloud_AR", "02acc001fe1fe8fd68685ba26c0bc245924cb592e10cec71e9917df98b0e9d7c37" }, // 40
        {"webworker01_SH", "031e50ba6de3c16f99d414bb89866e578d963a54bde7916c810608966fb5700776" },
        {"webworker01_NA", "032735e9cad1bb00eaababfa6d27864fa4c1db0300c85e01e52176be2ca6a243ce" },
        {"pbca26_NA", "03a97606153d52338bcffd1bf19bb69ef8ce5a7cbdc2dbc3ff4f89d91ea6bbb4dc" },
        {"indenodes_SH", "0334e6e1ec8285c4b85bd6dae67e17d67d1f20e7328efad17ce6fd24ae97cdd65e" },
        {"pirate_NA", "0255e32d8a56671dee8aa7f717debb00efa7f0086ee802de0692f2d67ee3ee06ee" },
        {"lukechilds_AR", "025c6a73ff6d750b9ddf6755b390948cffdd00f344a639472d398dd5c6b4735d23" },
        {"dragonhound_NA", "0224a9d951d3a06d8e941cc7362b788bb1237bb0d56cc313e797eb027f37c2d375" },
        {"fullmoon_AR", "03da64dd7cd0db4c123c2f79d548a96095a5a103e5b9d956e9832865818ffa7872" },
        {"chainzilla_SH", "0360804b8817fd25ded6e9c0b50e3b0782ac666545b5416644198e18bc3903d9f9" },
        {"titomane_EU", "03772ac0aad6b0e9feec5e591bff5de6775d6132e888633e73d3ba896bdd8e0afb" }, // 50
        {"jeezy_EU", "037f182facbad35684a6e960699f5da4ba89e99f0d0d62a87e8400dd086c8e5dd7" },
        {"titomane_SH", "03850fdddf2413b51790daf51dd30823addb37313c8854b508ea6228205047ef9b" },
        {"alien_AR", "03911a60395801082194b6834244fa78a3c30ff3e888667498e157b4aa80b0a65f" },
        {"pirate_EU", "03fff24efd5648870a23badf46e26510e96d9e79ce281b27cfe963993039dd1351" },
        {"thegaltmines_NA", "02db1a16c7043f45d6033ccfbd0a51c2d789b32db428902f98b9e155cf0d7910ed" },
        {"computergenie_NA", "03a78ae070a5e9e935112cf7ea8293f18950f1011694ea0260799e8762c8a6f0a4" },
        {"nutellalicka_SH", "02f7d90d0510c598ce45915e6372a9cd0ba72664cb65ce231f25d526fc3c5479fc" },
        {"chainstrike_SH", "03b806be3bf7a1f2f6290ec5c1ea7d3ea57774dcfcf2129a82b2569e585100e1cb" },
        {"dwy_SH", "036536d2d52d85f630b68b050f29ea1d7f90f3b42c10f8c5cdf3dbe1359af80aff" },
        {"alien_EU", "03bb749e337b9074465fa28e757b5aa92cb1f0fea1a39589bca91a602834d443cd" }, // 60
        {"gt_AR", "0348430538a4944d3162bb4749d8c5ed51299c2434f3ee69c11a1f7815b3f46135" },
        {"patchkez_SH", "03f45e9beb5c4cd46525db8195eb05c1db84ae7ef3603566b3d775770eba3b96ee" },
        {"decker_AR", "03ffdf1a116300a78729608d9930742cd349f11a9d64fcc336b8f18592dd9c91bc" }, // 63
    },
    {
        // Season 3.5
        {"madmax_NA", "0237e0d3268cebfa235958808db1efc20cc43b31100813b1f3e15cc5aa647ad2c3" }, // 0
        {"alright_AR", "020566fe2fb3874258b2d3cf1809a5d650e0edc7ba746fa5eec72750c5188c9cc9" },
        {"strob_NA", "0206f7a2e972d9dfef1c424c731503a0a27de1ba7a15a91a362dc7ec0d0fb47685" },
        {"hunter_EU", "0378224b4e9d8a0083ce36f2963ec0a4e231ec06b0c780de108e37f41181a89f6a" }, // FIXME verify this, kolo. Change name if you want
        {"phm87_SH", "021773a38db1bc3ede7f28142f901a161c7b7737875edbb40082a201c55dcf0add" },
        {"chainmakers_NA", "02285d813c30c0bf7eefdab1ff0a8ad08a07a0d26d8b95b3943ce814ac8e24d885" },
        {"indenodes_EU", "0221387ff95c44cb52b86552e3ec118a3c311ca65b75bf807c6c07eaeb1be8303c" },
        {"blackjok3r_SH", "021eac26dbad256cbb6f74d41b10763183ee07fb609dbd03480dd50634170547cc" },
        {"chainmakers_EU", "03fdf5a3fce8db7dee89724e706059c32e5aa3f233a6b6cc256fea337f05e3dbf7" },
        {"titomane_AR", "023e3aa9834c46971ff3e7cb86a200ec9c8074a9566a3ea85d400d5739662ee989" },
        {"fullmoon_SH", "023b7252968ea8a955cd63b9e57dee45a74f2d7ba23b4e0595572138ad1fb42d21" }, // 10
        {"indenodes_NA", "02698c6f1c9e43b66e82dbb163e8df0e5a2f62f3a7a882ca387d82f86e0b3fa988" },
        {"chmex_EU", "0281304ebbcc39e4f09fda85f4232dd8dacd668e20e5fc11fba6b985186c90086e" },
        {"metaphilibert_SH", "0284af1a5ef01503e6316a2ca4abf8423a794e9fc17ac6846f042b6f4adedc3309" },
        {"ca333_DEV", "02856843af2d9457b5b1c907068bef6077ea0904cc8bd4df1ced013f64bf267958" },
        {"cipi_NA", "02858904a2a1a0b44df4c937b65ee1f5b66186ab87a751858cf270dee1d5031f18" },
        {"pungocloud_SH", "024dfc76fa1f19b892be9d06e985d0c411e60dbbeb36bd100af9892a39555018f6" },
        {"voskcoin_EU", "034190b1c062a04124ad15b0fa56dfdf34aa06c164c7163b6aec0d654e5f118afb" },
        {"decker_DEV", "028eea44a09674dda00d88ffd199a09c9b75ba9782382cc8f1e97c0fd565fe5707" },
        {"cryptoeconomy_EU", "0290ab4937e85246e048552df3e9a66cba2c1602db76e03763e16c671e750145d1" },
        {"etszombi_EU", "0293ea48d8841af7a419a24d9da11c34b39127ef041f847651bae6ab14dcd1f6b4" },  // 20
        {"karasugoi_NA", "02a348b03b9c1a8eac1b56f85c402b041c9bce918833f2ea16d13452309052a982" },
        {"pirate_AR", "03e29c90354815a750db8ea9cb3c1b9550911bb205f83d0355a061ac47c4cf2fde" },
        {"metaphilibert_AR", "02adad675fae12b25fdd0f57250b0caf7f795c43f346153a31fe3e72e7db1d6ac6" },
        {"zatjum_SH", "02d6b0c89cacd58a0af038139a9a90c9e02cd1e33803a1f15fceabea1f7e9c263a" },
        {"madmax_AR", "03c5941fe49d673c094bc8e9bb1a95766b4670c88be76d576e915daf2c30a454d3" },
        {"lukechilds_NA", "03f1051e62c2d280212481c62fe52aab0a5b23c95de5b8e9ad5f80d8af4277a64b" },
        {"cipi_AR", "02c4f89a5b382750836cb787880d30e23502265054e1c327a5bfce67116d757ce8" },
        {"tonyl_AR", "02cc8bc862f2b65ad4f99d5f68d3011c138bf517acdc8d4261166b0be8f64189e1" },
        {"infotech_DEV", "0345ad4ab5254782479f6322c369cec77a7535d2f2162d103d666917d5e4f30c4c" },
        {"fullmoon_NA", "032c716701fe3a6a3f90a97b9d874a9d6eedb066419209eed7060b0cc6b710c60b" },  // 30
        {"etszombi_AR", "02e55e104aa94f70cde68165d7df3e162d4410c76afd4643b161dea044aa6d06ce" },
        {"node-9_EU", "0372e5b51e86e2392bb15039bac0c8f975b852b45028a5e43b324c294e9f12e411" },
        {"phba2061_EU", "03f6bd15dba7e986f0c976ea19d8a9093cb7c989d499f1708a0386c5c5659e6c4e" },
        {"indenodes_AR", "02ec0fa5a40f47fd4a38ea5c89e375ad0b6ddf4807c99733c9c3dc15fb978ee147" },
        {"and1-89_EU", "02736cbf8d7b50835afd50a319f162dd4beffe65f2b1dc6b90e64b32c8e7849ddd" },
        {"komodopioneers_SH", "032a238a5747777da7e819cfa3c859f3677a2daf14e4dce50916fc65d00ad9c52a" },
        {"komodopioneers_EU", "036d02425916444fff8cc7203fcbfc155c956dda5ceb647505836bef59885b6866" },
        {"d0ct0r_NA", "0303725d8525b6f969122faf04152653eb4bf34e10de92182263321769c334bf58" },
        {"kolo_DEV", "02849e12199dcc27ba09c3902686d2ad0adcbfcee9d67520e9abbdda045ba83227" },
        {"peer2cloud_AR", "02acc001fe1fe8fd68685ba26c0bc245924cb592e10cec71e9917df98b0e9d7c37" }, // 40
        {"webworker01_SH", "031e50ba6de3c16f99d414bb89866e578d963a54bde7916c810608966fb5700776" },
        {"webworker01_NA", "032735e9cad1bb00eaababfa6d27864fa4c1db0300c85e01e52176be2ca6a243ce" },
        {"pbca26_NA", "03a97606153d52338bcffd1bf19bb69ef8ce5a7cbdc2dbc3ff4f89d91ea6bbb4dc" },
        {"indenodes_SH", "0334e6e1ec8285c4b85bd6dae67e17d67d1f20e7328efad17ce6fd24ae97cdd65e" },
        {"pirate_NA", "0255e32d8a56671dee8aa7f717debb00efa7f0086ee802de0692f2d67ee3ee06ee" },
        {"lukechilds_AR", "025c6a73ff6d750b9ddf6755b390948cffdd00f344a639472d398dd5c6b4735d23" },
        {"dragonhound_NA", "0224a9d951d3a06d8e941cc7362b788bb1237bb0d56cc313e797eb027f37c2d375" },
        {"fullmoon_AR", "03da64dd7cd0db4c123c2f79d548a96095a5a103e5b9d956e9832865818ffa7872" },
        {"chainzilla_SH", "0360804b8817fd25ded6e9c0b50e3b0782ac666545b5416644198e18bc3903d9f9" },
        {"titomane_EU", "03772ac0aad6b0e9feec5e591bff5de6775d6132e888633e73d3ba896bdd8e0afb" }, // 50
        {"jeezy_EU", "037f182facbad35684a6e960699f5da4ba89e99f0d0d62a87e8400dd086c8e5dd7" },
        {"titomane_SH", "03850fdddf2413b51790daf51dd30823addb37313c8854b508ea6228205047ef9b" },
        {"alien_AR", "03911a60395801082194b6834244fa78a3c30ff3e888667498e157b4aa80b0a65f" },
        {"pirate_EU", "03fff24efd5648870a23badf46e26510e96d9e79ce281b27cfe963993039dd1351" },
        {"thegaltmines_NA", "02db1a16c7043f45d6033ccfbd0a51c2d789b32db428902f98b9e155cf0d7910ed" },
        {"computergenie_NA", "03a78ae070a5e9e935112cf7ea8293f18950f1011694ea0260799e8762c8a6f0a4" },
        {"nutellalicka_SH", "02f7d90d0510c598ce45915e6372a9cd0ba72664cb65ce231f25d526fc3c5479fc" },
        {"chainstrike_SH", "03b806be3bf7a1f2f6290ec5c1ea7d3ea57774dcfcf2129a82b2569e585100e1cb" },
        {"hunter_SH", "02407db70ad30ce4dfaee8b4ae35fae88390cad2b0ba0373fdd6231967537ccfdf" }, // FIXME verify this, Decker. Change name if you want
        {"alien_EU", "03bb749e337b9074465fa28e757b5aa92cb1f0fea1a39589bca91a602834d443cd" }, // 60
        {"gt_AR", "0348430538a4944d3162bb4749d8c5ed51299c2434f3ee69c11a1f7815b3f46135" },
        {"patchkez_SH", "03f45e9beb5c4cd46525db8195eb05c1db84ae7ef3603566b3d775770eba3b96ee" },
        {"decker_AR", "03ffdf1a116300a78729608d9930742cd349f11a9d64fcc336b8f18592dd9c91bc" }, // 63
    },
    {
        // Season 4
        { "alien_AR", "03911a60395801082194b6834244fa78a3c30ff3e888667498e157b4aa80b0a65f" },
        { "alien_EU", "03bb749e337b9074465fa28e757b5aa92cb1f0fea1a39589bca91a602834d443cd" },
        { "strob_NA", "02a1c0bd40b294f06d3e44a52d1b2746c260c475c725e9351f1312e49e01c9a405" },
        { "titomane_SH", "020014ad4eedf6b1aeb0ad3b101a58d0a2fc570719e46530fd98d4e585f63eb4ae" },
        { "fullmoon_AR", "03b251095e747f759505ec745a4bbff9a768b8dce1f65137300b7c21efec01a07a" },
        { "phba2061_EU", "03a9492d2a1601d0d98cfe94d8adf9689d1bb0e600088127a4f6ca937761fb1c66" },
        { "fullmoon_NA", "03931c1d654a99658998ce0ddae108d825943a821d1cddd85e948ac1d483f68fb6" },
        { "fullmoon_SH", "03c2a1ed9ddb7bb8344328946017b9d8d1357b898957dd6aaa8c190ae26740b9ff" },
        { "madmax_AR", "022be5a2829fa0291f9a51ff7aeceef702eef581f2611887c195e29da49092e6de" },
        { "titomane_EU", "0285cf1fdba761daf6f1f611c32d319cd58214972ef822793008b69dde239443dd" },
        { "cipi_NA", "022c6825a24792cc3b010b1531521eba9b5e2662d640ed700fd96167df37e75239" },
        { "indenodes_SH", "0334e6e1ec8285c4b85bd6dae67e17d67d1f20e7328efad17ce6fd24ae97cdd65e" },
        { "decker_AR", "03ffdf1a116300a78729608d9930742cd349f11a9d64fcc336b8f18592dd9c91bc" },
        { "indenodes_EU", "0221387ff95c44cb52b86552e3ec118a3c311ca65b75bf807c6c07eaeb1be8303c" },
        { "madmax_NA", "02997b7ab21b86bbea558ae79acc35d62c9cedf441578f78112f986d72e8eece08" },
        { "chainzilla_SH", "02288ba6dc57936b59d60345e397d62f5d7e7d975f34ed5c2f2e23288325661563" },
        { "peer2cloud_AR", "0250e7e43a3535731b051d1bcc7dc88fbb5163c3fe41c5dee72bd973bcc4dca9f2" },
        { "pirate_EU", "0231c0f50a06655c3d2edf8d7e722d290195d49c78d50de7786b9d196e8820c848" },
        { "webworker01_NA", "02dfd5f3cef1142879a7250752feb91ddd722c497fb98c7377c0fcc5ccc201bd55" },
        { "zatjum_SH", "036066fd638b10e555597623e97e032b28b4d1fa5a13c2b0c80c420dbddad236c2" },
        { "titomane_AR", "0268203a4c80047edcd66385c22e764ea5fb8bc42edae389a438156e7dca9a8251" },
        { "chmex_EU", "025b7209ba37df8d9695a23ea706ea2594863ab09055ca6bf485855937f3321d1d" },
        { "indenodes_NA", "02698c6f1c9e43b66e82dbb163e8df0e5a2f62f3a7a882ca387d82f86e0b3fa988" },
        { "patchkez_SH", "02cabd6c5fc0b5476c7a01e9d7b907e9f0a051d7f4f731959955d3f6b18ee9a242" },
        { "metaphilibert_AR", "02adad675fae12b25fdd0f57250b0caf7f795c43f346153a31fe3e72e7db1d6ac6" },
        { "etszombi_EU", "0341adbf238f33a33cc895633db996c3ad01275313ac6641e046a3db0b27f1c880" },
        { "pirate_NA", "02207f27a13625a0b8caef6a7bb9de613ff16e4a5f232da8d7c235c7c5bad72ffe" },
        { "metaphilibert_SH", "0284af1a5ef01503e6316a2ca4abf8423a794e9fc17ac6846f042b6f4adedc3309" },
        { "indenodes_AR", "02ec0fa5a40f47fd4a38ea5c89e375ad0b6ddf4807c99733c9c3dc15fb978ee147" },
        { "chainmakers_NA", "029415a1609c33dfe4a1016877ba35f9265d25d737649f307048efe96e76512877" },
        { "mihailo_EU", "037f9563f30c609b19fd435a19b8bde7d6db703012ba1aba72e9f42a87366d1941" },
        { "tonyl_AR", "0299684d7291abf90975fa493bf53212cf1456c374aa36f83cc94daece89350ae9" },
        { "alien_NA", "03bea1ac333b95c8669ec091907ea8713cae26f74b9e886e13593400e21c4d30a8" },
        { "pungocloud_SH", "025b97d8c23effaca6fa7efacce20bf54df73081b63004a0fe22f3f98fece5669f" },
        { "node9_EU", "029ffa793b5c3248f8ea3da47fa3cf1810dada5af032ecd0e37bab5b92dd63b34e" },
        { "smdmitry_AR", "022a2a45979a6631a25e4c96469423de720a2f4c849548957c35a35c91041ee7ac" },
        { "nodeone_NA", "03f9dd0484e81174fd50775cb9099691c7d140ff00c0f088847e38dc87da67eb9b" },
        { "gcharang_SH", "02ec4172eab854a0d8cd32bc691c83e93975a3df5a4a453a866736c56e025dc359" },
        { "cipi_EU", "02f2b6defff1c544202f66e47cfd6909c54d67c7c39b9c2a99f137dbaf6d0bd8fa" },
        { "etszombi_AR", "0329944b0ac65b6760787ede042a2fde0be9fca1d80dd756bc0ee0b98d389b7682" },
        { "pbca26_NA", "0387e0fb6f2ca951154c87e16c6cbf93a69862bb165c1a96bcd8722b3af24fe533" },
        { "mylo_SH", "03b58f57822e90fe105e6efb63fd8666033ea503d6cc165b1e479bbd8c2ba033e8" },
        { "swisscertifiers_EU", "03ebcc71b42d88994b8b2134bcde6cb269bd7e71a9dd7616371d9294ec1c1902c5" },
        { "marmarachain_AR", "035bbd81a098172592fe97f50a0ce13cbbf80e55cc7862eccdbd7310fab8a90c4c" },
        { "karasugoi_NA", "0262cf2559703464151153c12e00c4b67a969e39b330301fdcaa6667d7eb02c57d" },
        { "phm87_SH", "021773a38db1bc3ede7f28142f901a161c7b7737875edbb40082a201c55dcf0add" },
        { "oszy_EU", "03d1ffd680491b98a3ec5541715681d1a45293c8efb1722c32392a1d792622596a" },
        { "chmex_AR", "036c856ea778ea105b93c0be187004d4e51161eda32888aa307b8f72d490884005" },
        { "dragonhound_NA", "0227e5cad3731e381df157de189527aac8eb50d82a13ce2bd81153984ebc749515" },
        { "strob_SH", "025ceac4256cef83ca4b110f837a71d70a5a977ecfdf807335e00bc78b560d451a" },
        { "madmax_EU", "02ea0cf4d6d151d0528b07efa79cc7403d77cb9195e2e6c8374f5074b9a787e287" },
        { "dudezmobi_AR", "027ecd974ff2a27a37ee69956cd2e6bb31a608116206f3e31ef186823420182450" },
        { "daemonfox_NA", "022d6f4885f53cbd668ad7d03d4f8e830c233f74e3a918da1ed247edfc71820b3d" },
        { "nutellalicka_SH", "02f4b1e71bc865a79c05fe333952b97cb040d8925d13e83925e170188b3011269b" },
        { "starfleet_EU", "025c7275bd750936862b47793f1f0bb3cbed60fb75a48e7da016e557925fe375eb" },
        { "mrlynch_AR", "031987dc82b087cd53e23df5480e265a5928e9243e0e11849fa12359739d8b18a4" },
        { "greer_NA", "03e0995615d7d3cf1107effa6bdb1133e0876cf1768e923aa533a4e2ee675ec383" },
        { "mcrypt_SH", "025faab3cc2e83bf7dad6a9463cbff86c08800e937942126f258cf219bc2320043" },
        { "decker_EU", "03777777caebce56e17ca3aae4e16374335b156f1dd62ee3c7f8799c6b885f5560" },
        { "dappvader_SH", "02962e2e5af746632016bc7b24d444f7c90141a5f42ce54e361b302cf455d90e6a" },
        { "alright_DEV", "02b73a589d61691efa2ada15c006d27bc18493fea867ce6c14db3d3d28751f8ce3" },
        { "artemii235_DEV", "03bb616b12430bdd0483653de18733597a4fd416623c7065c0e21fe9d96460add1" },
        { "tonyl_DEV", "02d5f7fd6e25d34ab2f3318d60cdb89ff3a812ec5d0212c4c113bb12d12616cfdc" },
        { "decker_DEV", "028eea44a09674dda00d88ffd199a09c9b75ba9782382cc8f1e97c0fd565fe5707" }
   }
};

#define SETBIT(bits,bitoffset) (((uint8_t *)bits)[(bitoffset) >> 3] |= (1 << ((bitoffset) & 7)))
#define GETBIT(bits,bitoffset) (((uint8_t *)bits)[(bitoffset) >> 3] & (1 << ((bitoffset) & 7)))
#define CLEARBIT(bits,bitoffset) (((uint8_t *)bits)[(bitoffset) >> 3] &= ~(1 << ((bitoffset) & 7)))

#define KOMODO_MAXNVALUE (((uint64_t)1 << 63) - 1)
#define KOMODO_BIT63SET(x) ((x) & ((uint64_t)1 << 63))
#define KOMODO_VALUETOOBIG(x) ((x) > (uint64_t)10000000001*COIN)

//#ifndef TESTMODE
#define PRICES_DAYWINDOW ((3600*24/ASSETCHAINS_BLOCKTIME) + 1)
//#else
//#define PRICES_DAYWINDOW (7)
//#endif

extern uint8_t ASSETCHAINS_TXPOW,ASSETCHAINS_PUBLIC;
extern int8_t ASSETCHAINS_ADAPTIVEPOW;
int32_t MAX_BLOCK_SIZE(int32_t height);
extern char ASSETCHAINS_SYMBOL[KOMODO_ASSETCHAIN_MAXLEN];
extern uint16_t ASSETCHAINS_P2PPORT,ASSETCHAINS_RPCPORT;
extern uint32_t ASSETCHAIN_INIT, ASSETCHAINS_MAGIC;
extern int32_t VERUS_BLOCK_POSUNITS, ASSETCHAINS_LWMAPOS, ASSETCHAINS_SAPLING, ASSETCHAINS_OVERWINTER,ASSETCHAINS_BLOCKTIME;
extern uint64_t ASSETCHAINS_SUPPLY, ASSETCHAINS_FOUNDERS_REWARD;

extern uint64_t ASSETCHAINS_TIMELOCKGTE;
extern uint32_t ASSETCHAINS_ALGO, ASSETCHAINS_VERUSHASH,ASSETCHAINS_EQUIHASH,KOMODO_INITDONE;

extern int32_t KOMODO_MININGTHREADS,KOMODO_LONGESTCHAIN,ASSETCHAINS_SEED,IS_KOMODO_NOTARY,USE_EXTERNAL_PUBKEY,KOMODO_CHOSEN_ONE,KOMODO_ON_DEMAND,KOMODO_PASSPORT_INITDONE,ASSETCHAINS_STAKED,KOMODO_NSPV;
extern uint64_t ASSETCHAINS_COMMISSION, ASSETCHAINS_LASTERA,ASSETCHAINS_CBOPRET;
extern bool VERUS_MINTBLOCKS;
extern uint64_t ASSETCHAINS_REWARD[ASSETCHAINS_MAX_ERAS+1], ASSETCHAINS_NOTARY_PAY[ASSETCHAINS_MAX_ERAS+1], ASSETCHAINS_TIMELOCKGTE, ASSETCHAINS_NONCEMASK[],ASSETCHAINS_NK[2];
extern const char *ASSETCHAINS_ALGORITHMS[];
extern int32_t VERUS_MIN_STAKEAGE;
extern uint32_t ASSETCHAINS_VERUSHASH, ASSETCHAINS_VERUSHASHV1_1, ASSETCHAINS_NONCESHIFT[], ASSETCHAINS_HASHESPERROUND[];
extern std::string NOTARY_PUBKEY,ASSETCHAINS_OVERRIDE_PUBKEY,ASSETCHAINS_SCRIPTPUB;
extern uint8_t NOTARY_PUBKEY33[33],ASSETCHAINS_OVERRIDE_PUBKEY33[33],ASSETCHAINS_MARMARA;
extern std::vector<std::string> ASSETCHAINS_PRICES,ASSETCHAINS_STOCKS;

extern int32_t VERUS_BLOCK_POSUNITS, VERUS_CONSECUTIVE_POS_THRESHOLD, VERUS_NOPOS_THRESHHOLD;
extern uint256 KOMODO_EARLYTXID;

extern int32_t KOMODO_CONNECTING,KOMODO_CCACTIVATE,KOMODO_DEALERNODE;
extern uint32_t ASSETCHAINS_CC;
extern std::string CCerror,ASSETCHAINS_CCLIB;
extern uint8_t ASSETCHAINS_CCDISABLES[256];

extern int32_t USE_EXTERNAL_PUBKEY;
extern std::string NOTARY_PUBKEY,NOTARY_ADDRESS;
extern int32_t KOMODO_EXCHANGEWALLET;
extern int32_t VERUS_MIN_STAKEAGE;
extern std::string DONATION_PUBKEY;
extern uint8_t ASSETCHAINS_PRIVATE;
extern int32_t USE_EXTERNAL_PUBKEY;
extern char NOTARYADDRS[64][64];
extern char NOTARY_ADDRESSES[NUM_KMD_SEASONS][64][64];
extern int32_t KOMODO_TESTNODE, KOMODO_SNAPSHOT_INTERVAL,IS_STAKED_NOTARY,STAKED_ERA;
extern int32_t ASSETCHAINS_EARLYTXIDCONTRACT;
extern int32_t ASSETCHAINS_STAKED_SPLIT_PERCENTAGE;
int tx_height( const uint256 &hash );
extern std::vector<std::string> vWhiteListAddress;
extern std::map <std::int8_t, int32_t> mapHeightEvalActivate;
void komodo_netevent(std::vector<uint8_t> payload);
int32_t getacseason(uint32_t timestamp);
int32_t getkmdseason(int32_t height);

#define IGUANA_MAXSCRIPTSIZE 10001
#define KOMODO_KVDURATION 1440
#define KOMODO_KVBINARY 2
#define PRICES_SMOOTHWIDTH 1
#define PRICES_MAXDATAPOINTS 8
uint64_t komodo_paxprice(uint64_t *seedp,int32_t height,char *base,char *rel,uint64_t basevolume);
int32_t komodo_paxprices(int32_t *heights,uint64_t *prices,int32_t max,char *base,char *rel);
int32_t komodo_notaries(uint8_t pubkeys[64][33],int32_t height,uint32_t timestamp);
char *bitcoin_address(char *coinaddr,uint8_t addrtype,uint8_t *pubkey_or_rmd160,int32_t len);
int32_t komodo_minerids(uint8_t *minerids,int32_t height,int32_t width);
int32_t komodo_kvsearch(uint256 *refpubkeyp,int32_t current_height,uint32_t *flagsp,int32_t *heightp,uint8_t value[IGUANA_MAXSCRIPTSIZE],uint8_t *key,int32_t keylen);

uint32_t komodo_blocktime(uint256 hash);
int32_t komodo_longestchain();
int32_t komodo_dpowconfs(int32_t height,int32_t numconfs);
int8_t komodo_segid(int32_t nocache,int32_t height);
int32_t komodo_heightpricebits(uint64_t *seedp,uint32_t *heightbits,int32_t nHeight);
char *komodo_pricename(char *name,int32_t ind);
int32_t komodo_priceind(const char *symbol);
int32_t komodo_pricesinit();
int64_t komodo_priceave(int64_t *tmpbuf,int64_t *correlated,int32_t cskip);
int64_t komodo_pricecorrelated(uint64_t seed,int32_t ind,uint32_t *rawprices,int32_t rawskip,uint32_t *nonzprices,int32_t smoothwidth);
int32_t komodo_nextheight();
uint32_t komodo_heightstamp(int32_t height);
int64_t komodo_pricemult(int32_t ind);
int32_t komodo_priceget(int64_t *buf64,int32_t ind,int32_t height,int32_t numblocks);
uint64_t komodo_accrued_interest(int32_t *txheightp,uint32_t *locktimep,uint256 hash,int32_t n,int32_t checkheight,uint64_t checkvalue,int32_t tipheight);
int32_t komodo_currentheight();
int32_t komodo_notarized_bracket(struct notarized_checkpoint *nps[2],int32_t height);
arith_uint256 komodo_adaptivepow_target(int32_t height,arith_uint256 bnTarget,uint32_t nTime);
bool komodo_hardfork_active(uint32_t time);
int32_t komodo_newStakerActive(int32_t height, uint32_t timestamp);

uint256 Parseuint256(const char *hexstr);
void komodo_sendmessage(int32_t minpeers, int32_t maxpeers, const char *message, std::vector<uint8_t> payload);
CBlockIndex *komodo_getblockindex(uint256 hash);
int32_t komodo_nextheight();
CBlockIndex *komodo_blockindex(uint256 hash);
CBlockIndex *komodo_chainactive(int32_t height);
int32_t komodo_blockheight(uint256 hash);
bool komodo_txnotarizedconfirmed(uint256 txid);
int32_t komodo_blockload(CBlock& block, CBlockIndex *pindex);
uint32_t komodo_chainactive_timestamp();
uint32_t GetLatestTimestamp(int32_t height);

#ifndef KOMODO_NSPV_FULLNODE
#define KOMODO_NSPV_FULLNODE (KOMODO_NSPV <= 0)
#endif // !KOMODO_NSPV_FULLNODE
#ifndef KOMODO_NSPV_SUPERLITE
#define KOMODO_NSPV_SUPERLITE (KOMODO_NSPV > 0)
#endif // !KOMODO_NSPV_SUPERLITE

#endif
