/*
 * This file contains misc functions for dealing with armor
 * @(#)armor.c	4.14 (Berkeley) 02/05/99
 *
 * Rogue: Exploring the Dungeons of Doom
 * Copyright (C) 1980-1983, 1985, 1999 Michael Toy, Ken Arnold and Glenn Wichman
 * All rights reserved.
 *
 * See the file LICENSE.TXT for full copyright and licensing information.
 */

//#include <curses.h>
#include "rogue.h"

/*
 * wear:
 *	The player wants to wear something, so let him/her put it on.
 */
void
wear(struct rogue_state *rs)
{
    register THING *obj;
    register char *sp;

    if ((obj = get_item(rs,"wear", ARMOR)) == NULL)
	return;
    if (cur_armor != NULL)
    {
	addmsg(rs,"you are already wearing some");
	if (!terse)
	    addmsg(rs,".  You'll have to take it off first");
	endmsg(rs);
	after = FALSE;
	return;
    }
    if (obj->o_type != ARMOR)
    {
	msg(rs,"you can't wear that");
	return;
    }
    waste_time(rs);
    obj->o_flags |= ISKNOW;
    sp = inv_name(obj, TRUE);
    cur_armor = obj;
    if (!terse)
	addmsg(rs,"you are now ");
    msg(rs,"wearing %s", sp);
}

/*
 * take_off:
 *	Get the armor off of the players back
 */
void
take_off(struct rogue_state *rs)
{
    register THING *obj;

    if ((obj = cur_armor) == NULL)
    {
	after = FALSE;
	if (terse)
		msg(rs,"not wearing armor");
	else
		msg(rs,"you aren't wearing any armor");
	return;
    }
    if (!dropcheck(rs,cur_armor))
	return;
    cur_armor = NULL;
    if (terse)
	addmsg(rs,"was");
    else
	addmsg(rs,"you used to be");
    msg(rs," wearing %c) %s", obj->o_packch, inv_name(obj, TRUE));
}

/*
 * waste_time:
 *	Do nothing but let other things happen
 */
void
waste_time(struct rogue_state *rs)
{
    do_daemons(rs,BEFORE);
    do_fuses(rs,BEFORE);
    do_daemons(rs,AFTER);
    do_fuses(rs,AFTER);
}
