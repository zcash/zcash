/*
 * All the daemon and fuse functions are in here
 *
 * @(#)daemons.c	4.24 (Berkeley) 02/05/99
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
 * doctor:
 *	A healing daemon that restors hit points after rest
 */
void
doctor(struct rogue_state *rs,int arg)
{
    register int lv, ohp;
    if ( rs->logfp != 0 )
        fprintf(rs->logfp,"doctor\n");

    lv = pstats.s_lvl;
    ohp = pstats.s_hpt;
    quiet++;
    if (lv < 8)
    {
	if (quiet + (lv << 1) > 20)
	    pstats.s_hpt++;
    }
    else
	if (quiet >= 3)
	    pstats.s_hpt += rnd(lv - 7) + 1;
    if (ISRING(LEFT, R_REGEN))
	pstats.s_hpt++;
    if (ISRING(RIGHT, R_REGEN))
	pstats.s_hpt++;
    if (ohp != pstats.s_hpt)
    {
	if (pstats.s_hpt > max_hp)
	    pstats.s_hpt = max_hp;
	quiet = 0;
    }
}

/*
 * Swander:
 *	Called when it is time to start rolling for wandering monsters
 */
void
swander(struct rogue_state *rs,int arg)
{
    if ( rs->logfp != 0 )
        fprintf(rs->logfp,"swander\n");
    start_daemon(rollwand, 0, BEFORE);
}

/*
 * rollwand:
 *	Called to roll to see if a wandering monster starts up
 */
int between = 0;
void
rollwand(struct rogue_state *rs,int arg)
{
    if ( rs->logfp != 0 )
        fprintf(rs->logfp,"rollwand\n");
    if (++between >= 4)
    {
	if (roll(1, 6) == 4)
	{
	    wanderer(rs);
	    kill_daemon(rollwand);
	    fuse(swander, 0, WANDERTIME, BEFORE);
	}
	between = 0;
    }
}

/*
 * unconfuse:
 *	Release the poor player from his confusion
 */
void
unconfuse(struct rogue_state *rs,int arg)
{
    if ( rs->logfp != 0 )
        fprintf(rs->logfp,"unconfuse\n");
    player.t_flags &= ~ISHUH;
    msg(rs,"you feel less %s now", choose_str("trippy", "confused"));
}

/*
 * unsee:
 *	Turn off the ability to see invisible
 */
void
unsee(struct rogue_state *rs,int arg)
{
    register THING *th;
    if ( rs->logfp != 0 )
        fprintf(rs->logfp,"unsee\n");

    for (th = mlist; th != NULL; th = next(th))
	if (on(*th, ISINVIS) && see_monst(th))
	    mvaddch(th->t_pos.y, th->t_pos.x, th->t_oldch);
    player.t_flags &= ~CANSEE;
}

/*
 * sight:
 *	He gets his sight back
 */
void
sight(struct rogue_state *rs,int arg)
{
    if ( rs->logfp != 0 )
        fprintf(rs->logfp,"sight\n");
    if (on(player, ISBLIND))
    {
	extinguish(sight);
	player.t_flags &= ~ISBLIND;
	if (!(proom->r_flags & ISGONE))
	    enter_room(rs,&hero);
	msg(rs,choose_str("far out!  Everything is all cosmic again",
		       "the veil of darkness lifts"));
    }
}

/*
 * nohaste:
 *	End the hasting
 */
void
nohaste(struct rogue_state *rs,int arg)
{
    if ( rs->logfp != 0 )
        fprintf(rs->logfp,"nohaste\n");
    player.t_flags &= ~ISHASTE;
    msg(rs,"you feel yourself slowing down");
}

/*
 * stomach:
 *	Digest the hero's food
 */
void
stomach(struct rogue_state *rs,int arg)
{
    register int oldfood;
    int orig_hungry = hungry_state;
    if ( rs->logfp != 0 )
        fprintf(rs->logfp,"stomach\n");

    if (food_left <= 0)
    {
	if (food_left-- < -STARVETIME)
	    death(rs,'s');
	/*
	 * the hero is fainting
	 */
	if (no_command || rnd(5) != 0)
	    return;
	no_command += rnd(8) + 4;
	hungry_state = 3;
	if (!terse)
	    addmsg(rs,choose_str("the munchies overpower your motor capabilities.  ",
			      "you feel too weak from lack of food.  "));
	msg(rs,choose_str("You freak out", "You faint"));
    }
    else
    {
	oldfood = food_left;
	food_left -= ring_eat(LEFT) + ring_eat(RIGHT) + 1 - amulet;

	if (food_left < MORETIME && oldfood >= MORETIME)
	{
	    hungry_state = 2;
	    msg(rs,choose_str("the munchies are interfering with your motor capabilites",
			   "you are starting to feel weak"));
	}
	else if (food_left < 2 * MORETIME && oldfood >= 2 * MORETIME)
	{
	    hungry_state = 1;
	    if (terse)
		msg(rs,choose_str("getting the munchies", "getting hungry"));
	    else
		msg(rs,choose_str("you are getting the munchies",
			       "you are starting to get hungry"));
	}
    }
    if (hungry_state != orig_hungry) { 
        player.t_flags &= ~ISRUN; 
        running = FALSE; 
        to_death = FALSE; 
        count = 0; 
    } 
}

/*
 * come_down:
 *	Take the hero down off her acid trip.
 */
void
come_down(struct rogue_state *rs,int arg)
{
    register THING *tp;
    register bool seemonst;
    
    if ( rs->logfp != 0 )
        fprintf(rs->logfp,"come_down\n");
    if (!on(player, ISHALU))
        return;
    
    kill_daemon(visuals);
    player.t_flags &= ~ISHALU;
    
    if (on(player, ISBLIND))
        return;
    
    /*
     * undo the things
     */
    for (tp = lvl_obj; tp != NULL; tp = next(tp))
        if (cansee(rs,tp->o_pos.y, tp->o_pos.x))
            mvaddch(tp->o_pos.y, tp->o_pos.x, tp->o_type);
    
    /*
     * undo the monsters
     */
    seemonst = on(player, SEEMONST);
    for (tp = mlist; tp != NULL; tp = next(tp))
    {
        move(tp->t_pos.y, tp->t_pos.x);
        if (cansee(rs,tp->t_pos.y, tp->t_pos.x))
            if (!on(*tp, ISINVIS) || on(player, CANSEE))
                addch(tp->t_disguise);
            else
                addch(chat(tp->t_pos.y, tp->t_pos.x));
            else if (seemonst)
            {
                standout();
                addch(tp->t_type);
                standend();
            }
    }
    msg(rs,"Everything looks SO boring now.");
}

/*
 * visuals:
 *	change the characters for the player
 */
void
visuals(struct rogue_state *rs,int arg)
{
    register THING *tp;
    register bool seemonst;
    
    if (!after || (running && jump))
        return;
    if ( rs->logfp != 0 )
        fprintf(rs->logfp,"visuals\n");
    /*
     * change the things
     */
    for (tp = lvl_obj; tp != NULL; tp = next(tp))
        if (cansee(rs,tp->o_pos.y, tp->o_pos.x))
            mvaddch(tp->o_pos.y, tp->o_pos.x, rnd_thing());
    
    /*
     * change the stairs
     */
    if (!seenstairs && cansee(rs,stairs.y, stairs.x))
        mvaddch(stairs.y, stairs.x, rnd_thing());
    
    /*
     * change the monsters
     */
    seemonst = on(player, SEEMONST);
    for (tp = mlist; tp != NULL; tp = next(tp))
    {
        move(tp->t_pos.y, tp->t_pos.x);
        if (see_monst(tp))
        {
            if (tp->t_type == 'X' && tp->t_disguise != 'X')
                addch(rnd_thing());
            else
                addch(rnd(26) + 'A');
        }
        else if (seemonst)
        {
            standout();
            addch(rnd(26) + 'A');
            standend();
        }
    }
}

/*
 * land:
 *	Land from a levitation potion
 */
void
land(struct rogue_state *rs,int arg)
{
    if ( rs->logfp != 0 )
        fprintf(rs->logfp,"land\n");
    player.t_flags &= ~ISLEVIT;
    msg(rs,choose_str("bummer!  You've hit the ground",
		   "you float gently to the ground"));
}

/*
 * turn_see:
 *	Put on or off seeing monsters on this level
 */
bool
turn_see(struct rogue_state *rs,bool turn_off)
{
    THING *mp;
    bool can_see, add_new;
    if ( rs->logfp != 0 )
        fprintf(rs->logfp,"turn_see\n");
    
    add_new = FALSE;
    for (mp = mlist; mp != NULL; mp = next(mp))
    {
        move(mp->t_pos.y, mp->t_pos.x);
        can_see = see_monst(mp);
        if (turn_off)
        {
            if (!can_see)
                addch(mp->t_oldch);
        }
        else
        {
            if (!can_see)
                standout();
            if (!on(player, ISHALU))
                addch(mp->t_type);
            else
                addch(rnd(26) + 'A');
            if (!can_see)
            {
                standend();
                add_new ^= 1;//add_new++;
            }
        }
    }
    if (turn_off)
        player.t_flags &= ~SEEMONST;
    else
        player.t_flags |= SEEMONST;
    return add_new;
}

