/*
 * Various installation dependent routines
 *
 * @(#)mach_dep.c	4.37 (Berkeley) 05/23/83
 *
 * Rogue: Exploring the Dungeons of Doom
 * Copyright (C) 1980-1983, 1985, 1999 Michael Toy, Ken Arnold and Glenn Wichman
 * All rights reserved.
 *
 * See the file LICENSE.TXT for full copyright and licensing information.
 */

/*
 * The various tuneable defines are:
 *
 *	SCOREFILE	Where/if the score file should live.
 *	ALLSCORES	Score file is top ten scores, not top ten
 *			players.  This is only useful when only a few
 *			people will be playing; otherwise the score file
 *			gets hogged by just a few people.
 *	NUMSCORES	Number of scores in the score file (default 10).
 *	NUMNAME		String version of NUMSCORES (first character
 *			should be capitalized) (default "Ten").
 *	MAXLOAD		What (if any) the maximum load average should be
 *			when people are playing.  Since it is divided
 *			by 10, to specify a load limit of 4.0, MAXLOAD
 *			should be "40".	 If defined, then
 *      LOADAV		Should it use it's own routine to get
 *		        the load average?
 *      NAMELIST	If so, where does the system namelist
 *		        hide?
 *	MAXUSERS	What (if any) the maximum user count should be
 *	                when people are playing.  If defined, then
 *      UCOUNT		Should it use it's own routine to count
 *		        users?
 *      UTMP		If so, where does the user list hide?
 *	CHECKTIME	How often/if it should check during the game
 *			for high load average.
 */

#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <limits.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
//#include <curses.h>
#include "rogue.h"
#include "extern.h"

#define NOOP(x) (x += 0)

# ifndef NUMSCORES
#	define	NUMSCORES	10
#	define	NUMNAME		"Ten"
# endif

unsigned int numscores = NUMSCORES;
char *Numname = NUMNAME;

# ifdef ALLSCORES
bool allscore = TRUE;
# else  /* ALLSCORES */
bool allscore = FALSE;
# endif /* ALLSCORES */

#ifdef CHECKTIME
static int num_checks;		/* times we've gone over in checkout() */
#endif /* CHECKTIME */

/*
 * init_check:
 *	Check out too see if it is proper to play the game now
 */

void
init_check()
{
#if defined(MAXLOAD) || defined(MAXUSERS)
    if (too_much())
    {
	printf("Sorry, %s, but the system is too loaded now.\n", whoami);
	printf("Try again later.  Meanwhile, why not enjoy a%s %s?\n",
	    vowelstr(fruit), fruit);
	if (author())
	    printf("However, since you're a good guy, it's up to you\n");
	else
	    exit(1);
    }
#endif
}

/*
 * open_score:
 *	Open up the score file for future use
 */

void
open_score()
{
#ifdef SCOREFILE
    char *scorefile = SCOREFILE;
     /* 
      * We drop setgid privileges after opening the score file, so subsequent 
      * open()'s will fail.  Just reuse the earlier filehandle. 
      */

    if (scoreboard != NULL) { 
        rewind(scoreboard); 
        return; 
    } 

    scoreboard = fopen(scorefile, "r+");

    if ((scoreboard == NULL) && (errno == ENOENT))
    {
    	scoreboard = fopen(scorefile, "w+");
        md_chmod(scorefile,0664);
    }

    if (scoreboard == NULL) { 
         fprintf(stderr, "Could not open %s for writing: %s\n", scorefile, strerror(errno)); 
         fflush(stderr); 
    } 
#else
    scoreboard = NULL;
#endif
}

/*
 * setup:
 *	Get starting setup for all games
 */

void
setup()
{
#ifdef CHECKTIME
    int  checkout();
#endif

#ifdef DUMP
    md_onsignal_autosave();
#else
    md_onsignal_default();
#endif

#ifdef CHECKTIME
    md_start_checkout_timer(CHECKTIME*60);
    num_checks = 0;
#endif

    raw();				/* Raw mode */
    noecho();				/* Echo off */
    keypad(stdscr,1);
    getltchars();			/* get the local tty chars */
}

/*
 * getltchars:
 *	Get the local tty chars for later use
 */

void
getltchars()
{
    got_ltc = TRUE;
    orig_dsusp = md_dsuspchar();
    md_setdsuspchar( md_suspchar() );
}

/* 
 * resetltchars: 
 *      Reset the local tty chars to original values. 
 */ 
void 
resetltchars(void) 
{ 
    if (got_ltc) {
        md_setdsuspchar(orig_dsusp);
    } 
} 
  
/* 
 * playltchars: 
 *      Set local tty chars to the values we use when playing. 
 */ 
void 
playltchars(void) 
{ 
    if (got_ltc) { 
        md_setdsuspchar( md_suspchar() );
    } 
} 

/*
 * start_score:
 *	Start the scoring sequence
 */

void
start_score()
{
#ifdef CHECKTIME
    md_stop_checkout_timer();
#endif
}

/* 	 	 
 * is_symlink: 	 	 
 *      See if the file has a symbolic link 	 	 
  */ 	 	 
bool 	 	 
is_symlink(char *sp) 	 	 
{ 	 	 
#ifdef S_IFLNK 	 	 
    struct stat sbuf2; 	 	 
 	 	 
    if (lstat(sp, &sbuf2) < 0) 	 	 
        return FALSE; 	 	 
    else 	 	 
        return ((sbuf2.st_mode & S_IFMT) != S_IFREG); 	 	 
#else
	NOOP(sp);
    return FALSE; 	 	 
#endif 
} 

#if defined(MAXLOAD) || defined(MAXUSERS)
/*
 * too_much:
 *	See if the system is being used too much for this game
 */
bool
too_much()
{
#ifdef MAXLOAD
    double avec[3];
#else
    int cnt;
#endif

#ifdef MAXLOAD
    md_loadav(avec);
    if (avec[1] > (MAXLOAD / 10.0))
	return TRUE;
#endif
#ifdef MAXUSERS
    if (ucount() > MAXUSERS)
	return TRUE;
#endif
    return FALSE;
}

/*
 * author:
 *	See if a user is an author of the program
 */
bool
author()
{
#ifdef MASTER
    if (wizard)
	return TRUE;
#endif
    switch (md_getuid())
    {
	case -1:
	    return TRUE;
	default:
	    return FALSE;
    }
}
#endif

#ifdef CHECKTIME
/*
 * checkout:
 *	Check each CHECKTIME seconds to see if the load is too high
 */

checkout(struct rogue_state *rs,int sig)
{
    static char *msgs[] = {
	"The load is too high to be playing.  Please leave in %0.1f minutes",
	"Please save your game.  You have %0.1f minutes",
	"Last warning.  You have %0.1f minutes to leave",
    };
    int checktime;

    if (too_much())
    {
	if (author())
	{
	    num_checks = 1;
	    chmsg(rs,"The load is rather high, O exaulted one");
	}
	else if (num_checks++ == 3)
	    fatal("Sorry.  You took too long.  You are dead\n");
	checktime = (CHECKTIME * 60) / num_checks;
	chmsg(rs,msgs[num_checks - 1], ((double) checktime / 60.0));
    }
    else
    {
	if (num_checks)
	{
	    num_checks = 0;
	    chmsg(rs,"The load has dropped back down.  You have a reprieve");
	}
	checktime = (CHECKTIME * 60);
    }

	md_start_checkout_timer(checktime);
}

/*
 * chmsg:
 *	checkout()'s version of msg.  If we are in the middle of a
 *	shell, do a printf instead of a msg to a the refresh.
 */
/* VARARGS1 */

chmsg(struct rogue_state *rs,char *fmt, int arg)
{
    if (!in_shell)
	msg(rs,fmt, arg);
    else
    {
	printf(fmt, arg);
	putchar('\n');
	fflush(stdout);
    }
}
#endif

#ifdef UCOUNT
/*
 * ucount:
 *	count number of users on the system
 */
#include <utmp.h>

struct utmp buf;

int
ucount()
{
    struct utmp *up;
    FILE *utmp;
    int count;

    if ((utmp = fopen(UTMP, "r")) == NULL)
	return 0;

    up = &buf;
    count = 0;

    while (fread(up, 1, sizeof (*up), utmp) > 0)
	if (buf.ut_name[0] != '\0')
	    count++;
    fclose(utmp);
    return count;
}
#endif

/*
 * lock_sc:
 *	lock the score file.  If it takes too long, ask the user if
 *	they care to wait.  Return TRUE if the lock is successful.
 */
static FILE *lfd = NULL;
bool
lock_sc()
{
#if defined(SCOREFILE) && defined(LOCKFILE)
    int cnt;
    static struct stat sbuf;
    char *lockfile = LOCKFILE;

over:
    if ((lfd=fopen(lockfile, "w+")) != NULL)
	return TRUE;
    for (cnt = 0; cnt < 5; cnt++)
    {
	md_sleep(1);
	if ((lfd=fopen(lockfile, "w+")) != NULL)
	    return TRUE;
    }
    if (stat(lockfile, &sbuf) < 0)
    {
	lfd=fopen(lockfile, "w+");
	return TRUE;
    }
    if (time(NULL) - sbuf.st_mtime > 10)
    {
	if (md_unlink(lockfile) < 0)
	    return FALSE;
	goto over;
    }
    else
    {
	printf("The score file is very busy.  Do you want to wait longer\n");
	printf("for it to become free so your score can get posted?\n");
	printf("If so, type \"y\"\n");
	if (fgets(prbuf, MAXSTR, stdin) != 0 )
        ;
	if (prbuf[0] == 'y')
	    for (;;)
	    {
		if ((lfd=fopen(lockfile, "w+")) != 0)
		    return TRUE;
		if (stat(lockfile, &sbuf) < 0)
		{
		    lfd=fopen(lockfile, "w+");
		    return TRUE;
		}
		if (time(NULL) - sbuf.st_mtime > 10)
		{
		    if (md_unlink(lockfile) < 0)
			return FALSE;
		}
		md_sleep(1);
	    }
	else
	    return FALSE;
    }
#else
    return TRUE;
#endif
}

/*
 * unlock_sc:
 *	Unlock the score file
 */

void
unlock_sc()
{
#if defined(SCOREFILE) && defined(LOCKFILE)
    if (lfd != NULL)
        fclose(lfd);
    lfd = NULL;
    md_unlink(LOCKFILE);
#endif
}

/*
 * flush_type:
 *	Flush typeahead for traps, etc.
 */

void
flush_type()
{
    flushinp();
}
