
// start https://github.com/mentalmove/SudokuGenerator
//
//  main.c
//  SudokuGenerator
//
//  Malte Pagel
//

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <math.h>


#define SMALL_LINE 3
#define LINE 9
#define TOTAL 81

#define LIMIT 16777216

#define SHOW_SOLVED 1


struct dimensions_collection {
    int row;
    int column;
    int small_square;
};


static int indices[TOTAL];
static int riddle[TOTAL];
static int solved[TOTAL];
static int unsolved[TOTAL];
static int tries_to_set = 0;
static int taking_back;
static int global_unset_count = 0;


struct dimensions_collection get_collection(int);
int contains_element(int*, int, int);
void get_horizontal(int, int*);
void get_vertical(int, int*);
void get_square(int, int*);
int set_values(int, int);
void take_back(int);

int show_solution(int*);


int show_solution (int* solution) {
    
    int i;
    int counter = 0;
    
    printf( " -----------------------------------\n" );
    
    for ( i = 0; i < TOTAL; i++ ) {
        if ( i % LINE == 0 )
            printf( "|" );
        
        if ( solution[i] ) {
            printf( " %d ", solution[i]);
            counter++;
        }
        else
            printf( "   ");
        
        if ( i % LINE == (LINE - 1) ) {
            printf( "|\n" );
            if ( i != (TOTAL - 1) ) {
                if ( i % (SMALL_LINE * LINE) == (SMALL_LINE * LINE - 1) )
                    printf( "|-----------+-----------+-----------|\n" );
                else
                    printf( "|- - - - - -|- - - - - -|- - - - - -|\n" );
            }
        }
        else {
            if ( i % SMALL_LINE == (SMALL_LINE - 1) )
                printf( "|");
            else
                printf( ":" );
        }
    }
    
    printf( " -----------------------------------" );
    
    return counter;
}


/**
 * Takes a position inside the large square and returns
 *		- the row number
 *		- the column number
 *		- the small square number
 * where this position is situated in
 */
struct dimensions_collection get_collection (int index) {
    struct dimensions_collection ret;
    
    ret.row = (int) (index / LINE);
    ret.column = index % LINE;
    ret.small_square = SMALL_LINE * (int) (ret.row / SMALL_LINE) + (int) (ret.column / SMALL_LINE);
    
    return ret;
}

/**
 * Is 'the_element' in 'the_array'?
 */
int contains_element (int* the_array, int the_element, int length) {
    for ( int i = 0; i < length; i++ )
        if ( the_array[i] == the_element )
            return 1;
    return 0;
}

/**
 * Sets all members of row 'row'
 */
void get_horizontal (int row, int* ret) {
    int j = 0;
    for ( int i = (row * LINE); i < (row * LINE) + LINE; i++ )
        ret[j++] = riddle[i];
}
/**
 * Sets all members of column 'col'
 */
void get_vertical (int col, int* ret) {
    int j = 0;
    for ( int i = col; i < TOTAL; i += LINE )
        ret[j++] = riddle[i];
}
/**
 * Sets all members of small square 'which'
 */
void get_square (int which, int* ret) {
    for ( int i = 0; i < SMALL_LINE; i++ )
        for ( int j = 0; j < SMALL_LINE; j++ )
            ret[SMALL_LINE * i + j] = riddle[LINE * i + which * SMALL_LINE + j + ((int) (which / SMALL_LINE) * (SMALL_LINE - 1) * LINE)];
}

/**
 * Recursive function:
 * Try for each position the numbers from 1 to LINE
 *      (except 'forbidden_number' if given).
 * If all numbers collide with already set numbers, move is bad.
 * If a number doesn't collide with already set numbers,
 *	- move is bad if next move collides with the already set numbers
 *              (including actual one)
 *	- move is good if it's the last one
 */
int set_values (int index, int forbidden_number) {
    
    if ( taking_back && tries_to_set > (2 * LIMIT) )
        return 1;
    
    int real_index = indices[index];
    struct dimensions_collection blocks = get_collection(real_index);
    int elements[LINE];
    
    for ( int i = 1; i <= LINE; i++ ) {
        if ( forbidden_number && i == forbidden_number )
            continue;
        
        tries_to_set++;
        
        get_horizontal(blocks.row, elements);
        if ( contains_element(elements, i, LINE) )
            continue;
        
        get_vertical(blocks.column, elements);
        if ( contains_element(elements, i, LINE) )
            continue;
        
        get_square(blocks.small_square, elements);
        if ( contains_element(elements, i, LINE) )
            continue;
        
        riddle[real_index] = i;
        
        if ( index == (TOTAL - 1) || set_values((index + 1), 0) )
            return 1;
    }
    
    riddle[real_index] = 0;
    
    return 0;
}

/**
 * Some steps to hide unnecessary numbers:
 *	a) Define last piece as 'special piece'
 *	b) Remember this piece's value
 *	c) Try to create riddle from this position on,
 *          but forbid the value of the special piece
 *	d)	I)  If operation fails, define the piece before the special piece
 *          as 'special piece' and continue with b)
 *		II) If operation is possible, reset 'special piece'
 *			and put it to start of list
 *	e) Stop if all pieces are tried or calculation limit is reached
 */
void take_back (int unset_count) {
    
    global_unset_count++;
    
    int i;
    
    int tmp = riddle[indices[TOTAL - unset_count]];
    int redundant = set_values((TOTAL - unset_count), tmp);
    
    if ( !redundant ) {
        unsolved[indices[TOTAL - unset_count]] = 0;
        take_back(++unset_count);
    }
    else {
        riddle[indices[TOTAL - unset_count]] = tmp;
        for ( i = 1; i < unset_count; i++ )
            riddle[indices[TOTAL - unset_count + i]] = 0;
        
        for ( i = (TOTAL - unset_count); i > 0; i-- )
            indices[i] = indices[i - 1];
        indices[0] = tmp;
        
        if ( global_unset_count < TOTAL && tries_to_set < LIMIT )
            take_back(unset_count);
    }
}


int sudoku(uint8_t solved9[LINE][LINE],uint8_t unsolved9[LINE][LINE],uint32_t srandi)
{
    int i, j, random, small_rows, small_cols, tmp, redundant,ind;
    int multi_raw[LINE][LINE];
    
    memset(indices,0,sizeof(indices));
    memset(solved,0,sizeof(solved));
    memset(unsolved,0,sizeof(unsolved));
    tries_to_set = 0;
    taking_back = 0;
    global_unset_count = 0;
    
    //time_t t;
    //time(&t);
    srand(srandi);
    
    /**
     * Initialization:
     * Fields are set to 0 ( i.e. we dont' know the number yet)
     */
    for ( i = 0; i < TOTAL; i++ )
        riddle[i] = 0;
    
    /**
     * Second initialization:
     * LINE times numbers from 0 to (LINE - 1),
     * i.e. every square
     */
    int big_rows_array[] = {0, 1, 2};
    int big_cols_array[] = {0, 1, 2};
    random = rand() % 4;
    switch (random) {
        case 1:
            big_rows_array[0] = 2;
            big_rows_array[1] = 1;
            big_rows_array[2] = 0;
            break;
        case 2:
            big_cols_array[0] = 2;
            big_cols_array[1] = 1;
            big_cols_array[2] = 0;
            break;
        case 3:
            big_rows_array[0] = 2;
            big_rows_array[1] = 1;
            big_rows_array[2] = 0;
            big_cols_array[0] = 2;
            big_cols_array[1] = 1;
            big_cols_array[2] = 0;
    }
    int big_rows, big_cols, big_rows_index, big_cols_index, start_value;
    i = 0;
    j = 0;
    for ( big_rows_index = 0; big_rows_index < SMALL_LINE; big_rows_index++ ) {
        big_rows = big_rows_array[big_rows_index];
        for ( big_cols_index = 0; big_cols_index < SMALL_LINE; big_cols_index++ ) {
            big_cols = big_cols_array[big_cols_index];
            start_value = big_rows * LINE * SMALL_LINE + (big_cols * SMALL_LINE);
            for ( small_rows = 0; small_rows < SMALL_LINE; small_rows++ )
                for ( small_cols = 0; small_cols < SMALL_LINE; small_cols++ )
                    multi_raw[i][j++] = small_rows * LINE + small_cols + start_value;
            i++;
            j = 0;
        }
    }
    
    
    /**
     * Randomization for every element of multi_raw.
     * Suffle only inside squares
     */
    for ( i = 0; i < LINE; i++ ) {
        for ( j = 0; j < LINE; j++ ) {
            random = rand() % LINE;
            if ( j == random )
                continue;
            tmp = multi_raw[i][j];
            multi_raw[i][j] = multi_raw[i][random];
            multi_raw[i][random] = tmp;
        }
    }
    
    /**
     * Linearization
     */
    for ( i = 0; i < LINE; i++ )
        for ( j = 0; j < LINE; j++ )
            indices[i * LINE + j] = multi_raw[i][j];
    
    
    /**
     * Setting numbers, start with the first one.
     * Variable 'redundant' is needed only for formal reasons
     */
    taking_back = 0;
    redundant = set_values(0, 0);
    
    
    memcpy(solved, riddle, (TOTAL * sizeof(int)));
    memcpy(unsolved, riddle, (TOTAL * sizeof(int)));
    
    
    /**
     * Exchanging some (few) indices for more randomized game
     */
    int random2;
    for ( i = (LINE - 1); i > 0; i-- ) {
        for ( j = 0; j < (int) (sqrt(i)); j++ ) {
            
            if ( !(rand() % ((int) (i * sqrt(i)))) || !(LINE - j) )
                continue;
            
            random = i * LINE + (int) (rand() % (LINE - j));
            random2 = rand() % TOTAL;
            
            if ( random == random2 )
                continue;
            
            tmp = indices[random];
            indices[random] = indices[random2];
            indices[random2] = tmp;
        }
    }
    
    
    tries_to_set = 0;
    taking_back = 1;
    take_back(1);
    
    
    if ( SHOW_SOLVED ) {
        printf( "\n\n" );
        redundant = show_solution(solved);
    }
    
    int counter = show_solution(unsolved);
    printf( "\t *** %d numbers left *** \n", counter );
    ind = 0;
    for (i=0; i<LINE; i++)
        for (j=0; j<LINE; j++,ind++)
        {
            solved9[i][j] = solved[ind];
            unsolved9[i][j] = unsolved[ind];
        }
    
    return 0;
}
// end https://github.com/mentalmove/SudokuGenerator

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

#include "cJSON.h"
#define SUDOKU_NINETH 387420489

void sudoku_rowdisp(uint32_t x)
{
    int32_t i; char vals[10],val;
    x %= SUDOKU_NINETH;
    for (i=0; i<9; i++)
    {
        val = (x % 9);
        vals[i] = '1' + val;
        //printf("%d",val);
        x /= 9;
    }
    vals[i++] = 0;
    printf("%s\n",vals);
}

int32_t sudoku_privkeydisp(uint8_t key32[32])
{
    uint8_t i,val,flags[9]; char str[10]; uint32_t x,ind;
    memset(flags,0,sizeof(flags));
    ind = 0;
    for (i=0; i<8; i++)
    {
        x = (uint32_t)key32[ind++] << 24;
        x += (uint32_t)key32[ind++] << 16;
        x += (uint32_t)key32[ind++] << 8;
        x += (uint32_t)key32[ind++];
        sudoku_rowdisp(x);
        val = (x / SUDOKU_NINETH);
        flags[val]++;
        str[i] = val + '1';
    }
    for (i=0; i<9; i++)
        if ( flags[i] == 0 )
        {
            str[8] = i + '1';
            str[9] = 0;
            printf("%s\n",str);
            return(0);
        }
    return(-1);
}

void sudoku_privkey(uint8_t *privkey,uint8_t vals9[9][9])
{
    uint32_t x,keyvals[8]; int32_t i,j,ind;
    for (i=0; i<9; i++)
    {
        x = 0;
        for (j=8; j>=0; j--)
        {
            x *= 9;
            x += vals9[i][j]-1;
        }
        if ( i < 8 )
            keyvals[i] = x;
        else
        {
            for (j=0; j<8; j++)
                keyvals[j] += SUDOKU_NINETH * (vals9[i][j]-1);
        }
    }
    for (i=ind=0; i<8; i++)
    {
        privkey[ind++] = ((keyvals[i] >> 24) & 0xff);
        privkey[ind++] = ((keyvals[i] >> 16) & 0xff);
        privkey[ind++] = ((keyvals[i] >> 8) & 0xff);
        privkey[ind++] = (keyvals[i] & 0xff);
    }
}

void sudoku_gen(uint8_t key32[32],uint8_t unsolved[9][9],uint32_t srandi)
{
    uint8_t vals9[9][9],uniq9[9][9]; int32_t i,j;
    sudoku(vals9,unsolved,srandi);
    sudoku_privkey(key32,vals9);
    sudoku_privkeydisp(key32);
}

//////////////////////// start of CClib interface
// ./komodod -ac_name=SUDOKU -ac_supply=1000000 -pubkey=<yourpubkey> -addnode=5.9.102.210 -gen -genproclimit=1 -ac_cclib=sudoku -ac_perc=10000000 -ac_reward=100000000 -ac_cc=60000 -ac_script=2ea22c80203d1579313abe7d8ea85f48c65ea66fc512c878c0d0e6f6d54036669de940febf8103120c008203000401cc &
/* cclib "gen" 17 \"10\"
 5d13c1ad80daf37215c74809a36720c2ada90bacadb2e10bf0866092ce558432
*/

/* cclib "txidinfo" 17 \"5d13c1ad80daf37215c74809a36720c2ada90bacadb2e10bf0866092ce558432\"
{
    "result": "success",
    "txid": "5d13c1ad80daf37215c74809a36720c2ada90bacadb2e10bf0866092ce558432",
    "result": "success",
    "amount": 1.00000000,
    "unsolved": "46-8---15-75-61-3----4----8-1--75-----3--24----2-----6-4----------73----------36-",
    "name": "sudoku",
    "method": "txidinfo"
}*/

/* cclib "pending" 17
{
    "result": "success",
    "name": "sudoku",
    "method": "pending",
    "pending": [
                "5d13c1ad80daf37215c74809a36720c2ada90bacadb2e10bf0866092ce558432"
                ]
}*/

/*
 cclib "solution" 17 \"[%22fdc9409741f2ede29307da1a06438da0ea6f8d885d2d5c3199c4ef541ec1b5fd%22,%22469823715875961234231457698914675823653182479782394156346219587528736941197548362%22,1548777525,1548777526,...]\"
 {
 "name": "sudoku",
 "method": "solution",
 "sudokuaddr": "RSeoPJvMUSLfUHM1BomB97geW9zPznwHXk",
 "amount": 1.00000000,
 "result": "success",
 "hex": "0400008085202f8901328455ce926086f00be1b2adac0ba9adc22067a30948c71572f3da80adc1135d010000007b4c79a276a072a26ba067a565802102c57d40c1ddc92a5246a937bd7338823f1e8c916b137f2092d38cf250d74cb5ab8140f92d54f611aa3cb3d187eaadd56b06f3a8c0f5fba23956b26fdefc6038d9b6282de38525f72ebd8945a7994cef63ebca711ecf8fe6baeefcc218cf58efb59dc2a100af03800111a10001ffffffff02f0b9f505000000002321039433dc3749aece1bd568f374a45da3b0bc6856990d7da3cd175399577940a775ac0000000000000000fd9f016a4d9b01115351343639383233373135383735393631323334323331343537363938393134363735383233363533313832343739373832333934313536333436323139353837353238373336393431313937353438333632fd4401000000005c5078355c50783600000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000100000000000000000000000000"
 }
 */

int32_t sudoku_captcha(uint32_t timestamps[81])
{
    int32_t i,solvetime,diff,avetime,n = 0; uint64_t variance = 0; std::vector<uint32_t> list;
    for (i=0; i<81; i++)
    {
        if ( timestamps[i] != 0 )
        {
            list.push_back(timestamps[i]);
            n++;
        }
    }
    if ( n > 81/2 )
    {
        std::sort(list.begin(),list.end());
        solvetime = (list[n-1] - list[0]);
        if ( list[0] >= list[n-1] )
        {
            printf("list[0] %u vs list[%d-1] %u\n",list[0],n,list[n-1]);
            return(-1);
        }
        else if ( list[n-1] > chainActive.LastTip()->nTime+200 )
            return(-1);
        else if ( solvetime >= 777 )
            return(0);
        else
        {
            avetime = (solvetime / (n-1));
            if ( avetime == 0 )
                return(-1);
            for (i=0; i<n-1; i++)
            {
                diff = (list[i+1] - list[i]);
                printf("%d ",diff);
                diff -= avetime;
                variance += (diff * diff);
            }
            variance /= (n - 1);
            printf("solvetime.%d n.%d avetime.%d variance.%llu vs ave2 %d\n",solvetime,n,avetime,(long long)variance,avetime*avetime);
            if ( variance < avetime*avetime )
                return(-1 * 0);
            else return(0);
        }
    } else return(-1);
}

CScript sudoku_genopret(uint8_t unsolved[9][9])
{
    CScript opret; uint8_t evalcode = EVAL_SUDOKU; std::vector<uint8_t> data; int32_t i,j;
    for (i=0; i<9; i++)
        for (j=0; j<9; j++)
            data.push_back(unsolved[i][j]);
    opret << OP_RETURN << E_MARSHAL(ss << evalcode << 'G' << data);
    return(opret);
}

CScript sudoku_solutionopret(char *solution,uint32_t timestamps[81])
{
    CScript opret; uint8_t evalcode = EVAL_SUDOKU; std::string str(solution); std::vector<uint8_t> data; int32_t i;
    for (i=0; i<81; i++)
    {
        data.push_back((timestamps[i] >> 24) & 0xff);
        data.push_back((timestamps[i] >> 16) & 0xff);
        data.push_back((timestamps[i] >> 8) & 0xff);
        data.push_back(timestamps[i] & 0xff);
    }
    opret << OP_RETURN << E_MARSHAL(ss << evalcode << 'S' << str << data);
    return(opret);
}

uint8_t sudoku_genopreturndecode(char *unsolved,CScript scriptPubKey)
{
    std::vector<uint8_t> vopret; uint8_t *script,e,f; std::vector<uint8_t> data; int32_t i;
    GetOpReturnData(scriptPubKey,vopret);
    script = (uint8_t *)vopret.data();
    if ( vopret.size() > 2 && E_UNMARSHAL(vopret,ss >> e; ss >> f; ss >> data) != 0 && e == EVAL_SUDOKU && f == 'G' )
    {
        if ( data.size() == 81 )
        {
            for (i=0; i<81; i++)
                unsolved[i] = data[i] == 0 ? '-' : '0' + data[i];
            unsolved[i] = 0;
            return(f);
        }
    }
    return(0);
}

UniValue sudoku_generate(uint64_t txfee,struct CCcontract_info *cp,cJSON *params)
{
    CMutableTransaction mtx = CreateNewContextualCMutableTransaction(Params().GetConsensus(), komodo_nextheight());
    UniValue result(UniValue::VOBJ); CPubKey sudokupk,pk; uint8_t privkey[32],unsolved[9][9],pub33[33]; uint32_t srandi; uint256 hash; char coinaddr[64],*jsonstr; uint64_t inputsum,amount,change=0; std::string rawtx;
    amount = COIN;
    if ( params != 0 )
    {
        if ( (jsonstr= jprint(params,0)) != 0 )
        {
            if ( jsonstr[0] == '"' && jsonstr[strlen(jsonstr)-1] == '"' )
            {
                jsonstr[strlen(jsonstr)-1] = 0;
                jsonstr++;
            }
            amount = atof(jsonstr) * COIN + 0.0000000049;
        }
    }
    result.push_back(Pair("result","success"));
    result.push_back(Pair("name","sudoku"));
    result.push_back(Pair("method","gen"));
    hash = chainActive.LastTip()->GetBlockHash();
    memcpy(&srandi,&hash,sizeof(srandi));
    srandi ^= (uint32_t)time(NULL);
    sudoku_gen(privkey,unsolved,srandi);
    priv2addr(coinaddr,pub33,privkey);
    pk = buf2pk(pub33);
    sudokupk = GetUnspendable(cp,0);
    result.push_back(Pair("srand",(int)srandi));
    result.push_back(Pair("amount",ValueFromAmount(amount)));
    if ( (inputsum= AddCClibInputs(cp,mtx,sudokupk,amount+2*txfee,16,cp->unspendableCCaddr)) >= amount+2*txfee )
    {
        //printf("inputsum %.8f\n",(double)inputsum/COIN);
        mtx.vout.push_back(MakeCC1vout(cp->evalcode,txfee,sudokupk));
        mtx.vout.push_back(MakeCC1vout(cp->evalcode,amount,pk));
        if ( inputsum > amount + 2*txfee )
            change = (inputsum - amount - 2*txfee);
        if ( change > txfee )
        {
            if ( change > 10000*COIN )
            {
                mtx.vout.push_back(MakeCC1vout(cp->evalcode,change/2,sudokupk));
                mtx.vout.push_back(MakeCC1vout(cp->evalcode,change/2,sudokupk));
            } else mtx.vout.push_back(MakeCC1vout(cp->evalcode,change,sudokupk));
        }
        rawtx = FinalizeCCTx(0,cp,mtx,pubkey2pk(Mypubkey()),txfee,sudoku_genopret(unsolved));
        if ( rawtx.size() > 0 )
        {
            CTransaction tx;
            result.push_back(Pair("hex",rawtx));
            if ( DecodeHexTx(tx,rawtx) != 0 )
            {
                LOCK(cs_main);
                if ( myAddtomempool(tx) != 0 )
                {
                    RelayTransaction(tx);
                    result.push_back(Pair("txid",tx.GetHash().ToString()));
                }
            }
        } else result.push_back(Pair("error","couldnt finalize CCtx"));
    } else result.push_back(Pair("error","not enough SUDOKU funds"));
    return(result);
}

UniValue sudoku_txidinfo(uint64_t txfee,struct CCcontract_info *cp,cJSON *params)
{
    UniValue result(UniValue::VOBJ); int32_t numvouts; char str[65],*txidstr; uint256 txid,hashBlock; CTransaction tx; char unsolved[82];
    if ( params != 0 )
    {
        result.push_back(Pair("result","success"));
        if ( (txidstr= jprint(params,0)) != 0 )
        {
            if ( txidstr[0] == '"' && txidstr[strlen(txidstr)-1] == '"' )
            {
                txidstr[strlen(txidstr)-1] = 0;
                txidstr++;
            }
            //printf("params -> (%s)\n",txidstr);
            decode_hex((uint8_t *)&txid,32,txidstr);
            txid = revuint256(txid);
            result.push_back(Pair("txid",txid.GetHex()));
            if ( GetTransaction(txid,tx,hashBlock,false) != 0 && (numvouts= tx.vout.size()) > 1 )
            {
                if ( sudoku_genopreturndecode(unsolved,tx.vout[numvouts-1].scriptPubKey) == 'G' )
                {
                    result.push_back(Pair("result","success"));
                    result.push_back(Pair("amount",ValueFromAmount(tx.vout[1].nValue)));
                    result.push_back(Pair("unsolved",unsolved));
                }
                else
                {
                    result.push_back(Pair("result","error"));
                    result.push_back(Pair("error","couldnt extract sudoku_generate opreturn"));
                }
            }
            else
            {
                result.push_back(Pair("result","error"));
                result.push_back(Pair("error","couldnt find txid"));
            }
        }
    }
    else
    {
        result.push_back(Pair("result","error"));
        result.push_back(Pair("error","missing txid in params"));
    }
    result.push_back(Pair("name","sudoku"));
    result.push_back(Pair("method","txidinfo"));
    return(result);
}

UniValue sudoku_pending(uint64_t txfee,struct CCcontract_info *cp,cJSON *params)
{
    UniValue result(UniValue::VOBJ),a(UniValue::VARR);
    char coinaddr[64],unsolved[82]; int64_t nValue,total=0; uint256 txid,hashBlock; CTransaction tx; int32_t vout,numvouts; CPubKey sudokupk;
    std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> > unspentOutputs;
    sudokupk = GetUnspendable(cp,0);
    GetCCaddress(cp,coinaddr,sudokupk);
    SetCCunspents(unspentOutputs,coinaddr);
    for (std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> >::const_iterator it=unspentOutputs.begin(); it!=unspentOutputs.end(); it++)
    {
        txid = it->first.txhash;
        vout = (int32_t)it->first.index;
        //char str[65]; fprintf(stderr,"%s check %s/v%d %.8f\n",coinaddr,uint256_str(str,txid),vout,(double)it->second.satoshis/COIN);
        if ( it->second.satoshis != txfee || vout != 0 )
            continue;
        if ( GetTransaction(txid,tx,hashBlock,false) != 0 && (numvouts= tx.vout.size()) > 1 )
        {
            if ( (nValue= IsCClibvout(cp,tx,vout,coinaddr)) == txfee && myIsutxo_spentinmempool(txid,vout) == 0 )
            {
                if ( sudoku_genopreturndecode(unsolved,tx.vout[numvouts-1].scriptPubKey) == 'G' )
                {
                    a.push_back(txid.GetHex());
                    total += tx.vout[1].nValue;
                }
            }
        }
    }
    result.push_back(Pair("result","success"));
    result.push_back(Pair("name","sudoku"));
    result.push_back(Pair("method","pending"));
    result.push_back(Pair("pending",a));
    result.push_back(Pair("numpending",a.size()));
    result.push_back(Pair("total",ValueFromAmount(total)));
    return(result);
}

UniValue sudoku_solution(uint64_t txfee,struct CCcontract_info *cp,cJSON *params)
{
    CMutableTransaction mtx = CreateNewContextualCMutableTransaction(Params().GetConsensus(), komodo_nextheight());
    UniValue result(UniValue::VOBJ); int32_t i,j,good,ind,n,numvouts; uint256 txid; char *jsonstr,*newstr,*txidstr,coinaddr[64],CCaddr[64],*solution=0,unsolved[82]; CPubKey pk,mypk; uint8_t vals9[9][9],priv32[32],pub33[33]; uint32_t timestamps[81]; uint64_t balance,inputsum; std::string rawtx; CTransaction tx; uint256 hashBlock;
    mypk = pubkey2pk(Mypubkey());
    memset(timestamps,0,sizeof(timestamps));
    result.push_back(Pair("name","sudoku"));
    result.push_back(Pair("method","solution"));
    good = 0;
    if ( params != 0 )
    {
        if ( (jsonstr= jprint(params,0)) != 0 )
        {
            if ( jsonstr[0] == '"' && jsonstr[strlen(jsonstr)-1] == '"' )
            {
                jsonstr[strlen(jsonstr)-1] = 0;
                jsonstr++;
            }
            newstr = (char *)malloc(strlen(jsonstr)+1);
            for (i=j=0; jsonstr[i]!=0; i++)
            {
                if ( jsonstr[i] == '%' && jsonstr[i+1] == '2' && jsonstr[i+2] == '2' )
                {
                    newstr[j++] = '"';
                    i += 2;
                } else newstr[j++] = jsonstr[i];
            }
            newstr[j] = 0;
            params = cJSON_Parse(newstr);
        } else params = 0;
        if ( params != 0 )
        {
            if ( (n= cJSON_GetArraySize(params)) > 2 && n < (sizeof(timestamps)/sizeof(*timestamps))+2 )
            {
                for (i=2; i<n; i++)
                {
                    timestamps[i-2] = juinti(params,i);
                    //printf("%u ",timestamps[i]);
                }
                if ( (solution= jstri(params,1)) != 0 && strlen(solution) == 81 )
                {
                    for (i=ind=0; i<9; i++)
                        for (j=0; j<9; j++)
                            vals9[i][j] = solution[ind++] - '0';
                    sudoku_privkey(priv32,vals9);
                    priv2addr(coinaddr,pub33,priv32);
                    pk = buf2pk(pub33);
                    GetCCaddress(cp,CCaddr,pk);
                    result.push_back(Pair("sudokuaddr",CCaddr));
                    balance = CCaddress_balance(CCaddr);
                    result.push_back(Pair("amount",ValueFromAmount(balance)));
                    if ( sudoku_captcha(timestamps) < 0 )
                    {
                        result.push_back(Pair("result","error"));
                        result.push_back(Pair("error","captcha failure"));
                        return(result);
                    }
                    else
                    {
                        if ( (txidstr= jstri(params,0)) != 0 )
                        {
                            decode_hex((uint8_t *)&txid,32,txidstr);
                            txid = revuint256(txid);
                            result.push_back(Pair("txid",txid.GetHex()));
                            if ( CCgettxout(txid,0,1) < 0 )
                                result.push_back(Pair("error","already solved"));
                            else if ( GetTransaction(txid,tx,hashBlock,false) != 0 && (numvouts= tx.vout.size()) > 1 )
                            {
                                if ( sudoku_genopreturndecode(unsolved,tx.vout[numvouts-1].scriptPubKey) == 'G' )
                                {
                                    for (i=0; i<81; i++)
                                    {
                                        if ( unsolved[i] < '1' || unsolved[i] > '9')
                                            continue;
                                        else if ( unsolved[i] != solution[i] )
                                        {
                                            printf("i.%d [%c] != [%c]\n",i,unsolved[i],solution[i]);
                                            result.push_back(Pair("error","wrong sudoku solved"));
                                            break;
                                        }
                                    }
                                    if ( i == 81 )
                                        good = 1;
                                } else result.push_back(Pair("error","cant decode sudoku"));
                            } else result.push_back(Pair("error","couldnt find sudoku"));
                        }
                        if ( good != 0 )
                        {
                            mtx.vin.push_back(CTxIn(txid,0,CScript()));
                            if ( (inputsum= AddCClibInputs(cp,mtx,pk,balance,16,CCaddr)) >= balance )
                            {
                                mtx.vout.push_back(CTxOut(balance,CScript() << ParseHex(HexStr(mypk)) << OP_CHECKSIG));
                                CCaddr2set(cp,cp->evalcode,pk,priv32,CCaddr);
                                rawtx = FinalizeCCTx(0,cp,mtx,pubkey2pk(Mypubkey()),txfee,sudoku_solutionopret(solution,timestamps));
                            } else result.push_back(Pair("error","couldnt find funds in solution address"));
                        }
                    }
                }
            } else result.push_back(Pair("error","couldnt get all params"));
            if ( rawtx.size() > 0 )
            {
                result.push_back(Pair("result","success"));
                result.push_back(Pair("hex",rawtx));
            }
            else result.push_back(Pair("error","couldnt finalize CCtx"));
            //printf("params.(%s)\n",jprint(params,0));
            return(result);
        }
        else
        {
            result.push_back(Pair("result","error"));
            result.push_back(Pair("error","couldnt parse parameters"));
            result.push_back(Pair("parameters",newstr));
            return(result);
        }
    }
    result.push_back(Pair("result","error"));
    result.push_back(Pair("error","missing parameters"));
    return(result);
}

bool sudoku_validate(struct CCcontract_info *cp,int32_t height,Eval *eval,const CTransaction tx)
{
    return(true);
}
