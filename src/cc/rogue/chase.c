/*
 * Code for one creature to chase another
 *
 * @(#)chase.c	4.57 (Berkeley) 02/05/99
 *
 * Rogue: Exploring the Dungeons of Doom
 * Copyright (C) 1980-1983, 1985, 1999 Michael Toy, Ken Arnold and Glenn Wichman
 * All rights reserved.
 *
 * See the file LICENSE.TXT for full copyright and licensing information.
 */

//#include <stdlib.h>
//#include <curses.h>
#include "rogue.h"

#define DRAGONSHOT  5	/* one chance in DRAGONSHOT that a dragon will flame */

static coord ch_ret;				/* Where chasing takes you */

/*
 * runners:
 *	Make all the running monsters move.
 */
void
runners(struct rogue_state *rs,int arg)
{
    register THING *tp;
    THING *next;
    bool wastarget;
    static coord orig_pos;

    for (tp = mlist; tp != NULL; tp = next)
    {
        /* remember this in case the monster's "next" is changed */
        next = next(tp);
	if (!on(*tp, ISHELD) && on(*tp, ISRUN))
	{
	    orig_pos = tp->t_pos;
	    wastarget = on(*tp, ISTARGET);
	    if (move_monst(rs,tp) == -1)
                continue;
	    if (on(*tp, ISFLY) && dist_cp(&hero, &tp->t_pos) >= 3)
		move_monst(rs,tp);
	    if (wastarget && !ce(orig_pos, tp->t_pos))
	    {
		tp->t_flags &= ~ISTARGET;
		to_death = FALSE;
	    }
	}
    }
    if (has_hit)
    {
	endmsg(rs);
	has_hit = FALSE;
    }
}

/*
 * move_monst:
 *	Execute a single turn of running for a monster
 */
int
move_monst(struct rogue_state *rs,THING *tp)
{
    if (!on(*tp, ISSLOW) || tp->t_turn)
	if (do_chase(rs,tp) == -1)
            return(-1);
    if (on(*tp, ISHASTE))
	if (do_chase(rs,tp) == -1)
            return(-1);
    tp->t_turn ^= TRUE;
    return(0);
}

/*
 * relocate:
 *	Make the monster's new location be the specified one, updating
 *	all the relevant state.
 */
void
relocate(struct rogue_state *rs,THING *th, coord *new_loc)
{
    struct room *oroom;

    if (!ce(*new_loc, th->t_pos))
    {
	mvaddch(th->t_pos.y, th->t_pos.x, th->t_oldch);
	th->t_room = roomin(rs,new_loc);
	set_oldch(th, new_loc);
	oroom = th->t_room;
	moat(th->t_pos.y, th->t_pos.x) = NULL;

	if (oroom != th->t_room)
	    th->t_dest = find_dest(rs,th);
	th->t_pos = *new_loc;
	moat(new_loc->y, new_loc->x) = th;
    }
    move(new_loc->y, new_loc->x);
    if (see_monst(th))
	addch(th->t_disguise);
    else if (on(player, SEEMONST))
    {
	standout();
	addch(th->t_type);
	standend();
    }
}

/*
 * do_chase:
 *	Make one thing chase another.
 */
int
do_chase(struct rogue_state *rs,THING *th)
{
    register coord *cp;
    register struct room *rer, *ree;	/* room of chaser, room of chasee */
    register int mindist = 32767, curdist;
    register bool stoprun = FALSE;	/* TRUE means we are there */
    register bool door;
    register THING *obj;
    static coord DEST;			/* Temporary destination for chaser */

    rer = th->t_room;		/* Find room of chaser */
    if (on(*th, ISGREED) && rer->r_goldval == 0)
	th->t_dest = &hero;	/* If gold has been taken, run after hero */
    if (th->t_dest == &hero)	/* Find room of chasee */
	ree = proom;
    else
	ree = roomin(rs,th->t_dest);
    /*
     * We don't count doors as inside rooms for this routine
     */
    door = (chat(th->t_pos.y, th->t_pos.x) == DOOR);
    /*
     * If the object of our desire is in a different room,
     * and we are not in a corridor, run to the door nearest to
     * our goal.
     */
over:
    if (rer != ree)
    {
	for (cp = rer->r_exit; cp < &rer->r_exit[rer->r_nexits]; cp++)
	{
	    curdist = dist_cp(th->t_dest, cp);
	    if (curdist < mindist)
	    {
		DEST = *cp;
		mindist = curdist;
	    }
	}
	if (door)
	{
	    rer = &passages[flat(th->t_pos.y, th->t_pos.x) & F_PNUM];
	    door = FALSE;
	    goto over;
	}
    }
    else
    {
	DEST = *th->t_dest;
	/*
	 * For dragons check and see if (a) the hero is on a straight
	 * line from it, and (b) that it is within shooting distance,
	 * but outside of striking range.
	 */
	if (th->t_type == 'D' && (th->t_pos.y == hero.y || th->t_pos.x == hero.x
	    || abs(th->t_pos.y - hero.y) == abs(th->t_pos.x - hero.x))
	    && dist_cp(&th->t_pos, &hero) <= BOLT_LENGTH * BOLT_LENGTH
	    && !on(*th, ISCANC) && rnd(DRAGONSHOT) == 0)
	{
	    delta.y = sign(hero.y - th->t_pos.y);
	    delta.x = sign(hero.x - th->t_pos.x);
	    if (has_hit)
		endmsg(rs);
	    fire_bolt(rs,&th->t_pos, &delta, "flame");
	    running = FALSE;
	    count = 0;
	    quiet = 0;
	    if (to_death && !on(*th, ISTARGET))
	    {
		to_death = FALSE;
		kamikaze = FALSE;
	    }
	    return(0);
	}
    }
    /*
     * This now contains what we want to run to this time
     * so we run to it.  If we hit it we either want to fight it
     * or stop running
     */
    if (!chase(th, &DEST))
    {
	if (ce(DEST, hero))
	{
	    return( attack(rs,th) );
	}
	else if (ce(DEST, *th->t_dest))
	{
	    for (obj = lvl_obj; obj != NULL; obj = next(obj))
		if (th->t_dest == &obj->o_pos)
		{
		    detach(lvl_obj, obj);
		    attach(th->t_pack, obj);
		    chat(obj->o_pos.y, obj->o_pos.x) =
			(th->t_room->r_flags & ISGONE) ? PASSAGE : FLOOR;
		    th->t_dest = find_dest(rs,th);
		    break;
		}
	    if (th->t_type != 'F')
		stoprun = TRUE;
	}
    }
    else
    {
	if (th->t_type == 'F')
	    return(0);
    }
    relocate(rs,th, &ch_ret);
    /*
     * And stop running if need be
     */
    if (stoprun && ce(th->t_pos, *(th->t_dest)))
	th->t_flags &= ~ISRUN;
    return(0);
}

/*
 * set_oldch:
 *	Set the oldch character for the monster
 */
void
set_oldch(THING *tp, coord *cp)
{
    char sch;

    if (ce(tp->t_pos, *cp))
        return;

    sch = tp->t_oldch;
    tp->t_oldch = CCHAR( mvinch(cp->y,cp->x) );
    if (!on(player, ISBLIND))
    {
	    if ((sch == FLOOR || tp->t_oldch == FLOOR) &&
		(tp->t_room->r_flags & ISDARK))
		    tp->t_oldch = ' ';
	    else if (dist_cp(cp, &hero) <= LAMPDIST && see_floor)
		tp->t_oldch = chat(cp->y, cp->x);
    }
}

/*
 * see_monst:
 *	Return TRUE if the hero can see the monster
 */
bool
see_monst(THING *mp)
{
    int y, x;

    if (on(player, ISBLIND))
	return FALSE;
    if (on(*mp, ISINVIS) && !on(player, CANSEE))
	return FALSE;
    y = mp->t_pos.y;
    x = mp->t_pos.x;
    if (dist(y, x, hero.y, hero.x) < LAMPDIST)
    {
	if (y != hero.y && x != hero.x &&
	    !step_ok(chat(y, hero.x)) && !step_ok(chat(hero.y, x)))
		return FALSE;
	return TRUE;
    }
    if (mp->t_room != proom)
	return FALSE;
    return ((bool)!(mp->t_room->r_flags & ISDARK));
}

/*
 * runto:
 *	Set a monster running after the hero.
 */
void
runto(struct rogue_state *rs,coord *runner)
{
    register THING *tp;

    /*
     * If we couldn't find him, something is funny
     */
#ifdef MASTER
    if ((tp = moat(runner->y, runner->x)) == NULL)
	msg(rs,"couldn't find monster in runto at (%d,%d)", runner->y, runner->x);
#else
    tp = moat(runner->y, runner->x);
#endif
    /*
     * Start the beastie running
     */
    tp->t_flags |= ISRUN;
    tp->t_flags &= ~ISHELD;
    tp->t_dest = find_dest(rs,tp);
}

/*
 * chase:
 *	Find the spot for the chaser(er) to move closer to the
 *	chasee(ee).  Returns TRUE if we want to keep on chasing later
 *	FALSE if we reach the goal.
 */
bool
chase(THING *tp, coord *ee)
{
    register THING *obj;
    register int x, y;
    register int curdist, thisdist;
    register coord *er = &tp->t_pos;
    register char ch;
    register int plcnt = 1;
    static coord tryp;

    /*
     * If the thing is confused, let it move randomly. Invisible
     * Stalkers are slightly confused all of the time, and bats are
     * quite confused all the time
     */
    if ((on(*tp, ISHUH) && rnd(5) != 0) || (tp->t_type == 'P' && rnd(5) == 0)
	|| (tp->t_type == 'B' && rnd(2) == 0))
    {
	/*
	 * get a valid random move
	 */
	ch_ret = *rndmove(tp);
	curdist = dist_cp(&ch_ret, ee);
	/*
	 * Small chance that it will become un-confused 
	 */
	if (rnd(20) == 0)
	    tp->t_flags &= ~ISHUH;
    }
    /*
     * Otherwise, find the empty spot next to the chaser that is
     * closest to the chasee.
     */
    else
    {
	register int ey, ex;
	/*
	 * This will eventually hold where we move to get closer
	 * If we can't find an empty spot, we stay where we are.
	 */
	curdist = dist_cp(er, ee);
	ch_ret = *er;

	ey = er->y + 1;
	if (ey >= NUMLINES - 1)
	    ey = NUMLINES - 2;
	ex = er->x + 1;
	if (ex >= NUMCOLS)
	    ex = NUMCOLS - 1;

	for (x = er->x - 1; x <= ex; x++)
	{
	    if (x < 0)
		continue;
	    tryp.x = x;
	    for (y = er->y - 1; y <= ey; y++)
	    {
		tryp.y = y;
		if (!diag_ok(er, &tryp))
		    continue;
		ch = winat(y, x);
		if (step_ok(ch))
		{
		    /*
		     * If it is a scroll, it might be a scare monster scroll
		     * so we need to look it up to see what type it is.
		     */
		    if (ch == SCROLL)
		    {
			for (obj = lvl_obj; obj != NULL; obj = next(obj))
			{
			    if (y == obj->o_pos.y && x == obj->o_pos.x)
				break;
			}
			if (obj != NULL && obj->o_which == S_SCARE)
			    continue;
		    }
		    /*
		     * It can also be a Xeroc, which we shouldn't step on
		     */
		    if ((obj = moat(y, x)) != NULL && obj->t_type == 'X')
			continue;
		    /*
		     * If we didn't find any scrolls at this place or it
		     * wasn't a scare scroll, then this place counts
		     */
		    thisdist = dist(y, x, ee->y, ee->x);
		    if (thisdist < curdist)
		    {
			plcnt = 1;
			ch_ret = tryp;
			curdist = thisdist;
		    }
		    else if (thisdist == curdist && rnd(++plcnt) == 0)
		    {
			ch_ret = tryp;
			curdist = thisdist;
		    }
		}
	    }
	}
    }
    return (bool)(curdist != 0 && !ce(ch_ret, hero));
}

/*
 * roomin:
 *	Find what room some coordinates are in. NULL means they aren't
 *	in any room.
 */
struct room *
roomin(struct rogue_state *rs,coord *cp)
{
    register struct room *rp;
    register char *fp;


    fp = &flat(cp->y, cp->x);
    if (*fp & F_PASS)
	return &passages[*fp & F_PNUM];

    for (rp = rooms; rp < &rooms[MAXROOMS]; rp++)
	if (cp->x <= rp->r_pos.x + rp->r_max.x && rp->r_pos.x <= cp->x
	 && cp->y <= rp->r_pos.y + rp->r_max.y && rp->r_pos.y <= cp->y)
	    return rp;

    msg(rs,"in some bizarre place (%d, %d)", unc(*cp));
#ifdef MASTER
    abort();
    return NULL;
#else
    return NULL;
#endif
}

/*
 * diag_ok:
 *	Check to see if the move is legal if it is diagonal
 */
bool
diag_ok(coord *sp, coord *ep)
{
    if (ep->x < 0 || ep->x >= NUMCOLS || ep->y <= 0 || ep->y >= NUMLINES - 1)
	return FALSE;
    if (ep->x == sp->x || ep->y == sp->y)
	return TRUE;
    return (bool)(step_ok(chat(ep->y, sp->x)) && step_ok(chat(sp->y, ep->x)));
}

/*
 * cansee:
 *	Returns true if the hero can see a certain coordinate.
 */
bool
cansee(struct rogue_state *rs,int y, int x)
{
    register struct room *rer;
    static coord tp;

    if (on(player, ISBLIND))
	return FALSE;
    if (dist(y, x, hero.y, hero.x) < LAMPDIST)
    {
	if (flat(y, x) & F_PASS)
	    if (y != hero.y && x != hero.x &&
		!step_ok(chat(y, hero.x)) && !step_ok(chat(hero.y, x)))
		    return FALSE;
	return TRUE;
    }
    /*
     * We can only see if the hero in the same room as
     * the coordinate and the room is lit or if it is close.
     */
    tp.y = y;
    tp.x = x;
    return (bool)((rer = roomin(rs,&tp)) == proom && !(rer->r_flags & ISDARK));
}

/*
 * find_dest:
 *	find the proper destination for the monster
 */
coord *
find_dest(struct rogue_state *rs,THING *tp)
{
    register THING *obj;
    register int prob;

    if ((prob = monsters[tp->t_type - 'A'].m_carry) <= 0 || tp->t_room == proom
	|| see_monst(tp))
	    return &hero;
    for (obj = lvl_obj; obj != NULL; obj = next(obj))
    {
	if (obj->o_type == SCROLL && obj->o_which == S_SCARE)
	    continue;
	if (roomin(rs,&obj->o_pos) == tp->t_room && rnd(100) < prob)
	{
	    for (tp = mlist; tp != NULL; tp = next(tp))
		if (tp->t_dest == &obj->o_pos)
		    break;
	    if (tp == NULL)
		return &obj->o_pos;
	}
    }
    return &hero;
}

/*
 * dist:
 *	Calculate the "distance" between to points.  Actually,
 *	this calculates d^2, not d, but that's good enough for
 *	our purposes, since it's only used comparitively.
 */
int
dist(int y1, int x1, int y2, int x2)
{
    return ((x2 - x1) * (x2 - x1) + (y2 - y1) * (y2 - y1));
}

/*
 * dist_cp:
 *	Call dist() with appropriate arguments for coord pointers
 */
int
dist_cp(coord *c1, coord *c2)
{
    return dist(c1->y, c1->x, c2->y, c2->x);
}
