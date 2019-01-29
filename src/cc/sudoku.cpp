
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

UniValue sudoku_txidinfo(uint64_t txfee,struct CCcontract_info *cp,cJSON *params)
{
    UniValue result(UniValue::VOBJ);
    if ( params != 0 )
        printf("params.(%s)\n",jprint(params,0));
    result.push_back(Pair("result","success"));
    result.push_back(Pair("name","sudoku"));
    result.push_back(Pair("method","txidinfo"));
    return(result);
}

CScript sudoku_genopret(uint8_t unsolved[9][9])
{
    CScript opret; uint8_t evalcode = EVAL_SUDOKU; std::vector<uint8_t> data;
    for (i=0; i<9; i++)
        for (j=0; j<9; j++)
            data.push_back(unsolved[i][j]);
    opret << OP_RETURN << E_MARSHAL(ss << evalcode << 'G' << data);
    return(opret);
}

UniValue sudoku_generate(uint64_t txfee,struct CCcontract_info *cp,cJSON *params)
{
    CMutableTransaction mtx = CreateNewContextualCMutableTransaction(Params().GetConsensus(), komodo_nextheight());
    UniValue result(UniValue::VOBJ); CPubKey sudokupk,pk; uint8_t privkey[32],unsolved[9][9],pub33[33]; uint32_t srandi; uint256 hash; char coinaddr[64]; uint64_t inputsum,amount; std::string rawtx;
    if ( params != 0 )
    {
        printf("params.(%s)\n",jprint(params,0));
        amount = jdouble(jitem(params,0),0) * COIN + 0.0000000049;
    } else amount = COIN;
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
    inputsum = amount + 2*txfee;
    mtx.vout.push_back(MakeCC1vout(cp->evalcode,txfee,sudokupk));
    mtx.vout.push_back(MakeCC1vout(cp->evalcode,inputsum - 2*txfee,pk));
    rawtx = FinalizeCCTx(0,cp,mtx,sudokupk,txfee,sudoku_genopret(unsolved));
    result.push_back(Pair("srand",(int)srandi));
    result.push_back(Pair("amount",ValueFromAmount(amount)));
    result.push_back(Pair("hex",rawtx));
    return(result);
}

UniValue sudoku_solution(uint64_t txfee,struct CCcontract_info *cp,cJSON *params)
{
    UniValue result(UniValue::VOBJ);
    if ( params != 0 )
        printf("params.(%s)\n",jprint(params,0));
    result.push_back(Pair("result","success"));
    result.push_back(Pair("name","sudoku"));
    result.push_back(Pair("method","solution"));
    return(result);
}

UniValue sudoku_pending(uint64_t txfee,struct CCcontract_info *cp,cJSON *params)
{
    UniValue result(UniValue::VOBJ);
    if ( params != 0 )
        printf("params.(%s)\n",jprint(params,0));
    result.push_back(Pair("result","success"));
    result.push_back(Pair("name","sudoku"));
    result.push_back(Pair("method","pending"));
    return(result);
}

