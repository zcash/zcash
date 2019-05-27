/*
 * All sorts of miscellaneous routines
 *
 * @(#)misc.c	4.66 (Berkeley) 08/06/83
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
 * look:
 *	A quick glance all around the player
 */
#undef DEBUG


void
look(struct rogue_state *rs,bool wakeup)
{
    int x, y;
    int ch;
    THING *tp;
    PLACE *pp;
    struct room *rp;
    int ey, ex;
    int passcount;
    char pfl, *fp, pch;
    int sy, sx, sumhero = 0, diffhero = 0;
# ifdef DEBUG
    static bool done = FALSE;

    if (done)
	return;
    done = TRUE;
# endif /* DEBUG */
    passcount = 0;
    rp = proom;
    if (!ce(oldpos, hero))
    {
	erase_lamp(&oldpos, oldrp);
	oldpos = hero;
	oldrp = rp;
    }
    ey = hero.y + 1;
    ex = hero.x + 1;
    sx = hero.x - 1;
    sy = hero.y - 1;
    if (door_stop && !firstmove && running)
    {
	sumhero = hero.y + hero.x;
	diffhero = hero.y - hero.x;
    }
    pp = INDEX(hero.y, hero.x);
    pch = pp->p_ch;
    pfl = pp->p_flags;

    for (y = sy; y <= ey; y++)
	if (y > 0 && y < NUMLINES - 1) for (x = sx; x <= ex; x++)
	{
	    if (x < 0 || x >= NUMCOLS)
		continue;
	    if (!on(player, ISBLIND))
	    {
		if (y == hero.y && x == hero.x)
		    continue;
	    }

	    pp = INDEX(y, x);
	    ch = pp->p_ch;
	    if (ch == ' ')		/* nothing need be done with a ' ' */
		    continue;
	    fp = &pp->p_flags;
	    if (pch != DOOR && ch != DOOR)
		if ((pfl & F_PASS) != (*fp & F_PASS))
		    continue;
	    if (((*fp & F_PASS) || ch == DOOR) && 
		 ((pfl & F_PASS) || pch == DOOR))
	    {
		if (hero.x != x && hero.y != y &&
		    !step_ok(chat(y, hero.x)) && !step_ok(chat(hero.y, x)))
			continue;
	    }

	    if ((tp = pp->p_monst) == NULL)
		ch = trip_ch(y, x, ch);
	    else
		if (on(player, SEEMONST) && on(*tp, ISINVIS))
		{
		    if (door_stop && !firstmove)
			running = FALSE;
		    continue;
		}
		else
		{
		    if (wakeup)
			wake_monster(rs,y, x);
		    if (see_monst(tp))
		    {
			if (on(player, ISHALU))
			    ch = rnd(26) + 'A';
			else
			    ch = tp->t_disguise;
		    }
		}
	    if (on(player, ISBLIND) && (y != hero.y || x != hero.x))
		continue;

	    move(y, x);

	    if ((proom->r_flags & ISDARK) && !see_floor && ch == FLOOR)
		ch = ' ';

	    if (tp != NULL || ch != CCHAR( inch() ))
		addch(ch);

	    if (door_stop && !firstmove && running)
	    {
		switch (runch)
		{
		    case 'h':
			if (x == ex)
			    continue;
		    when 'j':
			if (y == sy)
			    continue;
		    when 'k':
			if (y == ey)
			    continue;
		    when 'l':
			if (x == sx)
			    continue;
		    when 'y':
			if ((y + x) - sumhero >= 1)
			    continue;
		    when 'u':
			if ((y - x) - diffhero >= 1)
			    continue;
		    when 'n':
			if ((y + x) - sumhero <= -1)
			    continue;
		    when 'b':
			if ((y - x) - diffhero <= -1)
			    continue;
		}
		switch (ch)
		{
		    case DOOR:
			if (x == hero.x || y == hero.y)
			    running = FALSE;
			break;
		    case PASSAGE:
			if (x == hero.x || y == hero.y)
			    passcount++;
			break;
		    case FLOOR:
		    case '|':
		    case '-':
		    case ' ':
			break;
		    default:
			running = FALSE;
			break;
		}
	    }
	}
    if (door_stop && !firstmove && passcount > 1)
	running = FALSE;
    if (!running || !jump)
	mvaddch(hero.y, hero.x, PLAYER);
# ifdef DEBUG
    done = FALSE;
# endif /* DEBUG */
}

/*
 * trip_ch:
 *	Return the character appropriate for this space, taking into
 *	account whether or not the player is tripping.
 */
int
trip_ch(int y, int x, int ch)
{
    if (on(player, ISHALU) && after)
	switch (ch)
	{
	    case FLOOR:
	    case ' ':
	    case PASSAGE:
	    case '-':
	    case '|':
	    case DOOR:
	    case TRAP:
		break;
	    default:
		if (y != stairs.y || x != stairs.x || !seenstairs)
		    ch = rnd_thing();
		break;
	}
    return ch;
}

/*
 * erase_lamp:
 *	Erase the area shown by a lamp in a dark room.
 */

void
erase_lamp(coord *pos, struct room *rp)
{
    int y, x, ey, sy, ex;

    if (!(see_floor && (rp->r_flags & (ISGONE|ISDARK)) == ISDARK
	&& !on(player,ISBLIND)))
	    return;

    ey = pos->y + 1;
    ex = pos->x + 1;
    sy = pos->y - 1;
    for (x = pos->x - 1; x <= ex; x++)
	for (y = sy; y <= ey; y++)
	{
	    if (y == hero.y && x == hero.x)
		continue;
	    move(y, x);
	    if (inch() == FLOOR)
		addch(' ');
	}
}

/*
 * show_floor:
 *	Should we show the floor in her room at this time?
 */
bool
show_floor()
{
    if ((proom->r_flags & (ISGONE|ISDARK)) == ISDARK && !on(player, ISBLIND))
	return see_floor;
    else
	return TRUE;
}

/*
 * find_obj:
 *	Find the unclaimed object at y, x
 */
THING *
find_obj(struct rogue_state *rs,int y, int x)
{
    THING *obj;

    for (obj = lvl_obj; obj != NULL; obj = next(obj))
    {
	if (obj->o_pos.y == y && obj->o_pos.x == x)
		return obj;
    }
#ifdef MASTER
    sprintf(prbuf, "Non-object %d,%d", y, x);
    msg(rs,prbuf);
    return NULL;
#else
    /* NOTREACHED */
    return NULL;
#endif
}

/*
 * eat:
 *	She wants to eat something, so let her try
 */

void
eat(struct rogue_state *rs)
{
    THING *obj;

    if ((obj = get_item(rs,"eat", FOOD)) == NULL)
	return;
    if (obj->o_type != FOOD)
    {
	if (!terse)
	    msg(rs,"ugh, you would get ill if you ate that");
	else
	    msg(rs,"that's Inedible!");
	return;
    }
    if (food_left < 0)
	food_left = 0;
    if ((food_left += HUNGERTIME - 200 + rnd(400)) > STOMACHSIZE)
	food_left = STOMACHSIZE;
    hungry_state = 0;
    if (obj == cur_weapon)
	cur_weapon = NULL;
    if (obj->o_which == 1)
	msg(rs,"my, that was a yummy %s", fruit);
    else
	if (rnd(100) > 70)
	{
	    pstats.s_exp++;
	    msg(rs,"%s, this food tastes awful", choose_str("bummer", "yuk"));
	    check_level(rs);
	}
	else
	    msg(rs,"%s, that tasted good", choose_str("oh, wow", "yum"));
    leave_pack(rs,obj, FALSE, FALSE);
}

/*
 * check_level:
 *	Check to see if the guy has gone up a level.
 */

void
check_level(struct rogue_state *rs)
{
    int i, add, olevel;

    for (i = 0; e_levels[i] != 0; i++)
	if (e_levels[i] > pstats.s_exp)
	    break;
    i++;
    olevel = pstats.s_lvl;
    pstats.s_lvl = i;
    if (i > olevel)
    {
	add = roll(i - olevel, 10);
	max_hp += add;
	pstats.s_hpt += add;
	msg(rs,"welcome to level %d", i);
    }
}

/*
 * chg_str:
 *	used to modify the playes strength.  It keeps track of the
 *	highest it has been, just in case
 */

void
chg_str(int amt)
{
    //auto jl777: strange compiler error
    uint32_t comp;
    
    if (amt == 0)
        return;
    //add_str(&pstats.s_str, amt);
    pstats.s_str += amt;
    if ( pstats.s_str < 3 )
        pstats.s_str = 3;
    else if ( pstats.s_str > 31 )
        pstats.s_str = 31;
    comp = pstats.s_str;
    if (ISRING(LEFT, R_ADDSTR))
    {
        // add_str(&comp, -cur_ring[LEFT]->o_arm);
        comp += -cur_ring[LEFT]->o_arm;
        if ( comp < 3 )
            comp = 3;
        else if ( comp > 31 )
            comp = 31;
    }
    if (ISRING(RIGHT, R_ADDSTR))
    {
        //add_str(&comp, -cur_ring[RIGHT]->o_arm);
        comp += -cur_ring[RIGHT]->o_arm;
        if ( comp < 3 )
            comp = 3;
        else if ( comp > 31 )
            comp = 31;
    }
    if ( comp > max_stats.s_str )
        max_stats.s_str = comp;
}

/*
 * add_str:
 *	Perform the actual add, checking upper and lower bound limits
 */
void
add_str(str_t *sp, int amt)
{
    if ((*sp += amt) < 3)
	*sp = 3;
    else if (*sp > 31)
	*sp = 31;
}

/*
 * add_haste:
 *	Add a haste to the player
 */
bool
add_haste(struct rogue_state *rs,bool potion)
{
    if (on(player, ISHASTE))
    {
        no_command += rnd(8);
        player.t_flags &= ~(ISRUN|ISHASTE);
        extinguish(nohaste);
        msg(rs,"you faint from exhaustion");
        return FALSE;
    }
    else
    {
        player.t_flags |= ISHASTE;
        if (potion)
            fuse(nohaste, 0, rnd(4)+4, AFTER);
        return TRUE;
    }
}

/*
 * aggravate:
 *	Aggravate all the monsters on this level
 */

void
aggravate(struct rogue_state *rs)
{
    THING *mp;

    for (mp = mlist; mp != NULL; mp = next(mp))
	runto(rs,&mp->t_pos);
}

/*
 * vowelstr:
 *      For printfs: if string starts with a vowel, return "n" for an
 *	"an".
 */
char *
vowelstr(char *str)
{
    switch (*str)
    {
	case 'a': case 'A':
	case 'e': case 'E':
	case 'i': case 'I':
	case 'o': case 'O':
	case 'u': case 'U':
	    return "n";
	default:
	    return "";
    }
}

/* 
 * is_current:
 *	See if the object is one of the currently used items
 */
bool
is_current(struct rogue_state *rs,THING *obj)
{
    if (obj == NULL)
	return FALSE;
    if (obj == cur_armor || obj == cur_weapon || obj == cur_ring[LEFT]
	|| obj == cur_ring[RIGHT])
    {
	if (!terse)
	    addmsg(rs,"That's already ");
	msg(rs,"in use");
	return TRUE;
    }
    return FALSE;
}

/*
 * get_dir:
 *      Set up the direction co_ordinate for use in varios "prefix"
 *	commands
 */
bool
get_dir(struct rogue_state *rs)
{
    char *prompt;
    bool gotit;
    static coord last_delt= {0,0};

    if (again && last_dir != '\0')
    {
	delta.y = last_delt.y;
	delta.x = last_delt.x;
	dir_ch = last_dir;
    }
    else
    {
	if (!terse)
	    msg(rs,prompt = "which direction? ");
	else
	    prompt = "direction: ";
	do
	{
	    gotit = TRUE;
	    switch (dir_ch = readchar(rs))
	    {
		case 'h': case'H': delta.y =  0; delta.x = -1;
		when 'j': case'J': delta.y =  1; delta.x =  0;
		when 'k': case'K': delta.y = -1; delta.x =  0;
		when 'l': case'L': delta.y =  0; delta.x =  1;
		when 'y': case'Y': delta.y = -1; delta.x = -1;
		when 'u': case'U': delta.y = -1; delta.x =  1;
		when 'b': case'B': delta.y =  1; delta.x = -1;
		when 'n': case'N': delta.y =  1; delta.x =  1;
		when ESCAPE: last_dir = '\0'; reset_last(); return FALSE;
		otherwise:
		    mpos = 0;
		    msg(rs,prompt);
		    gotit = FALSE;
	    }
	} until (gotit);
	if (isupper(dir_ch))
	    dir_ch = (char) tolower(dir_ch);
	last_dir = dir_ch;
	last_delt.y = delta.y;
	last_delt.x = delta.x;
    }
    if (on(player, ISHUH) && rnd(5) == 0)
	do
	{
	    delta.y = rnd(3) - 1;
	    delta.x = rnd(3) - 1;
	} while (delta.y == 0 && delta.x == 0);
    mpos = 0;
    return TRUE;
}

/*
 * sign:
 *	Return the sign of the number
 */
int
sign(int nm)
{
    if (nm < 0)
	return -1;
    else
	return (nm > 0);
}

/*
 * spread:
 *	Give a spread around a given number (+/- 20%)
 */
int
spread(int nm)
{
    return nm - nm / 20 + rnd(nm / 10);
}

/*
 * call_it:
 *	Call an object something after use.
 */

void
call_it(struct rogue_state *rs,struct obj_info *info)
{
    if (info->oi_know)
    {
	if (info->oi_guess)
	{
	    free(info->oi_guess);
	    info->oi_guess = NULL;
	}
    }
    else if (!info->oi_guess)
    {
	msg(rs,terse ? (char *)"call it: " : (char *)"what do you want to call it? ");
	if (get_str(rs,prbuf, stdscr) == NORM)
	{
	    if (info->oi_guess != NULL)
		free(info->oi_guess);
	    info->oi_guess = (char *)malloc((unsigned int) strlen(prbuf) + 1);
	    strcpy(info->oi_guess, prbuf);
	}
    }
}

/*
 * rnd_thing:
 *	Pick a random thing appropriate for this level
 */
char
rnd_thing()
{
    int i;
    static char thing_list[] = {
	POTION, SCROLL, RING, STICK, FOOD, WEAPON, ARMOR, STAIRS, GOLD, AMULET
    };

    if (level >= AMULETLEVEL)
        i = rnd(sizeof thing_list / sizeof (char));
    else
        i = rnd(sizeof thing_list / sizeof (char) - 1);
    return thing_list[i];
}

/*
 str str:
 *	Choose the first or second string depending on whether it the
 *	player is tripping
 */
char *
choose_str(char *ts, char *ns)
{
	return (on(player, ISHALU) ? ts : ns);
}
