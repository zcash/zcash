/******************************************************************************
 * Copyright © 2014-2016 The SuperNET Developers.                             *
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

#ifndef H_KOMODO_H
#define H_KOMODO_H

#include <stdint.h>
#include <stdio.h>

#define CRYPTO777_PUBSECPSTR "020e46e79a2a8d12b9b5d12c7a91adb4e454edfae43c0a0cb805427d2ac7613fd9"

const char *Notaries[64][2] =
{
    { "jl777_testA", "03b7621b44118017a16043f19b30cc8a4cfe068ac4e42417bae16ba460c80f3828" },
    { "jl777_testB", "02ebfc784a4ba768aad88d44d1045d240d47b26e248cafaf1c5169a42d7a61d344" },
    { "pondsea_SH", "02209073bc0943451498de57f802650311b1f12aa6deffcd893da198a544c04f36" },
    { "crackers_EU", "0340c66cf2c41c41efb420af57867baa765e8468c12aa996bfd816e1e07e410728" },
    { "pondsea_EU", "0225aa6f6f19e543180b31153d9e6d55d41bc7ec2ba191fd29f19a2f973544e29d" },
    { "locomb_EU", "025c6d26649b9d397e63323d96db42a9d3caad82e1d6076970efe5056c00c0779b" },
    { "fullmoon_AE", "0204a908350b8142698fdb6fabefc97fe0e04f537adc7522ba7a1e8f3bec003d4a" },
    { "movecrypto_EU", "021ab53bc6cf2c46b8a5456759f9d608966eff87384c2b52c0ac4cc8dd51e9cc42" },
    { "badass_EU", "0209d48554768dd8dada988b98aca23405057ac4b5b46838a9378b95c3e79b9b9e" },
    { "crackers_NA", "029e1c01131974f4cd3f564cc0c00eb87a0f9721043fbc1ca60f9bd0a1f73f64a1" },
    { "proto_EU", "03681ffdf17c8f4f0008cefb7fa0779c5e888339cdf932f0974483787a4d6747c1" },
    { "jeezy_EU", "023cb3e593fb85c5659688528e9a4f1c4c7f19206edc7e517d20f794ba686fd6d6" },
    { "farl4web_EU", "035caa40684ace968677dca3f09098aa02b70e533da32390a7654c626e0cf908e1" },
    { "nxtswe_EU", "032fb104e5eaa704a38a52c126af8f67e870d70f82977e5b2f093d5c1c21ae5899" },
    { "traderbill_EU", "03196e8de3e2e5d872f31d79d6a859c8704a2198baf0af9c7b21e29656a7eb455f" },
    { "vanbreuk_EU", "024f3cad7601d2399c131fd070e797d9cd8533868685ddbe515daa53c2e26004c3" },
    { "titomane_EU", "03517fcac101fed480ae4f2caf775560065957930d8c1facc83e30077e45bdd199" },
    { "supernet_AE", "029d93ef78197dc93892d2a30e5a54865f41e0ca3ab7eb8e3dcbc59c8756b6e355" },
    { "supernet_EU", "02061c6278b91fd4ac5cab4401100ffa3b2d5a277e8f71db23401cc071b3665546" },
    { "yassin_EU", "033fb7231bb66484081952890d9a03f91164fb27d392d9152ec41336b71b15fbd0" },
    { "durerus_EU", "02bcbd287670bdca2c31e5d50130adb5dea1b53198f18abeec7211825f47485d57" },
    { "badass_SH", "026b49dd3923b78a592c1b475f208e23698d3f085c4c3b4906a59faf659fd9530b" },
    { "baddass_NA" "02afa1a9f948e1634a29dc718d218e9d150c531cfa852843a1643a02184a63c1a7" },
    { "pondsea_NA", "031bcfdbb62268e2ff8dfffeb9ddff7fe95fca46778c77eebff9c3829dfa1bb411" },
    { "rnr_EU", "0287aa4b73988ba26cf6565d815786caf0d2c4af704d7883d163ee89cd9977edec" },
    { "crackers_SH", "02313d72f9a16055737e14cfc528dcd5d0ef094cfce23d0348fe974b6b1a32e5f0" },
    { "grewal_SH", "03212a73f5d38a675ee3cdc6e82542a96c38c3d1c79d25a1ed2e42fcf6a8be4e68" },
    { "polycryptoblock_NA", "02708dcda7c45fb54b78469673c2587bfdd126e381654819c4c23df0e00b679622" },
    { "titomane_NA", "0387046d9745414fb58a0fa3599078af5073e10347e4657ef7259a99cb4f10ad47" },
    { "titomane_AE", "03cda6ca5c2d02db201488a54a548dbfc10533bdc275d5ea11928e8d6ab33c2185" },
};

int32_t IS_KOMODO_NOTARY,USE_EXTERNAL_PUBKEY,NOTARIZED_HEIGHT;
std::string NOTARY_PUBKEY;
uint256 NOTARIZED_HASH;

int32_t komodo_blockindexcheck(CBlockIndex *pindex,uint32_t *nBitsp)
{
    // 1 -> valid notary block, change nBits to KOMODO_MINDIFF_NBITS
    // -1 -> invalid, ie, prior to notarized block
    CBlock block; int32_t i,height; char *coinbasestr;
    if ( pindex == 0 )
        return(0);
    if ( ReadBlockFromDisk(block,pindex,1) == 0 )
        return(0);
    if ( block.vtx.size() > 0 )
    {
        height = pindex->nHeight;
        coinbasestr = (char *)block.vtx[0].vout[0].scriptPubKey.ToString().c_str();
        for (i=0; i<64; i++)
        {
            if ( Notaries[i][0] == 0 || Notaries[i][1] == 0 || Notaries[i][0][0] == 0 || Notaries[i][1][0] == 0 )
                break;
            if ( strncmp(Notaries[i][1],coinbasestr,66) == 0 )
            {
                //printf("Notary.[%d] %s ht.%d (%s)\n",i,Notaries[i][0],height,coinbasestr);
                //*nBitsp = KOMODO_MINDIFF_NBITS;
                return(1);
            }
        }
    }
    // compare against elected notary pubkeys as of height
    return(0);
}

void komodo_connectblock(CBlockIndex *pindex,CBlock& block)
{
    char *scriptstr; int32_t i,numvins,numvouts,height,txn_count;
    // update voting results and official (height, notaries[])
    if ( pindex != 0 )
    {
        height = pindex->nHeight;
        txn_count = block.vtx.size();
        numvouts = block.vtx.vout.size();
        numvins = block.vtx.vin.size();
        for (i=0; i<txn_count; i++)
        {
            for (j=0; j<numvouts; j++)
            {
                scriptstr = (char *)block.vtx[i].vout[j].scriptPubKey.ToString().c_str();
                if ( strncmp(scriptstr,CRYPTO777_PUBSECPSTR,66) == 0 )
                    printf(">>>>>>>> ");
                else if ( j == 0 )
                {
                    
                }
                printf("ht.%d txi.%d vout.%d (%s)\n",height,i,j,scriptstr);
            }
        }
    } else printf("komodo_connectblock: unexpected null pindex\n");
}

int32_t komodo_is_notaryblock(CBlockHeader& blockhdr)
{
    //uint32_t nBits = 0;
    //return(komodo_blockindexcheck(mapBlockIndex[blockhdr.GetHash()],&nBits));
    return(0);
}

int32_t komodo_blockhdrcheck(CBlockHeader& blockhdr,uint32_t *nBitsp)
{
    int32_t retval;
    if ( (retval= komodo_is_notaryblock(blockhdr)) > 0 )
        *nBitsp = KOMODO_MINDIFF_NBITS;
    return(retval);
}

int32_t komodo_blockcheck(CBlock& block,uint32_t *nBitsp)
{
    return(komodo_blockhdrcheck(block,nBitsp));
}

#endif
