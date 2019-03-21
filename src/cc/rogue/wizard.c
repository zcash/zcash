/*
 * Special wizard commands (some of which are also non-wizard commands
 * under strange circumstances)
 *
 * @(#)wizard.c	4.30 (Berkeley) 02/05/99
 *
 * Rogue: Exploring the Dungeons of Doom
 * Copyright (C) 1980-1983, 1985, 1999 Michael Toy, Ken Arnold and Glenn Wichman
 * All rights reserved.
 *
 * See the file LICENSE.TXT for full copyright and licensing information.
 */

//#include <stdlib.h>
//#include <curses.h>
//#include <string.h>
//#include <ctype.h>
#include "rogue.h"

/*
 * whatis:
 *	What a certin object is
 */

void
whatis(struct rogue_state *rs,bool insist, int type)
{
    THING *obj;

    if (pack == NULL)
    {
	msg(rs,"you don't have anything in your pack to identify");
	return;
    }

    for (;;)
    {
        if ( rs->replaydone != 0 )
            return;
        obj = get_item(rs,"identify", type);
        if (insist)
        {
            if (n_objs == 0)
                return;
            else if (obj == NULL)
                msg(rs,"you must identify something");
            else if (type && obj->o_type != type &&
                     !(type == R_OR_S && (obj->o_type == RING || obj->o_type == STICK)) )
                msg(rs,"you must identify a %s", type_name(type));
            else
                break;
        }
	else
	    break;
    }

    if (obj == NULL)
	return;

    switch (obj->o_type)
    {
        case SCROLL:
	    set_know(obj, scr_info);
        when POTION:
	    set_know(obj, pot_info);
	when STICK:
	    set_know(obj, ws_info);
        when WEAPON:
        case ARMOR:
	    obj->o_flags |= ISKNOW;
        when RING:
	    set_know(obj, ring_info);
    }
    msg(rs,inv_name(obj, FALSE));
}

/*
 * set_know:
 *	Set things up when we really know what a thing is
 */

void
set_know(THING *obj, struct obj_info *info)
{
    char **guess;

    info[obj->o_which].oi_know = TRUE;
    obj->o_flags |= ISKNOW;
    guess = &info[obj->o_which].oi_guess;
    if (*guess)
    {
	free(*guess);
	*guess = NULL;
    }
}

/*
 * type_name:
 *	Return a pointer to the name of the type
 */
char *
type_name(int type)
{
    struct h_list *hp;
    static struct h_list tlist[] = {
	{POTION, "potion",		FALSE},
	{SCROLL, "scroll",		FALSE},
	{FOOD,	 "food",		FALSE},
	{R_OR_S, "ring, wand or staff",	FALSE},
	{RING,	 "ring",		FALSE},
	{STICK,	 "wand or staff",	FALSE},
	{WEAPON, "weapon",		FALSE},
	{ARMOR,	 "suit of armor",	FALSE},
    };

    for (hp = tlist; hp->h_ch; hp++)
	if (type == hp->h_ch)
	    return hp->h_desc;
    /* NOTREACHED */
    return(0);
}

#ifdef MASTER
/*
 * create_obj:
 *	wizard command for getting anything he wants
 */

void
create_obj(struct rogue_state *rs)
{
    THING *obj;
    char ch, bless;

    obj = new_item();
    msg(rs,"type of item: ");
    obj->o_type = readchar(rs);
    mpos = 0;
    msg(rs,"which %c do you want? (0-f)", obj->o_type);
    obj->o_which = (isdigit((ch = readchar(rs))) ? ch - '0' : ch - 'a' + 10);
    obj->o_group = 0;
    obj->o_count = 1;
    mpos = 0;
    if (obj->o_type == WEAPON || obj->o_type == ARMOR)
    {
	msg(rs,"blessing? (+,-,n)");
	bless = readchar(rs);
	mpos = 0;
	if (bless == '-')
	    obj->o_flags |= ISCURSED;
	if (obj->o_type == WEAPON)
	{
	    init_weapon(obj, obj->o_which);
	    if (bless == '-')
		obj->o_hplus -= rnd(3)+1;
	    if (bless == '+')
		obj->o_hplus += rnd(3)+1;
	}
	else
	{
	    obj->o_arm = a_class[obj->o_which];
	    if (bless == '-')
		obj->o_arm += rnd(3)+1;
	    if (bless == '+')
		obj->o_arm -= rnd(3)+1;
	}
    }
    else if (obj->o_type == RING)
	switch (obj->o_which)
	{
	    case R_PROTECT:
	    case R_ADDSTR:
	    case R_ADDHIT:
	    case R_ADDDAM:
		msg(rs,"blessing? (+,-,n)");
		bless = readchar(rs);
		mpos = 0;
		if (bless == '-')
		    obj->o_flags |= ISCURSED;
		obj->o_arm = (bless == '-' ? -1 : rnd(2) + 1);
	    when R_AGGR:
	    case R_TELEPORT:
		obj->o_flags |= ISCURSED;
	}
    else if (obj->o_type == STICK)
	fix_stick(obj);
    else if (obj->o_type == GOLD)
    {
	msg(rs,"how much?");
	get_num(&obj->o_goldval, stdscr);
    }
    add_pack(rs,obj, FALSE);
}
#endif

/*
 * telport:
 *	Bamf the hero someplace else
 */

void
teleport(struct rogue_state *rs)
{
    static coord c;

    mvaddch(hero.y, hero.x, floor_at());
    find_floor(rs,(struct room *) NULL, &c, FALSE, TRUE);
    if (roomin(rs,&c) != proom)
    {
	leave_room(rs,&hero);
	hero = c;
	enter_room(rs,&hero);
    }
    else
    {
	hero = c;
	look(rs,TRUE);
    }
    mvaddch(hero.y, hero.x, PLAYER);
    /*
     * turn off ISHELD in case teleportation was done while fighting
     * a Flytrap
     */
    if (on(player, ISHELD)) {
	player.t_flags &= ~ISHELD;
	vf_hit = 0;
	strcpy(monsters['F'-'A'].m_stats.s_dmg, "000x0");
    }
    no_move = 0;
    count = 0;
    running = FALSE;
    flush_type();
}

#ifdef MASTER
/*
 * passwd:
 *	See if user knows password
 */
int
passwd(struct rogue_state *rs)
{
    char *sp, c;
    static char buf[MAXSTR];

    msg(rs,"wizard's Password:");
    mpos = 0;
    sp = buf;
    while ((c = readchar(rs)) != '\n' && c != '\r' && c != ESCAPE)
	if (c == md_killchar())
	    sp = buf;
	else if (c == md_erasechar() && sp > buf)
	    sp--;
	else
	    *sp++ = c;
    if (sp == buf)
	return FALSE;
    *sp = '\0';
    return (strcmp(PASSWD, md_crypt(buf, "mT")) == 0);
}

/*
 * show_map:
 *	Print out the map for the wizard
 */

void
show_map(struct rogue_state *rs)
{
    int y, x, real;

    wclear(hw);
    for (y = 1; y < NUMLINES - 1; y++)
	for (x = 0; x < NUMCOLS; x++)
	{
	    real = flat(y, x);
	    if (!(real & F_REAL))
		wstandout(hw);
	    wmove(hw, y, x);
	    waddch(hw, chat(y, x));
	    if (!real)
		wstandend(hw);
	}
    show_win(rs,"---More (level map)---");
}
#endif
