/*
 * Contains functions for dealing with things that happen in the
 * future.
 *
 * @(#)daemon.c	4.7 (Berkeley) 02/05/99
 *
 * Rogue: Exploring the Dungeons of Doom
 * Copyright (C) 1980-1983, 1985, 1999 Michael Toy, Ken Arnold and Glenn Wichman
 * All rights reserved.
 *
 * See the file LICENSE.TXT for full copyright and licensing information.
 */

//#include <curses.h>
#include "rogue.h"

#define EMPTY 0
#define DAEMON -1
#define MAXDAEMONS 20

#define _X_ { EMPTY }

struct delayed_action d_list[MAXDAEMONS] = {
    _X_, _X_, _X_, _X_, _X_, _X_, _X_, _X_, _X_, _X_,
    _X_, _X_, _X_, _X_, _X_, _X_, _X_, _X_, _X_, _X_, 
};

/*
 * d_slot:
 *	Find an empty slot in the daemon/fuse list
 */
struct delayed_action *
d_slot()
{
    register struct delayed_action *dev;

    for (dev = d_list; dev <= &d_list[MAXDAEMONS-1]; dev++)
	if (dev->d_type == EMPTY)
	    return dev;
#ifdef MASTER
    debug("Ran out of fuse slots");
#endif
    return NULL;
}

/*
 * find_slot:
 *	Find a particular slot in the table
 */
struct delayed_action *
find_slot(void (*func)(struct rogue_state *rs,int))
{
    register struct delayed_action *dev;

    for (dev = d_list; dev <= &d_list[MAXDAEMONS-1]; dev++)
	if (dev->d_type != EMPTY && func == dev->d_func)
	    return dev;
    return NULL;
}

/*
 * start_daemon:
 *	Start a daemon, takes a function.
 */
void
start_daemon(void (*func)(struct rogue_state *rs,int), int arg, int type)
{
    register struct delayed_action *dev;

    dev = d_slot();
    dev->d_type = type;
    dev->d_func = func;
    dev->d_arg = arg;
    dev->d_time = DAEMON;
}

/*
 * kill_daemon:
 *	Remove a daemon from the list
 */
void
kill_daemon(void (*func)(struct rogue_state *rs,int))
{
    register struct delayed_action *dev;

    if ((dev = find_slot(func)) == NULL)
	return;
    /*
     * Take it out of the list
     */
    dev->d_type = EMPTY;
}

/*
 * do_daemons:
 *	Run all the daemons that are active with the current flag,
 *	passing the argument to the function.
 */
void
do_daemons(struct rogue_state *rs,int flag)
{
    register struct delayed_action *dev;

    /*
     * Loop through the devil list
     */
    for (dev = d_list; dev <= &d_list[MAXDAEMONS-1]; dev++)
	/*
	 * Executing each one, giving it the proper arguments
	 */
	if (dev->d_type == flag && dev->d_time == DAEMON)
	    (*dev->d_func)(rs,dev->d_arg);
}

/*
 * fuse:
 *	Start a fuse to go off in a certain number of turns
 */
void
fuse(void (*func)(struct rogue_state *rs,int), int arg, int time, int type)
{
    register struct delayed_action *wire;

    wire = d_slot();
    wire->d_type = type;
    wire->d_func = func;
    wire->d_arg = arg;
    wire->d_time = time;
}

/*
 * lengthen:
 *	Increase the time until a fuse goes off
 */
void
lengthen(void (*func)(struct rogue_state *rs,int), int xtime)
{
    register struct delayed_action *wire;

    if ((wire = find_slot(func)) == NULL)
	return;
    wire->d_time += xtime;
}

/*
 * extinguish:
 *	Put out a fuse
 */
void
extinguish(void (*func)(struct rogue_state *rs,int))
{
    register struct delayed_action *wire;

    if ((wire = find_slot(func)) == NULL)
	return;
    wire->d_type = EMPTY;
}

/*
 * do_fuses:
 *	Decrement counters and start needed fuses
 */

/*char *actionfunc_str(char *str,void *ptr)
{
    if ( ptr == (void *)runners )
        strcpy(str,"runners");
    else if ( ptr == (void *)doctor )
        strcpy(str,"doctor");
    else if ( ptr == (void *)stomach )
        strcpy(str,"stomach");
    else if ( ptr == (void *)nohaste )
        strcpy(str,"nohaste");
    else if ( ptr == (void *)unconfuse )
        strcpy(str,"unconfuse");
    else if ( ptr == (void *)swander )
        strcpy(str,"swander");
    else if ( ptr == (void *)come_down )
        strcpy(str,"come_down");
    else if ( ptr == (void *)unsee )
        strcpy(str,"unsee");
    else if ( ptr == (void *)sight )
        strcpy(str,"sight");
    else if ( ptr == (void *)land )
        strcpy(str,"land");
    else if ( ptr == (void *)rollwand )
        strcpy(str,"rollwand");
    else if ( ptr == (void *)visuals )
        strcpy(str,"visuals");
    else if ( ptr == (void *)turn_see )
        strcpy(str,"turn_see");
    else strcpy(str,"no match");
    return(str);
}*/

void
do_fuses(struct rogue_state *rs,int flag)
{
    register struct delayed_action *wire; char str[64];

    /*
     * Step though the list
     */
    for (wire = d_list; wire <= &d_list[MAXDAEMONS-1]; wire++)
	/*
	 * Decrementing counters and starting things we want.  We also need
	 * to remove the fuse from the list once it has gone off.
	 */
	if (flag == wire->d_type && wire->d_time > 0 && --wire->d_time == 0)
	{
        //if ( fp != 0 )
        //    fprintf(fp,"t.%d %d %s, ",wire->d_type,wire->d_time,actionfunc_str(str,wire->d_func));
	    wire->d_type = EMPTY;
	    (*wire->d_func)(rs,wire->d_arg);
	}
}
