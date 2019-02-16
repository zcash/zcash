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

WINDOW *initscr()
{
    if ( stdscr == 0 )
        stdscr = (WINDOW *)calloc(1,sizeof(*stdscr));
    return(stdscr);
}

void endwin()
{
    if ( stdscr != 0 )
        free(stdscr), stdscr = 0;
    endwinflag = 1;
}

int isendwin(void)
{
    return(endwinflag);
}

int wrefresh(WINDOW *win)
{
    return(0);
}

int refresh(void)
{
    endwinflag = 0;
    return(wrefresh(stdscr));
}

int wnoutrefresh(WINDOW *win)
{
    return(0);
}

int doupdate(void)
{
    return(0);
}

int redrawwin(WINDOW *win)
{
    return(wrefresh(win));
}

int wredrawln(WINDOW *win, int beg_line, int num_lines)
{
    return(wrefresh(win));
}

int werase(WINDOW *win)
{
    
}

int erase(void)
{
    return(werase(stdscr));
}

int wclear(WINDOW *win)
{
    
}

int clear(void)
{
    return(wclear(stdscr));
}

int wclrtobot(WINDOW *win)
{
    
}

int clrtobot(void)
{
    return(wclrtobot(stdscr));
}

int wclrtoeol(WINDOW *win)
{
    
}

int clrtoeol(void)
{
    return(wclrtoeol(stdscr));
}
