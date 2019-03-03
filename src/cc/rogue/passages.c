/*
 * Draw the connecting passages
 *
 * @(#)passages.c	4.22 (Berkeley) 02/05/99
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

/*
 * do_passages:
 *	Draw all the passages on a level.
 */

void
do_passages(struct rogue_state *rs)
{
    struct rdes *r1, *r2 = NULL;
    int i, j;
    int roomcount;
    static struct rdes
    {
        bool	conn[MAXROOMS];		/* possible to connect to room i? */
        bool	isconn[MAXROOMS];	/* connection been made to room i? */
        bool	ingraph;		/* this room in graph already? */
    } rdes[MAXROOMS] = {
        { { 0, 1, 0, 1, 0, 0, 0, 0, 0 }, { 0, 0, 0, 0, 0, 0, 0, 0, 0 }, 0 },
        { { 1, 0, 1, 0, 1, 0, 0, 0, 0 }, { 0, 0, 0, 0, 0, 0, 0, 0, 0 }, 0 },
        { { 0, 1, 0, 0, 0, 1, 0, 0, 0 }, { 0, 0, 0, 0, 0, 0, 0, 0, 0 }, 0 },
        { { 1, 0, 0, 0, 1, 0, 1, 0, 0 }, { 0, 0, 0, 0, 0, 0, 0, 0, 0 }, 0 },
        { { 0, 1, 0, 1, 0, 1, 0, 1, 0 }, { 0, 0, 0, 0, 0, 0, 0, 0, 0 }, 0 },
        { { 0, 0, 1, 0, 1, 0, 0, 0, 1 }, { 0, 0, 0, 0, 0, 0, 0, 0, 0 }, 0 },
        { { 0, 0, 0, 1, 0, 0, 0, 1, 0 }, { 0, 0, 0, 0, 0, 0, 0, 0, 0 }, 0 },
        { { 0, 0, 0, 0, 1, 0, 1, 0, 1 }, { 0, 0, 0, 0, 0, 0, 0, 0, 0 }, 0 },
        { { 0, 0, 0, 0, 0, 1, 0, 1, 0 }, { 0, 0, 0, 0, 0, 0, 0, 0, 0 }, 0 },
    };
    
    /*
     * reinitialize room graph description
     */
    for (r1 = rdes; r1 <= &rdes[MAXROOMS-1]; r1++)
    {
        for (j = 0; j < MAXROOMS; j++)
            r1->isconn[j] = FALSE;
        r1->ingraph = FALSE;
    }
    
    /*
     * starting with one room, connect it to a random adjacent room and
     * then pick a new room to start with.
     */
    roomcount = 1;
    r1 = &rdes[rnd(MAXROOMS)];
    r1->ingraph = TRUE;
    do
    {
        /*
         * find a room to connect with
         */
        j = 0;
        for (i = 0; i < MAXROOMS; i++)
            if (r1->conn[i] && !rdes[i].ingraph && rnd(++j) == 0)
                r2 = &rdes[i];
        /*
         * if no adjacent rooms are outside the graph, pick a new room
         * to look from
         */
        if (j == 0)
        {
            do
                r1 = &rdes[rnd(MAXROOMS)];
            until (r1->ingraph);
        }
        /*
         * otherwise, connect new room to the graph, and draw a tunnel
         * to it
         */
        else
        {
            r2->ingraph = TRUE;
            i = (int)(r1 - rdes);
            j = (int)(r2 - rdes);
            conn(rs,i, j);
            r1->isconn[j] = TRUE;
            r2->isconn[i] = TRUE;
            roomcount++;
        }
    } while (roomcount < MAXROOMS);
    
    /*
     * attempt to add passages to the graph a random number of times so
     * that there isn't always just one unique passage through it.
     */
    for (roomcount = rnd(5); roomcount > 0; roomcount--)
    {
        r1 = &rdes[rnd(MAXROOMS)];	/* a random room to look from */
        /*
         * find an adjacent room not already connected
         */
        j = 0;
        for (i = 0; i < MAXROOMS; i++)
            if (r1->conn[i] && !r1->isconn[i] && rnd(++j) == 0)
                r2 = &rdes[i];
        /*
         * if there is one, connect it and look for the next added
         * passage
         */
        if (j != 0)
        {
            i = (int)(r1 - rdes);
            j = (int)(r2 - rdes);
            conn(rs,i, j);
            r1->isconn[j] = TRUE;
            r2->isconn[i] = TRUE;
        }
    }
    passnum();
}

/*
 * conn:
 *	Draw a corridor from a room in a certain direction.
 */

void
conn(struct rogue_state *rs,int r1, int r2)
{
    struct room *rpf, *rpt = NULL;
    int rmt;
    int distance = 0, turn_spot, turn_distance = 0;
    int rm;
    char direc;
    static coord del, curr, turn_delta, spos, epos;
    
    if (r1 < r2)
    {
        rm = r1;
        if (r1 + 1 == r2)
            direc = 'r';
        else
            direc = 'd';
    }
    else
    {
        rm = r2;
        if (r2 + 1 == r1)
            direc = 'r';
        else
            direc = 'd';
    }
    rpf = &rooms[rm];
    /*
     * Set up the movement variables, in two cases:
     * first drawing one down.
     */
    if (direc == 'd')
    {
        rmt = rm + 3;				/* room # of dest */
        rpt = &rooms[rmt];			/* room pointer of dest */
        del.x = 0;				/* direction of move */
        del.y = 1;
        spos.x = rpf->r_pos.x;			/* start of move */
        spos.y = rpf->r_pos.y;
        epos.x = rpt->r_pos.x;			/* end of move */
        epos.y = rpt->r_pos.y;
        if (!(rpf->r_flags & ISGONE))		/* if not gone pick door pos */
            do
            {
                spos.x = rpf->r_pos.x + rnd(rpf->r_max.x - 2) + 1;
                spos.y = rpf->r_pos.y + rpf->r_max.y - 1;
            } while ((rpf->r_flags&ISMAZE) && !(flat(spos.y, spos.x)&F_PASS));
        if (!(rpt->r_flags & ISGONE))
            do
            {
                epos.x = rpt->r_pos.x + rnd(rpt->r_max.x - 2) + 1;
            } while ((rpt->r_flags&ISMAZE) && !(flat(epos.y, epos.x)&F_PASS));
        distance = abs(spos.y - epos.y) - 1;	/* distance to move */
        turn_delta.y = 0;			/* direction to turn */
        turn_delta.x = (spos.x < epos.x ? 1 : -1);
        turn_distance = abs(spos.x - epos.x);	/* how far to turn */
    }
    else if (direc == 'r')			/* setup for moving right */
    {
        rmt = rm + 1;
        rpt = &rooms[rmt];
        del.x = 1;
        del.y = 0;
        spos.x = rpf->r_pos.x;
        spos.y = rpf->r_pos.y;
        epos.x = rpt->r_pos.x;
        epos.y = rpt->r_pos.y;
        if (!(rpf->r_flags & ISGONE))
            do
            {
                spos.x = rpf->r_pos.x + rpf->r_max.x - 1;
                spos.y = rpf->r_pos.y + rnd(rpf->r_max.y - 2) + 1;
            } while ((rpf->r_flags&ISMAZE) && !(flat(spos.y, spos.x)&F_PASS));
        if (!(rpt->r_flags & ISGONE))
            do
            {
                epos.y = rpt->r_pos.y + rnd(rpt->r_max.y - 2) + 1;
            } while ((rpt->r_flags&ISMAZE) && !(flat(epos.y, epos.x)&F_PASS));
        distance = abs(spos.x - epos.x) - 1;
        turn_delta.y = (spos.y < epos.y ? 1 : -1);
        turn_delta.x = 0;
        turn_distance = abs(spos.y - epos.y);
    }
#ifdef MASTER
    else
        debug("error in connection tables");
#endif
    
    turn_spot = rnd(distance - 1) + 1;		/* where turn starts */
    
    /*
     * Draw in the doors on either side of the passage or just put #'s
     * if the rooms are gone.
     */
    if (!(rpf->r_flags & ISGONE))
        door(rpf, &spos);
    else
        putpass(&spos);
    if (!(rpt->r_flags & ISGONE))
        door(rpt, &epos);
    else
        putpass(&epos);
    /*
     * Get ready to move...
     */
    curr.x = spos.x;
    curr.y = spos.y;
    while (distance > 0)
    {
        /*
         * Move to new position
         */
        curr.x += del.x;
        curr.y += del.y;
        /*
         * Check if we are at the turn place, if so do the turn
         */
        if (distance == turn_spot)
            while (turn_distance--)
            {
                putpass(&curr);
                curr.x += turn_delta.x;
                curr.y += turn_delta.y;
            }
        /*
         * Continue digging along
         */
        putpass(&curr);
        distance--;
    }
    curr.x += del.x;
    curr.y += del.y;
    if (!ce(curr, epos))
        msg(rs,"warning, connectivity problem on this level");
}

/*
 * putpass:
 *	add a passage character or secret passage here
 */

void
putpass(coord *cp)
{
    PLACE *pp;
    
    pp = INDEX(cp->y, cp->x);
    pp->p_flags |= F_PASS;
    if (rnd(10) + 1 < level && rnd(40) == 0)
        pp->p_flags &= ~F_REAL;
    else
        pp->p_ch = PASSAGE;
}

/*
 * door:
 *	Add a door or possibly a secret door.  Also enters the door in
 *	the exits array of the room.
 */

void
door(struct room *rm, coord *cp)
{
    PLACE *pp;
    
    rm->r_exit[rm->r_nexits++] = *cp;
    
    if (rm->r_flags & ISMAZE)
        return;
    
    pp = INDEX(cp->y, cp->x);
    if (rnd(10) + 1 < level && rnd(5) == 0)
    {
        if (cp->y == rm->r_pos.y || cp->y == rm->r_pos.y + rm->r_max.y - 1)
            pp->p_ch = '-';
        else
            pp->p_ch = '|';
        pp->p_flags &= ~F_REAL;
    }
    else
        pp->p_ch = DOOR;
}

#ifdef MASTER
/*
 * add_pass:
 *	Add the passages to the current window (wizard command)
 */

void
add_pass()
{
    PLACE *pp;
    int y, x;
    char ch;
    
    for (y = 1; y < NUMLINES - 1; y++)
        for (x = 0; x < NUMCOLS; x++)
        {
            pp = INDEX(y, x);
            if ((pp->p_flags & F_PASS) || pp->p_ch == DOOR ||
                (!(pp->p_flags&F_REAL) && (pp->p_ch == '|' || pp->p_ch == '-')))
            {
                ch = pp->p_ch;
                if (pp->p_flags & F_PASS)
                    ch = PASSAGE;
                pp->p_flags |= F_SEEN;
                move(y, x);
                if (pp->p_monst != NULL)
                    pp->p_monst->t_oldch = pp->p_ch;
                else if (pp->p_flags & F_REAL)
                    addch(ch);
                else
                {
                    standout();
                    addch((pp->p_flags & F_PASS) ? PASSAGE : DOOR);
                    standend();
                }
            }
        }
}
#endif

/*
 * passnum:
 *	Assign a number to each passageway
 */
static int pnum;
static bool newpnum;


void
passnum()
{
    struct room *rp;
    int i;
    
    pnum = 0;
    newpnum = FALSE;
    for (rp = passages; rp < &passages[MAXPASS]; rp++)
        rp->r_nexits = 0;
    for (rp = rooms; rp < &rooms[MAXROOMS]; rp++)
        for (i = 0; i < rp->r_nexits; i++)
        {
            newpnum ^= 1;//newpnum++;
            numpass(rp->r_exit[i].y, rp->r_exit[i].x);
        }
}

/*
 * numpass:
 *	Number a passageway square and its brethren
 */

void
numpass(int y, int x)
{
    char *fp;
    struct room *rp;
    char ch;
    
    if (x >= NUMCOLS || x < 0 || y >= NUMLINES || y <= 0)
        return;
    fp = &flat(y, x);
    if (*fp & F_PNUM)
        return;
    if (newpnum)
    {
        pnum++;
        newpnum = FALSE;
    }
    /*
     * check to see if it is a door or secret door, i.e., a new exit,
     * or a numerable type of place
     */
    if ((ch = chat(y, x)) == DOOR ||
        (!(*fp & F_REAL) && (ch == '|' || ch == '-')))
    {
        rp = &passages[pnum];
        rp->r_exit[rp->r_nexits].y = y;
        rp->r_exit[rp->r_nexits++].x = x;
    }
    else if (!(*fp & F_PASS))
        return;
    *fp |= pnum;
    /*
     * recurse on the surrounding places
     */
    numpass(y + 1, x);
    numpass(y - 1, x);
    numpass(y, x + 1);
    numpass(y, x - 1);
}
