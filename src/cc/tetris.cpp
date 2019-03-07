
/***************************************************************************/
/** https://github.com/brenns10/tetris
  @file         main.c
  @author       Stephen Brennan
  @date         Created Wednesday, 10 June 2015
  @brief        Main program for tetris.
  @copyright    Copyright (c) 2015, Stephen Brennan.  Released under the Revised
  BSD License.  See LICENSE.txt for details.
  *******************************************************************************/


#ifndef TETRIS_H
#define TETRIS_H

#include <stdio.h> // for FILE
#include <stdbool.h> // for bool
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>

#include <ncurses.h>
#include <string.h>

//#include <SDL/SDL.h>
//#include <SDL/SDL_mixer.h>


/*
 Convert a tetromino type to its corresponding cell.
 */
#define TYPE_TO_CELL(x) ((x)+1)

/*
 Strings for how you would print a tetris board.
 */
#define TC_EMPTY_STR " "
#define TC_BLOCK_STR "\u2588"

/*
 Questions about a tetris cell.
 */
#define TC_IS_EMPTY(x) ((x) == TC_EMPTY)
#define TC_IS_FILLED(x) (!TC_IS_EMPTY(x))

/*
 How many cells in a tetromino?
 */
#define TETRIS 4
/*
 How many tetrominos?
 */
#define NUM_TETROMINOS 7
/*
 How many orientations of a tetromino?
 */
#define NUM_ORIENTATIONS 4

/*
 Level constants.
 */
#define MAX_LEVEL 19
#define LINES_PER_LEVEL 10

/*
 A "cell" is a 1x1 block within a tetris board.
 */
typedef enum {
    TC_EMPTY, TC_CELLI, TC_CELLJ, TC_CELLL, TC_CELLO, TC_CELLS, TC_CELLT, TC_CELLZ
} tetris_cell;

/*
 A "type" is a type/shape of a tetromino.  Not including orientation.
 */
typedef enum {
    TET_I, TET_J, TET_L, TET_O, TET_S, TET_T, TET_Z
} tetris_type;

/*
 A row,column pair.  Negative numbers allowed, because we need them for
 offsets.
 */
typedef struct {
    int row;
    int col;
} tetris_location;

/*
 A "block" is a struct that contains information about a tetromino.
 Specifically, what type it is, what orientation it has, and where it is.
 */
typedef struct {
    int typ;
    int ori;
    tetris_location loc;
} tetris_block;

/*
 All possible moves to give as input to the game.
 */
typedef enum {
    TM_LEFT, TM_RIGHT, TM_CLOCK, TM_COUNTER, TM_DROP, TM_HOLD, TM_NONE
} tetris_move;

/*
 A game object!
 */
typedef struct {
    /*
     Game board stuff:
     */
    int rows;
    int cols;
    char *board;
    /*
     Scoring information:
     */
    int points;
    int level;
    /*
     Falling block is the one currently going down.  Next block is the one that
     will be falling after this one.  Stored is the block that you can swap out.
     */
    tetris_block falling;
    tetris_block next;
    tetris_block stored;
    /*
     Number of game ticks until the block will move down.
     */
    int ticks_till_gravity;
    /*
     Number of lines until you advance to the next level.
     */
    int lines_remaining;
} tetris_game;

/*
 This array stores all necessary information about the cells that are filled by
 each tetromino.  The first index is the type of the tetromino (i.e. shape,
 e.g. I, J, Z, etc.).  The next index is the orientation (0-3).  The final
 array contains 4 tetris_location objects, each mapping to an offset from a
 point on the upper left that is the tetromino "origin".
 */
extern tetris_location TETROMINOS[NUM_TETROMINOS][NUM_ORIENTATIONS][TETRIS];

/*
 This array tells you how many ticks per gravity by level.  Decreases as level
 increases, to add difficulty.
 */
extern int GRAVITY_LEVEL[MAX_LEVEL+1];

// Data structure manipulation.
void tg_init(tetris_game *obj, int rows, int cols);
tetris_game *tg_create(int rows, int cols);
void tg_destroy(tetris_game *obj);
void tg_delete(tetris_game *obj);
tetris_game *tg_load(FILE *f);
void tg_save(tetris_game *obj, FILE *f);

// Public methods not related to memory:
char tg_get(tetris_game *obj, int row, int col);
bool tg_check(tetris_game *obj, int row, int col);
bool tg_tick(tetris_game *obj, tetris_move move);
void tg_print(tetris_game *obj, FILE *f);

#endif // TETRIS_H


#define MAX(X,Y) ((X) > (Y) ? (X) : (Y))
#define MIN(X,Y) ((X) < (Y) ? (X) : (Y))

/*******************************************************************************
 Array Definitions
 *******************************************************************************/

tetris_location TETROMINOS[NUM_TETROMINOS][NUM_ORIENTATIONS][TETRIS] = {
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

int GRAVITY_LEVEL[MAX_LEVEL+1] = {
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
 Return a random tetromino type.
 */
static int random_tetromino(void) {
    return rand() % NUM_TETROMINOS;
}

/*
 Create a new falling block and populate the next falling block with a random
 one.
 */
static void tg_new_falling(tetris_game *obj)
{
    // Put in a new falling tetromino.
    obj->falling = obj->next;
    obj->next.typ = random_tetromino();
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
static void tg_do_gravity_tick(tetris_game *obj)
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
            
            tg_new_falling(obj);
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
static void tg_down(tetris_game *obj)
{
    tg_remove(obj, obj->falling);
    while (tg_fits(obj, obj->falling)) {
        obj->falling.loc.row++;
    }
    obj->falling.loc.row--;
    tg_put(obj, obj->falling);
    tg_new_falling(obj);
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
static void tg_hold(tetris_game *obj)
{
    tg_remove(obj, obj->falling);
    if (obj->stored.typ == -1) {
        obj->stored = obj->falling;
        tg_new_falling(obj);
    } else {
        int typ = obj->falling.typ, ori = obj->falling.ori;
        obj->falling.typ = obj->stored.typ;
        obj->falling.ori = obj->stored.ori;
        obj->stored.typ = typ;
        obj->stored.ori = ori;
        while (!tg_fits(obj, obj->falling)) {
            obj->falling.loc.row--;
        }
    }
    tg_put(obj, obj->falling);
}

/*
 Perform the action specified by the move.
 */
static void tg_handle_move(tetris_game *obj, tetris_move move)
{
    switch (move) {
        case TM_LEFT:
            tg_move(obj, -1);
            break;
        case TM_RIGHT:
            tg_move(obj, 1);
            break;
        case TM_DROP:
            tg_down(obj);
            break;
        case TM_CLOCK:
            tg_rotate(obj, 1);
            break;
        case TM_COUNTER:
            tg_rotate(obj, -1);
            break;
        case TM_HOLD:
            tg_hold(obj);
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
bool tg_tick(tetris_game *obj, tetris_move move)
{
    int lines_cleared;
    // Handle gravity.
    tg_do_gravity_tick(obj);
    
    // Handle input.
    tg_handle_move(obj, move);
    
    // Check for cleared lines
    lines_cleared = tg_check_lines(obj);
    
    tg_adjust_score(obj, lines_cleared);
    
    // Return whether the game will continue (NOT whether it's over)
    return !tg_game_over(obj);
}

void tg_init(tetris_game *obj, int rows, int cols)
{
    // Initialization logic
    obj->rows = rows;
    obj->cols = cols;
    obj->board = (char *)malloc(rows * cols);
    memset(obj->board, TC_EMPTY, rows * cols);
    obj->points = 0;
    obj->level = 0;
    obj->ticks_till_gravity = GRAVITY_LEVEL[obj->level];
    obj->lines_remaining = LINES_PER_LEVEL;
    srand(time(NULL));
    tg_new_falling(obj);
    tg_new_falling(obj);
    obj->stored.typ = -1;
    obj->stored.ori = 0;
    obj->stored.loc.row = 0;
    obj->next.loc.col = obj->cols/2 - 2;
    printf("%d", obj->falling.loc.col);
}

tetris_game *tg_create(int rows, int cols)
{
    tetris_game *obj = (tetris_game *)malloc(sizeof(tetris_game));
    tg_init(obj, rows, cols);
    return obj;
}

void tg_destroy(tetris_game *obj)
{
    // Cleanup logic
    free(obj->board);
}

void tg_delete(tetris_game *obj) {
    tg_destroy(obj);
    free(obj);
}

/*
 Load a game from a file.
 */
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
}

/*
 Save a game to a file.
 */
void tg_save(tetris_game *obj, FILE *f)
{
    if (fwrite(obj, sizeof(tetris_game), 1, f) != 1 )
        fprintf(stderr,"error writing tetrisgame\n");
    else if (fwrite(obj->board, sizeof(char), obj->rows * obj->cols, f) != obj->rows * obj->cols )
        fprintf(stderr,"error writing board\n");
}

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
    wprintw(w, "Score\n%d\n", tg->points);
    wprintw(w, "Level\n%d\n", tg->level);
    wprintw(w, "Lines\n%d\n", tg->lines_remaining);
    wnoutrefresh(w);
}

/*
 Boss mode!  Make it look like you're doing work.
 */
void boss_mode(void)
{
    clear();
    //Mix_PauseMusic();
    printw("user@workstation-312:~/Documents/presentation $ ls -l\n"
           "total 528\n"
           "drwxr-xr-x 2 user users   4096 Jun  9 17:05 .\n"
           "drwxr-xr-x 4 user users   4096 Jun 10 09:52 ..\n"
           "-rw-r--r-- 1 user users  88583 Jun  9 14:13 figure1.png\n"
           "-rw-r--r-- 1 user users  65357 Jun  9 15:40 figure2.png\n"
           "-rw-r--r-- 1 user users   4469 Jun  9 16:17 presentation.aux\n"
           "-rw-r--r-- 1 user users  42858 Jun  9 16:17 presentation.log\n"
           "-rw-r--r-- 1 user users   2516 Jun  9 16:17 presentation.nav\n"
           "-rw-r--r-- 1 user users    183 Jun  9 16:17 presentation.out\n"
           "-rw-r--r-- 1 user users 349607 Jun  9 16:17 presentation.pdf\n"
           "-rw-r--r-- 1 user users      0 Jun  9 16:17 presentation.snm\n"
           "-rw-r--r-- 1 user users   9284 Jun  9 17:05 presentation.tex\n"
           "-rw-r--r-- 1 user users    229 Jun  9 16:17 presentation.toc\n"
           "\n"
           "user@workstation-312:~/Documents/presentation $ ");
    echo();
    timeout(-1);
    while (getch() != KEY_F(1));
    timeout(0);
    noecho();
    clear();
    //Mix_ResumeMusic();
}

/*
 Save and exit the game.
 */
void save(tetris_game *game, WINDOW *w)
{
    FILE *f;
    
    wclear(w);
    box(w, 0, 0); // return the border
    wmove(w, 1, 1);
    wprintw(w, "Save and exit? [Y/n] ");
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
    printf("Game saved to \"tetris.save\".\n");
    printf("Resume by passing the filename as an argument to this program.\n");
    exit(EXIT_SUCCESS);
}

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

/*
 Main tetris game!
 */
#ifdef STANDALONE

int main(int argc, char **argv)
{
    tetris_game *tg;
    tetris_move move = TM_NONE;
    bool running = true;
    WINDOW *board, *next, *hold, *score;
    //Mix_Music *music;
    
    // Load file if given a filename.
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
        tg = tg_create(22, 10);
    }
    
    /* Initialize music.
    if (SDL_Init(SDL_INIT_AUDIO) < 0) {
        fprintf(stderr, "unable to initialize SDL\n");
        exit(EXIT_FAILURE);
    }
    if (Mix_Init(MIX_INIT_MP3) != MIX_INIT_MP3) {
        fprintf(stderr, "unable to initialize SDL_mixer\n");
        exit(EXIT_FAILURE);
    }
    if (Mix_OpenAudio(MIX_DEFAULT_FREQUENCY, MIX_DEFAULT_FORMAT, 2, 1024) != 0) {
        fprintf(stderr, "unable to initialize audio\n");
        exit(EXIT_FAILURE);
    }
    Mix_AllocateChannels(1); // only need background music
    music = Mix_LoadMUS("tetris.mp3");
    if (music) {
        Mix_PlayMusic(music, -1);
    }*/
    
    // NCURSES initialization:
    initscr();             // initialize curses
    cbreak();              // pass key presses to program, but not signals
    noecho();              // don't echo key presses to screen
    keypad(stdscr, TRUE);  // allow arrow keys
    timeout(0);            // no blocking on getch()
    curs_set(0);           // set the cursor to invisible
    init_colors();         // setup tetris colors
    
    // Create windows for each section of the interface.
    board = newwin(tg->rows + 2, 2 * tg->cols + 2, 0, 0);
    next  = newwin(6, 10, 0, 2 * (tg->cols + 1) + 1);
    hold  = newwin(6, 10, 7, 2 * (tg->cols + 1) + 1);
    score = newwin(6, 10, 14, 2 * (tg->cols + 1 ) + 1);
    
    // Game loop
    while (running) {
        running = tg_tick(tg, move);
        display_board(board, tg);
        display_piece(next, tg->next);
        display_piece(hold, tg->stored);
        display_score(score, tg);
        doupdate();
        sleep_milli(10);
        
        switch (getch()) {
            case KEY_LEFT:
                move = TM_LEFT;
                break;
            case KEY_RIGHT:
                move = TM_RIGHT;
                break;
            case KEY_UP:
                move = TM_CLOCK;
                break;
            case KEY_DOWN:
                move = TM_DROP;
                break;
            case 'q':
                running = false;
                move = TM_NONE;
                break;
            case 'p':
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
            case 'b':
                boss_mode();
                move = TM_NONE;
                break;
            case 's':
                save(tg, board);
                move = TM_NONE;
                break;
            case ' ':
                move = TM_HOLD;
                break;
            default:
                move = TM_NONE;
        }
    }
    
    // Deinitialize NCurses
    wclear(stdscr);
    endwin();
    
    /* Deinitialize Sound
    Mix_HaltMusic();
    Mix_FreeMusic(music);
    Mix_CloseAudio();
    Mix_Quit();*/
    
    // Output ending message.
    printf("Game over!\n");
    printf("You finished with %d points on level %d.\n", tg->points, tg->level);
    
    // Deinitialize Tetris
    tg_delete(tg);
    return 0;
}
#else

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
#include "CCinclude.h"

#define TETRIS_REGISTRATION 5
#define TETRIS_REGISTRATIONSIZE (100 * 10000)
#define TETRIS_MAXPLAYERS 64 // need to send unused fees back to globalCC address to prevent leeching
#define TETRIS_MAXKEYSTROKESGAP 60
#define TETRIS_MAXITERATIONS 777


std::string Tetris_pname = "";

CScript tetris_newgameopret(int64_t buyin,int32_t maxplayers)
{
    CScript opret; uint8_t evalcode = EVAL_TETRIS;
    opret << OP_RETURN << E_MARSHAL(ss << evalcode << 'G' << buyin << maxplayers);
    return(opret);
}

CScript tetris_registeropret(uint256 gametxid,uint256 playertxid)
{
    CScript opret; uint8_t evalcode = EVAL_TETRIS;
    //fprintf(stderr,"opret.(%s %s).R\n",gametxid.GetHex().c_str(),playertxid.GetHex().c_str());
    opret << OP_RETURN << E_MARSHAL(ss << evalcode << 'R' << gametxid << playertxid);
    return(opret);
}

CScript tetris_keystrokesopret(uint256 gametxid,uint256 batontxid,CPubKey pk,std::vector<uint8_t>keystrokes)
{
    CScript opret; uint8_t evalcode = EVAL_TETRIS;
    opret << OP_RETURN << E_MARSHAL(ss << evalcode << 'K' << gametxid << batontxid << pk << keystrokes);
    return(opret);
}

CScript tetris_highlanderopret(uint8_t funcid,uint256 gametxid,int32_t regslot,CPubKey pk,std::vector<uint8_t>playerdata,std::string pname)
{
    CScript opret; uint8_t evalcode = EVAL_TETRIS; std::string symbol(ASSETCHAINS_SYMBOL);
    opret << OP_RETURN << E_MARSHAL(ss << evalcode << funcid << gametxid << symbol << pname << regslot << pk << playerdata );
    return(opret);
}

uint8_t tetris_highlanderopretdecode(uint256 &gametxid, uint256 &tokenid, int32_t &regslot, CPubKey &pk, std::vector<uint8_t> &playerdata, std::string &symbol, std::string &pname,CScript scriptPubKey)
{
    std::string name, description; std::vector<uint8_t> vorigPubkey;
    std::vector<std::pair<uint8_t, vscript_t>>  oprets, opretsDummy;
    std::vector<uint8_t> vopretNonfungible, vopret, vopretDummy,origpubkey;
    uint8_t e, f,*script; std::vector<CPubKey> voutPubkeys;
    tokenid = zeroid;
    GetOpReturnData(scriptPubKey, vopret);
    script = (uint8_t *)vopret.data();
    if ( script[1] == 'c' && (f= DecodeTokenCreateOpRet(scriptPubKey,origpubkey,name,description, oprets)) == 'c' )
    {
        GetOpretBlob(oprets, OPRETID_NONFUNGIBLEDATA, vopretNonfungible);
        vopret = vopretNonfungible;
    }
    else if ( script[1] != 'H' && script[1] != 'Q' && (f= DecodeTokenOpRet(scriptPubKey, e, tokenid, voutPubkeys, opretsDummy)) != 0 )
    {
        //fprintf(stderr,"decode opret %c tokenid.%s\n",script[1],tokenid.GetHex().c_str());
        GetNonfungibleData(tokenid, vopretNonfungible);  //load nonfungible data from the 'tokenbase' tx
        vopret = vopretNonfungible;
    }
    if ( vopret.size() > 2 && E_UNMARSHAL(vopret, ss >> e; ss >> f; ss >> gametxid;  ss >> symbol; ss >> pname; ss >> regslot; ss >> pk; ss >> playerdata) != 0 && e == EVAL_TETRIS && (f == 'H' || f == 'Q') )
    {
        return(f);
    }
    fprintf(stderr,"SKIP obsolete: e.%d f.%c game.%s regslot.%d psize.%d\n",e,f,gametxid.GetHex().c_str(),regslot,(int32_t)playerdata.size());
    return(0);
}

uint8_t tetris_keystrokesopretdecode(uint256 &gametxid,uint256 &batontxid,CPubKey &pk,std::vector<uint8_t> &keystrokes,CScript scriptPubKey)
{
    std::vector<uint8_t> vopret; uint8_t e,f;
    GetOpReturnData(scriptPubKey,vopret);
    if ( vopret.size() > 2 && E_UNMARSHAL(vopret,ss >> e; ss >> f; ss >> gametxid; ss >> batontxid; ss >> pk; ss >> keystrokes) != 0 && e == EVAL_TETRIS && f == 'K' )
    {
        return(f);
    }
    return(0);
}

uint8_t tetris_registeropretdecode(uint256 &gametxid,uint256 &tokenid,uint256 &playertxid,CScript scriptPubKey)
{
    std::string name, description; std::vector<uint8_t> vorigPubkey;
    std::vector<std::pair<uint8_t, vscript_t>>  oprets;
    std::vector<uint8_t> vopretNonfungible, vopret, vopretDummy,origpubkey;
    uint8_t e, f,*script; std::vector<CPubKey> voutPubkeys;
    tokenid = zeroid;
    GetOpReturnData(scriptPubKey, vopret);
    script = (uint8_t *)vopret.data();
    if ( script[1] == 'c' && (f= DecodeTokenCreateOpRet(scriptPubKey,origpubkey,name,description,oprets)) == 'c' )
    {
        GetOpretBlob(oprets, OPRETID_NONFUNGIBLEDATA, vopretNonfungible);
        vopret = vopretNonfungible;
    }
    else if ( script[1] != 'R' && (f= DecodeTokenOpRet(scriptPubKey, e, tokenid, voutPubkeys, oprets)) != 0 )
    {
        GetOpretBlob(oprets, OPRETID_TETRISGAMEDATA, vopretDummy);  // blob from non-creation tx opret
        vopret = vopretDummy;
    }
    if ( vopret.size() > 2 && E_UNMARSHAL(vopret,ss >> e; ss >> f; ss >> gametxid; ss >> playertxid) != 0 && e == EVAL_TETRIS && f == 'R' )
    {
        return(f);
    }
    //fprintf(stderr,"e.%d f.%c game.%s playertxid.%s\n",e,f,gametxid.GetHex().c_str(),playertxid.GetHex().c_str());
    return(0);
}

uint8_t tetris_newgameopreturndecode(int64_t &buyin,int32_t &maxplayers,CScript scriptPubKey)
{
    std::vector<uint8_t> vopret; uint8_t e,f;
    GetOpReturnData(scriptPubKey,vopret);
    if ( vopret.size() > 2 && E_UNMARSHAL(vopret,ss >> e; ss >> f; ss >> buyin; ss >> maxplayers) != 0 && e == EVAL_TETRIS && f == 'G' )
    {
        return(f);
    }
    return(0);
}

void tetris_univalue(UniValue &result,const char *method,int64_t maxplayers,int64_t buyin)
{
    if ( method != 0 )
    {
        result.push_back(Pair("name","tetris"));
        result.push_back(Pair("method",method));
    }
    if ( maxplayers > 0 )
        result.push_back(Pair("maxplayers",maxplayers));
    if ( buyin >= 0 )
    {
        result.push_back(Pair("buyin",ValueFromAmount(buyin)));
        if ( buyin == 0 )
            result.push_back(Pair("type","newbie"));
        else result.push_back(Pair("type","buyin"));
    }
}

int32_t tetris_iamregistered(int32_t maxplayers,uint256 gametxid,CTransaction tx,char *mytetrisaddr)
{
    int32_t i,vout; uint256 spenttxid,hashBlock; CTransaction spenttx; char destaddr[64];
    for (i=0; i<maxplayers; i++)
    {
        destaddr[0] = 0;
        vout = i+1;
        if ( myIsutxo_spent(spenttxid,gametxid,vout) >= 0 )
        {
            if ( myGetTransaction(spenttxid,spenttx,hashBlock) != 0 && spenttx.vout.size() > 0 )
            {
                Getscriptaddress(destaddr,spenttx.vout[0].scriptPubKey);
                if ( strcmp(mytetrisaddr,destaddr) == 0 )
                    return(1);
                //else fprintf(stderr,"myaddr.%s vs %s\n",mytetrisaddr,destaddr);
            } //else fprintf(stderr,"cant find spenttxid.%s\n",spenttxid.GetHex().c_str());
        } //else fprintf(stderr,"vout %d is unspent\n",vout);
    }
    return(0);
}

int32_t tetris_isvalidgame(struct CCcontract_info *cp,int32_t &gameheight,CTransaction &tx,int64_t &buyin,int32_t &maxplayers,uint256 txid,int32_t unspentv0)
{
    uint256 hashBlock; int32_t i,numvouts; char coinaddr[64]; CPubKey tetrispk; uint64_t txfee = 10000;
    buyin = maxplayers = 0;
    if ( (txid == zeroid || myGetTransaction(txid,tx,hashBlock) != 0) && (numvouts= tx.vout.size()) > 1 )
    {
        if ( txid != zeroid )
            gameheight = komodo_blockheight(hashBlock);
        else
        {
            txid = tx.GetHash();
            //fprintf(stderr,"set txid %s %llu\n",txid.GetHex().c_str(),(long long)CCgettxout(txid,0,1));
        }
        if ( IsCClibvout(cp,tx,0,cp->unspendableCCaddr) == txfee && (unspentv0 == 0 || CCgettxout(txid,0,1,0) == txfee) )
        {
            if ( tetris_newgameopreturndecode(buyin,maxplayers,tx.vout[numvouts-1].scriptPubKey) == 'G' )
            {
                if ( maxplayers < 1 || maxplayers > TETRIS_MAXPLAYERS || buyin < 0 )
                    return(-6);
                if ( numvouts > 2*maxplayers+1 )
                {
                    for (i=0; i<maxplayers; i++)
                    {
                        if ( tx.vout[i+1].nValue != TETRIS_REGISTRATIONSIZE )
                            break;
                        if ( tx.vout[maxplayers+i+1].nValue != txfee )
                            break;
                    }
                    if ( i == maxplayers )
                    {
                        // dimxy: make sure no vouts to any address other than main CC and 0.0001 faucet
                        return(0);
                    } else return(-5);
                } else return(-4);
            } else return(-3);
        } else return(-2);
    } else return(-1);
}

UniValue tetris_playerobj(std::vector<uint8_t> playerdata,uint256 playertxid,uint256 tokenid,std::string symbol,std::string pname,uint256 gametxid)
{
    int32_t i,vout,spentvini,numvouts,n=0; uint256 txid,spenttxid,hashBlock; struct tetris_player P; char packitemstr[512],*datastr=0; UniValue obj(UniValue::VOBJ),a(UniValue::VARR); CTransaction tx;
    memset(&P,0,sizeof(P));
    if ( playerdata.size() > 0 )
    {
        datastr = (char *)malloc(playerdata.size()*2+1);
        for (i=0; i<playerdata.size(); i++)
        {
            ((uint8_t *)&P)[i] = playerdata[i];
            sprintf(&datastr[i<<1],"%02x",playerdata[i]);
        }
        datastr[i<<1] = 0;
    }
    int32_t gold,hitpoints,strength,level,experience,packsize,dungeonlevel,pad;
    for (i=0; i<P.packsize&&i<MAXPACK; i++)
    {
        tetris_packitemstr(packitemstr,&P.tetrispack[i]);
        a.push_back(packitemstr);
    }
    txid = playertxid;
    vout = 1;
    while ( CCgettxout(txid,vout,1,0) < 0 )
    {
        spenttxid = zeroid;
        spentvini = -1;
        if ( (spentvini= myIsutxo_spent(spenttxid,txid,vout)) >= 0 )
            txid = spenttxid;
        else if ( myIsutxo_spentinmempool(spenttxid,spentvini,txid,vout) == 0 || spenttxid == zeroid )
        {
            fprintf(stderr,"mempool tracking error %s/v0\n",txid.ToString().c_str());
            break;
        }
        txid = spenttxid;
        vout = 0;
        if ( myGetTransaction(txid,tx,hashBlock) != 0 && (numvouts= tx.vout.size()) > 1 )
        {
            for (i=0; i<numvouts; i++)
                if ( tx.vout[i].nValue == 1 )
                {
                    vout = i;
                    break;
                }
        }
        //fprintf(stderr,"trace spend to %s/v%d\n",txid.GetHex().c_str(),vout);
        if ( n++ > TETRIS_MAXITERATIONS )
            break;
    }
    obj.push_back(Pair("gametxid",gametxid.GetHex()));
    if ( txid != playertxid )
        obj.push_back(Pair("batontxid",txid.GetHex()));
    obj.push_back(Pair("playertxid",playertxid.GetHex()));
    if ( tokenid != zeroid )
        obj.push_back(Pair("tokenid",tokenid.GetHex()));
    else obj.push_back(Pair("tokenid",playertxid.GetHex()));
    if ( datastr != 0 )
    {
        obj.push_back(Pair("data",datastr));
        free(datastr);
    }
    obj.push_back(Pair("pack",a));
    obj.push_back(Pair("packsize",(int64_t)P.packsize));
    obj.push_back(Pair("hitpoints",(int64_t)P.hitpoints));
    obj.push_back(Pair("strength",(int64_t)(P.strength&0xffff)));
    obj.push_back(Pair("maxstrength",(int64_t)(P.strength>>16)));
    obj.push_back(Pair("level",(int64_t)P.level));
    obj.push_back(Pair("experience",(int64_t)P.experience));
    obj.push_back(Pair("dungeonlevel",(int64_t)P.dungeonlevel));
    obj.push_back(Pair("chain",symbol));
    obj.push_back(Pair("pname",pname));
    return(obj);
}

int32_t tetris_iterateplayer(uint256 &registertxid,uint256 firsttxid,int32_t firstvout,uint256 lasttxid)     // retrace playertxid vins to reach highlander <- this verifies player is valid and tetris_playerdataspend makes sure it can only be used once
{
    uint256 spenttxid,txid = firsttxid; int32_t spentvini,n,vout = firstvout;
    registertxid = zeroid;
    if ( vout < 0 )
        return(-1);
    n = 0;
    while ( (spentvini= myIsutxo_spent(spenttxid,txid,vout)) == 0 )
    {
        txid = spenttxid;
        vout = spentvini;
        if ( registertxid == zeroid )
            registertxid = txid;
        if ( ++n >= TETRIS_MAXITERATIONS )
        {
            fprintf(stderr,"tetris_iterateplayer n.%d, seems something is wrong\n",n);
            return(-2);
        }
    }
    if ( txid == lasttxid )
        return(0);
    else
    {
        fprintf(stderr,"firsttxid.%s/v%d -> %s != last.%s\n",firsttxid.ToString().c_str(),firstvout,txid.ToString().c_str(),lasttxid.ToString().c_str());
        return(-1);
    }
}

/*
 playertxid is whoever owns the nonfungible satoshi and it might have been bought and sold many times.
 highlander is the game winning tx with the player data and is the only place where the unique player data exists
 origplayergame is the gametxid that ends up being won by the highlander and they are linked directly as the highlander tx spends gametxid.vout0
 */

int32_t tetris_playerdata(struct CCcontract_info *cp,uint256 &origplayergame,uint256 &tokenid,CPubKey &pk,std::vector<uint8_t> &playerdata,std::string &symbol,std::string &pname,uint256 playertxid)
{
    uint256 origplayertxid,hashBlock,gametxid,registertxid; CTransaction gametx,playertx,highlandertx; std::vector<uint8_t> vopret; uint8_t *script,e,f; int32_t i,regslot,gameheight,numvouts,maxplayers; int64_t buyin;
    if ( myGetTransaction(playertxid,playertx,hashBlock) != 0 && (numvouts= playertx.vout.size()) > 0 )
    {
        if ( (f= tetris_highlanderopretdecode(gametxid,tokenid,regslot,pk,playerdata,symbol,pname,playertx.vout[numvouts-1].scriptPubKey)) == 'H' || f == 'Q' )
        {
            origplayergame = gametxid;
            if ( tokenid != zeroid )
            {
                playertxid = tokenid;
                if ( myGetTransaction(playertxid,playertx,hashBlock) == 0 || (numvouts= playertx.vout.size()) <= 0 )
                {
                    fprintf(stderr,"couldnt get tokenid.%s\n",playertxid.GetHex().c_str());
                    return(-2);
                }
            }
            if ( tetris_isvalidgame(cp,gameheight,gametx,buyin,maxplayers,gametxid,0) == 0 )
            {
                //fprintf(stderr,"playertxid.%s got vin.%s/v%d gametxid.%s iterate.%d\n",playertxid.ToString().c_str(),playertx.vin[1].prevout.hash.ToString().c_str(),(int32_t)playertx.vin[1].prevout.n-maxplayers,gametxid.ToString().c_str(),tetris_iterateplayer(registertxid,gametxid,playertx.vin[1].prevout.n-maxplayers,playertxid));
                if ( (tokenid != zeroid || playertx.vin[1].prevout.hash == gametxid) && tetris_iterateplayer(registertxid,gametxid,playertx.vin[1].prevout.n-maxplayers,playertxid) == 0 )
                {
                    // if registertxid has vin from pk, it can be used
                    return(0);
                } else fprintf(stderr,"hash mismatch or illegal gametxid\n");
            } else fprintf(stderr,"invalid game %s\n",gametxid.GetHex().c_str());
        } //else fprintf(stderr,"invalid player funcid.%c\n",f);
    } else fprintf(stderr,"couldnt get playertxid.%s\n",playertxid.GetHex().c_str());
    return(-1);
}

int32_t tetris_playerdataspend(CMutableTransaction &mtx,uint256 playertxid,int32_t vout,uint256 origplayergame)
{
    int64_t txfee = 10000; CTransaction tx; uint256 hashBlock;
    if ( CCgettxout(playertxid,vout,1,0) == 1 ) // not sure if this is enough validation
    {
        mtx.vin.push_back(CTxIn(playertxid,vout,CScript()));
        return(0);
    }
    else
    {
        vout = 0;
        if ( myGetTransaction(playertxid,tx,hashBlock) != 0 && tx.vout[vout].nValue == 1 && tx.vout[vout].scriptPubKey.IsPayToCryptoCondition() != 0 )
        {
            if ( CCgettxout(playertxid,vout,1,0) == 1 ) // not sure if this is enough validation
            {
                mtx.vin.push_back(CTxIn(playertxid,vout,CScript()));
                return(0);
            }
        }
        return(-1);
    }
}

int32_t tetris_findbaton(struct CCcontract_info *cp,uint256 &playertxid,char **keystrokesp,int32_t &numkeys,int32_t &regslot,std::vector<uint8_t> &playerdata,uint256 &batontxid,int32_t &batonvout,int64_t &batonvalue,int32_t &batonht,uint256 gametxid,CTransaction gametx,int32_t maxplayers,char *destaddr,int32_t &numplayers,std::string &symbol,std::string &pname)
{
    int32_t i,numvouts,spentvini,n,matches = 0; CPubKey pk; uint256 tid,active,spenttxid,tokenid,hashBlock,txid,origplayergame; CTransaction spenttx,matchtx,batontx; std::vector<uint8_t> checkdata; CBlockIndex *pindex; char ccaddr[64],*keystrokes=0;
    batonvalue = numkeys = numplayers = batonht = 0;
    playertxid = batontxid = zeroid;
    for (i=0; i<maxplayers; i++)
    {
        //fprintf(stderr,"findbaton.%d of %d\n",i,maxplayers);
        if ( myIsutxo_spent(spenttxid,gametxid,i+1) >= 0 )
        {
            if ( myGetTransaction(spenttxid,spenttx,hashBlock) != 0 && spenttx.vout.size() > 0 )
            {
                numplayers++;
                Getscriptaddress(ccaddr,spenttx.vout[0].scriptPubKey);
                if ( strcmp(destaddr,ccaddr) == 0 )
                {
                    matches++;
                    regslot = i;
                    matchtx = spenttx;
                } //else fprintf(stderr,"%d+1 doesnt match %s vs %s\n",i,ccaddr,destaddr);
            } //else fprintf(stderr,"%d+1 couldnt find spenttx.%s\n",i,spenttxid.GetHex().c_str());
        } //else fprintf(stderr,"%d+1 unspent\n",i);
    }
    if ( matches == 1 )
    {
        numvouts = matchtx.vout.size();
        //fprintf(stderr,"matchtxid.%s matches.%d numvouts.%d\n",matchtx.GetHash().GetHex().c_str(),matches,numvouts);
        if ( tetris_registeropretdecode(txid,tokenid,playertxid,matchtx.vout[numvouts-1].scriptPubKey) == 'R' )//&& txid == gametxid )
        {
            //fprintf(stderr,"tokenid.%s txid.%s vs gametxid.%s player.%s\n",tokenid.GetHex().c_str(),txid.GetHex().c_str(),gametxid.GetHex().c_str(),playertxid.GetHex().c_str());
            if ( tokenid != zeroid )
                active = tokenid;
            else active = playertxid;
            if ( active == zeroid || tetris_playerdata(cp,origplayergame,tid,pk,playerdata,symbol,pname,active) == 0 )
            {
                txid = matchtx.GetHash();
                //fprintf(stderr,"scan forward active.%s spenttxid.%s\n",active.GetHex().c_str(),txid.GetHex().c_str());
                n = 0;
                while ( CCgettxout(txid,0,1,0) < 0 )
                {
                    spenttxid = zeroid;
                    spentvini = -1;
                    if ( (spentvini= myIsutxo_spent(spenttxid,txid,0)) >= 0 )
                        txid = spenttxid;
                    else
                    {
                        if ( myIsutxo_spentinmempool(spenttxid,spentvini,txid,0) == 0 || spenttxid == zeroid )
                        {
                            fprintf(stderr,"mempool tracking error %s/v0\n",txid.ToString().c_str());
                            return(-2);
                        }
                    }
                    txid = spenttxid;
                    //fprintf(stderr,"n.%d next txid.%s/v%d\n",n,txid.GetHex().c_str(),spentvini);
                    if ( spentvini != 0 ) // game is over?
                    {
                        return(0);
                    }
                    if ( keystrokesp != 0 && myGetTransaction(spenttxid,spenttx,hashBlock) != 0 && spenttx.vout.size() >= 2 )
                    {
                        uint256 g,b; CPubKey p; std::vector<uint8_t> k;
                        if ( tetris_keystrokesopretdecode(g,b,p,k,spenttx.vout[spenttx.vout.size()-1].scriptPubKey) == 'K' )
                        {
                            keystrokes = (char *)realloc(keystrokes,numkeys + (int32_t)k.size());
                            for (i=0; i<k.size(); i++)
                                keystrokes[numkeys+i] = (char)k[i];
                            numkeys += (int32_t)k.size();
                            (*keystrokesp) = keystrokes;
                            //fprintf(stderr,"updated keystrokes.%p[%d]\n",keystrokes,numkeys);
                        }
                    }
                    //fprintf(stderr,"n.%d txid.%s\n",n,txid.GetHex().c_str());
                    if ( ++n >= TETRIS_MAXITERATIONS )
                    {
                        fprintf(stderr,"tetris_findbaton n.%d, seems something is wrong\n",n);
                        return(-5);
                    }
                }
                //fprintf(stderr,"set baton %s\n",txid.GetHex().c_str());
                batontxid = txid;
                batonvout = 0; // not vini
                // how to detect timeout, bailedout, highlander
                hashBlock = zeroid;
                if ( myGetTransaction(batontxid,batontx,hashBlock) != 0 && batontx.vout.size() > 0 )
                {
                    if ( hashBlock == zeroid )
                        batonht = komodo_nextheight();
                    else if ( (pindex= komodo_blockindex(hashBlock)) == 0 )
                        return(-4);
                    else batonht = pindex->GetHeight();
                    batonvalue = batontx.vout[0].nValue;
                    //printf("batonht.%d keystrokes[%d]\n",batonht,numkeys);
                    return(0);
                } else fprintf(stderr,"couldnt find baton\n");
            } else fprintf(stderr,"error with playerdata\n");
        } else fprintf(stderr,"findbaton opret error\n");
    }
    return(-1);
}

int32_t tetris_playersalive(int32_t &openslots,int32_t &numplayers,uint256 gametxid,int32_t maxplayers,int32_t gameht,CTransaction gametx)
{
    int32_t i,n,vout,spentvini,registration_open = 0,alive = 0; CTransaction tx; uint256 txid,spenttxid,hashBlock; CBlockIndex *pindex; uint64_t txfee = 10000;
    numplayers = openslots = 0;
    if ( komodo_nextheight() <= gameht+TETRIS_MAXKEYSTROKESGAP )
        registration_open = 1;
    for (i=0; i<maxplayers; i++)
    {
        //fprintf(stderr,"players alive %d of %d\n",i,maxplayers);
        if ( CCgettxout(gametxid,1+i,1,0) < 0 )
        {
            numplayers++;
            //fprintf(stderr,"players alive %d spent baton\n",i);
            if ( CCgettxout(gametxid,1+maxplayers+i,1,0) == txfee )
            {
                txid = gametxid;
                vout = 1+i;
                //fprintf(stderr,"tetris_playersalive scan forward active.%s spenttxid.%s\n",gametxid.GetHex().c_str(),txid.GetHex().c_str());
                n = 0;
                while ( CCgettxout(txid,vout,1,0) < 0 )
                {
                    spenttxid = zeroid;
                    spentvini = -1;
                    if ( (spentvini= myIsutxo_spent(spenttxid,txid,vout)) >= 0 )
                        txid = spenttxid;
                    else if ( myIsutxo_spentinmempool(spenttxid,spentvini,txid,vout) == 0 || spenttxid == zeroid )
                    {
                        fprintf(stderr,"mempool tracking error %s/v0\n",txid.ToString().c_str());
                        break;
                    }
                    txid = spenttxid;
                    vout = 0;
                    //fprintf(stderr,"n.%d next txid.%s/v%d\n",n,txid.GetHex().c_str(),spentvini);
                    if ( spentvini != 0 )
                        break;
                    if ( n++ > TETRIS_MAXITERATIONS )
                        break;
                }
                if ( txid != zeroid )
                {
                    if ( myGetTransaction(txid,tx,hashBlock) != 0 )
                    {
                        if ( (pindex= komodo_blockindex(hashBlock)) != 0 )
                        {
                            if ( pindex->GetHeight() <= gameht+TETRIS_MAXKEYSTROKESGAP )
                                alive++;
                        }
                    }
                }
            }
        }
        else if ( registration_open != 0 )
            openslots++;
    }
    //fprintf(stderr,"numalive.%d openslots.%d\n",alive,openslots);
    return(alive);
}

uint64_t tetris_gamefields(UniValue &obj,int64_t maxplayers,int64_t buyin,uint256 gametxid,char *mytetrisaddr)
{
    CBlockIndex *pindex; int32_t ht,openslots,delay,numplayers; uint256 hashBlock; uint64_t seed=0; char cmd[512]; CTransaction tx;
    if ( myGetTransaction(gametxid,tx,hashBlock) != 0 && (pindex= komodo_blockindex(hashBlock)) != 0 )
    {
        ht = pindex->GetHeight();
        delay = TETRIS_REGISTRATION * (maxplayers > 1);
        obj.push_back(Pair("height",ht));
        obj.push_back(Pair("start",ht+delay));
        if ( komodo_nextheight() > ht+delay )
        {
            if ( (pindex= komodo_chainactive(ht+delay)) != 0 )
            {
                hashBlock = pindex->GetBlockHash();
                obj.push_back(Pair("starthash",hashBlock.ToString()));
                memcpy(&seed,&hashBlock,sizeof(seed));
                seed &= (1LL << 62) - 1;
                obj.push_back(Pair("seed",(int64_t)seed));
                if ( tetris_iamregistered(maxplayers,gametxid,tx,mytetrisaddr) > 0 )
                    sprintf(cmd,"cc/tetris %llu %s",(long long)seed,gametxid.ToString().c_str());
                else sprintf(cmd,"./komodo-cli -ac_name=%s cclib register %d \"[%%22%s%%22]\"",ASSETCHAINS_SYMBOL,EVAL_TETRIS,gametxid.ToString().c_str());
                obj.push_back(Pair("run",cmd));
            }
        }
        obj.push_back(Pair("alive",tetris_playersalive(openslots,numplayers,gametxid,maxplayers,ht,tx)));
        obj.push_back(Pair("openslots",openslots));
        obj.push_back(Pair("numplayers",numplayers));
    }
    obj.push_back(Pair("maxplayers",maxplayers));
    obj.push_back(Pair("buyin",ValueFromAmount(buyin)));
    return(seed);
}

void tetris_gameplayerinfo(struct CCcontract_info *cp,UniValue &obj,uint256 gametxid,CTransaction gametx,int32_t vout,int32_t maxplayers,char *mytetrisaddr)
{
    // identify if bailout or quit or timed out
    uint256 batontxid,spenttxid,gtxid,ptxid,tokenid,hashBlock,playertxid; CTransaction spenttx,batontx; int32_t numplayers,regslot,numkeys,batonvout,batonht,retval; int64_t batonvalue; std::vector<uint8_t> playerdata; char destaddr[64]; std::string symbol,pname;
    destaddr[0] = 0;
    if ( myIsutxo_spent(spenttxid,gametxid,vout) >= 0 )
    {
        if ( myGetTransaction(spenttxid,spenttx,hashBlock) != 0 && spenttx.vout.size() > 0 )
            Getscriptaddress(destaddr,spenttx.vout[0].scriptPubKey);
    }
    obj.push_back(Pair("slot",(int64_t)vout-1));
    if ( (retval= tetris_findbaton(cp,playertxid,0,numkeys,regslot,playerdata,batontxid,batonvout,batonvalue,batonht,gametxid,gametx,maxplayers,destaddr,numplayers,symbol,pname)) == 0 )
    {
        if ( CCgettxout(gametxid,maxplayers+vout,1,0) == 10000 )
        {
            if ( myGetTransaction(batontxid,batontx,hashBlock) != 0 && batontx.vout.size() > 1 )
            {
                if ( tetris_registeropretdecode(gtxid,tokenid,ptxid,batontx.vout[batontx.vout.size()-1].scriptPubKey) == 'R' && ptxid == playertxid && gtxid == gametxid )
                    obj.push_back(Pair("status","registered"));
                else obj.push_back(Pair("status","alive"));
            } else obj.push_back(Pair("status","error"));
        } else obj.push_back(Pair("status","finished"));
        obj.push_back(Pair("baton",batontxid.ToString()));
        obj.push_back(Pair("tokenid",tokenid.ToString()));
        obj.push_back(Pair("batonaddr",destaddr));
        obj.push_back(Pair("ismine",strcmp(mytetrisaddr,destaddr)==0));
        obj.push_back(Pair("batonvout",(int64_t)batonvout));
        obj.push_back(Pair("batonvalue",ValueFromAmount(batonvalue)));
        obj.push_back(Pair("batonht",(int64_t)batonht));
        if ( playerdata.size() > 0 )
            obj.push_back(Pair("player",tetris_playerobj(playerdata,playertxid,tokenid,symbol,pname,gametxid)));
    } else fprintf(stderr,"findbaton err.%d\n",retval);
}

int64_t tetris_registrationbaton(CMutableTransaction &mtx,uint256 gametxid,CTransaction gametx,int32_t maxplayers)
{
    int32_t vout,j,r; int64_t nValue;
    if ( gametx.vout.size() > maxplayers+1 )
    {
        r = rand() % maxplayers;
        for (j=0; j<maxplayers; j++)
        {
            vout = ((r + j) % maxplayers) + 1;
            if ( CCgettxout(gametxid,vout,1,0) == TETRIS_REGISTRATIONSIZE )
            {
                mtx.vin.push_back(CTxIn(gametxid,vout,CScript()));
                return(TETRIS_REGISTRATIONSIZE);
            }
        }
    }
    return(0);
}

UniValue tetris_rawtxresult(UniValue &result,std::string rawtx,int32_t broadcastflag)
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

UniValue tetris_newgame(uint64_t txfee,struct CCcontract_info *cp,cJSON *params)
{
    CMutableTransaction mtx = CreateNewContextualCMutableTransaction(Params().GetConsensus(), komodo_nextheight());
    UniValue result(UniValue::VOBJ); std::string rawtx; CPubKey tetrispk,mypk; char *jsonstr; uint64_t inputsum,change,required,buyin=0; int32_t i,n,maxplayers = 1;
    if ( txfee == 0 )
        txfee = 10000;
    if ( params != 0 && (n= cJSON_GetArraySize(params)) > 0 )
    {
        if ( n > 0 )
        {
            maxplayers = juint(jitem(params,0),0);
            if ( n > 1 )
                buyin = jdouble(jitem(params,1),0) * COIN + 0.0000000049;
        }
    }
    if ( maxplayers < 1 || maxplayers > TETRIS_MAXPLAYERS )
        return(cclib_error(result,"illegal maxplayers"));
    mypk = pubkey2pk(Mypubkey());
    tetrispk = GetUnspendable(cp,0);
    tetris_univalue(result,"newgame",maxplayers,buyin);
    required = (3*txfee + maxplayers*(TETRIS_REGISTRATIONSIZE+txfee));
    if ( (inputsum= AddCClibInputs(cp,mtx,tetrispk,required,16,cp->unspendableCCaddr)) >= required )
    {
        mtx.vout.push_back(MakeCC1vout(cp->evalcode,txfee,tetrispk)); // for highlander TCBOO creation
        for (i=0; i<maxplayers; i++)
            mtx.vout.push_back(MakeCC1of2vout(cp->evalcode,TETRIS_REGISTRATIONSIZE,tetrispk,tetrispk));
        for (i=0; i<maxplayers; i++)
            mtx.vout.push_back(MakeCC1of2vout(cp->evalcode,txfee,tetrispk,tetrispk));
        if ( (change= inputsum - required) >= txfee )
            mtx.vout.push_back(MakeCC1vout(cp->evalcode,change,tetrispk));
        rawtx = FinalizeCCTx(0,cp,mtx,mypk,txfee,tetris_newgameopret(buyin,maxplayers));
        return(tetris_rawtxresult(result,rawtx,1));
    }
    else return(cclib_error(result,"illegal maxplayers"));
    return(result);
}

UniValue tetris_playerinfo(uint64_t txfee,struct CCcontract_info *cp,cJSON *params)
{
    UniValue result(UniValue::VOBJ); std::vector<uint8_t> playerdata; uint256 playertxid,tokenid,origplayergame;int32_t n; CPubKey pk; bits256 t; std::string symbol,pname;
    result.push_back(Pair("result","success"));
    tetris_univalue(result,"playerinfo",-1,-1);
    if ( params != 0 && (n= cJSON_GetArraySize(params)) > 0 )
    {
        if ( n > 0 )
        {
            playertxid = juint256(jitem(params,0));
            if ( tetris_playerdata(cp,origplayergame,tokenid,pk,playerdata,symbol,pname,playertxid) < 0 )
                return(cclib_error(result,"invalid playerdata"));
            result.push_back(Pair("player",tetris_playerobj(playerdata,playertxid,tokenid,symbol,pname,origplayergame)));
        } else return(cclib_error(result,"no playertxid"));
        return(result);
    } else return(cclib_error(result,"couldnt reparse params"));
}

UniValue tetris_register(uint64_t txfee,struct CCcontract_info *cp,cJSON *params)
{
    // vin0 -> TETRIS_REGISTRATIONSIZE 1of2 registration baton from creategame
    // vin1 -> optional nonfungible character vout @
    // vin2 -> original creation TCBOO playerdata used
    // vin3+ -> buyin
    // vout0 -> keystrokes/completion baton
    CMutableTransaction mtx = CreateNewContextualCMutableTransaction(Params().GetConsensus(), komodo_nextheight());
    UniValue result(UniValue::VOBJ); char destaddr[64],coinaddr[64]; uint256 tokenid,gametxid,origplayergame,playertxid,hashBlock; int32_t err,maxplayers,gameheight,n,numvouts,vout=1; int64_t inputsum,buyin,CCchange=0; CPubKey pk,mypk,tetrispk,burnpk; CTransaction tx,playertx; std::vector<uint8_t> playerdata; std::string rawtx,symbol,pname; bits256 t;

    if ( txfee == 0 )
        txfee = 10000;
    mypk = pubkey2pk(Mypubkey());
    burnpk = pubkey2pk(ParseHex(CC_BURNPUBKEY));
    tetrispk = GetUnspendable(cp,0);
    tetris_univalue(result,"register",-1,-1);
    playertxid = tokenid = zeroid;
    if ( params != 0 && (n= cJSON_GetArraySize(params)) > 0 )
    {
        if ( n > 0 )
        {
            gametxid = juint256(jitem(params,0));
            if ( (err= tetris_isvalidgame(cp,gameheight,tx,buyin,maxplayers,gametxid,1)) == 0 )
            {
                if ( n > 1 )
                {
                    playertxid = juint256(jitem(params,1));
                    if ( tetris_playerdata(cp,origplayergame,tokenid,pk,playerdata,symbol,pname,playertxid) < 0 )
                        return(cclib_error(result,"couldnt extract valid playerdata"));
                    if ( tokenid != zeroid ) // if it is tokentransfer this will be 0
                        vout = 1;
                }
                if ( komodo_nextheight() > gameheight + TETRIS_MAXKEYSTROKESGAP )
                    return(cclib_error(result,"didnt register in time, TETRIS_MAXKEYSTROKESGAP"));
                tetris_univalue(result,0,maxplayers,buyin);
                GetCCaddress1of2(cp,coinaddr,tetrispk,mypk);
                if ( tetris_iamregistered(maxplayers,gametxid,tx,coinaddr) > 0 )
                    return(cclib_error(result,"already registered"));
                if ( (inputsum= tetris_registrationbaton(mtx,gametxid,tx,maxplayers)) != TETRIS_REGISTRATIONSIZE )
                    return(cclib_error(result,"couldnt find available registration baton"));
                else if ( playertxid != zeroid && tetris_playerdataspend(mtx,playertxid,vout,origplayergame) < 0 )
                    return(cclib_error(result,"couldnt find playerdata to spend"));
                else if ( buyin > 0 && AddNormalinputs(mtx,mypk,buyin,64) < buyin )
                    return(cclib_error(result,"couldnt find enough normal funds for buyin"));
                if ( tokenid != zeroid )
                {
                    mtx.vin.push_back(CTxIn(tokenid,0)); // spending cc marker as token is burned
                    char unspendableTokenAddr[64]; uint8_t tokenpriv[32]; struct CCcontract_info *cpTokens, tokensC;
                    cpTokens = CCinit(&tokensC, EVAL_TOKENS);
                    CPubKey unspPk = GetUnspendable(cpTokens, tokenpriv);
                    GetCCaddress(cpTokens, unspendableTokenAddr, unspPk);
                    CCaddr2set(cp, EVAL_TOKENS, unspPk, tokenpriv, unspendableTokenAddr);
                }
                mtx.vout.push_back(MakeCC1of2vout(cp->evalcode,buyin + inputsum - txfee,tetrispk,mypk));
                GetCCaddress1of2(cp,destaddr,tetrispk,tetrispk);
                CCaddr1of2set(cp,tetrispk,tetrispk,cp->CCpriv,destaddr);
                mtx.vout.push_back(MakeTokensCC1vout(cp->evalcode, 1, burnpk));

                uint8_t e, funcid; uint256 tid; std::vector<CPubKey> voutPubkeys, voutPubkeysEmpty; int32_t didtx = 0;
                CScript opretRegister = tetris_registeropret(gametxid, playertxid);
                if ( playertxid != zeroid )
                {
                    voutPubkeysEmpty.push_back(burnpk);
                    if ( myGetTransaction(playertxid,playertx,hashBlock) != 0 )
                    {
                        std::vector<std::pair<uint8_t, vscript_t>>  oprets;
                        if ( (funcid= DecodeTokenOpRet(playertx.vout.back().scriptPubKey, e, tid, voutPubkeys, oprets)) != 0)
                        {  // if token in the opret
                            didtx = 1;
                            if ( funcid == 'c' )
                                tid = tokenid == zeroid ? playertxid : tokenid;
                            vscript_t vopretRegister;
                            GetOpReturnData(opretRegister, vopretRegister);
                            rawtx = FinalizeCCTx(0, cp, mtx, mypk, txfee,
                                EncodeTokenOpRet(tid, voutPubkeysEmpty /*=never spent*/, std::make_pair(OPRETID_TETRISGAMEDATA, vopretRegister)));
                        }
                    }
                }
                if ( didtx == 0 )
                    rawtx = FinalizeCCTx(0, cp, mtx, mypk, txfee, opretRegister);

                return(tetris_rawtxresult(result,rawtx,1));
            } else return(cclib_error(result,"invalid gametxid"));
        } else return(cclib_error(result,"no gametxid"));
    } else return(cclib_error(result,"couldnt reparse params"));
}

UniValue tetris_keystrokes(uint64_t txfee,struct CCcontract_info *cp,cJSON *params)
{
    // vin0 -> baton from registration or previous keystrokes
    // vout0 -> new baton
    // opret -> user input chars
    // being killed should auto broadcast (possible to be suppressed?)
    // respawn to be prevented by including timestamps
    int32_t nextheight = komodo_nextheight();
    CMutableTransaction mtx = CreateNewContextualCMutableTransaction(Params().GetConsensus(),nextheight);
    UniValue result(UniValue::VOBJ); CPubKey tetrispk,mypk; uint256 gametxid,playertxid,batontxid; int64_t batonvalue,buyin; std::vector<uint8_t> keystrokes,playerdata; int32_t numplayers,regslot,numkeys,batonht,batonvout,n,elapsed,gameheight,maxplayers; CTransaction tx; CTxOut txout; char *keystrokestr,destaddr[64]; std::string rawtx,symbol,pname; bits256 t; uint8_t mypriv[32];
    if ( txfee == 0 )
        txfee = 10000;
    tetris_univalue(result,"keystrokes",-1,-1);
    if ( params != 0 && (n= cJSON_GetArraySize(params)) == 2 && (keystrokestr= jstr(jitem(params,1),0)) != 0 )
    {
        gametxid = juint256(jitem(params,0));
        result.push_back(Pair("gametxid",gametxid.GetHex()));
        result.push_back(Pair("keystrokes",keystrokestr));
        keystrokes = ParseHex(keystrokestr);
        mypk = pubkey2pk(Mypubkey());
        tetrispk = GetUnspendable(cp,0);
        GetCCaddress1of2(cp,destaddr,tetrispk,mypk);
        if ( tetris_isvalidgame(cp,gameheight,tx,buyin,maxplayers,gametxid,1) == 0 )
        {
            if ( tetris_findbaton(cp,playertxid,0,numkeys,regslot,playerdata,batontxid,batonvout,batonvalue,batonht,gametxid,tx,maxplayers,destaddr,numplayers,symbol,pname) == 0 )
            {
                result.push_back(Pair("batontxid",batontxid.GetHex()));
                result.push_back(Pair("playertxid",playertxid.GetHex()));
                if ( maxplayers == 1 || nextheight <= batonht+TETRIS_MAXKEYSTROKESGAP )
                {
                    mtx.vin.push_back(CTxIn(batontxid,batonvout,CScript())); //this validates user if pk
                    mtx.vout.push_back(MakeCC1of2vout(cp->evalcode,batonvalue-txfee,tetrispk,mypk));
                    Myprivkey(mypriv);
                    CCaddr1of2set(cp,tetrispk,mypk,mypriv,destaddr);
                    rawtx = FinalizeCCTx(0,cp,mtx,mypk,txfee,tetris_keystrokesopret(gametxid,batontxid,mypk,keystrokes));
                    //fprintf(stderr,"KEYSTROKES.(%s)\n",rawtx.c_str());
                    return(tetris_rawtxresult(result,rawtx,1));
                } else return(cclib_error(result,"keystrokes tx was too late"));
            } else return(cclib_error(result,"couldnt find batontxid"));
        } else return(cclib_error(result,"invalid gametxid"));
    } else return(cclib_error(result,"couldnt reparse params"));
}

char *tetris_extractgame(int32_t makefiles,char *str,int32_t *numkeysp,std::vector<uint8_t> &newdata,uint64_t &seed,uint256 &playertxid,struct CCcontract_info *cp,uint256 gametxid,char *tetrisaddr)
{
    CPubKey tetrispk; int32_t i,num,retval,maxplayers,gameheight,batonht,batonvout,numplayers,regslot,numkeys,err; std::string symbol,pname; CTransaction gametx; int64_t buyin,batonvalue; char fname[64],*keystrokes = 0; std::vector<uint8_t> playerdata; uint256 batontxid; FILE *fp; uint8_t newplayer[10000]; struct tetris_player P,endP;
    tetrispk = GetUnspendable(cp,0);
    *numkeysp = 0;
    seed = 0;
    num = numkeys = 0;
    playertxid = zeroid;
    str[0] = 0;
    if ( (err= tetris_isvalidgame(cp,gameheight,gametx,buyin,maxplayers,gametxid,0)) == 0 )
    {
        if ( (retval= tetris_findbaton(cp,playertxid,&keystrokes,numkeys,regslot,playerdata,batontxid,batonvout,batonvalue,batonht,gametxid,gametx,maxplayers,tetrisaddr,numplayers,symbol,pname)) == 0 )
        {
            UniValue obj;
            seed = tetris_gamefields(obj,maxplayers,buyin,gametxid,tetrisaddr);
            //fprintf(stderr,"(%s) found baton %s numkeys.%d seed.%llu playerdata.%d playertxid.%s\n",pname.size()!=0?pname.c_str():Tetris_pname.c_str(),batontxid.ToString().c_str(),numkeys,(long long)seed,(int32_t)playerdata.size(),playertxid.GetHex().c_str());
            memset(&P,0,sizeof(P));
            if ( playerdata.size() > 0 )
            {
                for (i=0; i<playerdata.size(); i++)
                    ((uint8_t *)&P)[i] = playerdata[i];
            }
            if ( keystrokes != 0 && numkeys != 0 )
            {
                if ( makefiles != 0 )
                {
                    sprintf(fname,"tetris.%llu.0",(long long)seed);
                    if ( (fp= fopen(fname,"wb")) != 0 )
                    {
                        if ( fwrite(keystrokes,1,numkeys,fp) != numkeys )
                            fprintf(stderr,"error writing %s\n",fname);
                        fclose(fp);
                    }
                    sprintf(fname,"tetris.%llu.player",(long long)seed);
                    if ( (fp= fopen(fname,"wb")) != 0 )
                    {
                        if ( fwrite(&playerdata[0],1,(int32_t)playerdata.size(),fp) != playerdata.size() )
                            fprintf(stderr,"error writing %s\n",fname);
                        fclose(fp);
                    }
                }
                num = tetris_replay2(newplayer,seed,keystrokes,numkeys,playerdata.size()==0?0:&P,0);
                newdata.resize(num);
                for (i=0; i<num; i++)
                {
                    newdata[i] = newplayer[i];
                    ((uint8_t *)&endP)[i] = newplayer[i];
                }
                if ( endP.gold <= 0 || endP.hitpoints <= 0 || (endP.strength&0xffff) <= 0 || endP.level <= 0 || endP.experience <= 0 || endP.dungeonlevel <= 0 )
                {
                    sprintf(str,"zero value character was killed -> no playerdata\n");
                    newdata.resize(0);
                    *numkeysp = numkeys;
                    return(keystrokes);
                    /* P.gold = (P.gold * 8) / 10;
                    if ( keystrokes != 0 )
                    {
                        free(keystrokes);
                        keystrokes = 0;
                        *numkeysp = 0;
                        return(keystrokes);
                    }*/
                }
                else
                {
                    sprintf(str,"$$$gold.%d hp.%d strength.%d/%d level.%d exp.%d dl.%d",endP.gold,endP.hitpoints,endP.strength&0xffff,endP.strength>>16,endP.level,endP.experience,endP.dungeonlevel);
                    //fprintf(stderr,"%s\n",str);
                    *numkeysp = numkeys;
                    return(keystrokes);
                }
            } else num = 0;
        }
        else
        {
            fprintf(stderr,"extractgame: couldnt find baton keystrokes.%p retval.%d\n",keystrokes,retval);
            if ( keystrokes != 0 )
                free(keystrokes), keystrokes = 0;
        }
    } else fprintf(stderr,"extractgame: invalid game\n");
    //fprintf(stderr,"extract %s\n",gametxid.GetHex().c_str());
    return(0);
}

UniValue tetris_extract(uint64_t txfee,struct CCcontract_info *cp,cJSON *params)
{
    UniValue result(UniValue::VOBJ); CPubKey pk,tetrispk; int32_t i,n,numkeys,flag = 0; uint64_t seed; char str[512],tetrisaddr[64],*pubstr,*hexstr,*keystrokes = 0; std::vector<uint8_t> newdata; uint256 gametxid,playertxid; FILE *fp; uint8_t pub33[33];
    pk = pubkey2pk(Mypubkey());
    tetrispk = GetUnspendable(cp,0);
    result.push_back(Pair("name","tetris"));
    result.push_back(Pair("method","extract"));
    tetrisaddr[0] = 0;
    if ( params != 0 && (n= cJSON_GetArraySize(params)) > 0 )
    {
        if ( n > 0 )
        {
            gametxid = juint256(jitem(params,0));
            result.push_back(Pair("gametxid",gametxid.GetHex()));
            if ( n == 2 )
            {
                if ( (pubstr= jstr(jitem(params,1),0)) != 0 )
                {
                    if (strlen(pubstr) == 66 )
                    {
                        decode_hex(pub33,33,pubstr);
                        pk = buf2pk(pub33);
                    }
                    else if ( strlen(pubstr) < 36 )
                        strcpy(tetrisaddr,pubstr);
                }
                //fprintf(stderr,"gametxid.%s %s\n",gametxid.GetHex().c_str(),pubstr);
            }
            if ( tetrisaddr[0] == 0 )
                GetCCaddress1of2(cp,tetrisaddr,tetrispk,pk);
            result.push_back(Pair("tetrisaddr",tetrisaddr));
            str[0] = 0;
            if ( (keystrokes= tetris_extractgame(1,str,&numkeys,newdata,seed,playertxid,cp,gametxid,tetrisaddr)) != 0 )
            {
                result.push_back(Pair("status","success"));
                flag = 1;
                hexstr = (char *)malloc(numkeys*2 + 1);
                for (i=0; i<numkeys; i++)
                    sprintf(&hexstr[i<<1],"%02x",keystrokes[i]);
                hexstr[i<<1] = 0;
                result.push_back(Pair("keystrokes",hexstr));
                free(hexstr);
                result.push_back(Pair("numkeys",(int64_t)numkeys));
                result.push_back(Pair("playertxid",playertxid.GetHex()));
                result.push_back(Pair("extracted",str));
                result.push_back(Pair("seed",(int64_t)seed));
                sprintf(str,"cc/tetris %llu",(long long)seed);
                result.push_back(Pair("replay",str));
                free(keystrokes);
            }
        }
    }
    if ( flag == 0 )
        result.push_back(Pair("status","error"));
    return(result);
}

int32_t tetris_playerdata_validate(int64_t *cashoutp,uint256 &playertxid,struct CCcontract_info *cp,std::vector<uint8_t> playerdata,uint256 gametxid,CPubKey pk)
{
    static uint32_t good,bad; static uint256 prevgame;
    char str[512],*keystrokes,tetrisaddr[64],str2[67],fname[64]; int32_t i,dungeonlevel,numkeys; std::vector<uint8_t> newdata; uint64_t seed,mult = 10; CPubKey tetrispk; struct tetris_player P;
    *cashoutp = 0;
    tetrispk = GetUnspendable(cp,0);
    GetCCaddress1of2(cp,tetrisaddr,tetrispk,pk);
    //fprintf(stderr,"call extractgame\n");
    if ( (keystrokes= tetris_extractgame(0,str,&numkeys,newdata,seed,playertxid,cp,gametxid,tetrisaddr)) != 0 )
    {
        //fprintf(stderr,"numkeys.%d tetris_extractgame %s\n",numkeys,gametxid.GetHex().c_str());
        free(keystrokes);
        sprintf(fname,"tetris.%llu.pack",(long long)seed);
        remove(fname);

        //fprintf(stderr,"extracted.(%s)\n",str);
        for (i=0; i<playerdata.size(); i++)
            ((uint8_t *)&P)[i] = playerdata[i];
        if ( P.amulet != 0 )
            mult *= 5;
        dungeonlevel = P.dungeonlevel;
        if ( P.amulet != 0 && dungeonlevel < 26 )
            dungeonlevel = 26;
        *cashoutp = (uint64_t)P.gold * P.gold * mult * dungeonlevel;
        if ( newdata == playerdata )
        {
            if ( gametxid != prevgame )
            {
                prevgame = gametxid;
                good++;
                fprintf(stderr,"%s good.%d bad.%d\n",gametxid.GetHex().c_str(),good,bad);
            }
            return(0);
        }
        newdata[10] = newdata[11] = playerdata[10] = playerdata[11] = 0;
        if ( newdata == playerdata )
        {
            if ( gametxid != prevgame )
            {
                prevgame = gametxid;
                good++;
                fprintf(stderr,"%s matched after clearing maxstrength good.%d bad.%d\n",gametxid.GetHex().c_str(),good,bad);
            }
            return(0);
        }
        if ( P.gold <= 0 || P.hitpoints <= 0 || (P.strength&0xffff) <= 0 || P.level <= 0 || P.experience <= 0 || P.dungeonlevel <= 0 )
        {
            //P.gold = (P.gold * 8) / 10;
            //for (i=0; i<playerdata.size(); i++)
            //    playerdata[i] = ((uint8_t *)&P)[i];
            if ( newdata.size() == 0 )
            {
                if ( gametxid != prevgame )
                {
                    prevgame = gametxid;
                    good++;
                    fprintf(stderr,"zero value character was killed -> no playerdata, good.%d bad.%d\n",good,bad);
                }
                *cashoutp = 0;
                return(0);
            }
        }
        if ( gametxid != prevgame )
        {
            prevgame = gametxid;
            bad++;
            fprintf(stderr,"%s playerdata: gold.%d hp.%d strength.%d/%d level.%d exp.%d dl.%d\n",gametxid.GetHex().c_str(),P.gold,P.hitpoints,P.strength&0xffff,P.strength>>16,P.level,P.experience,P.dungeonlevel);
            fprintf(stderr,"newdata[%d] != playerdata[%d], numkeys.%d %s pub.%s playertxid.%s good.%d bad.%d\n",(int32_t)newdata.size(),(int32_t)playerdata.size(),numkeys,tetrisaddr,pubkey33_str(str2,(uint8_t *)&pk),playertxid.GetHex().c_str(),good,bad);
        }
    }
    sprintf(fname,"tetris.%llu.pack",(long long)seed);
    remove(fname);
 //fprintf(stderr,"no keys tetris_extractgame %s\n",gametxid.GetHex().c_str());
    return(-1);
}

UniValue tetris_finishgame(uint64_t txfee,struct CCcontract_info *cp,cJSON *params,char *method)
{
    //vin0 -> highlander vout from creategame TCBOO
    //vin1 -> keystrokes baton of completed game, must be last to quit or first to win, only spent registration batons matter. If more than 60 blocks since last keystrokes, it is forfeit
    //vins2+ -> rest of unspent registration utxo so all newgame vouts are spent
    //vout0 -> nonfungible character with pack @
    //vout1 -> 1% ingame gold and all the buyins

    // detect if last to bailout
    // vin0 -> kestrokes baton of completed game with Q
    // vout0 -> playerdata marker
    // vout0 -> 1% ingame gold
    // get any playerdata, get all keystrokes, replay game and compare final state
    CMutableTransaction mtx = CreateNewContextualCMutableTransaction(Params().GetConsensus(), komodo_nextheight());
    UniValue result(UniValue::VOBJ); std::string rawtx,symbol,pname; CTransaction gametx; uint64_t seed,mult; int64_t buyin,batonvalue,inputsum,cashout,CCchange=0; int32_t i,err,gameheight,tmp,numplayers,regslot,n,num,dungeonlevel,numkeys,maxplayers,batonht,batonvout; char mytetrisaddr[64],*keystrokes = 0; std::vector<uint8_t> playerdata,newdata,nodata; uint256 batontxid,playertxid,gametxid; CPubKey mypk,tetrispk; uint8_t player[10000],mypriv[32],funcid;
    struct CCcontract_info *cpTokens, tokensC;

    if ( txfee == 0 )
        txfee = 10000;
    mypk = pubkey2pk(Mypubkey());
    tetrispk = GetUnspendable(cp,0);
    GetCCaddress1of2(cp,mytetrisaddr,tetrispk,mypk);
    result.push_back(Pair("name","tetris"));
    result.push_back(Pair("method",method));
    result.push_back(Pair("mytetrisaddr",mytetrisaddr));
    if ( strcmp(method,"bailout") == 0 )
    {
        funcid = 'Q';
        mult = 10; //100000;
    }
    else
    {
        funcid = 'H';
        mult = 20; //200000;
    }
    if ( params != 0 && (n= cJSON_GetArraySize(params)) > 0 )
    {
        if ( n > 0 )
        {
            gametxid = juint256(jitem(params,0));
            result.push_back(Pair("gametxid",gametxid.GetHex()));
            if ( (err= tetris_isvalidgame(cp,gameheight,gametx,buyin,maxplayers,gametxid,1)) == 0 )
            {
                if ( tetris_findbaton(cp,playertxid,&keystrokes,numkeys,regslot,playerdata,batontxid,batonvout,batonvalue,batonht,gametxid,gametx,maxplayers,mytetrisaddr,numplayers,symbol,pname) == 0 )
                {
                    UniValue obj; struct tetris_player P;
                    seed = tetris_gamefields(obj,maxplayers,buyin,gametxid,mytetrisaddr);
                    fprintf(stderr,"(%s) found baton %s numkeys.%d seed.%llu playerdata.%d\n",pname.size()!=0?pname.c_str():Tetris_pname.c_str(),batontxid.ToString().c_str(),numkeys,(long long)seed,(int32_t)playerdata.size());
                    memset(&P,0,sizeof(P));
                    if ( playerdata.size() > 0 )
                    {
                        for (i=0; i<playerdata.size(); i++)
                            ((uint8_t *)&P)[i] = playerdata[i];
                    }
                    if ( keystrokes != 0 )
                    {
                        num = tetris_replay2(player,seed,keystrokes,numkeys,playerdata.size()==0?0:&P,0);
                        if ( keystrokes != 0 )
                            free(keystrokes), keystrokes = 0;
                    } else num = 0;
                    mtx.vin.push_back(CTxIn(batontxid,batonvout,CScript()));
                    mtx.vin.push_back(CTxIn(gametxid,1+maxplayers+regslot,CScript()));
                    if ( funcid == 'H' )
                        mtx.vin.push_back(CTxIn(gametxid,0,CScript()));
                    if ( num > 0 )
                    {
                        newdata.resize(num);
                        for (i=0; i<num; i++)
                        {
                            newdata[i] = player[i];
                            ((uint8_t *)&P)[i] = player[i];
                        }
                        if ( (P.gold <= 0 || P.hitpoints <= 0 || (P.strength&0xffff) <= 0 || P.level <= 0 || P.experience <= 0 || P.dungeonlevel <= 0) )
                        {
                            //fprintf(stderr,"zero value character was killed -> no playerdata\n");
                            newdata.resize(0);
                            //P.gold = (P.gold * 8) / 10;
                        }
                        else
                        {
                            cpTokens = CCinit(&tokensC, EVAL_TOKENS);
                            mtx.vout.push_back(MakeCC1vout(EVAL_TOKENS, txfee, GetUnspendable(cpTokens,NULL)));            // marker to token cc addr, burnable and validated
                            mtx.vout.push_back(MakeTokensCC1vout(cp->evalcode,1,mypk));
                            if ( P.amulet != 0 )
                                mult *= 5;
                            dungeonlevel = P.dungeonlevel;
                            if ( P.amulet != 0 && dungeonlevel < 21 )
                                dungeonlevel = 21;
                            cashout = (uint64_t)P.gold * P.gold * mult * dungeonlevel;
                            fprintf(stderr,"\nextracted $$$gold.%d -> %.8f TETRIS hp.%d strength.%d/%d level.%d exp.%d dl.%d n.%d amulet.%d\n",P.gold,(double)cashout/COIN,P.hitpoints,P.strength&0xffff,P.strength>>16,P.level,P.experience,P.dungeonlevel,n,P.amulet);
                            if ( funcid == 'H' && maxplayers > 1 )
                            {
                                if ( P.amulet == 0 )
                                {
                                    if ( numplayers != maxplayers )
                                        return(cclib_error(result,"numplayers != maxplayers"));
                                    else if ( tetris_playersalive(tmp,tmp,gametxid,maxplayers,gameheight,gametx) > 1 )
                                        return(cclib_error(result,"highlander must be a winner or last one standing"));
                                }
                                cashout += numplayers * buyin;
                            }
                            if ( cashout >= txfee )
                            {
                                if ( (inputsum= AddCClibInputs(cp,mtx,tetrispk,cashout,16,cp->unspendableCCaddr)) > (uint64_t)P.gold*mult )
                                    CCchange = (inputsum - cashout);
                                mtx.vout.push_back(CTxOut(cashout,CScript() << ParseHex(HexStr(mypk)) << OP_CHECKSIG));
                            }
                        }
                    }
                    mtx.vout.push_back(MakeCC1vout(cp->evalcode,CCchange + (batonvalue-3*txfee),tetrispk));
                    Myprivkey(mypriv);
                    CCaddr1of2set(cp,tetrispk,mypk,mypriv,mytetrisaddr);
                    CScript opret;
                    if ( pname.size() == 0 )
                        pname = Tetris_pname;
                    if ( newdata.size() == 0 )
                    {
                        opret = tetris_highlanderopret(funcid, gametxid, regslot, mypk, nodata,pname);
                        rawtx = FinalizeCCTx(0,cp,mtx,mypk,txfee,opret);
                        //fprintf(stderr,"nodata finalizetx.(%s)\n",rawtx.c_str());
                    }
                    else
                    {
                        opret = tetris_highlanderopret(funcid, gametxid, regslot, mypk, newdata,pname);
                        char seedstr[32];
                        sprintf(seedstr,"%llu",(long long)seed);
                        std::vector<uint8_t> vopretNonfungible;
                        GetOpReturnData(opret, vopretNonfungible);
                        rawtx = FinalizeCCTx(0, cp, mtx, mypk, txfee, EncodeTokenCreateOpRet('c', Mypubkey(), std::string(seedstr), gametxid.GetHex(), vopretNonfungible));
                    }
                    return(tetris_rawtxresult(result,rawtx,1));
                }
                result.push_back(Pair("result","success"));
            } else fprintf(stderr,"illegal game err.%d\n",err);
        } else fprintf(stderr,"parameters only n.%d\n",n);
    }
    return(result);
}

UniValue tetris_bailout(uint64_t txfee,struct CCcontract_info *cp,cJSON *params)
{
    return(tetris_finishgame(txfee,cp,params,"bailout"));
}

UniValue tetris_highlander(uint64_t txfee,struct CCcontract_info *cp,cJSON *params)
{
    return(tetris_finishgame(txfee,cp,params,"highlander"));
}

UniValue tetris_gameinfo(uint64_t txfee,struct CCcontract_info *cp,cJSON *params)
{
    UniValue result(UniValue::VOBJ),a(UniValue::VARR); int32_t i,n,gameheight,maxplayers,numvouts; uint256 txid; CTransaction tx; int64_t buyin; uint64_t seed; bits256 t; char mytetrisaddr[64],str[64]; CPubKey mypk,tetrispk;
    result.push_back(Pair("name","tetris"));
    result.push_back(Pair("method","gameinfo"));
    if ( params != 0 && (n= cJSON_GetArraySize(params)) > 0 )
    {
        if ( n > 0 )
        {
            txid = juint256(jitem(params,0));
            result.push_back(Pair("gametxid",txid.GetHex()));
            if ( tetris_isvalidgame(cp,gameheight,tx,buyin,maxplayers,txid,0) == 0 )
            {
                result.push_back(Pair("result","success"));
                result.push_back(Pair("gameheight",(int64_t)gameheight));
                mypk = pubkey2pk(Mypubkey());
                tetrispk = GetUnspendable(cp,0);
                GetCCaddress1of2(cp,mytetrisaddr,tetrispk,mypk);
                //fprintf(stderr,"mytetrisaddr.%s\n",mytetrisaddr);
                seed = tetris_gamefields(result,maxplayers,buyin,txid,mytetrisaddr);
                result.push_back(Pair("seed",(int64_t)seed));
                for (i=0; i<maxplayers; i++)
                {
                    if ( CCgettxout(txid,i+1,1,0) < 0 )
                    {
                        UniValue obj(UniValue::VOBJ);
                        tetris_gameplayerinfo(cp,obj,txid,tx,i+1,maxplayers,mytetrisaddr);
                        a.push_back(obj);
                    }
                    else if ( 0 )
                    {
                        sprintf(str,"vout %d+1 is unspent",i);
                        result.push_back(Pair("unspent",str));
                    }
                }
                result.push_back(Pair("players",a));
            } else return(cclib_error(result,"couldnt find valid game"));
        } else return(cclib_error(result,"couldnt parse params"));
    } else return(cclib_error(result,"missing txid in params"));
    return(result);
}

UniValue tetris_pending(uint64_t txfee,struct CCcontract_info *cp,cJSON *params)
{
    UniValue result(UniValue::VOBJ),a(UniValue::VARR); int64_t buyin; uint256 txid,hashBlock; CTransaction tx; int32_t openslots,maxplayers,numplayers,gameheight,nextheight,vout,numvouts; CPubKey tetrispk; char coinaddr[64];
    std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> > unspentOutputs;
    tetrispk = GetUnspendable(cp,0);
    GetCCaddress(cp,coinaddr,tetrispk);
    SetCCunspents(unspentOutputs,coinaddr);
    nextheight = komodo_nextheight();
    for (std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> >::const_iterator it=unspentOutputs.begin(); it!=unspentOutputs.end(); it++)
    {
        txid = it->first.txhash;
        vout = (int32_t)it->first.index;
        //char str[65]; fprintf(stderr,"%s check %s/v%d %.8f\n",coinaddr,uint256_str(str,txid),vout,(double)it->second.satoshis/COIN);
        if ( it->second.satoshis != txfee || vout != 0 ) // reject any that are not highlander markers
            continue;
        if ( tetris_isvalidgame(cp,gameheight,tx,buyin,maxplayers,txid,1) == 0 && nextheight <= gameheight+TETRIS_MAXKEYSTROKESGAP )
        {
            tetris_playersalive(openslots,numplayers,txid,maxplayers,gameheight,tx);
            if ( openslots > 0 )
                a.push_back(txid.GetHex());
        }
    }
    result.push_back(Pair("result","success"));
    tetris_univalue(result,"pending",-1,-1);
    result.push_back(Pair("pending",a));
    result.push_back(Pair("numpending",(int64_t)a.size()));
    return(result);
}

UniValue tetris_players(uint64_t txfee,struct CCcontract_info *cp,cJSON *params)
{
    UniValue result(UniValue::VOBJ),a(UniValue::VARR); int64_t buyin; uint256 tokenid,gametxid,txid,hashBlock; CTransaction playertx,tx; int32_t maxplayers,vout,numvouts; std::vector<uint8_t> playerdata; CPubKey tetrispk,mypk,pk; std::string symbol,pname; char coinaddr[64];
    std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> > unspentOutputs;
    tetrispk = GetUnspendable(cp,0);
    mypk = pubkey2pk(Mypubkey());
    GetTokensCCaddress(cp,coinaddr,mypk);
    SetCCunspents(unspentOutputs,coinaddr);
    tetris_univalue(result,"players",-1,-1);
    for (std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> >::const_iterator it=unspentOutputs.begin(); it!=unspentOutputs.end(); it++)
    {
        txid = it->first.txhash;
        vout = (int32_t)it->first.index;
        //char str[65]; fprintf(stderr,"%s check %s/v%d %.8f\n",coinaddr,uint256_str(str,txid),vout,(double)it->second.satoshis/COIN);
        if ( it->second.satoshis != 1 || vout > 1 )
            continue;
        if ( tetris_playerdata(cp,gametxid,tokenid,pk,playerdata,symbol,pname,txid) == 0 )//&& pk == mypk )
        {
            a.push_back(txid.GetHex());
            //a.push_back(Pair("playerdata",tetris_playerobj(playerdata)));
        }
    }
    result.push_back(Pair("playerdata",a));
    result.push_back(Pair("numplayerdata",(int64_t)a.size()));
    return(result);
}

UniValue tetris_games(uint64_t txfee,struct CCcontract_info *cp,cJSON *params)
{
    UniValue result(UniValue::VOBJ),a(UniValue::VARR),b(UniValue::VARR); uint256 txid,hashBlock,gametxid,tokenid,playertxid; int32_t vout,maxplayers,gameheight,numvouts; CPubKey tetrispk,mypk; char coinaddr[64]; CTransaction tx,gametx; int64_t buyin;
    std::vector<std::pair<CAddressIndexKey, CAmount> > addressIndex;
    //std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> > unspentOutputs;
    tetrispk = GetUnspendable(cp,0);
    mypk = pubkey2pk(Mypubkey());
    GetCCaddress1of2(cp,coinaddr,tetrispk,mypk);
    //SetCCunspents(unspentOutputs,coinaddr);
    SetCCtxids(addressIndex,coinaddr);
    tetris_univalue(result,"games",-1,-1);
    for (std::vector<std::pair<CAddressIndexKey, CAmount> >::const_iterator it=addressIndex.begin(); it!=addressIndex.end(); it++)
    //for (std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> >::const_iterator it=unspentOutputs.begin(); it!=unspentOutputs.end(); it++)
    {
        txid = it->first.txhash;
        vout = (int32_t)it->first.index;
        //char str[65]; fprintf(stderr,"%s check %s/v%d %.8f\n",coinaddr,uint256_str(str,txid),vout,(double)it->second.satoshis/COIN);
        if ( vout == 0 )
        {
            if ( myGetTransaction(txid,tx,hashBlock) != 0 && (numvouts= tx.vout.size()) > 1 )
            {
                if ( tetris_registeropretdecode(gametxid,tokenid,playertxid,tx.vout[numvouts-1].scriptPubKey) == 'R' )
                {
                    if ( tetris_isvalidgame(cp,gameheight,gametx,buyin,maxplayers,gametxid,0) == 0 )
                    {
                        if ( CCgettxout(txid,vout,1,0) < 0 )
                            b.push_back(gametxid.GetHex());
                        else a.push_back(gametxid.GetHex());
                    }
                }
            }
        }
    }
    result.push_back(Pair("pastgames",b));
    result.push_back(Pair("games",a));
    result.push_back(Pair("numgames",(int64_t)(a.size()+b.size())));
    return(result);
}

UniValue tetris_setname(uint64_t txfee,struct CCcontract_info *cp,cJSON *params)
{
    UniValue result(UniValue::VOBJ); int32_t n; char *namestr = 0;
    tetris_univalue(result,"setname",-1,-1);
    if ( params != 0 && (n= cJSON_GetArraySize(params)) > 0 )
    {
        if ( n > 0 )
        {
            if ( (namestr= jstri(params,0)) != 0 )
            {
                result.push_back(Pair("result","success"));
                result.push_back(Pair("pname",namestr));
                tetris_pname = namestr;
                return(result);
            }
        }
    }
    result.push_back(Pair("result","error"));
    result.push_back(Pair("error","couldnt get name"));
    return(result);
}

bool tetris_validate(struct CCcontract_info *cp,int32_t height,Eval *eval,const CTransaction tx)
{
    CScript scriptPubKey; std::vector<uint8_t> vopret; uint8_t *script,e,f,funcid,tokentx=0; int32_t i,maxplayers,enabled = 0,decoded=0,regslot,ind,err,dispflag,gameheight,score,numvouts; CTransaction vintx,gametx; CPubKey pk; uint256 hashBlock,gametxid,txid,tokenid,batontxid,playertxid,ptxid; int64_t buyin,cashout; std::vector<uint8_t> playerdata,keystrokes; std::string symbol,pname;
    if ( strcmp(ASSETCHAINS_SYMBOL,"ROGUE") == 0 )
    {
        if (height < 21274 )
            return(true);
        else if ( height > 50000 )
            enabled = 1;
    } else enabled = 1;
    if ( (numvouts= tx.vout.size()) > 1 )
    {
        txid = tx.GetHash();
        scriptPubKey = tx.vout[numvouts-1].scriptPubKey;
        GetOpReturnData(scriptPubKey,vopret);
        if ( vopret.size() > 2 )
        {
            script = (uint8_t *)vopret.data();
            funcid = script[1];
            if ( (e= script[0]) == EVAL_TOKENS )
            {
                tokentx = funcid;
                if ( (funcid= tetris_highlanderopretdecode(gametxid,tokenid,regslot,pk,playerdata,symbol,pname,scriptPubKey)) == 0 )
                {
                    if ( (funcid= tetris_registeropretdecode(gametxid,tokenid,playertxid,scriptPubKey)) == 0 )
                    {
                        fprintf(stderr,"ht.%d couldnt decode tokens opret (%c)\n",height,script[1]);
                    } else e = EVAL_TETRIS, decoded = 1;
                } else e = EVAL_TETRIS, decoded = 1;
            }
            if ( e == EVAL_TETRIS )
            {
                //fprintf(stderr,"ht.%d tetris.(%c)\n",height,script[1]);
                if ( decoded == 0 )
                {
                    switch ( funcid )
                    {
                        case 'G': // seems just need to make sure no vout abuse is left to do
                            gametx = tx;
                            gametxid = tx.GetHash();
                            gameheight = height;
                            if ( (err= tetris_isvalidgame(cp,gameheight,gametx,buyin,maxplayers,zeroid,0)) != 0 )
                            {
                                fprintf(stderr,"height.%d %s tetris_isvalidgame error.%d\n",height,gametxid.GetHex().c_str(),err);
                                return eval->Invalid("invalid gametxid");
                            }
                            //fprintf(stderr,"height.%d %s tetris_isvalidgame\n",height,gametxid.GetHex().c_str());
                            return(true);
                            break;
                        case 'R':
                            if ( (funcid= tetris_registeropretdecode(gametxid,tokenid,playertxid,scriptPubKey)) != 'R' )
                            {
                                return eval->Invalid("couldnt decode register opret");
                            }
                            // baton is created
                            // validation is done below
                            break;
                        case 'K':
                            if ( (funcid= tetris_keystrokesopretdecode(gametxid,batontxid,pk,keystrokes,scriptPubKey)) != 'K' )
                            {
                                return eval->Invalid("couldnt decode keystrokes opret");
                            }
                            // spending the baton proves it is the user if the pk is the signer
                            return(true);
                            break;
                        case 'H': case 'Q':
                            // done in the next switch statement as there are some H/Q tx with playerdata which would skip this section
                            break;
                        default:
                            return eval->Invalid("illegal tetris non-decoded funcid");
                            break;
                    }
                }
                switch ( funcid )
                {
                    case 'R': // register
                        // verify vout amounts are as they should be and no vins that shouldnt be
                        return(true);
                    case 'H': // win
                    case 'Q': // bailout
                        if ( (f= tetris_highlanderopretdecode(gametxid,tokenid,regslot,pk,playerdata,symbol,pname,scriptPubKey)) != funcid )
                        {
                            //fprintf(stderr,"height.%d couldnt decode H/Q opret\n",height);
                            //if ( height > 20000 )
                            return eval->Invalid("couldnt decode H/Q opret");
                        }
                        // verify pk belongs to this tx
                        if ( tokentx == 'c' && playerdata.size() > 0 )
                        {
                            static char laststr[512]; char cashstr[512];
                            if ( tetris_playerdata_validate(&cashout,ptxid,cp,playerdata,gametxid,pk) < 0 )
                            {
                                sprintf(cashstr,"tokentx.(%c) decoded.%d ht.%d gametxid.%s player.%s invalid playerdata[%d]\n",tokentx,decoded,height,gametxid.GetHex().c_str(),ptxid.GetHex().c_str(),(int32_t)playerdata.size());
                                if ( strcmp(laststr,cashstr) != 0 )
                                {
                                    strcpy(laststr,cashstr);
                                    fprintf(stderr,"%s\n",cashstr);
                                }
                                if ( enabled != 0 )
                                    return eval->Invalid("mismatched playerdata");
                            }
                            if ( funcid == 'H' )
                                cashout *= 2;
                            sprintf(cashstr,"tokentx.(%c) decoded.%d ht.%d txid.%s %.8f vs vout2 %.8f",tokentx,decoded,height,txid.GetHex().c_str(),(double)cashout/COIN,(double)tx.vout[2].nValue/COIN);
                            if ( strcmp(laststr,cashstr) != 0 )
                            {
                                strcpy(laststr,cashstr);
                                fprintf(stderr,"%s\n",cashstr);
                            }
                            if ( enabled != 0 && tx.vout[2].nValue != cashout )
                                return eval->Invalid("mismatched cashout amount");
                        }
                        if ( funcid == 'Q' )
                        {
                            // verify vin/vout
                        }
                        else // 'H'
                        {
                            // verify vin/vout and proper payouts
                        }
                        return(true);
                        break;
                    default:
                        return eval->Invalid("illegal tetris funcid");
                        break;
                }
            } else return eval->Invalid("illegal evalcode");
        } else return eval->Invalid("opret too small");
    } else return eval->Invalid("not enough vouts");
    return(true);
}
#endif

