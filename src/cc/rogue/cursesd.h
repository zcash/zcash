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

struct cursesd_info
{
    uint8_t screen[LINES][COLS];
} *stdscr;
typedef struct cursesd_info WINDOW;

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

WINDOW *initscr(void);
int endwin(void);
int isendwin(void);
//SCREEN *newterm(const char *type, FILE *outfd, FILE *infd);
//SCREEN *set_term(SCREEN *new);
//void delscreen(SCREEN* sp);

int refresh(void);
int wrefresh(WINDOW *win);
//int wnoutrefresh(WINDOW *win);
//int doupdate(void);
int redrawwin(WINDOW *win);
int wredrawln(WINDOW *win, int beg_line, int num_lines);

int erase(void);
int werase(WINDOW *win);
int clear(void);
int wclear(WINDOW *win);
int clrtobot(void);
int wclrtobot(WINDOW *win);
int clrtoeol(void);
int wclrtoeol(WINDOW *win);

#define standout()
#define standend()
#define refresh()
#define raw()
#define noecho()
#define flushinp()
#define clear()
#define clrtoeol()

#define addch(a)
#define werase(a)
#define wclear(a)
#define delwin(a)
#define addstr(a)
#define touchwin(a)
#define idlok(a,b)
#define clearok(a,b)
#define keypad(a,b)
#define leaveok(a,b)
#define waddch(a,b)
#define waddstr(a,b)
#define move(a,b)
#define mvwin(a,b,c)
#define wmove(a,b,c)
#define mvaddch(a,b,c)
#define mvaddstr(a,b,c)
#define wgetnstr(a,b,c)
#define getyx(a,b,c)
#define mvcur(a,b,c,d)
#define mvwaddch(a,b,c,d)
#define mvprintw(...)
#define printw(...)
#define wprintw(...)
#define mvwprintw(...)


#define A_CHARTEXT 0xff
#define inch() 0
#define endwin() 1
#define isendwin() 0
#define baudrate() 9600
#define killchar() 3
#define erasechar() 8
#define wclrtoeol(a) 0
#define wrefresh(a) 0
#define unctrl(a) "^x"
#define getmaxx(a) COLS
#define getmaxy(a) LINES
#define mvinch(a,b) '.'
#define mvwinch(a,b,c) 0
#define newwin(a,b,c,d) 0
#define subwin(a,b,c,d,e) 0

#endif
