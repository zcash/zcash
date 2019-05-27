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

#include "cursesd.h"

static int32_t endwinflag;
WINDOW *stdscr,*curscr;
int32_t ESCDELAY;

WINDOW *newwin(int32_t nlines,int32_t ncols,int32_t begin_y,int32_t begin_x)
{
    WINDOW *scr = 0;
    if ( nlines == LINES && ncols == COLS && begin_y == 0 && begin_x == 0 )
        scr = (WINDOW *)calloc(1,sizeof(*stdscr));
    curscr = scr;
    return(scr);
}

WINDOW *initscr()
{
    if ( stdscr == 0 )
        stdscr = newwin(LINES,COLS,0,0);
    return(stdscr);
}

int32_t delwin(WINDOW *win)
{
    free(win);
    return(0);
}

int32_t wmove(WINDOW *win, int32_t y, int32_t x)
{
    win->y = y;
    win->x = x;
    return(0);
}

int32_t move(int32_t y, int32_t x)
{
    return(wmove(stdscr,y,x));
}

int32_t werase(WINDOW *win)
{
    memset(win->screen,' ',sizeof(win->screen));
    return(0);
}

int32_t wclear(WINDOW *win)
{
    werase(win);
    clearok(win,TRUE);
    return(0);
}

int32_t wclrtoeol(WINDOW *win)
{
    if ( win->x < COLS-1 )
        memset(&win->screen[win->y][win->x],' ',COLS - win->x);
    return(0);
}

int32_t wclrtobot(WINDOW *win)
{
    wclrtoeol(win);
    if ( win->y < LINES-1 )
        memset(&win->screen[win->y+1][0],' ',COLS);
    return(0);
}

int32_t erase(void)
{
    return(werase(stdscr));
}

int32_t clear(void)
{
    return(wclear(stdscr));
}

int32_t clrtobot(void)
{
    return(wclrtobot(stdscr));
}

int32_t clrtoeol(void)
{
    return(wclrtoeol(stdscr));
}

int32_t waddch(WINDOW *win, chtype ch)
{
    int32_t i;
    if ( ch == '\t' )
    {
        for (i=0; i<8; i++)
        {
            if ( win->x >= COLS-1 )
                break;
            win->x++;
            if ( (win->x & 7) == 0 )
                break;
        }
    }
    else if ( ch == '\n' )
    {
        wclrtoeol(win);
        win->x = 0;
        win->y++;
        if ( win->y >= LINES )
            win->y = 0;
    }
    else if ( ch == '\b' )
    {
        if ( win->x > 0 )
            win->x--;
    }
    else
    {
        win->screen[win->y][win->x++] = ch;
        if ( win->x >= COLS )
        {
            win->x = 0;
            win->y++;
            if ( win->y >= LINES )
                win->y = 0;
        }
    }
    return(0);
}

int32_t mvwaddch(WINDOW *win, int32_t y, int32_t x, chtype ch)
{
    win->y = y;
    win->x = x;
    return(waddch(win,ch));
}

int32_t addch(chtype ch)
{
    return(waddch(stdscr,ch));
}

int32_t mvaddch(int32_t y, int32_t x, chtype ch)
{
    return(mvwaddch(stdscr,y,x,ch));
}

int32_t waddstr(WINDOW *win, const char *str)
{
    int32_t i;
    //fprintf(stderr,"%s\n",str);
    for (i=0; str[i]!=0; i++)
        waddch(win,str[i]);
    return(0);
}

int32_t waddnstr(WINDOW *win, const char *str, int32_t n)
{
    int32_t i;
    for (i=0; str[i]!=0 && i<n; i++)
        waddch(win,str[i]);
    return(0);
}

int32_t mvwaddstr(WINDOW *win, int32_t y, int32_t x, const char *str)
{
    win->y = y;
    win->x = x;
    return(waddstr(win,str));
}

int32_t mvwaddnstr(WINDOW *win, int32_t y, int32_t x, const char *str, int32_t n)
{
    win->y = y;
    win->x = x;
    return(waddnstr(win,str,n));
}

int32_t addstr(const char *str)
{
    return(waddstr(stdscr,str));
}

int32_t addnstr(const char *str, int32_t n)
{
    return(waddnstr(stdscr,str,n));
}

int32_t mvaddstr(int32_t y, int32_t x, const char *str)
{
    stdscr->y = y;
    stdscr->x = x;
    return(waddstr(stdscr,str));
}

int32_t mvaddnstr(int32_t y, int32_t x, const char *str, int32_t n)
{
    stdscr->y = y;
    stdscr->x = x;
    return(waddnstr(stdscr,str,n));
}

int32_t printw(char *fmt,...)
{
    char str[512]; int32_t ret; va_list myargs; // Declare a va_list type variable
    va_start(myargs,fmt); // Initialise the va_list variable with the ... after fmt
    ret = vsprintf(str,fmt,myargs); // Forward the '...' to vsprintf
    va_end(myargs); // Clean up the va_list
    return(addstr(str));
}

int32_t wprintw(WINDOW *win,char *fmt,...)
{
    char str[512]; int32_t ret; va_list myargs; // Declare a va_list type variable
    va_start(myargs,fmt); // Initialise the va_list variable with the ... after fmt
    ret = vsprintf(str,fmt,myargs); // Forward the '...' to vsprintf
    va_end(myargs); // Clean up the va_list
    return(waddstr(win,str));
}

int32_t mvprintw(int32_t y,int32_t x,char *fmt,...)
{
    char str[512]; int32_t ret; va_list myargs; // Declare a va_list type variable
    va_start(myargs,fmt); // Initialise the va_list variable with the ... after fmt
    ret = vsprintf(str,fmt,myargs); // Forward the '...' to vsprintf
    va_end(myargs); // Clean up the va_list
    stdscr->y = y;
    stdscr->x = x;
    return(addstr(str));
}

int32_t mvwprintw(WINDOW *win,int32_t y,int32_t x,char *fmt,...)
{
    char str[512]; int32_t ret; va_list myargs; // Declare a va_list type variable
    va_start(myargs,fmt); // Initialise the va_list variable with the ... after fmt
    ret = vsprintf(str,fmt,myargs); // Forward the '...' to vsprintf
    va_end(myargs); // Clean up the va_list
    win->y = y;
    win->x = x;
    return(waddstr(win,str));
}

chtype winch(WINDOW *win)
{
    return(win->screen[win->y][win->x]);
}

chtype inch(void)
{
    return(winch(stdscr));
}

chtype mvwinch(WINDOW *win, int32_t y, int32_t x)
{
    win->y = y;
    win->x = x;
    return(win->screen[win->y][win->x]);
}

chtype mvinch(int32_t y, int32_t x)
{
    return(mvwinch(stdscr,y,x));
}

int32_t mvcur(int32_t oldrow, int32_t oldcol, int32_t newrow, int32_t newcol)
{
    stdscr->y = newrow;
    stdscr->x = newcol;
    return(0);
}

int32_t endwin(void)
{
    if ( stdscr != 0 )
        free(stdscr), stdscr = 0;
    endwinflag = 1;
    return(0);
}

int32_t isendwin(void)
{
    return(endwinflag);
}

int32_t refresh(void)
{
    endwinflag = 0;
    return(wrefresh(stdscr));
}

int32_t redrawwin(WINDOW *win)
{
    return(wrefresh(win));
}

int32_t wredrawln(WINDOW *win, int32_t beg_line, int32_t num_lines)
{
    return(wrefresh(win));
}

// functions with no data side effect
#ifdef they_are_macros
int32_t wrefresh(WINDOW *win)
{
    return(0);
}

int32_t wnoutrefresh(WINDOW *win)
{
    return(0);
}

int32_t doupdate(void)
{
    return(0);
}

int32_t touchwin(WINDOW *win)
{
    return(0);
}

int32_t standout(void)
{
    return(0);
}

int32_t standend(void)
{
    return(0);
}

int32_t raw(void)
{
    return(0);
}

int32_t keypad(WINDOW *win, bool bf)
{
    return(0);
}

int32_t noecho(void)
{
    return(0);
}

int32_t flushinp(void)
{
    return(0);
}

int32_t clearok(WINDOW *win, bool bf)
{
    return(0);
}

int32_t idlok(WINDOW *win, bool bf)
{
    return(0);
}

int32_t leaveok(WINDOW *win, bool bf)
{
    return(0);
}
#endif

int32_t mvwin(WINDOW *win, int32_t y, int32_t x) // stub
{
    fprintf(stderr,"unexpected call to mvwin\n");
    return(0);
}

WINDOW *subwin(WINDOW *orig, int32_t nlines, int32_t ncols, int32_t begin_y, int32_t begin_x)
{
    fprintf(stderr,"unexpected and unsupported call to subwin\n");
    return(0);
}

char erasechar(void)
{
    fprintf(stderr,"unexpected and unsupported call to erasechar\n");
    return(8);
}

char killchar(void)
{
    fprintf(stderr,"unexpected and unsupported call to erasechar\n");
    return(3);
}

int32_t wgetnstr(WINDOW *win, char *str, int32_t n) // stub
{
    fprintf(stderr,"unexpected and unsupported call to mvgetnstr\n");
    return(0);
}

#ifndef __MINGW32__
int32_t getch(void)
{
    fprintf(stderr,"unexpected and unsupported call to getch\n");
    return(0);
}
#endif

int32_t md_readchar(void)
{
    fprintf(stderr,"unexpected and unsupported call to md_readchar\n");
    return(0);
}

char *unctrl(char c)
{
    static char ctrlstr[5];
    sprintf(ctrlstr,"^%%%02x",c);
    return(ctrlstr);
}
