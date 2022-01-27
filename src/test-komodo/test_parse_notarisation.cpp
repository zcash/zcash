#include <gtest/gtest.h>

#include "cc/eval.h"
#include "core_io.h"
#include "key.h"

#include "testutils.h"

#include "komodo_structs.h"
#include "test_parse_notarisation.h"

komodo_state *komodo_stateptr(char *symbol,char *dest);
void komodo_notarized_update(struct komodo_state *sp,int32_t nHeight,int32_t notarized_height,
        uint256 notarized_hash,uint256 notarized_desttxid,uint256 MoM,int32_t MoMdepth);
const notarized_checkpoint *komodo_npptr(int32_t height);
int32_t komodo_prevMoMheight();
int32_t komodo_notarizeddata(int32_t nHeight,uint256 *notarized_hashp,uint256 *notarized_desttxidp);

class komodo_state_accessor : public komodo_state
{
public:
    void clear_npoints()
    {
        NPOINTS.clear();
    }
    const notarized_checkpoint *last_checkpoint()
    {
        auto &idx = NPOINTS.get<0>();
        const auto &cp = idx.back();
        return &cp;
    }
};

#define portable_mutex_lock pthread_mutex_lock
#define portable_mutex_unlock pthread_mutex_unlock
/***
 * Test the old way (KomodoPlatform/komodo tag 0.7.0) of doing things, to assure backwards compatibility
 */
namespace old_space {

    struct komodo_state
    {
        uint256 NOTARIZED_HASH,NOTARIZED_DESTTXID,MoM;
        int32_t SAVEDHEIGHT,CURRENT_HEIGHT,NOTARIZED_HEIGHT,MoMdepth;
        uint32_t SAVEDTIMESTAMP;
        uint64_t deposited,issued,withdrawn,approved,redeemed,shorted;
        struct notarized_checkpoint *NPOINTS; int32_t NUM_NPOINTS,last_NPOINTSi;
        //std::list<std::shared_ptr<komodo::event>> events;
        uint32_t RTbufs[64][3]; uint64_t RTmask;
        //bool add_event(const std::string& symbol, const uint32_t height, std::shared_ptr<komodo::event> in);
    };

    komodo_state ks_old;
    komodo_state *sp = &ks_old;
    pthread_mutex_t komodo_mutex,staked_mutex;

    /* komodo_notary.h */
    void komodo_notarized_update(struct komodo_state *sp,int32_t nHeight,int32_t notarized_height,uint256 notarized_hash,uint256 notarized_desttxid,uint256 MoM,int32_t MoMdepth)
    {
        struct notarized_checkpoint *np;
        if ( notarized_height >= nHeight )
        {
            fprintf(stderr,"komodo_notarized_update REJECT notarized_height %d > %d nHeight\n",notarized_height,nHeight);
            return;
        }
        if ( 0 && ASSETCHAINS_SYMBOL[0] != 0 )
            fprintf(stderr,"[%s] komodo_notarized_update nHeight.%d notarized_height.%d\n",ASSETCHAINS_SYMBOL,nHeight,notarized_height);
        portable_mutex_lock(&komodo_mutex);
        sp->NPOINTS = (struct notarized_checkpoint *)realloc(sp->NPOINTS,(sp->NUM_NPOINTS+1) * sizeof(*sp->NPOINTS));
        np = &sp->NPOINTS[sp->NUM_NPOINTS++];
        memset(np,0,sizeof(*np));
        np->nHeight = nHeight;
        sp->NOTARIZED_HEIGHT = np->notarized_height = notarized_height;
        sp->NOTARIZED_HASH = np->notarized_hash = notarized_hash;
        sp->NOTARIZED_DESTTXID = np->notarized_desttxid = notarized_desttxid;
        sp->MoM = np->MoM = MoM;
        sp->MoMdepth = np->MoMdepth = MoMdepth;
        portable_mutex_unlock(&komodo_mutex);
    }
    struct notarized_checkpoint *komodo_npptr_for_height(int32_t height, int *idx)
    {
        char symbol[KOMODO_ASSETCHAIN_MAXLEN],dest[KOMODO_ASSETCHAIN_MAXLEN]; int32_t i; struct komodo_state *sp; struct notarized_checkpoint *np = 0;
        //if ( (sp= komodo_stateptr(symbol,dest)) != 0 )
        sp = &old_space::ks_old;

        {
            for (i=sp->NUM_NPOINTS-1; i>=0; i--)
            {
                *idx = i;
                np = &sp->NPOINTS[i];
                if ( np->MoMdepth != 0 && height > np->notarized_height-(np->MoMdepth&0xffff) && height <= np->notarized_height )
                    return(np);
            }
        }
        *idx = -1;
        return(0);
    }

    struct notarized_checkpoint *komodo_npptr(int32_t height)
    {
        int idx;
        return komodo_npptr_for_height(height, &idx);
    }

    struct notarized_checkpoint *komodo_npptr_at(int idx)
    {
        char symbol[KOMODO_ASSETCHAIN_MAXLEN],dest[KOMODO_ASSETCHAIN_MAXLEN]; struct komodo_state *sp;
        //if ( (sp= komodo_stateptr(symbol,dest)) != 0 )
        sp = &old_space::ks_old;

            if (idx < sp->NUM_NPOINTS)
                return &sp->NPOINTS[idx];
        return(0);
    }

    int32_t komodo_prevMoMheight()
    {
        static uint256 zero;
        zero.SetNull();

        char symbol[KOMODO_ASSETCHAIN_MAXLEN],dest[KOMODO_ASSETCHAIN_MAXLEN]; int32_t i; struct komodo_state *sp; struct notarized_checkpoint *np = 0;
        //if ( (sp= komodo_stateptr(symbol,dest)) != 0 )
        sp = &old_space::ks_old;
        {
            for (i=sp->NUM_NPOINTS-1; i>=0; i--)
            {
                np = &sp->NPOINTS[i];
                if ( np->MoM != zero )
                    return(np->notarized_height);
            }
        }
        return(0);
    }

    int32_t komodo_notarizeddata(int32_t nHeight,uint256 *notarized_hashp,uint256 *notarized_desttxidp)
    {
        struct notarized_checkpoint *np = 0;
        int32_t i=0,flag = 0;
        char symbol[KOMODO_ASSETCHAIN_MAXLEN],dest[KOMODO_ASSETCHAIN_MAXLEN];
        struct komodo_state *sp;

        //if ( (sp= komodo_stateptr(symbol,dest)) != 0 )
        sp = &old_space::ks_old;
        {
            if ( sp->NUM_NPOINTS > 0 )
            {
                flag = 0;
                if ( sp->last_NPOINTSi < sp->NUM_NPOINTS && sp->last_NPOINTSi > 0 )
                {
                    np = &sp->NPOINTS[sp->last_NPOINTSi-1];
                    if ( np->nHeight < nHeight )
                    {
                        for (i=sp->last_NPOINTSi; i<sp->NUM_NPOINTS; i++)
                        {
                            if ( sp->NPOINTS[i].nHeight >= nHeight )
                            {
                                //printf("flag.1 i.%d np->ht %d [%d].ht %d >= nHeight.%d, last.%d num.%d\n",i,np->nHeight,i,sp->NPOINTS[i].nHeight,nHeight,sp->last_NPOINTSi,sp->NUM_NPOINTS);
                                flag = 1;
                                break;
                            }
                            np = &sp->NPOINTS[i];
                            sp->last_NPOINTSi = i;
                        }
                    }
                }
                if ( flag == 0 )
                {
                    np = 0;
                    for (i=0; i<sp->NUM_NPOINTS; i++)
                    {
                        if ( sp->NPOINTS[i].nHeight >= nHeight )
                        {
                            //printf("i.%d np->ht %d [%d].ht %d >= nHeight.%d\n",i,np->nHeight,i,sp->NPOINTS[i].nHeight,nHeight);
                            break;
                        }
                        np = &sp->NPOINTS[i];
                        sp->last_NPOINTSi = i;
                    }
                }
            }
            if ( np != 0 )
            {
                //char str[65],str2[65]; printf("[%s] notarized_ht.%d\n",ASSETCHAINS_SYMBOL,np->notarized_height);
                if ( np->nHeight >= nHeight || (i < sp->NUM_NPOINTS && np[1].nHeight < nHeight) )
                    printf("warning: flag.%d i.%d np->ht %d [1].ht %d >= nHeight.%d\n",flag,i,np->nHeight,np[1].nHeight,nHeight);
                *notarized_hashp = np->notarized_hash;
                *notarized_desttxidp = np->notarized_desttxid;
                return(np->notarized_height);
            }
        }
        memset(notarized_hashp,0,sizeof(*notarized_hashp));
        memset(notarized_desttxidp,0,sizeof(*notarized_desttxidp));
        return(0);
    }
}

namespace TestParseNotarisation {

class TestParseNotarisation : public ::testing::Test, public Eval {};


TEST(TestParseNotarisation, test_ee2fa)
{
    // ee2fa47820a31a979f9f21cb3fedbc484bf9a8957cb6c9acd0af28ced29bdfe1
    std::vector<uint8_t> opret = ParseHex("c349ff90f3bce62c1b7b49d1da0423b1a3d9b733130cce825b95b9e047c729066e020d00743a06fdb95ad5775d032b30bbb3680dac2091a0f800cf54c79fd3461ce9b31d4b4d4400");
    NotarisationData nd;
    ASSERT_TRUE(E_UNMARSHAL(opret, ss >> nd));
}

TEST(TestParseNotarisation, test__)
{
    // 576e910a1f704207bcbcf724124ff9adc5237f45cb6919589cd0aa152caec424
    std::vector<uint8_t> opret = ParseHex("b3ed7fbbfbc027caeeeec81e65489ec5d9cd47cda675a5cbb75b4a845e67cf0ef6330300b5a6bd8385feb833f3be961c9d8a46fcecd36dcdfa42ad81a20a892433722f0b4b4d44004125a06024eae24c11f36ea110acd707b041d5355b6e1b42de5e2614357999c6aa02000d26ad0300000000404b4c000000000005130300500d000061f22ba7d19fe29ac3baebd839af8b7127d1f90755534400");
    NotarisationData nd;
    // We can't parse this one
    ASSERT_FALSE(E_UNMARSHAL(opret, ss >> nd));
}

TEST(TestParseNotarisation, test__a)
{
    // be55101e6c5a93fb3611a44bd66217ad8714d204275ea4e691cfff9d65dff85c TXSCL
    std::vector<uint8_t> opret = ParseHex("fb9ea2818eec8b07f8811bab49d64379db074db478997f8114666f239bd79803cc460000d0fac4e715b7e2b917a5d79f85ece0c423d27bd3648fd39ac1dc7db8e1bd4b16545853434c00a69eab9f23d7fb63c4624973e7a9079d6ada2f327040936356d7af5e849f6d670a0003001caf7b7b9e1c9bc59d0c7a619c9683ab1dd0794b6f3ea184a19f8fda031150e700000000");
    NotarisationData nd(1);
    bool res = E_UNMARSHAL(opret, ss >> nd);
    ASSERT_TRUE(res);
}

TEST(TestParseNotarisation, test__b)
{
    // 03085dafed656aaebfda25bf43ffe9d1fb72565bb1fc8b2a12a631659f28f877 TXSCL
    std::vector<uint8_t> opret = ParseHex("48c71a10aa060eab1a43f52acefac3b81fb2a2ce310186b06141884c0501d403c246000052e6d49afd82d9ab3d97c996dd9b6a78a554ffa1625e8dadf0494bd1f8442e3e545853434c007cc5c07e3b67520fd14e23cd5b49f2aa022f411500fd3326ff91e6dc0544a1c90c0003008b69117bb1376ac8df960f785d8c208c599d3a36248c98728256bb6d4737e59600000000");
    NotarisationData nd(1);
    bool res = E_UNMARSHAL(opret, ss >> nd);
    ASSERT_TRUE(res);
}

void clear_npoints(komodo_state *sp)
{
    reinterpret_cast<komodo_state_accessor*>(sp)->clear_npoints();
}

size_t count_npoints(komodo_state *sp)
{
    return sp->NumCheckpoints();
}

const notarized_checkpoint *last_checkpoint(komodo_state *sp)
{
    return reinterpret_cast<komodo_state_accessor*>(sp)->last_checkpoint();
}

TEST(TestParseNotarisation, test_notarized_update)
{
    // get the komodo_state to play with
    char src[KOMODO_ASSETCHAIN_MAXLEN];
    char dest[KOMODO_ASSETCHAIN_MAXLEN];
    komodo_state *sp = komodo_stateptr(src, dest);
    EXPECT_NE(sp, nullptr);

    clear_npoints(sp);
    // height lower than notarized_height
    komodo_notarized_update(sp, 9, 10, uint256(), uint256(), uint256(), 1);
    EXPECT_EQ(0, count_npoints(sp));
    auto npptr = komodo_npptr(11);
    EXPECT_EQ(npptr, nullptr);

    // 1 inserted with height 10
    komodo_notarized_update(sp, 10, 8, uint256(), uint256(), uint256(), 2);
    EXPECT_EQ(1, count_npoints(sp));
    const notarized_checkpoint *tmp = last_checkpoint(sp);
    EXPECT_EQ(tmp->nHeight, 10);
    EXPECT_EQ(tmp->notarized_height, 8);
    EXPECT_EQ(tmp->MoMdepth, 2);
    clear_npoints(sp);
}

TEST(TestParseNotarisation, test_npptr)
{
    // get the komodo_state to play with
    char src[KOMODO_ASSETCHAIN_MAXLEN];
    char dest[KOMODO_ASSETCHAIN_MAXLEN];
    komodo_state *sp = komodo_stateptr(src, dest);
    EXPECT_NE(sp, nullptr);

    // empty NPOINTS
    clear_npoints(sp);
    komodo_notarized_update(sp, 9, 10, uint256(), uint256(), uint256(), 1);
    EXPECT_EQ(0, count_npoints(sp));
    auto npptr = komodo_npptr(11);
    EXPECT_EQ(npptr, nullptr);

    // 1 inserted with height 10
    komodo_notarized_update(sp, 10, 8, uint256(), uint256(), uint256(), 2);
    EXPECT_EQ(1, count_npoints(sp));
    const notarized_checkpoint *tmp = last_checkpoint(sp);
    EXPECT_EQ(tmp->nHeight, 10);
    EXPECT_EQ(tmp->notarized_height, 8);
    EXPECT_EQ(tmp->MoMdepth, 2);
    // test komodo_npptr
    npptr = komodo_npptr(-1); // none found with a notarized_height so low
    EXPECT_EQ(npptr, nullptr);
    npptr = komodo_npptr(0); // none found with a notarized_height so low
    EXPECT_EQ(npptr, nullptr);
    npptr = komodo_npptr(1); // none found with a notarized_height so low
    EXPECT_EQ(npptr, nullptr);
    npptr = komodo_npptr(6); // none found with a notarized_height so low
    EXPECT_EQ(npptr, nullptr);
    npptr = komodo_npptr(7); // one found
    ASSERT_NE(npptr, nullptr);
    EXPECT_EQ(npptr->nHeight, 10);
    EXPECT_EQ(npptr->notarized_height, 8);
    EXPECT_EQ(npptr->MoMdepth, 2);
    npptr = komodo_npptr(9); // none found with a notarized_height so high
    EXPECT_EQ(npptr, nullptr);

    // add another with the same index
    komodo_notarized_update(sp, 10, 9, uint256(), uint256(), uint256(), 2);
    EXPECT_EQ(2, count_npoints(sp)); 

    npptr = komodo_npptr(-1); // none found with a notarized_height so low
    EXPECT_EQ(npptr, nullptr);
    npptr = komodo_npptr(0); // none found with a notarized_height so low
    EXPECT_EQ(npptr, nullptr);
    npptr = komodo_npptr(1); // none found with a notarized_height so low
    EXPECT_EQ(npptr, nullptr);
    npptr = komodo_npptr(6); // none found with a notarized_height so low
    EXPECT_EQ(npptr, nullptr);
    npptr = komodo_npptr(7); // original found
    ASSERT_NE(npptr, nullptr);
    EXPECT_EQ(npptr->nHeight, 10);
    EXPECT_EQ(npptr->notarized_height, 8);
    EXPECT_EQ(npptr->MoMdepth, 2);
    npptr = komodo_npptr(8); // new one found
    ASSERT_NE(npptr, nullptr);
    EXPECT_EQ(npptr->nHeight, 10);
    EXPECT_EQ(npptr->notarized_height, 9);
    EXPECT_EQ(npptr->MoMdepth, 2);
    npptr = komodo_npptr(9); // new one found
    ASSERT_NE(npptr, nullptr);
    EXPECT_EQ(npptr->nHeight, 10);
    EXPECT_EQ(npptr->notarized_height, 9);
    EXPECT_EQ(npptr->MoMdepth, 2);
    npptr = komodo_npptr(10); // none found with a notarized_height so high
    EXPECT_EQ(npptr, nullptr);
    npptr = komodo_npptr(11); // none found with a notarized_height so high
    EXPECT_EQ(npptr, nullptr);
    clear_npoints(sp);
}

TEST(TestParseNotarisation, test_prevMoMheight)
{
    // get the komodo_state to play with
    char src[KOMODO_ASSETCHAIN_MAXLEN];
    char dest[KOMODO_ASSETCHAIN_MAXLEN];
    komodo_state *sp = komodo_stateptr(src, dest);
    EXPECT_NE(sp, nullptr);

    // empty NPOINTS
    clear_npoints(sp);
    EXPECT_EQ(komodo_prevMoMheight(), 0);
    uint256 mom;
    mom.SetHex("A0");
    komodo_notarized_update(sp, 10, 9, uint256(), uint256(), mom, 1);
    EXPECT_EQ(komodo_prevMoMheight(), 9);
    komodo_notarized_update(sp, 11, 10, uint256(), uint256(), mom, 1);
    EXPECT_EQ(komodo_prevMoMheight(), 10);
    komodo_notarized_update(sp, 9, 8, uint256(), uint256(), mom, 1); // we're not sorted by anything other than chronological
    EXPECT_EQ(komodo_prevMoMheight(), 8);
}

TEST(TestParseNotarisation, test_notarizeddata)
{
    // get the komodo_state to play with
    char src[KOMODO_ASSETCHAIN_MAXLEN];
    char dest[KOMODO_ASSETCHAIN_MAXLEN];
    komodo_state *sp = komodo_stateptr(src, dest);
    EXPECT_NE(sp, nullptr);

    // empty NPOINTS
    clear_npoints(sp);
    uint256 hash;
    uint256 expected_hash;
    uint256 txid;
    uint256 expected_txid;
    auto rslt = komodo_notarizeddata(0, &hash, &txid);
    EXPECT_EQ(rslt, 0);
    EXPECT_EQ(hash, expected_hash);
    EXPECT_EQ(txid, expected_txid);

    // now add a notarization
    expected_hash.SetHex("0A");
    expected_txid.SetHex("0B");
    komodo_notarized_update(sp, 10, 9, expected_hash, expected_txid, uint256(), 1);
    rslt = komodo_notarizeddata(0, &hash, &txid); // too low
    EXPECT_EQ(rslt, 0);
    rslt = komodo_notarizeddata(9, &hash, &txid); // too low
    EXPECT_EQ(rslt, 0);
    rslt = komodo_notarizeddata(10, &hash, &txid); // just right, but will return nothing (still too low)
    EXPECT_EQ(rslt, 0);
    rslt = komodo_notarizeddata(11, &hash, &txid); // over the height in the array, so should find the one below
    EXPECT_EQ(rslt, 9);
    EXPECT_EQ(hash, expected_hash);
    EXPECT_EQ(txid, expected_txid);
 }

bool equal(const notarized_checkpoint* lhs, const notarized_checkpoint* rhs)
{
    if (lhs == nullptr && rhs == nullptr)
        return true;
    if (lhs == nullptr && rhs != nullptr)
        return false;
    if (lhs != nullptr && rhs == nullptr)
        return false;

    if( lhs->notarized_hash != rhs->notarized_hash )
        return false;
    if( lhs->notarized_desttxid != rhs->notarized_desttxid )
        return false;
    if( lhs->MoM != rhs->MoM )
        return false;
    if( lhs->MoMoM != rhs->MoMoM )
        return false;
    if( lhs->nHeight != rhs->nHeight)
        return false;
    if( lhs->notarized_height != rhs->notarized_height)
        return false;
    if( lhs->MoMdepth != rhs->MoMdepth)
        return false;
    if( lhs->MoMoMdepth != rhs->MoMoMdepth)
        return false;
    if( lhs->kmdstarti != rhs->kmdstarti)
        return false;
    if( lhs->kmdendi != rhs->kmdendi)
        return false;
    return true;
}

 TEST(TestParseNotarisation, OldVsNew)
 {
    // see test_parse_notarisation.h for notarized_checkpoints
    // how many are in the array?
    size_t npoints_max =  sizeof(notarized_checkpoints)/sizeof(notarized_checkpoints[0]);
    EXPECT_EQ(npoints_max, 8043);

    komodo_state new_ks;

    // [ !!! ] set the MoMdepth for tests [ !!! ]
    notarized_checkpoints[npoints_max-1].MoMdepth = 1;
    notarized_checkpoints[npoints_max-3].MoMdepth = 20;

    notarized_checkpoints[777].MoM.SetHex("0xdead");
    notarized_checkpoints[777].MoMdepth = 1;

    // fill the structures
    for (size_t idx = 0; idx < npoints_max; idx++)
    {
        old_space::komodo_notarized_update(old_space::sp,
        notarized_checkpoints[idx].nHeight,
        notarized_checkpoints[idx].notarized_height,
        notarized_checkpoints[idx].notarized_hash,
        notarized_checkpoints[idx].notarized_desttxid,
        notarized_checkpoints[idx].MoM,
        notarized_checkpoints[idx].MoMdepth);

        ::komodo_notarized_update(&new_ks,
        notarized_checkpoints[idx].nHeight,
        notarized_checkpoints[idx].notarized_height,
        notarized_checkpoints[idx].notarized_hash,
        notarized_checkpoints[idx].notarized_desttxid,
        notarized_checkpoints[idx].MoM,
        notarized_checkpoints[idx].MoMdepth);
    }

    EXPECT_EQ(old_space::sp->NUM_NPOINTS, new_ks.NumCheckpoints() );

    // Check retrieval of notarization for height

    // 2524171 - FAIL
    //int32_t arr_heights[] = { 2524224, 2524223, 2524210, 2524209, 2524200, 2524190, 2524171, 2524170 };
    //size_t max_arr_heights = sizeof(arr_heights)/sizeof(arr_heights[0]);

    int32_t max_height = 2524223;
    bool all_good = true;
    for (size_t i = 0; i < max_height; i++)
    {
        int idx_old = 0;
        notarized_checkpoint *np_old = old_space::komodo_npptr_for_height(i, &idx_old);
        const notarized_checkpoint *np_new = ::komodo_npptr(i);
        if (!equal(np_old, np_new) )
        {
            std::cout << "Chceckpoints did not match at index " << std::to_string(i) << std::endl;
            all_good = false;
        }
    }

    EXPECT_TRUE(all_good);

    /*
        DONE:

        old_space::komodo_npptr_for_height - new_space::komodo_npptr
        old_space::komodo_notarizeddata    - new_space::komodo_notarizeddata

        TODO:

        - komodo_notarized_height

    */

    // Check retrieval of data using komodo_notarizeddata()

    // 2524223, 2524192, 2441342 - FAIL
    //int32_t arr_nheights[] = { 2524224, 2524223, 2524210, 2524209, 2524200, 2524192, 2524190, 2524171, 2524170, 2441342 };
    //
    //size_t max_arr_nheights = sizeof(arr_nheights)/sizeof(arr_nheights[0]);
    //int32_t old_ret_height, new_ret_height;

    /*
    for (size_t i = 0; i < max_height; i++) {
        uint256 ret_notarized_hashp, ret_notarized_desttxidp;
        int32_t old_ret_height = old_space::komodo_notarizeddata(i, 
                &ret_notarized_hashp, &ret_notarized_desttxidp);

        int32_t new_ret_height = komodo_notarizeddata(i, &ret_notarized_hashp, &ret_notarized_desttxidp);
        EXPECT_EQ(old_ret_height, new_ret_height);
        // TODO: Check the out params for equality
    }
    */
    // check some specific locations (0, 777)
    /*
    notarized_checkpoint *np_old = old_space::komodo_npptr_at(0);

    std::cout << "komodo_npptr_at(" << (0) << ") = " << (*np_old) << std::endl;
    np_old = old_space::komodo_npptr_at(npoints_max-1);
    std::cout << "komodo_npptr_at(" << (npoints_max-1) << ") = " << (*np_old) << std::endl;
    np_old = old_space::komodo_npptr_at(777);
    std::cout << "komodo_npptr_at(" << (777) << ") = " << (*np_old) << std::endl;
    */

    // check komodo_prevMoMheight()
    EXPECT_EQ( old_space::komodo_prevMoMheight(), ::komodo_prevMoMheight());

 }

// for l in `g 'parse notarisation' ~/.komodo/debug.log | pyline 'l.split()[8]'`; do hoek decodeTx '{"hex":"'`src/komodo-cli getrawtransaction "$l"`'"}' | jq '.outputs[1].script.op_return' | pyline 'import base64; print base64.b64decode(l).encode("hex")'; done

}
