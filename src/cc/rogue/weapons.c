/*
 * Functions for dealing with problems brought about by weapons
 *
 * @(#)weapons.c	4.34 (Berkeley) 02/05/99
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

#define NO_WEAPON -1

int group = 2;

static struct init_weaps {
    char *iw_dam;	/* Damage when wielded */
    char *iw_hrl;	/* Damage when thrown */
    char iw_launch;	/* Launching weapon */
    int iw_flags;	/* Miscellaneous flags */
} init_dam[MAXWEAPONS] = {
    { "2x4",	"1x3",	NO_WEAPON,	0,		},	/* Mace */
    { "3x4",	"1x2",	NO_WEAPON,	0,		},	/* Long sword */
    { "1x1",	"1x1",	NO_WEAPON,	0,		},	/* Bow */
    { "1x1",	"2x3",	BOW,		ISMANY|ISMISL,	},	/* Arrow */
    { "1x6",	"1x4",	NO_WEAPON,	ISMISL|ISMISL,	},	/* Dagger */
    { "4x4",	"1x2",	NO_WEAPON,	0,		},	/* 2h sword */
    { "1x1",	"1x3",	NO_WEAPON,	ISMANY|ISMISL,	},	/* Dart */
    { "1x2",	"2x4",	NO_WEAPON,	ISMANY|ISMISL,	},	/* Shuriken */
    { "2x3",	"1x6",	NO_WEAPON,	ISMISL,		},	/* Spear */
};

/*
 * missile:
 *	Fire a missile in a given direction
 */

void
missile(struct rogue_state *rs,int ydelta, int xdelta)
{
    THING *obj;

    /*
     * Get which thing we are hurling
     */
    if ((obj = get_item(rs,"throw", WEAPON)) == NULL)
	return;
    if (!dropcheck(rs,obj) || is_current(rs,obj))
	return;
    obj = leave_pack(rs,obj, TRUE, FALSE);
    do_motion(rs,obj, ydelta, xdelta);
    /*
     * AHA! Here it has hit something.  If it is a wall or a door,
     * or if it misses (combat) the monster, put it on the floor
     */
    if (moat(obj->o_pos.y, obj->o_pos.x) == NULL ||
	!hit_monster(rs,unc(obj->o_pos), obj))
	    fall(rs,obj, TRUE);
}

/*
 * do_motion:
 *	Do the actual motion on the screen done by an object traveling
 *	across the room
 */

void
do_motion(struct rogue_state *rs,THING *obj, int ydelta, int xdelta)
{
    int ch;

    /*
     * Come fly with us ...
     */
    obj->o_pos = hero;
    for (;;)
    {
        if ( rs->replaydone != 0 )
            return;
	/*
	 * Erase the old one
	 */
	if (!ce(obj->o_pos, hero) && cansee(rs,unc(obj->o_pos)) && !terse)
	{
	    ch = chat(obj->o_pos.y, obj->o_pos.x);
	    if (ch == FLOOR && !show_floor())
		ch = ' ';
	    mvaddch(obj->o_pos.y, obj->o_pos.x, ch);
	}
	/*
	 * Get the new position
	 */
	obj->o_pos.y += ydelta;
	obj->o_pos.x += xdelta;
	if (step_ok(ch = winat(obj->o_pos.y, obj->o_pos.x)) && ch != DOOR)
	{
	    /*
	     * It hasn't hit anything yet, so display it
	     * If it alright.
	     */
	    if (cansee(rs,unc(obj->o_pos)) && !terse)
	    {
		mvaddch(obj->o_pos.y, obj->o_pos.x, obj->o_type);
            if ( rs->sleeptime != 0 )
                refresh();
	    }
	    continue;
	}
	break;
    }
}

/*
 * fall:
 *	Drop an item someplace around here.
 */

void
fall(struct rogue_state *rs,THING *obj, bool pr)
{
    PLACE *pp;
    static coord fpos;

    if (fallpos(&obj->o_pos, &fpos))
    {
        pp = INDEX(fpos.y, fpos.x);
        pp->p_ch = (char) obj->o_type;
        obj->o_pos = fpos;
        if (cansee(rs,fpos.y, fpos.x))
        {
            if (pp->p_monst != NULL)
                pp->p_monst->t_oldch = (char) obj->o_type;
            else
                mvaddch(fpos.y, fpos.x, obj->o_type);
        }
        attach(lvl_obj, obj);
        return;
    }
    if (pr)
    {
        if (has_hit)
        {
            endmsg(rs);
            has_hit = FALSE;
        }
        msg(rs,"the %s vanishes as it hits the ground",
            weap_info[obj->o_which].oi_name);
    }
    discard(obj);
}

/*
 * init_weapon:
 *	Set up the initial goodies for a weapon
 */

void
init_weapon(THING *weap, int which)
{
    struct init_weaps *iwp;
    weap->o_type = WEAPON;
    weap->o_which = which;
    iwp = &init_dam[which];
    strncpy(weap->o_damage, iwp->iw_dam, sizeof(weap->o_damage));
    strncpy(weap->o_hurldmg,iwp->iw_hrl, sizeof(weap->o_hurldmg));
    weap->o_launch = iwp->iw_launch;
    weap->o_flags = iwp->iw_flags;
    weap->o_hplus = 0;
    weap->o_dplus = 0;
    if (which == DAGGER)
    {
	weap->o_count = rnd(4) + 2;
	weap->o_group = group++;
    }
    else if (weap->o_flags & ISMANY)
    {
	weap->o_count = rnd(8) + 8;
	weap->o_group = group++;
    }
    else
    {
	weap->o_count = 1;
	weap->o_group = 0;
    }
}

/*
 * hit_monster:
 *	Does the missile hit the monster?
 */
int
hit_monster(struct rogue_state *rs,int y, int x, THING *obj)
{
    static coord mp;

    mp.y = y;
    mp.x = x;
    return fight(rs,&mp, obj, TRUE);
}

/*
 * num:
 *	Figure out the plus number for armor/weapons
 */
char *
num(int n1, int n2, char type)
{
    static char numbuf[10];

    sprintf(numbuf, n1 < 0 ? "%d" : "+%d", n1);
    if (type == WEAPON)
	sprintf(&numbuf[strlen(numbuf)], n2 < 0 ? ",%d" : ",+%d", n2);
    return numbuf;
}

/*
 * wield:
 *	Pull out a certain weapon
 */

void
wield(struct rogue_state *rs)
{
    THING *obj, *oweapon;
    char *sp;

    oweapon = cur_weapon;
    if (!dropcheck(rs,cur_weapon))
    {
	cur_weapon = oweapon;
	return;
    }
    cur_weapon = oweapon;
    if ((obj = get_item(rs,"wield", WEAPON)) == NULL)
    {
bad:
	after = FALSE;
	return;
    }

    if (obj->o_type == ARMOR)
    {
	msg(rs,"you can't wield armor");
	goto bad;
    }
    if (is_current(rs,obj))
        goto bad;

    sp = inv_name(obj, TRUE);
    cur_weapon = obj;
    if (!terse)
	addmsg(rs,"you are now ");
    msg(rs,"wielding %s (%c)", sp, obj->o_packch);
}

/*
 * fallpos:
 *	Pick a random position around the give (y, x) coordinates
 */
bool
fallpos(coord *pos, coord *newpos)
{
    int y, x, cnt, ch;

    cnt = 0;
    for (y = pos->y - 1; y <= pos->y + 1; y++)
	for (x = pos->x - 1; x <= pos->x + 1; x++)
	{
	    /*
	     * check to make certain the spot is empty, if it is,
	     * put the object there, set it in the level list
	     * and re-draw the room if he can see it
	     */
	    if (y == hero.y && x == hero.x)
		continue;
	    if (((ch = chat(y, x)) == FLOOR || ch == PASSAGE)
					&& rnd(++cnt) == 0)
	    {
		newpos->y = y;
		newpos->x = x;
	    }
	}
    return (bool)(cnt != 0);
}
