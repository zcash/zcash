/*
 * Rogue: Exploring the Dungeons of Doom
 * Copyright (C) 1980-1983, 1985, 1999 Michael Toy, Ken Arnold and Glenn Wichman
 * All rights reserved.
 *
 * See the file LICENSE.TXT for full copyright and licensing information.
 *
 * @(#)main.c	4.22 (Berkeley) 02/05/99
 */

//#include <stdlib.h>
//#include <string.h>
#include <signal.h>
//#include <unistd.h>
//#include <curses.h>

#include "rogue.h"
#ifdef STANDALONE
#include "../komodo/src/komodo_cJSON.h"
#else
#include "../../komodo_cJSON.h"
#endif

/*
 * main:
 *	The main program, of course
 */
struct rogue_state globalR;
char Gametxidstr[67];
void garbage_collect();

void purge_obj_guess(struct obj_info *array,int32_t n)
{
    int32_t i;
    for (i=0; i<n; i++)
        if ( array[i].oi_guess != 0 )
            free(array[i].oi_guess), array[i].oi_guess = 0;
}

void rogueiterate(struct rogue_state *rs)
{
    THING *tp;
    seed = rs->seed;
    //clear();
    purge_obj_guess(things,NUMTHINGS);
    purge_obj_guess(ring_info,MAXRINGS);
    purge_obj_guess(pot_info,MAXPOTIONS);
    purge_obj_guess(arm_info,MAXARMORS);
    purge_obj_guess(scr_info,MAXSCROLLS);
    purge_obj_guess(weap_info,MAXWEAPONS + 1);
    purge_obj_guess(ws_info,MAXSTICKS);
    free_list(player._t._t_pack);
    for (tp = mlist; tp != NULL; tp = next(tp))
        free_list(tp->t_pack);
    free_list(mlist);
    free_list(lvl_obj);
    garbage_collect();

    externs_clear();
    memset(d_list,0,sizeof(d_list));

    memcpy(passages,origpassages,sizeof(passages));
    memcpy(monsters,origmonsters,sizeof(monsters));
    memcpy(things,origthings,sizeof(things));
    
    memcpy(ring_info,origring_info,sizeof(ring_info));
    memcpy(pot_info,origpot_info,sizeof(pot_info));
    memcpy(arm_info,origarm_info,sizeof(arm_info));
    memcpy(scr_info,origscr_info,sizeof(scr_info));
    memcpy(weap_info,origweap_info,sizeof(weap_info));
    memcpy(ws_info,origws_info,sizeof(ws_info));

    initscr();				/* Start up cursor package */
    init_probs();			/* Set up prob tables for objects */
    init_player(rs);		/* Set up initial player stats */
    init_names();			/* Set up names of scrolls */
    init_colors();			/* Set up colors of potions */
    init_stones();			/* Set up stone settings of rings */
    init_materials();		/* Set up materials of wands */
    setup();
    
    /*
     * The screen must be at least NUMLINES x NUMCOLS
     */
    if (LINES < NUMLINES || COLS < NUMCOLS)
    {
        printf("\nSorry, the screen must be at least %dx%d\n", NUMLINES, NUMCOLS);
        endwin();
        my_exit(1);
    }
    //fprintf(stderr,"LINES %d, COLS %d\n",LINES,COLS);
    
    // Set up windows
    if ( hw == NULL )
    {
        hw = newwin(LINES, COLS, 0, 0);
    }
    idlok(stdscr, TRUE);
    idlok(hw, TRUE);
#ifdef MASTER
    noscore = wizard;
#endif
    new_level(rs);			// Draw current level
    // Start up daemons and fuses
    start_daemon(runners, 0, AFTER);
    start_daemon(doctor, 0, AFTER);
    fuse(swander, 0, WANDERTIME, AFTER);
    start_daemon(stomach, 0, AFTER);
    if ( rs->restoring != 0 )
    {
        restore_player(rs);
    }
    playit(rs);
}

int32_t roguefname(char *fname,uint64_t seed,int32_t counter)
{
    sprintf(fname,"rogue.%llu.%d",(long long)seed,counter);
    return(0);
}

int32_t flushkeystrokes_local(struct rogue_state *rs,int32_t waitflag)
{
#ifndef BUILD_ROGUE
    char fname[1024]; FILE *fp; int32_t i,retflag = -1;
    rs->counter++;
    roguefname(fname,rs->seed,rs->counter);
    if ( (fp= fopen(fname,"wb")) != 0 )
    {
        if ( fwrite(rs->buffered,1,rs->num,fp) == rs->num )
        {
            rs->num = 0;
            retflag = 0;
            fclose(fp);
            /*if ( (fp= fopen("savefile","wb")) != 0 )
            {
                save_file(rs,fp,0);
                if ( 0 && (fp= fopen("savefile","rb")) != 0 )
                {
                    for (i=0; i<0x150; i++)
                        fprintf(stderr,"%02x",fgetc(fp));
                    fprintf(stderr," first part rnd.%d\n",rnd(1000));
                    fclose(fp);
                }*/
                roguefname(fname,rs->seed,rs->counter+1);
                if ( (fp= fopen(fname,"wb")) != 0 ) // truncate next file
                    fclose(fp);
                //fprintf(stderr,"savefile <- %s retflag.%d\n",fname,retflag);
            //}
        } else fprintf(stderr,"error writing (%s)\n",fname);
    } else fprintf(stderr,"error creating (%s)\n",fname);
    return(retflag);
#else
    return(0);
#endif
}

#ifdef BUILD_ROGUE
// stubs for inside daemon

int32_t rogue_progress(struct rogue_state *rs,int32_t waitflag,uint64_t seed,char *keystrokes,int32_t num)
{
}

int32_t rogue_setplayerdata(struct rogue_state *rs,char *gametxidstr)
{
    return(-1);
}
#endif

int32_t flushkeystrokes(struct rogue_state *rs,int32_t waitflag)
{
    if ( rs->num > 0 )
    {
        if ( rogue_progress(rs,waitflag,rs->seed,rs->buffered,rs->num) > 0 )
        {
            flushkeystrokes_local(rs,waitflag);
            memset(rs->buffered,0,sizeof(rs->buffered));
        }
    }
    return(0);
}

void rogue_bailout(struct rogue_state *rs)
{
    char cmd[512];
    flushkeystrokes(rs,1);
    //sleep(5);
    return;
    /*fprintf(stderr,"bailing out\n");
    sprintf(cmd,"./komodo-cli -ac_name=ROGUE cclib bailout 17 \\\"[%%22%s%%22]\\\" >> bailout.log",Gametxidstr);
    if ( system(cmd) != 0 )
        fprintf(stderr,"error issuing (%s)\n",cmd);*/
}

#ifdef _WIN32
#ifdef _MSC_VER
#define sleep(x) Sleep(1000*(x))
#endif
#endif

int32_t rogue_replay2(uint8_t *newdata,uint64_t seed,char *keystrokes,int32_t num,struct rogue_player *player,int32_t sleepmillis)
{
    struct rogue_state *rs; FILE *fp; int32_t i,n;
    rs = (struct rogue_state *)calloc(1,sizeof(*rs));
    rs->seed = seed;
    rs->keystrokes = keystrokes;
    rs->numkeys = num;
    rs->sleeptime = sleepmillis * 1000;
    if ( player != 0 )
    {
        rs->P = *player;
        rs->restoring = 1;
        //fprintf(stderr,"restore player packsize.%d HP.%d\n",rs->P.packsize,rs->P.hitpoints);
        if ( rs->P.packsize > MAXPACK )
            rs->P.packsize = MAXPACK;
    }
    globalR = *rs;
    uint32_t starttime = (uint32_t)time(NULL);
    rogueiterate(rs);

	/*
	// keypress after replay
	printf("[Press return to continue]");
	fflush(stdout);
	if (fgets(prbuf, 10, stdin) != 0);
	*/
	
    if ( 0 )
    {
        fprintf(stderr,"elapsed %d seconds\n",(uint32_t)time(NULL) - starttime);
        sleep(2);
        
        starttime = (uint32_t)time(NULL);
        for (i=0; i<10000; i++)
        {
            memset(rs,0,sizeof(*rs));
            rs->seed = seed;
            rs->keystrokes = keystrokes;
            rs->numkeys = num;
            rs->sleeptime = 0;
            rogueiterate(rs);
        }
        fprintf(stderr,"elapsed %d seconds\n",(uint32_t)time(NULL)-starttime);
        sleep(3);
    }
    if ( (fp= fopen("checkfile","wb")) != 0 )
    {
        save_file(rs,fp,0);
        //fprintf(stderr,"gold.%d hp.%d strength.%d/%d level.%d exp.%d dungeon.%d data[%d]\n",rs->P.gold,rs->P.hitpoints,rs->P.strength&0xffff,rs->P.strength>>16,rs->P.level,rs->P.experience,rs->P.dungeonlevel,rs->playersize);
        if ( newdata != 0 && rs->playersize > 0 )
            memcpy(newdata,rs->playerdata,rs->playersize);
    }
    n = rs->playersize;
    free(rs);
    return(n);
}

long get_filesize(FILE *fp)
{
    long fsize,fpos = ftell(fp);
    fseek(fp,0,SEEK_END);
    fsize = ftell(fp);
    fseek(fp,fpos,SEEK_SET);
    return(fsize);
}

char *rogue_keystrokesload(int32_t *numkeysp,uint64_t seed,int32_t counter)
{
    char fname[1024],*keystrokes = 0; FILE *fp; long fsize; int32_t num = 0;
    *numkeysp = 0;
    while ( 1 )
    {
        roguefname(fname,seed,counter);
        //printf("check (%s)\n",fname);
        if ( (fp= fopen(fname,"rb")) == 0 )
            break;
        if ( (fsize= get_filesize(fp)) <= 0 )
        {
            fclose(fp);
            //printf("fsize.%ld\n",fsize);
            break;
        }
        if ( (keystrokes= (char *)realloc(keystrokes,num+fsize)) == 0 )
        {
            fprintf(stderr,"error reallocating keystrokes\n");
            fclose(fp);
            return(0);
        }
        if ( fread(&keystrokes[num],1,fsize,fp) != fsize )
        {
            fprintf(stderr,"error reading keystrokes from (%s)\n",fname);
            fclose(fp);
            free(keystrokes);
            return(0);
        }
        fclose(fp);
        num += fsize;
        counter++;
        //fprintf(stderr,"loaded %ld from (%s) total %d\n",fsize,fname,num);
    }
    *numkeysp = num;
    return(keystrokes);
}

int32_t rogue_replay(uint64_t seed,int32_t sleeptime)
{
    FILE *fp; char fname[1024]; char *keystrokes = 0; long fsize; int32_t i,num=0,counter = 0; struct rogue_state *rs; struct rogue_player P,*player = 0;
    if ( seed == 0 )
        seed = 777;
    keystrokes = rogue_keystrokesload(&num,seed,counter);
    if ( num > 0 )
    {
        sprintf(fname,"rogue.%llu.player",(long long)seed);
        if ( (fp=fopen(fname,"rb")) != 0 )
        {
            if ( fread(&P,1,sizeof(P),fp) > 0 )
            {
                //printf("max size player\n");
                player = &P;
            }
            fclose(fp);
        }
        rogue_replay2(0,seed,keystrokes,num,player,sleeptime);
        mvaddstr(LINES - 2, 0, (char *)"replay completed");
        endwin();
        my_exit(0);
    }
    if ( keystrokes != 0 )
        free(keystrokes);
    return(num);
}

int rogue(int argc, char **argv, char **envp)
{
    char *env; int lowtime; struct rogue_state *rs = &globalR;
    memset(rs,0,sizeof(*rs));
    rs->guiflag = 1;
    rs->sleeptime = 1; // non-zero to allow refresh()
    if ( argc == 3 && strlen(argv[2]) == 64 )
    {
		#ifdef _WIN32
		#ifdef _MSC_VER
		rs->seed = _strtoui64(argv[1], NULL, 10);
		#else
		rs->seed = atol(argv[1]); // windows, but not MSVC
		#endif // _MSC_VER
		#else
		rs->seed = atol(argv[1]); // non-windows
		#endif // _WIN32
        strcpy(Gametxidstr,argv[2]);
        fprintf(stderr,"setplayerdata\n");
        if ( rogue_setplayerdata(rs,Gametxidstr) < 0 )
        {
            fprintf(stderr,"invalid gametxid, or already started\n");
            return(-1);
        }
    } else rs->seed = 777;
    md_init();

#ifdef MASTER
    /*
     * Check to see if he is a wizard
     */
    if (argc >= 2 && argv[1][0] == '\0')
	if (strcmp(PASSWD, md_crypt(md_getpass("wizard's password: "), "mT")) == 0)
	{
	    wizard = TRUE;
	    player.t_flags |= SEEMONST;
	    argv++;
	    argc--;
	}

#endif

    /*
     * get home and options from environment
     */

    strncpy(home, md_gethomedir(), MAXSTR);

    strcpy(file_name, home);
    strcat(file_name, "rogue.save");

    if ((env = getenv("ROGUEOPTS")) != NULL)
        parse_opts(env);
    //if (env == NULL || whoami[0] == '\0')
    //    strucpy(whoami, md_getusername(), (int) strlen(md_getusername()));
    lowtime = (int) time(NULL);
#ifdef MASTER
    if (wizard && getenv("SEED") != NULL)
        rs->seed = atoi(getenv("SEED"));
    else
#endif
	//dnum = lowtime + md_getpid();
    if ( rs != 0 )
        seed = rs->seed;
    else seed = 777;
    //dnum = (int)seed;
    
    open_score();

	/* 
     * Drop setuid/setgid after opening the scoreboard file. 
     */ 

    md_normaluser();

    /*
     * check for print-score option
     */

	md_normaluser(); /* we drop any setgid/setuid priveldges here */

    if (argc == 2)
    {
        if (strcmp(argv[1], "-s") == 0)
        {
            noscore = TRUE;
            score(rs,0, -1, 0);
            exit(0);
        }
        else if (strcmp(argv[1], "-d") == 0)
        {
            rs->seed = rnd(100);	/* throw away some rnd()s to break patterns */
            while (--rs->seed)
                rnd(100);
            purse = rnd(100) + 1;
            level = rnd(100) + 1;
            initscr();
            getltchars();
            death(rs,death_monst());
            exit(0);
        }
    }

    init_check();			/* check for legal startup */
    if (argc == 2)
	if (!restore(rs,argv[1], envp))	/* Note: restore will never return */
	    my_exit(1);
#ifdef MASTER
    if (wizard)
	printf("Hello %s, welcome to dungeon #%d", whoami, dnum);
    else
#endif
	printf("Hello %s, just a moment while I dig the dungeon... seed.%llu", whoami,(long long)rs->seed);
    fflush(stdout);
    fprintf(stderr,"rogueiterate\n");
    rogueiterate(rs);
    return(0);
}

/*
 * endit:
 *	Exit the program abnormally.
 */

void
endit(int sig)
{
    NOOP(sig);
    fatal("Okay, bye bye!\n");
}

/*
 * fatal:
 *	Exit the program, printing a message.
 */

void
fatal(char *s)
{
    mvaddstr(LINES - 2, 0, s);
    refresh();
    endwin();
    my_exit(0);
}

/*
 * rnd:
 *	Pick a very random number.
 */
int
rnd(int range)
{
    return range == 0 ? 0 : abs((int) RN) % range;
}

/*
 * roll:
 *	Roll a number of dice
 */
int 
roll(int number, int sides)
{
    int dtotal = 0;

    while (number--)
	dtotal += rnd(sides)+1;
    return dtotal;
}

/*
 * tstp:
 *	Handle stop and start signals
 */

void
tstp(int ignored)
{
    int y, x;
    int oy, ox;

	NOOP(ignored);

    /*
     * leave nicely
     */
    getyx(curscr, oy, ox);
    mvcur(0, COLS - 1, LINES - 1, 0);
    endwin();
    resetltchars();
    fflush(stdout);
	md_tstpsignal();

    /*
     * start back up again
     */
	md_tstpresume();
    raw();
    noecho();
    keypad(stdscr,1);
    playltchars();
    clearok(curscr, TRUE);
    wrefresh(curscr);
    getyx(curscr, y, x);
    mvcur(y, x, oy, ox);
    fflush(stdout);
    wmove(curscr,oy,ox);
/*#ifndef __APPLE__
#ifndef BUILD_ROGUE
    curscr->_cury = oy;
    curscr->_curx = ox;
#endif
#endif*/
}


#ifdef _WIN32
#ifdef _MSC_VER
void usleep(int32_t micros)
{
	if (micros < 1000)
		Sleep(1);
	else Sleep(micros / 1000);
}
#endif
#endif

/*
 * playit:
 *	The main loop of the program.  Loop until the game is over,
 *	refreshing things and looking at the proper times.
 */

void
playit(struct rogue_state *rs)
{
    char *opts;

    /*
     * set up defaults for slow terminals
     */

    if (baudrate() <= 1200)
    {
	terse = TRUE;
	jump = TRUE;
	see_floor = FALSE;
    }

    if (md_hasclreol())
        inv_type = INV_CLEAR;

    /*
     * parse environment declaration of options
     */
    if ((opts = getenv("ROGUEOPTS")) != NULL)
	parse_opts(opts);


    oldpos = hero;
    oldrp = roomin(rs,&hero);
    while (playing)
    {
        command(rs);			// Command execution
        if ( rs->guiflag == 0 )
        {
            if ( rs->replaydone != 0 )
            {
                if ( 0 && rs->sleeptime != 0 )
                    sleep(3);
                return;
            }
            if ( rs->sleeptime != 0 )
                usleep(rs->sleeptime);
        }
        else
        {
            if ( rs->needflush != 0 )
            {
                if ( flushkeystrokes(rs,0) == 0 )
                    rs->needflush = 0;
            }
        }
    }
    if ( rs->guiflag != 0 )
        flushkeystrokes(rs,1);
    endit(0);
}



int32_t _quit()
{
    struct rogue_state *rs = &globalR;
    int oy, ox, c;
    //fprintf(stderr,"inside quit(%d)\n",sig);
    getyx(curscr, oy, ox);
    msg(rs,"really quit?");
    //sleep(1);
    if ( (c= readchar(rs)) == 'y')
    {
        if ( rs->guiflag != 0 )
        {
            signal(SIGINT, leave);
            clear();
            mvprintw(LINES - 2, 0, "You quit with %d gold pieces", purse);
            move(LINES - 1, 0);
            if ( rs->sleeptime != 0 )
                refresh();
            score(rs,purse, 1, 0);
            flushkeystrokes(rs,1);
            my_exit(0);
        }
        else
        {
            //score(rs,purse, 1, 0);
            //fprintf(stderr,"done! (%c)\n",c);
        }
        return(1);
    }
    else
    {
        //fprintf(stderr,"'Q' answer (%c)\n",c);
        move(0, 0);
        clrtoeol();
        status(rs);
        move(oy, ox);
        if ( rs->sleeptime != 0 )
            refresh();
        mpos = 0;
        count = 0;
        to_death = FALSE;
        return(0);
    }
}

/*
 * quit:
 *	Have player make certain, then exit.
 */

void quit(int sig)
{
    struct rogue_state *rs = &globalR;
    int oy, ox, c;
    //fprintf(stderr,"inside quit(%d)\n",sig);
    if ( rs->guiflag != 0 )
    {
        NOOP(sig);
        
        /*
         * Reset the signal in case we got here via an interrupt
         */
        if (!q_comm)
            mpos = 0;
    }
    _quit();
}

/*
 * leave:
 *	Leave quickly, but curteously
 */

void
leave(int sig)
{
    static char buf[BUFSIZ];

    NOOP(sig);

    setbuf(stdout, buf);	/* throw away pending output */

    if (!isendwin())
    {
	mvcur(0, COLS - 1, LINES - 1, 0);
	endwin();
    }

    putchar('\n');
    my_exit(0);
}

/*
 * shell:
 *	Let them escape for a while
 */

void
shell(struct rogue_state *rs)
{
    if ( rs != 0 && rs->guiflag != 0 )
    {
        /*
         * Set the terminal back to original mode
         */
        move(LINES-1, 0);
        refresh();
        endwin();
        resetltchars();
        putchar('\n');
        in_shell = TRUE;
        after = FALSE;
        fflush(stdout);
        /*
         * Fork and do a shell
         */
        md_shellescape();
        
        printf("\n[Press return to continue]");
        fflush(stdout);
        noecho();
        raw();
        keypad(stdscr,1);
        playltchars();
        in_shell = FALSE;
        wait_for(rs,'\n');
        clearok(stdscr, TRUE);
    }
    else fprintf(stderr,"no shell in the blockchain\n");
}

/*
 * my_exit:
 *	Leave the process properly
 */

void
my_exit(int st)
{
    uint32_t counter;
    resetltchars();
    if ( globalR.guiflag != 0 || globalR.sleeptime != 0 )
        exit(st);
    else if ( counter++ < 10 )
    {
        fprintf(stderr,"would have exit.(%d) sleeptime.%d\n",st,globalR.sleeptime);
        globalR.replaydone = 1;
    }
}

