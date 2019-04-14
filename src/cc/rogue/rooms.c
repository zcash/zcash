/*
 * Create the layout for the new level
 *
 * @(#)rooms.c	4.45 (Berkeley) 02/05/99
 *
 * Rogue: Exploring the Dungeons of Doom
 * Copyright (C) 1980-1983, 1985, 1999 Michael Toy, Ken Arnold and Glenn Wichman
 * All rights reserved.
 *
 * See the file LICENSE.TXT for full copyright and licensing information.
 */

//#include <ctype.h>
//#include <curses.h>
#include "rogue.h"

typedef struct spot {		/* position matrix for maze positions */
	int	nexits;
	coord	exits[4];
	int	used;
} SPOT;

#define GOLDGRP 1

/*
 * do_rooms:
 *	Create rooms and corridors with a connectivity graph
 */

void
do_rooms(struct rogue_state *rs)
{
    int i;
    struct room *rp;
    THING *tp;
    int left_out;
    static coord top;
    coord bsze;				/* maximum room size */
    coord mp;
    
    bsze.x = NUMCOLS / 3;
    bsze.y = NUMLINES / 3;
    /*
     * Clear things for a new level
     */
    for (rp = rooms; rp < &rooms[MAXROOMS]; rp++)
    {
        rp->r_goldval = 0;
        rp->r_nexits = 0;
        rp->r_flags = 0;
    }
    /*
     * Put the gone rooms, if any, on the level
     */
    left_out = rnd(4);
    for (i = 0; i < left_out; i++)
        rooms[rnd_room()].r_flags |= ISGONE;
    /*
     * dig and populate all the rooms on the level
     */
    for (i = 0, rp = rooms; i < MAXROOMS; rp++, i++)
    {
        /*
         * Find upper left corner of box that this room goes in
         */
        top.x = (i % 3) * bsze.x + 1;
        top.y = (i / 3) * bsze.y;
        if (rp->r_flags & ISGONE)
        {
            /*
             * Place a gone room.  Make certain that there is a blank line
             * for passage drawing.
             */
            do
            {
                rp->r_pos.x = top.x + rnd(bsze.x - 2) + 1;
                rp->r_pos.y = top.y + rnd(bsze.y - 2) + 1;
                rp->r_max.x = -NUMCOLS;
                rp->r_max.y = -NUMLINES;
            } until (rp->r_pos.y > 0 && rp->r_pos.y < NUMLINES-1);
            continue;
        }
        /*
         * set room type
         */
        if (rnd(10) < level - 1)
        {
            rp->r_flags |= ISDARK;		/* dark room */
            if (rnd(15) == 0)
                rp->r_flags = ISMAZE;		/* maze room */
        }
        /*
         * Find a place and size for a random room
         */
        if (rp->r_flags & ISMAZE)
        {
            rp->r_max.x = bsze.x - 1;
            rp->r_max.y = bsze.y - 1;
            if ((rp->r_pos.x = top.x) == 1)
                rp->r_pos.x = 0;
            if ((rp->r_pos.y = top.y) == 0)
            {
                rp->r_pos.y++;
                rp->r_max.y--;
            }
        }
        else
            do
            {
                rp->r_max.x = rnd(bsze.x - 4) + 4;
                rp->r_max.y = rnd(bsze.y - 4) + 4;
                rp->r_pos.x = top.x + rnd(bsze.x - rp->r_max.x);
                rp->r_pos.y = top.y + rnd(bsze.y - rp->r_max.y);
            } until (rp->r_pos.y != 0);
        draw_room(rs,rp);
        /*
         * Put the gold in
         */
        if (rnd(2) == 0 && (!amulet || level >= max_level))
        {
            THING *gold;
            
            gold = new_item();
            gold->o_goldval = rp->r_goldval = GOLDCALC;
            find_floor(rs,rp, &rp->r_gold, FALSE, FALSE);
            gold->o_pos = rp->r_gold;
            chat(rp->r_gold.y, rp->r_gold.x) = GOLD;
            gold->o_flags = ISMANY;
            gold->o_group = GOLDGRP;
            gold->o_type = GOLD;
            attach(lvl_obj, gold);
        }
        /*
         * Put the monster in
         */
        if (rnd(100) < (rp->r_goldval > 0 ? 80 : 25))
        {
            tp = new_item();
            find_floor(rs,rp, &mp, FALSE, TRUE);
            new_monster(rs,tp, randmonster(FALSE), &mp);
            give_pack(rs,tp);
        }
    }
}

/*
 * draw_room:
 *	Draw a box around a room and lay down the floor for normal
 *	rooms; for maze rooms, draw maze.
 */

void
draw_room(struct rogue_state *rs,struct room *rp)
{
    int y, x;

    if (rp->r_flags & ISMAZE)
	do_maze(rs,rp);
    else
    {
	vert(rp, rp->r_pos.x);				/* Draw left side */
	vert(rp, rp->r_pos.x + rp->r_max.x - 1);	/* Draw right side */
	horiz(rp, rp->r_pos.y);				/* Draw top */
	horiz(rp, rp->r_pos.y + rp->r_max.y - 1);	/* Draw bottom */

	/*
	 * Put the floor down
	 */
	for (y = rp->r_pos.y + 1; y < rp->r_pos.y + rp->r_max.y - 1; y++)
	    for (x = rp->r_pos.x + 1; x < rp->r_pos.x + rp->r_max.x - 1; x++)
		chat(y, x) = FLOOR;
    }
}

/*
 * vert:
 *	Draw a vertical line
 */

void
vert(struct room *rp, int startx)
{
    int y;

    for (y = rp->r_pos.y + 1; y <= rp->r_max.y + rp->r_pos.y - 1; y++)
	chat(y, startx) = '|';
}

/*
 * horiz:
 *	Draw a horizontal line
 */

void
horiz(struct room *rp, int starty)
{
    int x;

    for (x = rp->r_pos.x; x <= rp->r_pos.x + rp->r_max.x - 1; x++)
	chat(starty, x) = '-';
}

/*
 * do_maze:
 *	Dig a maze
 */

static int	Maxy, Maxx, Starty, Startx;

static SPOT	maze[NUMLINES/3+1][NUMCOLS/3+1];


void
do_maze(struct rogue_state *rs,struct room *rp)
{
    SPOT *sp;
    int starty, startx;
    static coord pos;

    for (sp = &maze[0][0]; sp <= &maze[NUMLINES / 3][NUMCOLS / 3]; sp++)
    {
	sp->used = FALSE;
	sp->nexits = 0;
    }

    Maxy = rp->r_max.y;
    Maxx = rp->r_max.x;
    Starty = rp->r_pos.y;
    Startx = rp->r_pos.x;
    starty = (rnd(rp->r_max.y) / 2) * 2;
    startx = (rnd(rp->r_max.x) / 2) * 2;
    pos.y = starty + Starty;
    pos.x = startx + Startx;
    putpass(&pos);
    dig(rs,starty, startx);
}

/*
 * dig:
 *	Dig out from around where we are now, if possible
 */

void
dig(struct rogue_state *rs,int y, int x)
{
    coord *cp;
    int cnt, newy, newx, nexty = 0, nextx = 0;
    static coord pos;
    static coord del[4] = {
	{2, 0}, {-2, 0}, {0, 2}, {0, -2}
    };

    for (;;)
    {
        if ( rs->replaydone != 0 )
            return;
	cnt = 0;
	for (cp = del; cp <= &del[3]; cp++)
	{
	    newy = y + cp->y;
	    newx = x + cp->x;
	    if (newy < 0 || newy > Maxy || newx < 0 || newx > Maxx)
		continue;
	    if (flat(newy + Starty, newx + Startx) & F_PASS)
		continue;
	    if (rnd(++cnt) == 0)
	    {
		nexty = newy;
		nextx = newx;
	    }
	}
	if (cnt == 0)
	    return;
	accnt_maze(y, x, nexty, nextx);
	accnt_maze(nexty, nextx, y, x);
	if (nexty == y)
	{
	    pos.y = y + Starty;
	    if (nextx - x < 0)
		pos.x = nextx + Startx + 1;
	    else
		pos.x = nextx + Startx - 1;
	}
	else
	{
	    pos.x = x + Startx;
	    if (nexty - y < 0)
		pos.y = nexty + Starty + 1;
	    else
		pos.y = nexty + Starty - 1;
	}
	putpass(&pos);
	pos.y = nexty + Starty;
	pos.x = nextx + Startx;
	putpass(&pos);
	dig(rs,nexty, nextx);
    }
}

/*
 * accnt_maze:
 *	Account for maze exits
 */

void
accnt_maze(int y, int x, int ny, int nx)
{
    SPOT *sp;
    coord *cp;

    sp = &maze[y][x];
    for (cp = sp->exits; cp < &sp->exits[sp->nexits]; cp++)
	if (cp->y == ny && cp->x == nx)
	    return;
    cp->y = ny;
    cp->x = nx;
}

/*
 * rnd_pos:
 *	Pick a random spot in a room
 */

void
rnd_pos(struct room *rp, coord *cp)
{
    cp->x = rp->r_pos.x + rnd(rp->r_max.x - 2) + 1;
    cp->y = rp->r_pos.y + rnd(rp->r_max.y - 2) + 1;
}

/*
 * find_floor:
 *	Find a valid floor spot in this room.  If rp is NULL, then
 *	pick a new room each time around the loop.
 */
bool
find_floor(struct rogue_state *rs,struct room *rp, coord *cp, int limit, bool monst)
{
    PLACE *pp;
    int cnt;
    char compchar = 0;
    bool pickroom;

    pickroom = (bool)(rp == NULL);

    if (!pickroom)
	compchar = ((rp->r_flags & ISMAZE) ? PASSAGE : FLOOR);
    cnt = limit;
    for (;;)
    {
        if ( rs->replaydone != 0 )
            return(FALSE);
	if (limit && cnt-- == 0)
	    return FALSE;
	if (pickroom)
	{
	    rp = &rooms[rnd_room()];
	    compchar = ((rp->r_flags & ISMAZE) ? PASSAGE : FLOOR);
	}
	rnd_pos(rp, cp);
	pp = INDEX(cp->y, cp->x);
	if (monst)
	{
	    if (pp->p_monst == NULL && step_ok(pp->p_ch))
		return TRUE;
	}
	else if (pp->p_ch == compchar)
	    return TRUE;
    }
}

/*
 * enter_room:
 *	Code that is executed whenver you appear in a room
 */

void
enter_room(struct rogue_state *rs,coord *cp)
{
    struct room *rp;
    THING *tp;
    int y, x;
    char ch;

    rp = proom = roomin(rs,cp);
    door_open(rs,rp);
    if (!(rp->r_flags & ISDARK) && !on(player, ISBLIND))
	for (y = rp->r_pos.y; y < rp->r_max.y + rp->r_pos.y; y++)
	{
	    move(y, rp->r_pos.x);
	    for (x = rp->r_pos.x; x < rp->r_max.x + rp->r_pos.x; x++)
	    {
		tp = moat(y, x);
		ch = chat(y, x);
		if (tp == NULL)
		    if (CCHAR(inch()) != ch)
			addch(ch);
		    else
			move(y, x + 1);
		else
		{
		    tp->t_oldch = ch;
		    if (!see_monst(tp))
			if (on(player, SEEMONST))
			{
			    standout();
			    addch(tp->t_disguise);
			    standend();
			}
			else
			    addch(ch);
		    else
			addch(tp->t_disguise);
		}
	    }
	}
}

/*
 * leave_room:
 *	Code for when we exit a room
 */

void
leave_room(struct rogue_state *rs,coord *cp)
{
    PLACE *pp;
    struct room *rp;
    int y, x;
    char floor;
    char ch;

    rp = proom;

    if (rp->r_flags & ISMAZE)
	return;

    if (rp->r_flags & ISGONE)
	floor = PASSAGE;
    else if (!(rp->r_flags & ISDARK) || on(player, ISBLIND))
	floor = FLOOR;
    else
	floor = ' ';

    proom = &passages[flat(cp->y, cp->x) & F_PNUM];
    for (y = rp->r_pos.y; y < rp->r_max.y + rp->r_pos.y; y++)
	for (x = rp->r_pos.x; x < rp->r_max.x + rp->r_pos.x; x++)
	{
	    move(y, x);
	    switch ( ch = CCHAR(inch()) )
	    {
		case FLOOR:
		    if (floor == ' ' && ch != ' ')
			addch(' ');
		    break;
		default:
		    /*
		     * to check for monster, we have to strip out
		     * standout bit
		     */
		    if (isupper(toascii(ch)))
		    {
			if (on(player, SEEMONST))
			{
			    standout();
			    addch(ch);
			    standend();
			    break;
			}
                        pp = INDEX(y,x);
			addch(pp->p_ch == DOOR ? DOOR : floor);
		    }
	    }
	}
    door_open(rs,rp);
}
