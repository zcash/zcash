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

// komodo functions that interact with bitcoind C++

uint32_t komodo_txtime(uint256 hash)
{
    CTransaction tx;
    uint256 hashBlock;
    if (!GetTransaction(hash, tx, hashBlock, true))
    {
        //printf("null GetTransaction\n");
        return(tx.nLockTime);
    }
    return(0);
}

void komodo_disconnect(CBlockIndex *pindex,CBlock& block)
{
    komodo_init();
    //uint256 zero;
    //printf("disconnect ht.%d\n",pindex->nHeight);
    //memset(&zero,0,sizeof(zero));
    //komodo_stateupdate(-pindex->nHeight,0,0,0,zero,0,0,0,0);
}

int32_t komodo_block2height(CBlock *block)
{
    int32_t i,n,height = 0; uint8_t *ptr = (uint8_t *)block->vtx[0].vin[0].scriptSig.data();
    komodo_init();
    if ( block->vtx[0].vin[0].scriptSig.size() > 5 )
    {
        //for (i=0; i<6; i++)
        //    printf("%02x",ptr[i]);
        n = ptr[0];
        for (i=0; i<n; i++)
        {
            //03bb81000101(bb 187) (81 48001) (00 12288256)  <- coinbase.6 ht.12288256
            height += ((uint32_t)ptr[i+1] << (i*8));
            //printf("(%02x %x %d) ",ptr[i+1],((uint32_t)ptr[i+1] << (i*8)),height);
        }
        //printf(" <- coinbase.%d ht.%d\n",(int32_t)block->vtx[0].vin[0].scriptSig.size(),height);
    }
    return(height);
}

void komodo_block2pubkey33(uint8_t *pubkey33,CBlock& block)
{
    uint8_t *ptr = (uint8_t *)block.vtx[0].vout[0].scriptPubKey.data();
    komodo_init();
    memcpy(pubkey33,ptr+1,33);
}

void komodo_index2pubkey33(uint8_t *pubkey33,CBlockIndex *pindex,int32_t height)
{
    CBlock block;
    komodo_init();
    memset(pubkey33,0,33);
    if ( pindex != 0 )
    {
        if ( ReadBlockFromDisk(block,(const CBlockIndex *)pindex) != 0 )
        {
            komodo_block2pubkey33(pubkey33,block);
        }
    }
    else
    {
        // height -> pubkey33
        //printf("extending chaintip komodo_index2pubkey33 height.%d need to get pubkey33\n",height);
    }
}

int32_t komodo_checkpoint(int32_t *notarized_heightp,int32_t nHeight,uint256 hash)
{
    int32_t notarized_height; uint256 notarized_hash,notarized_desttxid; CBlockIndex *notary;
    notarized_height = komodo_notarizeddata(chainActive.Tip()->nHeight,&notarized_hash,&notarized_desttxid);
    *notarized_heightp = notarized_height;
    if ( notarized_height >= 0 && notarized_height <= chainActive.Tip()->nHeight && (notary= mapBlockIndex[notarized_hash]) != 0 )
    {
        //printf("nHeight.%d -> (%d %s)\n",chainActive.Tip()->nHeight,notarized_height,notarized_hash.ToString().c_str());
        if ( notary->nHeight == notarized_height ) // if notarized_hash not in chain, reorg
        {
            if ( nHeight < notarized_height )
            {
                fprintf(stderr,"nHeight.%d < NOTARIZED_HEIGHT.%d\n",nHeight,notarized_height);
                return(-1);
            }
            else if ( nHeight == notarized_height && memcmp(&hash,&notarized_hash,sizeof(hash)) != 0 )
            {
                fprintf(stderr,"nHeight.%d == NOTARIZED_HEIGHT.%d, diff hash\n",nHeight,notarized_height);
                return(-1);
            }
        } else fprintf(stderr,"unexpected error notary_hash %s ht.%d at ht.%d\n",notarized_hash.ToString().c_str(),notarized_height,notary->nHeight);
    } else if ( notarized_height > 0 )
        fprintf(stderr,"couldnt find notary_hash %s ht.%d\n",notarized_hash.ToString().c_str(),notarized_height);
    return(0);
}
