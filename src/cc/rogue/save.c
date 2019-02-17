/*
 * save and restore routines
 *
 * @(#)save.c	4.33 (Berkeley) 06/01/83
 *
 * Rogue: Exploring the Dungeons of Doom
 * Copyright (C) 1980-1983, 1985, 1999 Michael Toy, Ken Arnold and Glenn Wichman
 * All rights reserved.
 *
 * See the file LICENSE.TXT for full copyright and licensing information.
 */

//#include <stdlib.h>
//#include <sys/types.h>
//#include <sys/stat.h>
//#include <errno.h>
//#include <signal.h>
//#include <string.h>
//#include <curses.h>
#include "rogue.h"
#include "score.h"

typedef struct stat STAT;

extern char *version, *encstr;

static STAT sbuf;

/*
 * save_game:
 *	Implement the "save game" command
 */

void
save_game(struct rogue_state *rs)
{
    FILE *savef;
    int c;
    //auto
    char buf[MAXSTR];

    /*
     * get file name
     */
    mpos = 0;
over:
    if (file_name[0] != '\0')
    {
	for (;;)
	{
	    msg(rs,"save file (%s)? ", file_name);
	    c = readchar(rs);
	    mpos = 0;
	    if (c == ESCAPE)
	    {
		msg(rs,"");
		return;
	    }
	    else if (c == 'n' || c == 'N' || c == 'y' || c == 'Y')
		break;
	    else
		msg(rs,"please answer Y or N");
	}
	if (c == 'y' || c == 'Y')
	{
	    addstr("Yes\n");
        if ( rs->sleeptime != 0 )
            refresh();
	    strcpy(buf, file_name);
	    goto gotfile;
	}
    }

    do
    {
	mpos = 0;
	msg(rs,"file name: ");
	buf[0] = '\0';
	if (get_str(rs,buf, stdscr) == QUIT)
	{
quit_it:
	    msg(rs,"");
	    return;
	}
	mpos = 0;
gotfile:
	/*
	 * test to see if the file exists
	 */
	if (stat(buf, &sbuf) >= 0)
	{
	    for (;;)
	    {
		msg(rs,"File exists.  Do you wish to overwrite it?");
		mpos = 0;
		if ((c = readchar(rs)) == ESCAPE)
		    goto quit_it;
		if (c == 'y' || c == 'Y')
		    break;
		else if (c == 'n' || c == 'N')
		    goto over;
		else
		    msg(rs,"Please answer Y or N");
	    }
	    msg(rs,"file name: %s", buf);
	    md_unlink(file_name);
	}
	strcpy(file_name, buf);
	if ((savef = fopen(file_name, "w")) == NULL)
	    msg(rs,strerror(errno));
    } while (savef == NULL);

    save_file(rs,savef,1);
    /* NOTREACHED */
}

/*
 * auto_save:
 *	Automatically save a file.  This is used if a HUP signal is
 *	recieved
 */

void
auto_save(int sig)
{
    FILE *savef;
    NOOP(sig);

    md_ignoreallsignals();
    if (file_name[0] != '\0' && ((savef = fopen(file_name, "w")) != NULL ||
	(md_unlink_open_file(file_name, savef) >= 0 && (savef = fopen(file_name, "w")) != NULL)))
	    save_file(&globalR,savef,1);
    my_exit(0);
}

/*
 * save_file:
 *	Write the saved game on the file
 */

char *rogue_packfname(struct rogue_state *rs,char *fname)
{
    sprintf(fname,"rogue.%llu.pack",(long long)rs->seed);
    return(fname);
}

void
save_file(struct rogue_state *rs,FILE *savef,int32_t guiflag)
{
    char buf[80],fname[512]; int32_t i,n,nonz,histo[0x100]; FILE *fp;
    if ( rs->guiflag != 0 )
    {
        mvcur(0, COLS - 1, LINES - 1, 0);
        putchar('\n');
        endwin();
        resetltchars();
        md_chmod(file_name, 0400);
        if ( guiflag != 0 )
        {
            encwrite(version, strlen(version)+1, savef);
            sprintf(buf,"%d x %d\n", LINES, COLS);
            encwrite(buf,80,savef);
        }
    }
    memset(&rs->P,0,sizeof(rs->P));
    rs_save_file(rs,savef); // sets rs->P
    //fprintf(stderr,"gold.%d hp.%d strength.%d level.%d exp.%d %d\n",rs->P.gold,rs->P.hitpoints,rs->P.strength,rs->P.level,rs->P.experience,rs->P.dungeonlevel);

    n = sizeof(rs->P) - sizeof(rs->P.roguepack) + sizeof(rs->P.roguepack[0])*rs->P.packsize;
    memset(histo,0,sizeof(histo));
    for (i=0; i<n; i++)
    {
        //fprintf(stderr,"%02x",((uint8_t *)&rs->P)[i]);
        histo[((uint8_t *)&rs->P)[i]]++;
        rs->playerdata[i] = ((uint8_t *)&rs->P)[i];
    }
    rs->playersize = n;
    //fprintf(stderr," packsize.%d playersize.%d\n",rs->P.packsize,n);
    if ( (fp= fopen(rogue_packfname(rs,fname),"wb")) != 0 )
    {
        fwrite(&rs->P,1,n,fp);
        fclose(fp);
    }
    if ( 0 )
    {
        for (i=nonz=0; i<0x100; i++)
            if ( histo[i] != 0 )
                fprintf(stderr,"(%d %d) ",i,histo[i]), nonz++;
        fprintf(stderr,"nonz.%d\n",nonz);
    }
    fflush(savef);
    fclose(savef);
    if ( guiflag != 0 )
        my_exit(0);
}

int32_t rogue_restorepack(struct rogue_state *rs)
{
    FILE *fp; char fname[512]; int32_t retflag = -1;
    memset(&rs->P,0,sizeof(rs->P));
    if ( (fp= fopen(rogue_packfname(rs,fname),"rb")) != 0 )
    {
        if ( fread(&rs->P,1,sizeof(rs->P) - sizeof(rs->P.roguepack),fp) == sizeof(rs->P) - sizeof(rs->P.roguepack) )
        {
            if ( rs->P.packsize > 0 && rs->P.packsize <= MAXPACK )
            {
                if ( fread(&rs->P.roguepack,1,rs->P.packsize*sizeof(rs->P.roguepack[0]),fp) == rs->P.packsize*sizeof(rs->P.roguepack[0]) )
                {
                    fprintf(stderr,"roguepack[%d] restored\n",rs->P.packsize);
                    retflag = 0;
                }
            }
        }
    }
    if ( retflag < 0 )
        memset(&rs->P,0,sizeof(rs->P));
    return(retflag);
}

/*
 * restore:
 *	Restore a saved game from a file with elaborate checks for file
 *	integrity from cheaters
 */
bool
restore(struct rogue_state *rs,char *file, char **envp)
{
    FILE *inf;
    int syml,l, cols;
    extern char **environ;
    //auto
    char buf[MAXSTR];
    //auto
    STAT sbuf2;
    if ( rs->guiflag == 0 )
        return(0);
    
    if (strcmp(file, "-r") == 0)
	file = file_name;

	md_tstphold();

	if ((inf = fopen(file,"r")) == NULL)
    {
	perror(file);
	return FALSE;
    }
    stat(file, &sbuf2);
    syml = is_symlink(file);

    fflush(stdout);
    encread(buf, (unsigned) strlen(version) + 1, inf);
    if (strcmp(buf, version) != 0)
    {
	printf("Sorry, saved game is out of date.\n");
	return FALSE;
    }
    encread(buf,80,inf);
    sscanf(buf,"%d x %d\n", &l, &cols);

    initscr();                          /* Start up cursor package */
    keypad(stdscr, 1);

    if (l > LINES)
    {
        endwin();
        printf("Sorry, original game was played on a screen with %d lines.\n",l);
        printf("Current screen only has %d lines. Unable to restore game\n",LINES);
        return(FALSE);
    }
    if (cols > COLS)
    {
        endwin();
        printf("Sorry, original game was played on a screen with %d columns.\n",cols);
        printf("Current screen only has %d columns. Unable to restore game\n",COLS);
        return(FALSE);
    }

    hw = newwin(LINES, COLS, 0, 0);
    setup();

    rs_restore_file(inf);
    /*
     * we do not close the file so that we will have a hold of the
     * inode for as long as possible
     */

    if (
#ifdef MASTER
	!wizard &&
#endif
        md_unlink_open_file(file, inf) < 0)
    {
	printf("Cannot unlink file\n");
	return FALSE;
    }
    mpos = 0;
/*    printw(0, 0, "%s: %s", file, ctime(&sbuf2.st_mtime)); */
/*
    printw("%s: %s", file, ctime(&sbuf2.st_mtime));
*/
    clearok(stdscr,TRUE);
    /*
     * defeat multiple restarting from the same place
     */
#ifdef MASTER
    if (!wizard)
#endif
	if (sbuf2.st_nlink != 1 || syml)
	{
	    endwin();
	    printf("\nCannot restore from a linked file\n");
	    return FALSE;
	}

    if (pstats.s_hpt <= 0)
    {
	endwin();
	printf("\n\"He's dead, Jim\"\n");
	return FALSE;
    }

	md_tstpresume();

    environ = envp;
    strcpy(file_name, file);
    clearok(curscr, TRUE);
    srand((int32_t)rs->seed);//md_getpid());
    msg(rs,"file name: %s", file);
    playit(rs);
    /*NOTREACHED*/
    return(0);
}

/*
 * encwrite:
 *	Perform an encrypted write
 */
#define CRYPT_ENABLE 0

size_t
encwrite(char *start, size_t size, FILE *outf)
{
    char *e1, *e2, fb;
    int temp;
    extern char *statlist;
    size_t o_size = size;
    e1 = encstr;
    e2 = statlist;
    fb = 0;

    while(size)
    {
        if ( CRYPT_ENABLE )
        {
            if (putc(*start++ ^ *e1 ^ *e2 ^ fb, outf) == EOF)
                break;
            
            temp = *e1++;
            fb = fb + ((char) (temp * *e2++));
            if (*e1 == '\0')
                e1 = encstr;
            if (*e2 == '\0')
                e2 = statlist;
        }
        else if ( putc(*start++,outf) == EOF )
            break;
        size--;
    }

    return(o_size - size);
}

/*
 * encread:
 *	Perform an encrypted read
 */
size_t
encread(char *start, size_t size, FILE *inf)
{
    char *e1, *e2, fb;
    int temp;
    size_t read_size;
    extern char *statlist;

    fb = 0;

    if ((read_size = fread(start,1,size,inf)) == 0 || read_size == -1)
	return(read_size);
    if ( CRYPT_ENABLE )
    {
        e1 = encstr;
        e2 = statlist;
        while (size--)
        {
            *start++ ^= *e1 ^ *e2 ^ fb;
            temp = *e1++;
            fb = fb + (char)(temp * *e2++);
            if (*e1 == '\0')
                e1 = encstr;
            if (*e2 == '\0')
                e2 = statlist;
        }
    }
    return(read_size);
}

static char scoreline[100];
/*
 * read_scrore
 *	Read in the score file
 */
void
rd_score(SCORE *top_ten)
{
    unsigned int i;

	if (scoreboard == NULL)
		return;

	rewind(scoreboard); 

	for(i = 0; i < numscores; i++)
    {
        encread(top_ten[i].sc_name, MAXSTR, scoreboard);
        encread(scoreline, 100, scoreboard);
        sscanf(scoreline, " %u %d %u %hu %d %x \n",
            &top_ten[i].sc_uid, &top_ten[i].sc_score,
            &top_ten[i].sc_flags, &top_ten[i].sc_monster,
            &top_ten[i].sc_level, &top_ten[i].sc_time);
    }

	rewind(scoreboard); 
}

/*
 * write_scrore
 *	Read in the score file
 */
void
wr_score(SCORE *top_ten)
{
    unsigned int i;

	if (scoreboard == NULL)
		return;

	rewind(scoreboard);

    for(i = 0; i < numscores; i++)
    {
          memset(scoreline,0,100);
          encwrite(top_ten[i].sc_name, MAXSTR, scoreboard);
          sprintf(scoreline, " %u %d %u %hu %d %x \n",
              top_ten[i].sc_uid, top_ten[i].sc_score,
              top_ten[i].sc_flags, top_ten[i].sc_monster,
              top_ten[i].sc_level, top_ten[i].sc_time);
          encwrite(scoreline,100,scoreboard);
    }

	rewind(scoreboard); 
}
