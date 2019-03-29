
#include "tetris.h"

/*
 In order to port a game into gamesCC, the RNG needs to be seeded with the gametxid seed, also events needs to be broadcast using issue_games_events. Also the game engine needs to be daemonized, preferably by putting all globals into a single data structure.
 
 also, the standalone game needs to support argv of seed gametxid, along with replay args
 */

int random_tetromino(struct games_state *rs)
{
    rs->seed = _games_rngnext(rs->seed);
    return(rs->seed % NUM_TETROMINOS);
}

int32_t tetrisdata(struct games_player *P,void *ptr)
{
    tetris_game *tg = (tetris_game *)ptr;
    P->gold = tg->points;
    P->dungeonlevel = tg->level;
    //fprintf(stderr,"score.%d level.%d\n",tg->points,tg->level);
    return(0);
}

/***************************************************************************/
/** https://github.com/brenns10/tetris
 @file         main.c
 @author       Stephen Brennan
 @date         Created Wednesday, 10 June 2015
 @brief        Main program for tetris.
 @copyright    Copyright (c) 2015, Stephen Brennan.  Released under the Revised
 BSD License.  See LICENSE.txt for details.
 *******************************************************************************/


#include <stdio.h> // for FILE
#include <stdbool.h> // for bool
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <string.h>

#ifdef BUILD_GAMESCC
#include "../rogue/cursesd.h"
#else
#include <curses.h>
#endif


#define MAX(X,Y) ((X) > (Y) ? (X) : (Y))
#define MIN(X,Y) ((X) < (Y) ? (X) : (Y))

/*******************************************************************************
 Array Definitions
 *******************************************************************************/

const tetris_location TETROMINOS[NUM_TETROMINOS][NUM_ORIENTATIONS][TETRIS] =
{
    // I
    {{{1, 0}, {1, 1}, {1, 2}, {1, 3}},
        {{0, 2}, {1, 2}, {2, 2}, {3, 2}},
        {{3, 0}, {3, 1}, {3, 2}, {3, 3}},
        {{0, 1}, {1, 1}, {2, 1}, {3, 1}}},
    // J
    {{{0, 0}, {1, 0}, {1, 1}, {1, 2}},
        {{0, 1}, {0, 2}, {1, 1}, {2, 1}},
        {{1, 0}, {1, 1}, {1, 2}, {2, 2}},
        {{0, 1}, {1, 1}, {2, 0}, {2, 1}}},
    // L
    {{{0, 2}, {1, 0}, {1, 1}, {1, 2}},
        {{0, 1}, {1, 1}, {2, 1}, {2, 2}},
        {{1, 0}, {1, 1}, {1, 2}, {2, 0}},
        {{0, 0}, {0, 1}, {1, 1}, {2, 1}}},
    // O
    {{{0, 1}, {0, 2}, {1, 1}, {1, 2}},
        {{0, 1}, {0, 2}, {1, 1}, {1, 2}},
        {{0, 1}, {0, 2}, {1, 1}, {1, 2}},
        {{0, 1}, {0, 2}, {1, 1}, {1, 2}}},
    // S
    {{{0, 1}, {0, 2}, {1, 0}, {1, 1}},
        {{0, 1}, {1, 1}, {1, 2}, {2, 2}},
        {{1, 1}, {1, 2}, {2, 0}, {2, 1}},
        {{0, 0}, {1, 0}, {1, 1}, {2, 1}}},
    // T
    {{{0, 1}, {1, 0}, {1, 1}, {1, 2}},
        {{0, 1}, {1, 1}, {1, 2}, {2, 1}},
        {{1, 0}, {1, 1}, {1, 2}, {2, 1}},
        {{0, 1}, {1, 0}, {1, 1}, {2, 1}}},
    // Z
    {{{0, 0}, {0, 1}, {1, 1}, {1, 2}},
        {{0, 2}, {1, 1}, {1, 2}, {2, 1}},
        {{1, 0}, {1, 1}, {2, 1}, {2, 2}},
        {{0, 1}, {1, 0}, {1, 1}, {2, 0}}},
};

const int GRAVITY_LEVEL[MAX_LEVEL+1] = {
    // 0,  1,  2,  3,  4,  5,  6,  7,  8,  9,
    50, 48, 46, 44, 42, 40, 38, 36, 34, 32,
    //10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
    30, 28, 26, 24, 22, 20, 16, 12,  8,  4
};

/*******************************************************************************
 Helper Functions for Blocks
 *******************************************************************************/

void sleep_milli(int milliseconds)
{
    struct timespec ts;
    ts.tv_sec = 0;
    ts.tv_nsec = milliseconds * 1000 * 1000;
    nanosleep(&ts, NULL);
}

/*
 Return the block at the given row and column.
 */
char tg_get(tetris_game *obj, int row, int column)
{
    return obj->board[obj->cols * row + column];
}

/*
 Set the block at the given row and column.
 */
static void tg_set(tetris_game *obj, int row, int column, char value)
{
    obj->board[obj->cols * row + column] = value;
}

/*
 Check whether a row and column are in bounds.
 */
bool tg_check(tetris_game *obj, int row, int col)
{
    return 0 <= row && row < obj->rows && 0 <= col && col < obj->cols;
}

/*
 Place a block onto the board.
 */
static void tg_put(tetris_game *obj, tetris_block block)
{
    int i;
    for (i = 0; i < TETRIS; i++) {
        tetris_location cell = TETROMINOS[block.typ][block.ori][i];
        tg_set(obj, block.loc.row + cell.row, block.loc.col + cell.col,
               TYPE_TO_CELL(block.typ));
    }
}

/*
 Clear a block out of the board.
 */
static void tg_remove(tetris_game *obj, tetris_block block)
{
    int i;
    for (i = 0; i < TETRIS; i++) {
        tetris_location cell = TETROMINOS[block.typ][block.ori][i];
        tg_set(obj, block.loc.row + cell.row, block.loc.col + cell.col, TC_EMPTY);
    }
}

/*
 Check if a block can be placed on the board.
 */
static bool tg_fits(tetris_game *obj, tetris_block block)
{
    int i, r, c;
    for (i = 0; i < TETRIS; i++) {
        tetris_location cell = TETROMINOS[block.typ][block.ori][i];
        r = block.loc.row + cell.row;
        c = block.loc.col + cell.col;
        if (!tg_check(obj, r, c) || TC_IS_FILLED(tg_get(obj, r, c))) {
            return false;
        }
    }
    return true;
}

/*
 Create a new falling block and populate the next falling block with a random
 one.
 */
static void tg_new_falling(struct games_state *rs,tetris_game *obj)
{
    // Put in a new falling tetromino.
    obj->falling = obj->next;
    obj->next.typ = random_tetromino(rs);
    obj->next.ori = 0;
    obj->next.loc.row = 0;
    obj->next.loc.col = obj->cols/2 - 2;
}

/*******************************************************************************
 Game Turn Helpers
 *******************************************************************************/

/*
 Tick gravity, and move the block down if gravity should act.
 */
static void tg_do_gravity_tick(struct games_state *rs,tetris_game *obj)
{
    obj->ticks_till_gravity--;
    if (obj->ticks_till_gravity <= 0) {
        tg_remove(obj, obj->falling);
        obj->falling.loc.row++;
        if (tg_fits(obj, obj->falling)) {
            obj->ticks_till_gravity = GRAVITY_LEVEL[obj->level];
        } else {
            obj->falling.loc.row--;
            tg_put(obj, obj->falling);
            
            tg_new_falling(rs,obj);
        }
        tg_put(obj, obj->falling);
    }
}

/*
 Move the falling tetris block left (-1) or right (+1).
 */
static void tg_move(tetris_game *obj, int direction)
{
    tg_remove(obj, obj->falling);
    obj->falling.loc.col += direction;
    if (!tg_fits(obj, obj->falling)) {
        obj->falling.loc.col -= direction;
    }
    tg_put(obj, obj->falling);
}

/*
 Send the falling tetris block to the bottom.
 */
static void tg_down(struct games_state *rs,tetris_game *obj)
{
    tg_remove(obj, obj->falling);
    while (tg_fits(obj, obj->falling)) {
        obj->falling.loc.row++;
    }
    obj->falling.loc.row--;
    tg_put(obj, obj->falling);
    tg_new_falling(rs,obj);
}

/*
 Rotate the falling block in either direction (+/-1).
 */
static void tg_rotate(tetris_game *obj, int direction)
{
    tg_remove(obj, obj->falling);
    
    while (true) {
        obj->falling.ori = (obj->falling.ori + direction) % NUM_ORIENTATIONS;
        
        // If the new orientation fits, we're done.
        if (tg_fits(obj, obj->falling))
            break;
        
        // Otherwise, try moving left to make it fit.
        obj->falling.loc.col--;
        if (tg_fits(obj, obj->falling))
            break;
        
        // Finally, try moving right to make it fit.
        obj->falling.loc.col += 2;
        if (tg_fits(obj, obj->falling))
            break;
        
        // Put it back in its original location and try the next orientation.
        obj->falling.loc.col--;
        // Worst case, we come back to the original orientation and it fits, so this
        // loop will terminate.
    }
    
    tg_put(obj, obj->falling);
}

/*
 Swap the falling block with the block in the hold buffer.
 */
static void tg_hold(struct games_state *rs,tetris_game *obj)
{
    tg_remove(obj, obj->falling);
    if (obj->stored.typ == -1) {
        obj->stored = obj->falling;
        tg_new_falling(rs,obj);
    } else {
        int typ = obj->falling.typ, ori = obj->falling.ori;
        obj->falling.typ = obj->stored.typ;
        obj->falling.ori = obj->stored.ori;
        obj->stored.typ = typ;
        obj->stored.ori = ori;
        while (!tg_fits(obj, obj->falling)) {
            obj->falling.loc.row--;
            if (tg_fits(obj, obj->falling)) {
                break;
            }
            obj->falling.loc.col--;
            if (tg_fits(obj, obj->falling)) {
                break;
            }
            obj->falling.loc.col += 2;
        }
    }
    tg_put(obj, obj->falling);
}

/*
 Perform the action specified by the move.
 */
static void tg_handle_move(struct games_state *rs,tetris_game *obj, tetris_move move)
{
    switch (move) {
        case TM_LEFT:
            //fprintf(stderr,"LEFT ");
            tg_move(obj, -1);
            break;
        case TM_RIGHT:
            //fprintf(stderr,"RIGHT ");
            tg_move(obj, 1);
            break;
        case TM_DROP:
            tg_down(rs,obj);
            break;
        case TM_CLOCK:
            tg_rotate(obj, 1);
            break;
        case TM_COUNTER:
            tg_rotate(obj, -1);
            break;
        case TM_HOLD:
            tg_hold(rs,obj);
            break;
        default:
            // pass
            break;
    }
}

/*
 Return true if line i is full.
 */
static bool tg_line_full(tetris_game *obj, int i)
{
    int j;
    for (j = 0; j < obj->cols; j++) {
        if (TC_IS_EMPTY(tg_get(obj, i, j)))
            return false;
    }
    return true;
}

/*
 Shift every row above r down one.
 */
static void tg_shift_lines(tetris_game *obj, int r)
{
    int i, j;
    for (i = r-1; i >= 0; i--) {
        for (j = 0; j < obj->cols; j++) {
            tg_set(obj, i+1, j, tg_get(obj, i, j));
            tg_set(obj, i, j, TC_EMPTY);
        }
    }
}

/*
 Find rows that are filled, remove them, shift, and return the number of
 cleared rows.
 */
static int tg_check_lines(tetris_game *obj)
{
    int i, nlines = 0;
    tg_remove(obj, obj->falling); // don't want to mess up falling block
    
    for (i = obj->rows-1; i >= 0; i--) {
        if (tg_line_full(obj, i)) {
            tg_shift_lines(obj, i);
            i++; // do this line over again since they're shifted
            nlines++;
        }
    }
    
    tg_put(obj, obj->falling); // replace
    return nlines;
}

/*
 Adjust the score for the game, given how many lines were just cleared.
 */
static void tg_adjust_score(tetris_game *obj, int lines_cleared)
{
    static int line_multiplier[] = {0, 40, 100, 300, 1200};
    obj->points += line_multiplier[lines_cleared] * (obj->level + 1);
    if (lines_cleared >= obj->lines_remaining) {
        obj->level = MIN(MAX_LEVEL, obj->level + 1);
        lines_cleared -= obj->lines_remaining;
        obj->lines_remaining = LINES_PER_LEVEL - lines_cleared;
    } else {
        obj->lines_remaining -= lines_cleared;
    }
}

/*
 Return true if the game is over.
 */
static bool tg_game_over(tetris_game *obj)
{
    int i, j;
    bool over = false;
    tg_remove(obj, obj->falling);
    for (i = 0; i < 2; i++) {
        for (j = 0; j < obj->cols; j++) {
            if (TC_IS_FILLED(tg_get(obj, i, j))) {
                over = true;
            }
        }
    }
    tg_put(obj, obj->falling);
    return over;
}

/*******************************************************************************
 Main Public Functions
 *******************************************************************************/

/*
 Do a single game tick: process gravity, user input, and score.  Return true if
 the game is still running, false if it is over.
 */
bool tg_tick(struct games_state *rs,tetris_game *obj, tetris_move move)
{
    int lines_cleared;
    // Handle gravity.
    tg_do_gravity_tick(rs,obj);
    
    // Handle input.
    tg_handle_move(rs,obj, move);
    
    // Check for cleared lines
    lines_cleared = tg_check_lines(obj);
    
    tg_adjust_score(obj, lines_cleared);
    
    // Return whether the game will continue (NOT whether it's over)
    return !tg_game_over(obj);
}

void tg_init(struct games_state *rs,tetris_game *obj, int rows, int cols)
{
    // Initialization logic
    obj->rows = rows;
    obj->cols = cols;
    //obj->board = (char *)malloc(rows * cols);
    memset(obj->board, TC_EMPTY, rows * cols);
    obj->points = 0;
    obj->level = 0;
    obj->ticks_till_gravity = GRAVITY_LEVEL[obj->level];
    obj->lines_remaining = LINES_PER_LEVEL;
    //srand(time(NULL));
    tg_new_falling(rs,obj);
    tg_new_falling(rs,obj);
    obj->stored.typ = -1;
    obj->stored.ori = 0;
    obj->stored.loc.row = 0;
    obj->next.loc.col = obj->cols/2 - 2;
    //printf("%d", obj->falling.loc.col);
}

tetris_game *tg_create(struct games_state *rs,int rows, int cols)
{
    tetris_game *obj = (tetris_game *)malloc(sizeof(tetris_game) + rows*cols);
    tg_init(rs,obj, rows, cols);
    return obj;
}

/*void tg_destroy(tetris_game *obj)
{
    // Cleanup logic
    free(obj->board);
}*/

void tg_delete(tetris_game *obj) {
    //tg_destroy(obj);
    free(obj);
}

/*
 Load a game from a file.
 
tetris_game *tg_load(FILE *f)
{
    tetris_game *obj = (tetris_game *)malloc(sizeof(tetris_game));
    if (fread(obj, sizeof(tetris_game), 1, f) != 1 )
    {
        fprintf(stderr,"read game error\n");
        free(obj);
        obj = 0;
    }
    else
    {
        obj->board = (char *)malloc(obj->rows * obj->cols);
        if (fread(obj->board, sizeof(char), obj->rows * obj->cols, f) != obj->rows * obj->cols )
        {
            fprintf(stderr,"fread error\n");
            free(obj->board);
            free(obj);
            obj = 0;
        }
    }
    return obj;
}*/

/*
 Save a game to a file.
 
void tg_save(tetris_game *obj, FILE *f)
{
    if (fwrite(obj, sizeof(tetris_game), 1, f) != 1 )
        fprintf(stderr,"error writing tetrisgame\n");
    else if (fwrite(obj->board, sizeof(char), obj->rows * obj->cols, f) != obj->rows * obj->cols )
        fprintf(stderr,"error writing board\n");
}*/

/*
 Print a game board to a file.  Really just for early debugging.
 */
void tg_print(tetris_game *obj, FILE *f) {
    int i, j;
    for (i = 0; i < obj->rows; i++) {
        for (j = 0; j < obj->cols; j++) {
            if (TC_IS_EMPTY(tg_get(obj, i, j))) {
                fputs(TC_EMPTY_STR, f);
            } else {
                fputs(TC_BLOCK_STR, f);
            }
        }
        fputc('\n', f);
    }
}

/*
 2 columns per cell makes the game much nicer.
 */
#define COLS_PER_CELL 2
/*
 Macro to print a cell of a specific type to a window.
 */
#define ADD_BLOCK(w,x) waddch((w),' '|A_REVERSE|COLOR_PAIR(x));     \
waddch((w),' '|A_REVERSE|COLOR_PAIR(x))
#define ADD_EMPTY(w) waddch((w), ' '); waddch((w), ' ')

/*
 Print the tetris board onto the ncurses window.
 */
void display_board(WINDOW *w, tetris_game *obj)
{
    int i, j;
    box(w, 0, 0);
    for (i = 0; i < obj->rows; i++) {
        wmove(w, 1 + i, 1);
        for (j = 0; j < obj->cols; j++) {
            if (TC_IS_FILLED(tg_get(obj, i, j))) {
                ADD_BLOCK(w,tg_get(obj, i, j));
            } else {
                ADD_EMPTY(w);
            }
        }
    }
    wnoutrefresh(w);
}

/*
 Display a tetris piece in a dedicated window.
 */
void display_piece(WINDOW *w, tetris_block block)
{
    int b;
    tetris_location c;
    wclear(w);
    box(w, 0, 0);
    if (block.typ == -1) {
        wnoutrefresh(w);
        return;
    }
    for (b = 0; b < TETRIS; b++) {
        c = TETROMINOS[block.typ][block.ori][b];
        wmove(w, c.row + 1, c.col * COLS_PER_CELL + 1);
        ADD_BLOCK(w, TYPE_TO_CELL(block.typ));
    }
    wnoutrefresh(w);
}

/*
 Display score information in a dedicated window.
 */
void display_score(WINDOW *w, tetris_game *tg)
{
    wclear(w);
    box(w, 0, 0);
    wprintw(w, (char *)"Score\n%d\n", tg->points);
    wprintw(w, (char *)"Level\n%d\n", tg->level);
    wprintw(w, (char *)"Lines\n%d\n", tg->lines_remaining);
    wnoutrefresh(w);
}

/*
 Save and exit the game.
 
void save(tetris_game *game, WINDOW *w)
{
    FILE *f;
    
    wclear(w);
    box(w, 0, 0); // return the border
    wmove(w, 1, 1);
    wprintw(w, (char *)"Save and exit? [Y/n] ");
    wrefresh(w);
    timeout(-1);
    if (getch() == 'n') {
        timeout(0);
        return;
    }
    f = fopen("tetris.save", "w");
    tg_save(game, f);
    fclose(f);
    tg_delete(game);
    endwin();
    fprintf(stderr,"Game saved to \"tetris.save\".\n");
    fprintf(stderr,"Resume by passing the filename as an argument to this program.\n");
    exit(EXIT_SUCCESS);
}*/

/*
 Do the NCURSES initialization steps for color blocks.
 */
void init_colors(void)
{
    start_color();
    //init_color(COLOR_ORANGE, 1000, 647, 0);
    init_pair(TC_CELLI, COLOR_CYAN, COLOR_BLACK);
    init_pair(TC_CELLJ, COLOR_BLUE, COLOR_BLACK);
    init_pair(TC_CELLL, COLOR_WHITE, COLOR_BLACK);
    init_pair(TC_CELLO, COLOR_YELLOW, COLOR_BLACK);
    init_pair(TC_CELLS, COLOR_GREEN, COLOR_BLACK);
    init_pair(TC_CELLT, COLOR_MAGENTA, COLOR_BLACK);
    init_pair(TC_CELLZ, COLOR_RED, COLOR_BLACK);
}

struct games_state globalR;
extern char Gametxidstr[];
int32_t issue_games_events(struct games_state *rs,char *gametxidstr,uint32_t eventid,gamesevent c);
gamesevent games_readevent(struct games_state *rs);

void *gamesiterate(struct games_state *rs)
{
    uint32_t counter = 0; bool running = true; tetris_move move = TM_NONE;
    gamesevent c; uint16_t skipcount=0; int32_t prevlevel; uint32_t eventid = 0; tetris_game *tg;
    WINDOW *board, *next, *hold, *score;
    if ( rs->guiflag != 0 || rs->sleeptime != 0 )
    {
        // NCURSES initialization:
        initscr();             // initialize curses
        cbreak();              // pass key presses to program, but not signals
        noecho();              // don't echo key presses to screen
        keypad(stdscr, TRUE);  // allow arrow keys
        timeout(0);            // no blocking on getch()
        curs_set(0);           // set the cursor to invisible
        init_colors();         // setup tetris colors
    }
    tg = tg_create(rs,22, 10);
    prevlevel = tg->level;
    // Create windows for each section of the interface.
    board = newwin(tg->rows + 2, 2 * tg->cols + 2, 0, 0);
    next  = newwin(6, 10, 0, 2 * (tg->cols + 1) + 1);
    hold  = newwin(6, 10, 7, 2 * (tg->cols + 1) + 1);
    score = newwin(6, 10, 14, 2 * (tg->cols + 1 ) + 1);
    while ( running != 0 )
    {
        running = tg_tick(rs,tg,move);
        if ( 1 && (rs->guiflag != 0 || rs->sleeptime != 0) )
        {
            display_board(board,tg);
            display_piece(next,tg->next);
            display_piece(hold,tg->stored);
            display_score(score,tg);
        }
        if ( rs->guiflag != 0 )
        {
#ifdef STANDALONE
            sleep_milli(15);
            if ( (counter++ % 10) == 0 )
                doupdate();
            c = games_readevent(rs);
            if ( c <= 0x7f || skipcount == 0x3fff )
            {
                if ( skipcount > 0 )
                    issue_games_events(rs,Gametxidstr,eventid-skipcount,skipcount | 0x4000);
                if ( c <= 0x7f )
                    issue_games_events(rs,Gametxidstr,eventid,c);
                if ( tg->level != prevlevel )
                {
                    flushkeystrokes(rs,0);
                    prevlevel = tg->level;
                }
                skipcount = 0;
            } else skipcount++;
#endif
        }
        else
        {
            if ( rs->replaydone != 0 )
                break;
            if ( rs->sleeptime != 0 )
            {
                sleep_milli(1);
                if ( (counter++ % 20) == 0 )
                    doupdate();
            }
            if ( skipcount == 0 )
            {
                c = games_readevent(rs);
                //fprintf(stderr,"%04x score.%d level.%d\n",c,tg->points,tg->level);
                if ( (c & 0x4000) == 0x4000 )
                {
                    skipcount = (c & 0x3fff);
                    c = 'S';
                }
            }
            if ( skipcount > 0 )
                skipcount--;
        }
        eventid++;
        switch ( c )
        {
            case 'h':
                move = TM_LEFT;
                break;
            case 'l':
                move = TM_RIGHT;
                break;
            case 'k':
                move = TM_CLOCK;
                break;
            case 'j':
                move = TM_DROP;
                break;
            case 'q':
                running = false;
                move = TM_NONE;
                break;
                /*case 'p':
                 wclear(board);
                 box(board, 0, 0);
                 wmove(board, tg->rows/2, (tg->cols*COLS_PER_CELL-6)/2);
                 wprintw(board, "PAUSED");
                 wrefresh(board);
                 timeout(-1);
                 getch();
                 timeout(0);
                 move = TM_NONE;
                 break;
                 case 's':
                 save(tg, board);
                 move = TM_NONE;
                 break;*/
            case ' ':
                move = TM_HOLD;
                break;
            default:
                move = TM_NONE;
        }
    }
    return(tg);
}

#ifdef STANDALONE
/*
 Main tetris game!
 */
#include "dapps/dappstd.c"


char *clonestr(char *str)
{
    char *clone; int32_t len;
    if ( str == 0 || str[0] == 0 )
    {
        printf("warning cloning nullstr.%p\n",str);
#ifdef __APPLE__
        while ( 1 ) sleep(1);
#endif
        str = (char *)"<nullstr>";
    }
    len = strlen(str);
    clone = (char *)calloc(1,len+16);
    strcpy(clone,str);
    return(clone);
}

int32_t issue_games_events(struct games_state *rs,char *gametxidstr,uint32_t eventid,gamesevent c)
{
    static FILE *fp;
    char params[512],*retstr; cJSON *retjson,*resobj; int32_t retval = -1;
    if ( fp == 0 )
        fp = fopen("events.log","wb");
    rs->buffered[rs->num++] = c;
    if ( 0 )
    {
        if ( sizeof(c) == 1 )
            sprintf(params,"[\"events\",\"17\",\"[%%22%02x%%22,%%22%s%%22,%u]\"]",(uint8_t)c&0xff,gametxidstr,eventid);
        else if ( sizeof(c) == 2 )
            sprintf(params,"[\"events\",\"17\",\"[%%22%04x%%22,%%22%s%%22,%u]\"]",(uint16_t)c&0xffff,gametxidstr,eventid);
        else if ( sizeof(c) == 4 )
            sprintf(params,"[\"events\",\"17\",\"[%%22%08x%%22,%%22%s%%22,%u]\"]",(uint32_t)c&0xffffffff,gametxidstr,eventid);
        else if ( sizeof(c) == 8 )
            sprintf(params,"[\"events\",\"17\",\"[%%22%016llx%%22,%%22%s%%22,%u]\"]",(long long)c,gametxidstr,eventid);
        if ( (retstr= komodo_issuemethod(USERPASS,(char *)"cclib",params,GAMES_PORT)) != 0 )
        {
            if ( (retjson= cJSON_Parse(retstr)) != 0 )
            {
                if ( (resobj= jobj(retjson,(char *)"result")) != 0 )
                {
                    retval = 0;
                    if ( fp != 0 )
                    {
                        fprintf(fp,"%s\n",jprint(resobj,0));
                        fflush(fp);
                    }
                }
                free_json(retjson);
            } else fprintf(fp,"error parsing %s\n",retstr);
            free(retstr);
        } else fprintf(fp,"error issuing method %s\n",params);
        return(retval);
    } else return(0);
}

int tetris(int argc, char **argv)
{
    struct games_state *rs = &globalR;
    int32_t c,skipcount=0; uint32_t eventid = 0; tetris_game *tg = 0;
    memset(rs,0,sizeof(*rs));
    rs->guiflag = 1;
    rs->sleeptime = 1; // non-zero to allow refresh()
    if ( argc >= 2 && strlen(argv[2]) == 64 )
    {
#ifdef _WIN32
#ifdef _MSC_VER
        rs->origseed = _strtoui64(argv[1], NULL, 10);
#else
        rs->origseed = atol(argv[1]); // windows, but not MSVC
#endif // _MSC_VER
#else
        rs->origseed = atol(argv[1]); // non-windows
#endif // _WIN32
        rs->seed = rs->origseed;
        if ( argc >= 3 )
        {
            strcpy(Gametxidstr,argv[2]);
            fprintf(stderr,"setplayerdata %s\n",Gametxidstr);
            if ( games_setplayerdata(rs,Gametxidstr) < 0 )
            {
                fprintf(stderr,"invalid gametxid, or already started\n");
                return(-1);
            }
        }
    } else rs->seed = 777;

    /* Load file if given a filename.
    if (argc >= 2) {
        FILE *f = fopen(argv[1], "r");
        if (f == NULL) {
            perror("tetris");
            exit(EXIT_FAILURE);
        }
        tg = tg_load(f);
        fclose(f);
    } else {
        // Otherwise create new game.
        tg = tg_create(rs,22, 10);
    }*/

    // Game loop
    tg = (tetris_game *)gamesiterate(rs);
    gamesbailout(rs);
    // Deinitialize NCurses
    wclear(stdscr);
    endwin();
    // Output ending message.
    printf("Game over!\n");
    printf("You finished with %d points on level %d.\n", tg->points, tg->level);
    
    // Deinitialize Tetris
    tg_delete(tg);
    return 0;
}

#endif

