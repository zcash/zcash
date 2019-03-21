/*
 * Read a scroll and let it happen
 *
 * @(#)scrolls.c	4.44 (Berkeley) 02/05/99
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

/*
 * read_scroll:
 *	Read a scroll from the pack and do the appropriate thing
 */

void
read_scroll(struct rogue_state *rs)
{
    THING *obj;
    PLACE *pp;
    int y, x;
    char ch;
    int i;
    bool discardit = FALSE;
    struct room *cur_room;
    THING *orig_obj;
    static coord mp;

    obj = get_item(rs,"read", SCROLL);
    if (obj == NULL)
	return;
    if (obj->o_type != SCROLL)
    {
	if (!terse)
	    msg(rs,"there is nothing on it to read");
	else
	    msg(rs,"nothing to read");
	return;
    }
    /*
     * Calculate the effect it has on the poor guy.
     */
    if (obj == cur_weapon)
	cur_weapon = NULL;
    /*
     * Get rid of the thing
     */
    discardit = (bool)(obj->o_count == 1);
    leave_pack(rs,obj, FALSE, FALSE);
    orig_obj = obj;

    switch (obj->o_which)
    {
	case S_CONFUSE:
	    /*
	     * Scroll of monster confusion.  Give him that power.
	     */
	    player.t_flags |= CANHUH;
	    msg(rs,"your hands begin to glow %s", pick_color("red"));
	when S_ARMOR:
	    if (cur_armor != NULL)
	    {
		cur_armor->o_arm--;
		cur_armor->o_flags &= ~ISCURSED;
		msg(rs,"your armor glows %s for a moment", pick_color("silver"));
	    }
	when S_HOLD:
	    /*
	     * Hold monster scroll.  Stop all monsters within two spaces
	     * from chasing after the hero.
	     */

	    ch = 0;
	    for (x = hero.x - 2; x <= hero.x + 2; x++)
		if (x >= 0 && x < NUMCOLS)
		    for (y = hero.y - 2; y <= hero.y + 2; y++)
			if (y >= 0 && y <= NUMLINES - 1)
			    if ((obj = moat(y, x)) != NULL && on(*obj, ISRUN))
			    {
				obj->t_flags &= ~ISRUN;
				obj->t_flags |= ISHELD;
				ch++;
			    }
	    if (ch)
	    {
		addmsg(rs,"the monster");
		if (ch > 1)
		    addmsg(rs,"s around you");
		addmsg(rs," freeze");
		if (ch == 1)
		    addmsg(rs,"s");
		endmsg(rs);
		scr_info[S_HOLD].oi_know = TRUE;
	    }
	    else
		msg(rs,"you feel a strange sense of loss");
	when S_SLEEP:
	    /*
	     * Scroll which makes you fall asleep
	     */
	    scr_info[S_SLEEP].oi_know = TRUE;
	    no_command += rnd(SLEEPTIME) + 4;
	    player.t_flags &= ~ISRUN;
	    msg(rs,"you fall asleep");
	when S_CREATE:
	    /*
	     * Create a monster:
	     * First look in a circle around him, next try his room
	     * otherwise give up
	     */
	    i = 0;
	    for (y = hero.y - 1; y <= hero.y + 1; y++)
		for (x = hero.x - 1; x <= hero.x + 1; x++)
		    /*
		     * Don't put a monster in top of the player.
		     */
		    if (y == hero.y && x == hero.x)
			continue;
		    /*
		     * Or anything else nasty
		     */
		    else if (step_ok(ch = winat(y, x)))
		    {
			if (ch == SCROLL
			    && find_obj(rs,y, x)->o_which == S_SCARE)
				continue;
			else if (rnd(++i) == 0)
			{
			    mp.y = y;
			    mp.x = x;
			}
		    }
	    if (i == 0)
		msg(rs,"you hear a faint cry of anguish in the distance");
	    else
	    {
		obj = new_item();
		new_monster(rs,obj, randmonster(FALSE), &mp);
	    }
	when S_ID_POTION:
	case S_ID_SCROLL:
	case S_ID_WEAPON:
	case S_ID_ARMOR:
	case S_ID_R_OR_S:
	{
	    static char id_type[S_ID_R_OR_S + 1] =
		{ 0, 0, 0, 0, 0, POTION, SCROLL, WEAPON, ARMOR, R_OR_S };
	    /*
	     * Identify, let him figure something out
	     */
	    scr_info[obj->o_which].oi_know = TRUE;
	    msg(rs,"this scroll is an %s scroll", scr_info[obj->o_which].oi_name);
	    whatis(rs,TRUE, id_type[obj->o_which]);
	}
	when S_MAP:
	    /*
	     * Scroll of magic mapping.
	     */
	    scr_info[S_MAP].oi_know = TRUE;
	    msg(rs,"oh, now this scroll has a map on it");
	    /*
	     * take all the things we want to keep hidden out of the window
	     */
	    for (y = 1; y < NUMLINES - 1; y++)
		for (x = 0; x < NUMCOLS; x++)
		{
		    pp = INDEX(y, x);
		    switch (ch = pp->p_ch)
		    {
			case DOOR:
			case STAIRS:
			    break;

			case '-':
			case '|':
			    if (!(pp->p_flags & F_REAL))
			    {
				ch = pp->p_ch = DOOR;
				pp->p_flags |= F_REAL;
			    }
			    break;

			case ' ':
			    if (pp->p_flags & F_REAL)
				goto def;
			    pp->p_flags |= F_REAL;
			    ch = pp->p_ch = PASSAGE;
			    /* FALLTHROUGH */

			case PASSAGE:
pass:
			    if (!(pp->p_flags & F_REAL))
				pp->p_ch = PASSAGE;
			    pp->p_flags |= (F_SEEN|F_REAL);
			    ch = PASSAGE;
			    break;

			case FLOOR:
			    if (pp->p_flags & F_REAL)
				ch = ' ';
			    else
			    {
				ch = TRAP;
				pp->p_ch = TRAP;
				pp->p_flags |= (F_SEEN|F_REAL);
			    }
			    break;

			default:
def:
			    if (pp->p_flags & F_PASS)
				goto pass;
			    ch = ' ';
			    break;
		    }
		    if (ch != ' ')
		    {
			if ((obj = pp->p_monst) != NULL)
			    obj->t_oldch = ch;
			if (obj == NULL || !on(player, SEEMONST))
			    mvaddch(y, x, ch);
		    }
		}
	when S_FDET:
	    /*
	     * Potion of gold detection
	     */
	    ch = FALSE;
	    wclear(hw);
	    for (obj = lvl_obj; obj != NULL; obj = next(obj))
		if (obj->o_type == FOOD)
		{
		    ch = TRUE;
		    wmove(hw, obj->o_pos.y, obj->o_pos.x);
		    waddch(hw, FOOD);
		}
	    if (ch)
	    {
		scr_info[S_FDET].oi_know = TRUE;
		show_win(rs,"Your nose tingles and you smell food.--More--");
	    }
	    else
		msg(rs,"your nose tingles");
	when S_TELEP:
	    /*
	     * Scroll of teleportation:
	     * Make him dissapear and reappear
	     */
	    {
		cur_room = proom;
		teleport(rs);
		if (cur_room != proom)
		    scr_info[S_TELEP].oi_know = TRUE;
	    }
	when S_ENCH:
	    if (cur_weapon == NULL || cur_weapon->o_type != WEAPON)
		msg(rs,"you feel a strange sense of loss");
	    else
	    {
		cur_weapon->o_flags &= ~ISCURSED;
		if (rnd(2) == 0)
		    cur_weapon->o_hplus++;
		else
		    cur_weapon->o_dplus++;
		msg(rs,"your %s glows %s for a moment",
		    weap_info[cur_weapon->o_which].oi_name, pick_color("blue"));
	    }
	when S_SCARE:
	    /*
	     * Reading it is a mistake and produces laughter at her
	     * poor boo boo.
	     */
	    msg(rs,"you hear maniacal laughter in the distance");
	when S_REMOVE:
	    uncurse(cur_armor);
	    uncurse(cur_weapon);
	    uncurse(cur_ring[LEFT]);
	    uncurse(cur_ring[RIGHT]);
	    msg(rs,choose_str("you feel in touch with the Universal Onenes",
			   "you feel as if somebody is watching over you"));
	when S_AGGR:
	    /*
	     * This scroll aggravates all the monsters on the current
	     * level and sets them running towards the hero
	     */
	    aggravate(rs);
	    msg(rs,"you hear a high pitched humming noise");
	when S_PROTECT:
	    if (cur_armor != NULL)
	    {
		cur_armor->o_flags |= ISPROT;
		msg(rs,"your armor is covered by a shimmering %s shield",
		    pick_color("gold"));
	    }
	    else
		msg(rs,"you feel a strange sense of loss");
#ifdef MASTER
	otherwise:
	    msg(rs,"what a puzzling scroll!");
	    return;
#endif
    }
    obj = orig_obj;
    look(rs,TRUE);	/* put the result of the scroll on the screen */
    status(rs);

    call_it(rs,&scr_info[obj->o_which]);

    if (discardit)
        discard(obj);
}

/*
 * uncurse:
 *	Uncurse an item
 */

void
uncurse(THING *obj)
{
    if (obj != NULL)
	obj->o_flags &= ~ISCURSED;
}
