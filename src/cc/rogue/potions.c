/*
 * Function(s) for dealing with potions
 *
 * @(#)potions.c	4.46 (Berkeley) 06/07/83
 *
 * Rogue: Exploring the Dungeons of Doom
 * Copyright (C) 1980-1983, 1985, 1999 Michael Toy, Ken Arnold and Glenn Wichman
 * All rights reserved.
 *
 * See the file LICENSE.TXT for full copyright and licensing information.
 */

//#include <curses.h>
//#include <ctype.h>
#include "rogue.h"

typedef struct
{
    int pa_flags;
    void (*pa_daemon)(struct rogue_state *rs,int);
    int pa_time;
    char *pa_high, *pa_straight;
} PACT;

static PACT p_actions[] =
{
	{ ISHUH,	unconfuse,	HUHDURATION,	/* P_CONFUSE */
		"what a tripy feeling!",
		"wait, what's going on here. Huh? What? Who?" },
	{ ISHALU,	come_down,	SEEDURATION,	/* P_LSD */
		"Oh, wow!  Everything seems so cosmic!",
		"Oh, wow!  Everything seems so cosmic!" },
	{ 0,		NULL,	0 },			/* P_POISON */
	{ 0,		NULL,	0 },			/* P_STRENGTH */
	{ CANSEE,	unsee,	SEEDURATION,		/* P_SEEINVIS */
		prbuf,
		prbuf },
	{ 0,		NULL,	0 },			/* P_HEALING */
	{ 0,		NULL,	0 },			/* P_MFIND */
	{ 0,		NULL,	0 },			/* P_TFIND  */
	{ 0,		NULL,	0 },			/* P_RAISE */
	{ 0,		NULL,	0 },			/* P_XHEAL */
	{ 0,		NULL,	0 },			/* P_HASTE */
	{ 0,		NULL,	0 },			/* P_RESTORE */
	{ ISBLIND,	sight,	SEEDURATION,		/* P_BLIND */
		"oh, bummer!  Everything is dark!  Help!",
		"a cloak of darkness falls around you" },
	{ ISLEVIT,	land,	HEALTIME,		/* P_LEVIT */
		"oh, wow!  You're floating in the air!",
		"you start to float in the air" }
};

/*
 * quaff:
 *	Quaff a potion from the pack
 */

void
quaff(struct rogue_state *rs)
{
    THING *obj, *tp, *mp;
    bool discardit = FALSE;
    bool show, trip;

    obj = get_item(rs,"quaff", POTION);
    /*
     * Make certain that it is somethings that we want to drink
     */
    if (obj == NULL)
	return;
    if (obj->o_type != POTION)
    {
	if (!terse)
	    msg(rs,"yuk! Why would you want to drink that?");
	else
	    msg(rs,"that's undrinkable");
	return;
    }
    if (obj == cur_weapon)
	cur_weapon = NULL;
    /*
     * Calculate the effect it has on the poor guy.
     */
    trip = on(player, ISHALU);
    discardit = (bool)(obj->o_count == 1);
    leave_pack(rs,obj, FALSE, FALSE);
    switch (obj->o_which)
    {
	case P_CONFUSE:
	    do_pot(rs,P_CONFUSE, !trip);
	when P_POISON:
	    pot_info[P_POISON].oi_know = TRUE;
        if (ISWEARING(R_SUSTSTR))
		msg(rs,"you feel momentarily sick");
	    else
	    {
		chg_str(-(rnd(3) + 1));
		msg(rs,"you feel very sick now");
		come_down(rs,0);
	    }
	when P_HEALING:
	    pot_info[P_HEALING].oi_know = TRUE;
	    if ((pstats.s_hpt += roll(pstats.s_lvl, 4)) > max_hp)
		pstats.s_hpt = ++max_hp;
	    sight(rs,0);
	    msg(rs,"you begin to feel better");
	when P_STRENGTH:
	    pot_info[P_STRENGTH].oi_know = TRUE;
	    chg_str(1);
	    msg(rs,"you feel stronger, now.  What bulging muscles!");
	when P_MFIND:
	    player.t_flags |= SEEMONST;
	    fuse((void(*)(struct rogue_state *rs,int))turn_see, TRUE, HUHDURATION, AFTER);
	    if (!turn_see(rs,FALSE))
		msg(rs,"you have a %s feeling for a moment, then it passes",
		    choose_str("normal", "strange"));
	when P_TFIND:
	    /*
	     * Potion of magic detection.  Show the potions and scrolls
	     */
	    show = FALSE;
	    if (lvl_obj != NULL)
	    {
		wclear(hw);
		for (tp = lvl_obj; tp != NULL; tp = next(tp))
		{
		    if (is_magic(tp))
		    {
			show = TRUE;
			wmove(hw, tp->o_pos.y, tp->o_pos.x);
			waddch(hw, MAGIC);
			pot_info[P_TFIND].oi_know = TRUE;
		    }
		}
		for (mp = mlist; mp != NULL; mp = next(mp))
		{
		    for (tp = mp->t_pack; tp != NULL; tp = next(tp))
		    {
			if (is_magic(tp))
			{
			    show = TRUE;
			    wmove(hw, mp->t_pos.y, mp->t_pos.x);
			    waddch(hw, MAGIC);
			}
		    }
		}
	    }
	    if (show)
	    {
		pot_info[P_TFIND].oi_know = TRUE;
		show_win(rs,"You sense the presence of magic on this level.--More--");
	    }
	    else
		msg(rs,"you have a %s feeling for a moment, then it passes",
		    choose_str("normal", "strange"));
	when P_LSD:
	    if (!trip)
	    {
		if (on(player, SEEMONST))
		    turn_see(rs,FALSE);
		start_daemon(visuals, 0, BEFORE);
		seenstairs = seen_stairs();
	    }
	    do_pot(rs,P_LSD, TRUE);
	when P_SEEINVIS:
	    sprintf(prbuf, "this potion tastes like %s juice", fruit);
	    show = on(player, CANSEE);
	    do_pot(rs,P_SEEINVIS, FALSE);
	    if (!show)
		invis_on();
	    sight(rs,0);
	when P_RAISE:
	    pot_info[P_RAISE].oi_know = TRUE;
	    msg(rs,"you suddenly feel much more skillful");
	    raise_level(rs);
	when P_XHEAL:
	    pot_info[P_XHEAL].oi_know = TRUE;
	    if ((pstats.s_hpt += roll(pstats.s_lvl, 8)) > max_hp)
	    {
		if (pstats.s_hpt > max_hp + pstats.s_lvl + 1)
		    ++max_hp;
		pstats.s_hpt = ++max_hp;
	    }
	    sight(rs,0);
	    come_down(rs,0);
	    msg(rs,"you begin to feel much better");
	when P_HASTE:
	    pot_info[P_HASTE].oi_know = TRUE;
	    after = FALSE;
	    if (add_haste(rs,TRUE))
		msg(rs,"you feel yourself moving much faster");
	when P_RESTORE:
	    if (ISRING(LEFT, R_ADDSTR))
		add_str(&pstats.s_str, -cur_ring[LEFT]->o_arm);
	    if (ISRING(RIGHT, R_ADDSTR))
		add_str(&pstats.s_str, -cur_ring[RIGHT]->o_arm);
	    if (pstats.s_str < max_stats.s_str)
		pstats.s_str = max_stats.s_str;
	    if (ISRING(LEFT, R_ADDSTR))
		add_str(&pstats.s_str, cur_ring[LEFT]->o_arm);
	    if (ISRING(RIGHT, R_ADDSTR))
		add_str(&pstats.s_str, cur_ring[RIGHT]->o_arm);
	    msg(rs,"hey, this tastes great.  It make you feel warm all over");
	when P_BLIND:
	    do_pot(rs,P_BLIND, TRUE);
	when P_LEVIT:
	    do_pot(rs,P_LEVIT, TRUE);
#ifdef MASTER
	otherwise:
	    msg(rs,"what an odd tasting potion!");
	    return;
#endif
    }
    status(rs);
    /*
     * Throw the item away
     */

    call_it(rs,&pot_info[obj->o_which]);

    if (discardit)
        discard(obj);
    return;
}

/*
 * is_magic:
 *	Returns true if an object radiates magic
 */
bool
is_magic(THING *obj)
{
    switch (obj->o_type)
    {
	case ARMOR:
	    return (bool)((obj->o_flags&ISPROT) || obj->o_arm != a_class[obj->o_which]);
	case WEAPON:
	    return (bool)(obj->o_hplus != 0 || obj->o_dplus != 0);
	case POTION:
	case SCROLL:
	case STICK:
	case RING:
	case AMULET:
	    return TRUE;
    }
    return FALSE;
}

/*
 * invis_on:
 *	Turn on the ability to see invisible
 */

void
invis_on()
{
    THING *mp;

    player.t_flags |= CANSEE;
    for (mp = mlist; mp != NULL; mp = next(mp))
	if (on(*mp, ISINVIS) && see_monst(mp) && !on(player, ISHALU))
	    mvaddch(mp->t_pos.y, mp->t_pos.x, mp->t_disguise);
}


/*
 * seen_stairs:
 *	Return TRUE if the player has seen the stairs
 */
bool
seen_stairs()
{
    THING	*tp;

    move(stairs.y, stairs.x);
    if (inch() == STAIRS)			/* it's on the map */
	return TRUE;
    if (ce(hero, stairs))			/* It's under him */
	return TRUE;

    /*
     * if a monster is on the stairs, this gets hairy
     */
    if ((tp = moat(stairs.y, stairs.x)) != NULL)
    {
	if (see_monst(tp) && on(*tp, ISRUN))	/* if it's visible and awake */
	    return TRUE;			/* it must have moved there */

	if (on(player, SEEMONST)		/* if she can detect monster */
	    && tp->t_oldch == STAIRS)		/* and there once were stairs */
		return TRUE;			/* it must have moved there */
    }
    return FALSE;
}

/*
 * raise_level:
 *	The guy just magically went up a level.
 */

void
raise_level(struct rogue_state *rs)
{
    pstats.s_exp = e_levels[pstats.s_lvl-1] + 1L;
    check_level(rs);
}

/*
 * do_pot:
 *	Do a potion with standard setup.  This means it uses a fuse and
 *	turns on a flag
 */

void
do_pot(struct rogue_state *rs,int type, bool knowit)
{
    PACT *pp;
    int t;
    
    pp = &p_actions[type];
    if (!pot_info[type].oi_know)
        pot_info[type].oi_know = knowit;
    t = spread(pp->pa_time);
    if (!on(player, pp->pa_flags))
    {
        player.t_flags |= pp->pa_flags;
        fuse(pp->pa_daemon, 0, t, AFTER);
        look(rs,FALSE);
    }
    else
        lengthen(pp->pa_daemon, t);
    msg(rs,choose_str(pp->pa_high, pp->pa_straight));
}
