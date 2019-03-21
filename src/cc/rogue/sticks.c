/*
 * Functions to implement the various sticks one might find
 * while wandering around the dungeon.
 *
 * @(#)sticks.c	4.39 (Berkeley) 02/05/99
 *
 * Rogue: Exploring the Dungeons of Doom
 * Copyright (C) 1980-1983, 1985, 1999 Michael Toy, Ken Arnold and Glenn Wichman
 * All rights reserved.
 *
 * See the file LICENSE.TXT for full copyright and licensing information.
 */

//#include <curses.h>
//#include <string.h>
//#include <ctype.h>
#include "rogue.h"

/*
 * fix_stick:
 *	Set up a new stick
 */

void
fix_stick(THING *cur)
{
    if (strcmp(ws_type[cur->o_which], "staff") == 0)
	strncpy(cur->o_damage,"2x3",sizeof(cur->o_damage));
    else
	strncpy(cur->o_damage,"1x1",sizeof(cur->o_damage));
    strncpy(cur->o_hurldmg,"1x1",sizeof(cur->o_hurldmg));

    switch (cur->o_which)
    {
	case WS_LIGHT:
	    cur->o_charges = rnd(10) + 10;
	otherwise:
	    cur->o_charges = rnd(5) + 3;
    }
}

/*
 * do_zap:
 *	Perform a zap with a wand
 */

void
do_zap(struct rogue_state *rs)
{
    THING *obj, *tp;
    int y, x;
    char *name;
    char monster, oldch;
    static THING bolt;

    if ((obj = get_item(rs,"zap with", STICK)) == NULL)
	return;
    if (obj->o_type != STICK)
    {
	after = FALSE;
	msg(rs,"you can't zap with that!");
	return;
    }
    if (obj->o_charges == 0)
    {
	msg(rs,"nothing happens");
	return;
    }
    switch (obj->o_which)
    {
	case WS_LIGHT:
	    /*
	     * Reddy Kilowat wand.  Light up the room
	     */
	    ws_info[WS_LIGHT].oi_know = TRUE;
	    if (proom->r_flags & ISGONE)
		msg(rs,"the corridor glows and then fades");
	    else
	    {
		proom->r_flags &= ~ISDARK;
		/*
		 * Light the room and put the player back up
		 */
		enter_room(rs,&hero);
		addmsg(rs,"the room is lit");
		if (!terse)
		    addmsg(rs," by a shimmering %s light", pick_color("blue"));
		endmsg(rs);
	    }
	when WS_DRAIN:
	    /*
	     * take away 1/2 of hero's hit points, then take it away
	     * evenly from the monsters in the room (or next to hero
	     * if he is in a passage)
	     */
	    if (pstats.s_hpt < 2)
	    {
		msg(rs,"you are too weak to use it");
		return;
	    }
	    else
		drain(rs);
	when WS_INVIS:
	case WS_POLYMORPH:
	case WS_TELAWAY:
	case WS_TELTO:
	case WS_CANCEL:
	    y = hero.y;
	    x = hero.x;
	    while (step_ok(winat(y, x)))
	    {
		y += delta.y;
		x += delta.x;
	    }
	    if ((tp = moat(y, x)) != NULL)
	    {
		monster = tp->t_type;
		if (monster == 'F')
		    player.t_flags &= ~ISHELD;
		switch (obj->o_which) {
		    case WS_INVIS:
			tp->t_flags |= ISINVIS;
			if (cansee(rs,y, x))
			    mvaddch(y, x, tp->t_oldch);
			break;
		    case WS_POLYMORPH:
		    {
			THING *pp;

			pp = tp->t_pack;
			detach(mlist, tp);
			if (see_monst(tp))
			    mvaddch(y, x, chat(y, x));
			oldch = tp->t_oldch;
			delta.y = y;
			delta.x = x;
			new_monster(rs,tp, monster = (char)(rnd(26) + 'A'), &delta);
			if (see_monst(tp))
			    mvaddch(y, x, monster);
			tp->t_oldch = oldch;
			tp->t_pack = pp;
			ws_info[WS_POLYMORPH].oi_know |= see_monst(tp);
			break;
		    }
		    case WS_CANCEL:
			tp->t_flags |= ISCANC;
			tp->t_flags &= ~(ISINVIS|CANHUH);
			tp->t_disguise = tp->t_type;
			if (see_monst(tp))
			    mvaddch(y, x, tp->t_disguise);
			break;
		    case WS_TELAWAY:
		    case WS_TELTO:
                    {
			coord new_pos;

			if (obj->o_which == WS_TELAWAY)
			{
			    do
			    {
				find_floor(rs,NULL, &new_pos, FALSE, TRUE);
			    } while (ce(new_pos, hero));
			}
			else
			{
			    new_pos.y = hero.y + delta.y;
			    new_pos.x = hero.x + delta.x;
			}
			tp->t_dest = &hero;
			tp->t_flags |= ISRUN;
			relocate(rs,tp, &new_pos);
		    }
		}
	    }
	when WS_MISSILE:
	    ws_info[WS_MISSILE].oi_know = TRUE;
	    bolt.o_type = '*';
	    strncpy(bolt.o_hurldmg,"1x4",sizeof(bolt.o_hurldmg));
	    bolt.o_hplus = 100;
	    bolt.o_dplus = 1;
	    bolt.o_flags = ISMISL;
	    if (cur_weapon != NULL)
		bolt.o_launch = cur_weapon->o_which;
	    do_motion(rs,&bolt, delta.y, delta.x);
	    if ((tp = moat(bolt.o_pos.y, bolt.o_pos.x)) != NULL
		&& !save_throw(VS_MAGIC, tp))
		    hit_monster(rs,unc(bolt.o_pos), &bolt);
	    else if (terse)
		msg(rs,"missle vanishes");
	    else
		msg(rs,"the missle vanishes with a puff of smoke");
	when WS_HASTE_M:
	case WS_SLOW_M:
	    y = hero.y;
	    x = hero.x;
	    while (step_ok(winat(y, x)))
	    {
		y += delta.y;
		x += delta.x;
	    }
	    if ((tp = moat(y, x)) != NULL)
	    {
		if (obj->o_which == WS_HASTE_M)
		{
		    if (on(*tp, ISSLOW))
			tp->t_flags &= ~ISSLOW;
		    else
			tp->t_flags |= ISHASTE;
		}
		else
		{
		    if (on(*tp, ISHASTE))
			tp->t_flags &= ~ISHASTE;
		    else
			tp->t_flags |= ISSLOW;
		    tp->t_turn = TRUE;
		}
		delta.y = y;
		delta.x = x;
		runto(rs,&delta);
	    }
	when WS_ELECT:
	case WS_FIRE:
	case WS_COLD:
	    if (obj->o_which == WS_ELECT)
		name = "bolt";
	    else if (obj->o_which == WS_FIRE)
		name = "flame";
	    else
		name = "ice";
	    fire_bolt(rs,&hero, &delta, name);
	    ws_info[obj->o_which].oi_know = TRUE;
	when WS_NOP:
	    break;
#ifdef MASTER
	otherwise:
	    msg(rs,"what a bizarre schtick!");
#endif
    }
    obj->o_charges--;
}

/*
 * drain:
 *	Do drain hit points from player shtick
 */

void
drain(struct rogue_state *rs)
{
    THING *mp;
    struct room *corp;
    THING **dp;
    int cnt;
    bool inpass;
    static THING *drainee[40];

    /*
     * First cnt how many things we need to spread the hit points among
     */
    cnt = 0;
    if (chat(hero.y, hero.x) == DOOR)
	corp = &passages[flat(hero.y, hero.x) & F_PNUM];
    else
	corp = NULL;
    inpass = (bool)(proom->r_flags & ISGONE);
    dp = drainee;
    for (mp = mlist; mp != NULL; mp = next(mp))
	if (mp->t_room == proom || mp->t_room == corp ||
	    (inpass && chat(mp->t_pos.y, mp->t_pos.x) == DOOR &&
	    &passages[flat(mp->t_pos.y, mp->t_pos.x) & F_PNUM] == proom))
		*dp++ = mp;
    if ((cnt = (int)(dp - drainee)) == 0)
    {
	msg(rs,"you have a tingling feeling");
	return;
    }
    *dp = NULL;
    pstats.s_hpt /= 2;
    cnt = pstats.s_hpt / cnt;
    /*
     * Now zot all of the monsters
     */
    for (dp = drainee; *dp; dp++)
    {
	mp = *dp;
	if ((mp->t_stats.s_hpt -= cnt) <= 0)
	    killed(rs,mp, see_monst(mp));
	else
	    runto(rs,&mp->t_pos);
    }
}

/*
 * fire_bolt:
 *	Fire a bolt in a given direction from a specific starting place
 */

void
fire_bolt(struct rogue_state *rs,coord *start, coord *dir, char *name)
{
    coord *c1, *c2;
    THING *tp;
    char dirch = 0, ch;
    bool hit_hero, used, changed;
    static coord pos;
    static coord spotpos[BOLT_LENGTH];
    THING bolt;

    bolt.o_type = WEAPON;
    bolt.o_which = FLAME;
    strncpy(bolt.o_hurldmg,"6x6",sizeof(bolt.o_hurldmg));
    bolt.o_hplus = 100;
    bolt.o_dplus = 0;
    weap_info[FLAME].oi_name = name;
    switch (dir->y + dir->x)
    {
	case 0: dirch = '/';
	when 1: case -1: dirch = (dir->y == 0 ? '-' : '|');
	when 2: case -2: dirch = '\\';
    }
    pos = *start;
    hit_hero = (bool)(start != &hero);
    used = FALSE;
    changed = FALSE;
    for (c1 = spotpos; c1 <= &spotpos[BOLT_LENGTH-1] && !used; c1++)
    {
	pos.y += dir->y;
	pos.x += dir->x;
	*c1 = pos;
	ch = winat(pos.y, pos.x);
	switch (ch)
	{
	    case DOOR:
		/*
		 * this code is necessary if the hero is on a door
		 * and he fires at the wall the door is in, it would
		 * otherwise loop infinitely
		 */
		if (ce(hero, pos))
		    goto def;
		/* FALLTHROUGH */
	    case '|':
	    case '-':
	    case ' ':
		if (!changed)
		    hit_hero = !hit_hero;
		changed = FALSE;
		dir->y = -dir->y;
		dir->x = -dir->x;
		c1--;
		msg(rs,"the %s bounces", name);
		break;
	    default:
def:
		if (!hit_hero && (tp = moat(pos.y, pos.x)) != NULL)
		{
		    hit_hero = TRUE;
		    changed = !changed;
		    tp->t_oldch = chat(pos.y, pos.x);
		    if (!save_throw(VS_MAGIC, tp))
		    {
			bolt.o_pos = pos;
			used = TRUE;
			if (tp->t_type == 'D' && strcmp(name, "flame") == 0)
			{
			    addmsg(rs,"the flame bounces");
			    if (!terse)
				addmsg(rs," off the dragon");
			    endmsg(rs);
			}
			else
			    hit_monster(rs,unc(pos), &bolt);
		    }
		    else if (ch != 'M' || tp->t_disguise == 'M')
		    {
			if (start == &hero)
			    runto(rs,&pos);
			if (terse)
			    msg(rs,"%s misses", name);
			else
			    msg(rs,"the %s whizzes past %s", name, set_mname(tp));
		    }
		}
		else if (hit_hero && ce(pos, hero))
		{
		    hit_hero = FALSE;
		    changed = !changed;
		    if (!save(VS_MAGIC))
		    {
			if ((pstats.s_hpt -= roll(6, 6)) <= 0)
			{
			    if (start == &hero)
				death(rs,'b');
			    else
				death(rs,moat(start->y, start->x)->t_type);
			}
			used = TRUE;
			if (terse)
			    msg(rs,"the %s hits", name);
			else
			    msg(rs,"you are hit by the %s", name);
		    }
		    else
			msg(rs,"the %s whizzes by you", name);
		}
		mvaddch(pos.y, pos.x, dirch);
            if ( rs->sleeptime != 0 )
                refresh();
	}
    }
    for (c2 = spotpos; c2 < c1; c2++)
	mvaddch(c2->y, c2->x, chat(c2->y, c2->x));
}

/*
 * charge_str:
 *	Return an appropriate string for a wand charge
 */
char *
charge_str(THING *obj)
{
    static char buf[20];

    if (!(obj->o_flags & ISKNOW))
	buf[0] = '\0';
    else if (terse)
	sprintf(buf, " [%d]", obj->o_charges);
    else
	sprintf(buf, " [%d charges]", obj->o_charges);
    return buf;
}
