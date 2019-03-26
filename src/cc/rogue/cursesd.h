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

#ifndef H_CURSESD_H
#define H_CURSESD_H

#define KEY_OFFSET 0x100
#define KEY_DOWN         (KEY_OFFSET + 0x02) /* Down arrow key */
#define KEY_UP           (KEY_OFFSET + 0x03) /* Up arrow key */
#define KEY_LEFT         (KEY_OFFSET + 0x04) /* Left arrow key */
#define KEY_RIGHT        (KEY_OFFSET + 0x05) /* Right arrow key */


#define COLOR_BLACK   0

#ifdef PDC_RGB        /* RGB */
# define COLOR_RED    1
# define COLOR_GREEN  2
# define COLOR_BLUE   4
#else                 /* BGR */
# define COLOR_BLUE   1
# define COLOR_GREEN  2
# define COLOR_RED    4
#endif

#define COLOR_CYAN    (COLOR_BLUE | COLOR_GREEN)
#define COLOR_MAGENTA (COLOR_RED | COLOR_BLUE)
#define COLOR_YELLOW  (COLOR_RED | COLOR_GREEN)

#define COLOR_WHITE   7

#define	LINES	24
#define	COLS	80

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <memory.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>	/* we need va_list */
#include <stddef.h>	/* we want wchar_t */
#include <stdbool.h>
#include <ctype.h>

#include <time.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>

#define ERR (-1)

struct cursesd_info
{
    uint8_t screen[LINES][COLS];
    int32_t x,y;
};
typedef struct cursesd_info WINDOW;
extern WINDOW *stdscr,*curscr;
extern int32_t ESCDELAY;
typedef char chtype;

#ifndef __MINGW32__
int32_t getch(void); // stub
#endif

int32_t md_readchar(void); // stub

WINDOW *initscr(void);
int32_t endwin(void);
int32_t isendwin(void);
//SCREEN *newterm(const char *type, FILE *outfd, FILE *infd);
//SCREEN *set_term(SCREEN *new);
//void delscreen(SCREEN* sp);

int32_t refresh(void);
int32_t wrefresh(WINDOW *win);
//int32_t wnoutrefresh(WINDOW *win);
//int32_t doupdate(void);
int32_t redrawwin(WINDOW *win);
int32_t wredrawln(WINDOW *win, int32_t beg_line, int32_t num_lines);

int32_t erase(void);
int32_t werase(WINDOW *win);
int32_t clear(void);
int32_t wclear(WINDOW *win);
int32_t clrtobot(void);
int32_t wclrtobot(WINDOW *win);
int32_t clrtoeol(void);
int32_t wclrtoeol(WINDOW *win);

int32_t standout(void);
int32_t standend(void);
int32_t raw(void);
int32_t noecho(void);
int32_t flushinp(void);
int32_t keypad(WINDOW *win, bool bf);

int32_t clearok(WINDOW *win, bool bf);
int32_t idlok(WINDOW *win, bool bf);
int32_t leaveok(WINDOW *win, bool bf);

WINDOW *newwin(int32_t nlines,int32_t ncols,int32_t begin_y,int32_t begin_x); // only LINES,COLS,0,0
int32_t delwin(WINDOW *win);
int32_t touchwin(WINDOW *win); // stub
WINDOW *subwin(WINDOW *orig, int32_t nlines, int32_t ncols, int32_t begin_y, int32_t begin_x); // stub
int32_t mvwin(WINDOW *win, int32_t y, int32_t x); // stub
int32_t mvcur(int32_t oldrow, int32_t oldcol, int32_t newrow, int32_t newcol);

char erasechar(void); // stub
char killchar(void); // stub

int32_t move(int32_t y, int32_t x);
int32_t wmove(WINDOW *win, int32_t y, int32_t x);

chtype inch(void);
chtype winch(WINDOW *win);
chtype mvinch(int32_t y, int32_t x);
chtype mvwinch(WINDOW *win, int32_t y, int32_t x);

int32_t addch(chtype ch);
int32_t waddch(WINDOW *win, chtype ch);
int32_t mvaddch(int32_t y, int32_t x, chtype ch);
int32_t mvwaddch(WINDOW *win, int32_t y, int32_t x, chtype ch);

int32_t addstr(const char *str);
int32_t addnstr(const char *str, int32_t n);
int32_t waddstr(WINDOW *win, const char *str);
int32_t waddnstr(WINDOW *win, const char *str, int32_t n);
int32_t mvaddstr(int32_t y, int32_t x, const char *str);
int32_t mvaddnstr(int32_t y, int32_t x, const char *str, int32_t n);
int32_t mvwaddstr(WINDOW *win, int32_t y, int32_t x, const char *str);
int32_t mvwaddnstr(WINDOW *win, int32_t y, int32_t x, const char *str, int32_t n);

int32_t wgetnstr(WINDOW *win, char *str, int32_t n); // stub
int32_t printw(char *fmt,...);
int32_t wprintw(WINDOW *win,char *fmt,...);
int32_t mvprintw(int32_t y,int32_t x,char *fmt,...);
int32_t mvwprintw(WINDOW *win,int32_t y,int32_t x,char *fmt,...);

char *unctrl(char c);

#define A_CHARTEXT 0xff
#define baudrate() 9600
#define getmaxx(a) COLS
#define getmaxy(a) LINES
#define getyx(win,_argfory,_argforx) _argfory = win->y, _argforx = win->x

// functions with only visible effects
#define wrefresh(win) 0
#define wnoutrefresh(win) 0
#define doupdate() 0
#define touchwin(win) 0
#define standout() 0
#define standend() 0
#define raw() 0
#define keypad(win,bf) 0
#define noecho() 0
#define flushinp() 0
#define clearok(win,bf) 0
#define idlok(win,bf) 0
#define leaveok(win,bf) 0
#define halfdelay(x) 0
#define nocbreak() 0
#define cbreak() 0
#define curs_set(x) 0

// for tetris
#define init_pair(a,b,c) 0
#define start_color() 0
#define box(a,b,c) 0
#define A_REVERSE 0
#define COLOR_PAIR(a) 0
#define timeout(x) 0
// end for tetris

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

#endif

