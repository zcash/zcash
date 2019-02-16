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
void garbage_collect();
char Gametxidstr[67];

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
        hw = newwin(LINES, COLS, 0, 0);
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
        restore_player(rs);
    playit(rs);
}

int32_t roguefname(char *fname,uint64_t seed,int32_t counter)
{
    sprintf(fname,"rogue.%llu.%d",(long long)seed,counter);
    return(0);
}

#ifdef test
int32_t flushkeystrokes(struct rogue_state *rs)
{
    char fname[1024]; FILE *fp; int32_t i,retflag = -1;
    roguefname(fname,rs->seed,rs->counter);
    if ( (fp= fopen(fname,"wb")) != 0 )
    {
        if ( fwrite(rs->buffered,1,rs->num,fp) == rs->num )
        {
            rs->counter++;
            rs->num = 0;
            retflag = 0;
            fclose(fp);
            if ( (fp= fopen("savefile","wb")) != 0 )
            {
                save_file(rs,fp,0);
                if ( 0 && (fp= fopen("savefile","rb")) != 0 )
                {
                    for (i=0; i<0x150; i++)
                        fprintf(stderr,"%02x",fgetc(fp));
                    fprintf(stderr," first part rnd.%d\n",rnd(1000));
                    fclose(fp);
                }
                roguefname(fname,rs->seed,rs->counter);
                if ( (fp= fopen(fname,"wb")) != 0 ) // truncate next file
                    fclose(fp);
                //fprintf(stderr,"savefile <- %s retflag.%d\n",fname,retflag);
            }
        } else fprintf(stderr,"error writing (%s)\n",fname);
    } else fprintf(stderr,"error creating (%s)\n",fname);
    return(retflag);
}
#else

uint8_t *OS_fileptr(long *allocsizep,char *fname);
#define is_cJSON_True(json) ((json) != 0 && ((json)->type & 0xff) == cJSON_True)

int32_t rogue_setplayerdata(struct rogue_state *rs,char *gametxidstr)
{
    char cmd[32768]; int32_t i,n,retval=-1; char *filestr,*pname,*statusstr,*datastr,fname[128]; long allocsize; cJSON *retjson,*array,*item;
    if ( gametxidstr == 0 || *gametxidstr == 0 )
        return(retval);
    sprintf(fname,"%s.gameinfo",gametxidstr);
    sprintf(cmd,"./komodo-cli -ac_name=ROGUE cclib gameinfo 17 \\\"[%%22%s%%22]\\\" > %s",gametxidstr,fname);
    if ( system(cmd) != 0 )
        fprintf(stderr,"error issuing (%s)\n",cmd);
    else
    {
        filestr = (char *)OS_fileptr(&allocsize,fname);
        if ( (retjson= cJSON_Parse(filestr)) != 0 )
        {
            if ( (array= jarray(&n,retjson,"players")) != 0 )
            {
                for (i=0; i<n; i++)
                {
                    item = jitem(array,i);
                    if ( is_cJSON_True(jobj(item,"ismine")) != 0 && (statusstr= jstr(item,"status")) != 0 )
                    {
                        if ( strcmp(statusstr,"registered") == 0 )
                        {
                            retval = 0;
                            if ( (item= jobj(item,"player")) != 0 && (datastr= jstr(item,"data")) != 0 )
                            {
                                if ( (pname= jstr(item,"pname")) != 0 && strlen(pname) < MAXSTR-1 )
                                    strcpy(whoami,pname);
                                decode_hex((uint8_t *)&rs->P,(int32_t)strlen(datastr)/2,datastr);
                                fprintf(stderr,"set pname[%s] %s\n",pname==0?"":pname,jprint(item,0));
                                rs->restoring = 1;
                            }
                        }
                    }
                }
            }
            free_json(retjson);
        }
        free(filestr);
    }
    return(retval);
}

void rogue_progress(uint64_t seed,char *keystrokes,int32_t num)
{
    char cmd[16384],hexstr[16384]; int32_t i;
    if ( Gametxidstr[0] != 0 )
    {
        for (i=0; i<num; i++)
            sprintf(&hexstr[i<<1],"%02x",keystrokes[i]);
        hexstr[i<<1] = 0;
        sprintf(cmd,"./komodo-cli -ac_name=ROGUE cclib keystrokes 17 \\\"[%%22%s%%22,%%22%s%%22]\\\" >> keystrokes.log",Gametxidstr,hexstr);
        if ( system(cmd) != 0 )
            fprintf(stderr,"error issuing (%s)\n",cmd);
    }
}

int32_t flushkeystrokes(struct rogue_state *rs)
{
    if ( rs->num > 0 )
    {
        rogue_progress(rs->seed,rs->buffered,rs->num);
        memset(rs->buffered,0,sizeof(rs->buffered));
        rs->counter++;
        rs->num = 0;
    }
    return(0);
}

void rogue_bailout(struct rogue_state *rs)
{
    char cmd[512];
    flushkeystrokes(rs);
    //sleep(5);
    return;
    fprintf(stderr,"bailing out\n");
    sprintf(cmd,"./komodo-cli -ac_name=ROGUE cclib bailout 17 \\\"[%%22%s%%22]\\\" >> bailout.log",Gametxidstr);
    if ( system(cmd) != 0 )
        fprintf(stderr,"error issuing (%s)\n",cmd);
}

int32_t rogue_replay2(uint8_t *newdata,uint64_t seed,char *keystrokes,int32_t num,struct rogue_player *player)
{
    struct rogue_state *rs; FILE *fp; int32_t i;
    rs = (struct rogue_state *)calloc(1,sizeof(*rs));
    rs->seed = seed;
    rs->keystrokes = keystrokes;
    rs->numkeys = num;
    rs->sleeptime = 0*50000;
    if ( player != 0 )
    {
        rs->P = *player;
        rs->restoring = 1;
        fprintf(stderr,"restore player packsize.%d\n",rs->P.packsize);
    }
    uint32_t starttime = (uint32_t)time(NULL);
    rogueiterate(rs);
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
        if ( newdata != 0 && rs->playersize > 0 )
            memcpy(newdata,rs->playerdata,rs->playersize);
    }
    free(rs);
    return(rs->playersize);
}
#endif

long get_filesize(FILE *fp)
{
    long fsize,fpos = ftell(fp);
    fseek(fp,0,SEEK_END);
    fsize = ftell(fp);
    fseek(fp,fpos,SEEK_SET);
    return(fsize);
}

int32_t rogue_replay(uint64_t seed,int32_t sleeptime)
{
    FILE *fp; char fname[1024]; char *keystrokes = 0; long num=0,fsize; int32_t i,counter = 0; struct rogue_state *rs;
    if ( seed == 0 )
        seed = 777;
    while ( 1 )
    {
        roguefname(fname,seed,counter);
        if ( (fp= fopen(fname,"rb")) == 0 )
            break;
        if ( (fsize= get_filesize(fp)) <= 0 )
        {
            fclose(fp);
            break;
        }
        if ( (keystrokes= (char *)realloc(keystrokes,num+fsize)) == 0 )
        {
            fprintf(stderr,"error reallocating keystrokes\n");
            fclose(fp);
            return(-1);
        }
        if ( fread(&keystrokes[num],1,fsize,fp) != fsize )
        {
            fprintf(stderr,"error reading keystrokes from (%s)\n",fname);
            fclose(fp);
            return(-1);
        }
        fclose(fp);
        num += fsize;
        counter++;
        fprintf(stderr,"loaded %ld from (%s) total %ld\n",fsize,fname,num);
    }
    if ( num > 0 )
    {
        rogue_replay2(0,seed,keystrokes,num,0);
        mvaddstr(LINES - 2, 0, (char *)"replay completed");
        endwin();
    }
    if ( keystrokes != 0 )
        free(keystrokes);
    return(num);
}

int rogue(int argc, char **argv, char **envp)
{
    char *env; int lowtime; struct rogue_state *rs = &globalR;
    memset(rs,0,sizeof(*rs));
    if ( argc == 3 && strlen(argv[2]) == 64 )
    {
        rs->seed = atol(argv[1]);
        strcpy(Gametxidstr,argv[2]);
        if ( rogue_setplayerdata(rs,Gametxidstr) < 0 )
        {
            fprintf(stderr,"invalid gametxid, or already started\n");
            return(-1);
        }
    } else rs->seed = 777;
    rs->guiflag = 1;
    rs->sleeptime = 1; // non-zero to allow refresh()
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
    //wmove(curscr,oy,ox);
#ifndef __APPLE__
    curscr->_cury = oy;
    curscr->_curx = ox;
#endif
}

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
                //fprintf(stderr,"replaydone\n"); sleep(3);
                return;
            }
            if ( rs->sleeptime != 0 )
                usleep(rs->sleeptime);
        }
        else
        {
            if ( rs->needflush != 0 && rs->num > 4096 )
            {
                if ( flushkeystrokes(rs) == 0 )
                    rs->needflush = 0;
            }
        }
    }
    endit(0);
}

/*
 * quit:
 *	Have player make certain, then exit.
 */

void
quit(int sig)
{
    struct rogue_state *rs = &globalR;
    int oy, ox;
    //fprintf(stderr,"inside quit(%d)\n",sig);
    if ( rs->guiflag != 0 )
    {
        NOOP(sig);
        
        /*
         * Reset the signal in case we got here via an interrupt
         */
        if (!q_comm)
            mpos = 0;
        getyx(curscr, oy, ox);
        msg(rs,"really quit?");
    }
    if (readchar(rs) == 'y')
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
            flushkeystrokes(rs);
            my_exit(0);
        }
        else
        {
            score(rs,purse, 1, 0);
            fprintf(stderr,"done!\n");
        }
    }
    else
    {
        move(0, 0);
        clrtoeol();
        status(rs);
        move(oy, ox);
        if ( rs->sleeptime != 0 )
            refresh();
        mpos = 0;
        count = 0;
        to_death = FALSE;
    }
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
    if ( globalR.guiflag != 0 )
        exit(st);
    else if ( counter++ < 10 )
    {
        fprintf(stderr,"would have exit.(%d)\n",st);
        globalR.replaydone = 1;
    }
}

