/*
 * Various input/output functions
 *
 * @(#)io.c	4.32 (Berkeley) 02/05/99
 */

//#include <stdarg.h>
//#include <curses.h>
//#include <ctype.h>
//#include <string.h>

#include "rogue.h"

/*
 * msg:
 *	Display a message at the top of the screen.
 */
#define MAXMSG	(NUMCOLS - sizeof "--More--")

static char msgbuf[2*MAXMSG+1];
static int newpos = 0;

/* VARARGS1 */
int
msg(struct rogue_state *rs,char *fmt, ...)
{
    va_list args;
    
    /*
     * if the string is "", just clear the line
     */
    if (*fmt == '\0')
    {
        move(0, 0);
        clrtoeol();
        mpos = 0;
        return ~ESCAPE;
    }
    /*
     * otherwise add to the message and flush it out
     */
    va_start(args, fmt);
    doadd(rs,fmt, args);
    va_end(args);
    return endmsg(rs);
}

/*
 * addmsg:
 *	Add things to the current message
 */
/* VARARGS1 */
void
addmsg(struct rogue_state *rs,char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    doadd(rs,fmt, args);
    va_end(args);
}

/*
 * endmsg:
 *	Display a new msg (giving him a chance to see the previous one
 *	if it is up there with the --More--)
 */
int
endmsg(struct rogue_state *rs)
{
    char ch;

    if (save_msg)
        strcpy(huh, msgbuf);
    if (mpos)
    {
        look(rs,FALSE);
        mvaddstr(0, mpos, "--More--");
        if ( rs->sleeptime != 0 )
            refresh();
        if (!msg_esc)
            wait_for(rs,' ');
        else
        {
            while ((ch = readchar(rs)) != ' ')
                if (ch == ESCAPE)
                {
                    msgbuf[0] = '\0';
                    mpos = 0;
                    newpos = 0;
                    msgbuf[0] = '\0';
                    return ESCAPE;
                }
        }
    }
    /*
     * All messages should start with uppercase, except ones that
     * start with a pack addressing character
     */
    if (islower(msgbuf[0]) && !lower_msg && msgbuf[1] != ')')
        msgbuf[0] = (char) toupper(msgbuf[0]);
    mvaddstr(0, 0, msgbuf);
    clrtoeol();
    mpos = newpos;
    newpos = 0;
    msgbuf[0] = '\0';
    if ( rs->sleeptime != 0 )
        refresh();
    return ~ESCAPE;
}

/*
 * doadd:
 *	Perform an add onto the message buffer
 */
void
doadd(struct rogue_state *rs,char *fmt, va_list args)
{
    static char buf[MAXSTR];

    /*
     * Do the printf into buf
     */
    vsprintf(buf, fmt, args);
    if (strlen(buf) + newpos >= MAXMSG)
        endmsg(rs);
    strcat(msgbuf, buf);
    newpos = (int) strlen(msgbuf);
}

/*
 * step_ok:
 *	Returns true if it is ok to step on ch
 */
int
step_ok(int ch)
{
    switch (ch)
    {
	case ' ':
	case '|':
	case '-':
	    return FALSE;
	default:
	    return (!isalpha(ch));
    }
}

/*
 * readchar:
 *	Reads and returns a character, checking for gross input errors
 */
char
readchar(struct rogue_state *rs)
{
    char c,ch = -1;
    if ( rs != 0 && rs->guiflag == 0 )
    {
        static uint32_t counter;
        if ( rs->ind < rs->numkeys )
        {
            c = rs->keystrokes[rs->ind++];
            if ( 0 )
            {
                static FILE *fp; static int32_t counter;
                if ( fp == 0 )
                    fp = fopen("log","wb");
                if ( fp != 0 )
                {
                    fprintf(fp,"%d: (%c) hp.%d num.%d gold.%d seed.%llu\n",counter,c,pstats.s_hpt,num_packitems(rs),purse,(long long)seed);
                    fflush(fp);
                    counter++;
                }
            }
            while ( c == 'Q' && rs->ind < rs->numkeys )
            {
                //fprintf(stderr,"Got 'Q' next (%c)\n",rs->keystrokes[rs->ind]); sleep(2);
                if ( rs->keystrokes[rs->ind] == 'y' )
                    return(c);
                rs->ind++;
                c = rs->keystrokes[rs->ind++];
            }
            return(c);
        }
        if ( rs->replaydone != 0 && counter++ < 3 )
            fprintf(stderr,"replay finished but readchar called\n");
        rs->replaydone = (uint32_t)time(NULL);
        if ( counter < 3 || (counter & 1) == 0 )
            return('y');
        else return(ESCAPE);
    }
    if ( rs == 0 || rs->guiflag != 0 )
    {
        ch = (char) md_readchar();
        
        if (ch == 3)
        {
            _quit();
            return(27);
        }
        if ( rs != 0 && rs->guiflag != 0 )
        {
            if ( rs->num < sizeof(rs->buffered) )
            {
                rs->buffered[rs->num++] = ch;
                if ( rs->num > (sizeof(rs->buffered)*9)/10 && rs->needflush == 0 )
                {
                    rs->needflush = (uint32_t)time(NULL);
                    //fprintf(stderr,"needflush.%u %d of %d\n",rs->needflush,rs->num,(int32_t)sizeof(rs->buffered));
                    //sleep(3);
                }
            } else fprintf(stderr,"buffer filled without flushed\n");
        }
    } else fprintf(stderr,"readchar rs.%p non-gui error?\n",rs);
    return(ch);
}

/*
 * status:
 *	Display the important stats line.  Keep the cursor where it was.
 */
void
status(struct rogue_state *rs)
{
    register int oy, ox, temp;
    static int hpwidth = 0;
    static int s_hungry = 0;
    static int s_lvl = 0;
    static int s_pur = -1;
    static int s_hp = 0;
    static int s_arm = 0;
    static str_t s_str = 0;
    static int s_exp = 0;
    static char *state_name[] =
    {
	"", "Hungry", "Weak", "Faint"
    };

    /*
     * If nothing has changed since the last status, don't
     * bother.
     */
    temp = (cur_armor != NULL ? cur_armor->o_arm : pstats.s_arm);
    if (s_hp == pstats.s_hpt && s_exp == pstats.s_exp && s_pur == purse
	&& s_arm == temp && s_str == pstats.s_str && s_lvl == level
	&& s_hungry == hungry_state
	&& !stat_msg
	)
	    return;

    s_arm = temp;

    getyx(stdscr, oy, ox);
    if (s_hp != max_hp)
    {
	temp = max_hp;
	s_hp = max_hp;
	for (hpwidth = 0; temp; hpwidth++)
	    temp /= 10;
    }

    /*
     * Save current status
     */
    s_lvl = level;
    s_pur = purse;
    s_hp = pstats.s_hpt;
    s_str = pstats.s_str;
    s_exp = pstats.s_exp; 
    s_hungry = hungry_state;

    if (stat_msg)
    {
	move(0, 0);
        msg(rs,"Level: %d  Gold: %-5d  Hp: %*d(%*d)  Str: %2d(%d)  Arm: %-2d  Exp: %d/%ld  %s",
	    level, purse, hpwidth, pstats.s_hpt, hpwidth, max_hp, pstats.s_str,
	    max_stats.s_str, 10 - s_arm, pstats.s_lvl, pstats.s_exp,
	    state_name[hungry_state]);
    }
    else
    {
	move(STATLINE, 0);
                
        printw("Level: %d  Gold: %-5d  Hp: %*d(%*d)  Str: %2d(%d)  Arm: %-2d  Exp: %d/%d  %s",
	    level, purse, hpwidth, pstats.s_hpt, hpwidth, max_hp, pstats.s_str,
	    max_stats.s_str, 10 - s_arm, pstats.s_lvl, pstats.s_exp,
	    state_name[hungry_state]);
    }

    clrtoeol();
    move(oy, ox);
}

/*
 * wait_for
 *	Sit around until the guy types the right key
 */
void
wait_for(struct rogue_state *rs,int ch)
{
    register char c;

    if (ch == '\n')
        while ((c = readchar(rs)) != '\n' && c != '\r')
        {
            if ( rs->replaydone != 0 )
                return;
            continue;
        }
    else
        while (readchar(rs) != ch)
        {
            if ( rs->replaydone != 0 )
                return;
            continue;
        }
}

/*
 * show_win:
 *	Function used to display a window and wait before returning
 */
void
show_win(struct rogue_state *rs,char *message)
{
    WINDOW *win;

    win = hw;
    wmove(win, 0, 0);
    waddstr(win, message);
    touchwin(win);
    wmove(win, hero.y, hero.x);
    wrefresh(win);
    wait_for(rs,' ');
    clearok(curscr, TRUE);
    touchwin(stdscr);
}
