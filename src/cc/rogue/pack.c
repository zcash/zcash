/*
 * Routines to deal with the pack
 *
 * @(#)pack.c	4.40 (Berkeley) 02/05/99
 *
 * Rogue: Exploring the Dungeons of Doom
 * Copyright (C) 1980-1983, 1985, 1999 Michael Toy, Ken Arnold and Glenn Wichman
 * All rights reserved.
 *
 * See the file LICENSE.TXT for full copyright and licensing information.
 */

//#include <string.h>
//#include <curses.h>
//#include <ctype.h>
#include "rogue.h"

/*
 * add_pack:
 *	Pick up an object and add it to the pack.  If the argument is
 *	non-null use it as the linked_list pointer instead of gettting
 *	it off the ground.
 */

void
add_pack(struct rogue_state *rs,THING *obj, bool silent)
{
    THING *op, *lp;
    bool from_floor;

    from_floor = FALSE;
    if (obj == NULL)
    {
	if ((obj = find_obj(rs,hero.y, hero.x)) == NULL)
	    return;
	from_floor = TRUE;
    }

    /*
     * Check for and deal with scare monster scrolls
     */
    if (obj->o_type == SCROLL && obj->o_which == S_SCARE)
	if (obj->o_flags & ISFOUND)
	{
	    detach(lvl_obj, obj);
	    mvaddch(hero.y, hero.x, floor_ch());
	    chat(hero.y, hero.x) = (proom->r_flags & ISGONE) ? PASSAGE : FLOOR;
	    discard(obj);
	    msg(rs,"the scroll turns to dust as you pick it up");
	    return;
	}

    if (pack == NULL)
    {
        pack = obj;
        obj->o_packch = pack_char();
        inpack++;
    }
    else
    {
	lp = NULL;
	for (op = pack; op != NULL; op = next(op))
	{
	    if (op->o_type != obj->o_type)
		lp = op;
	    else
	    {
		while (op->o_type == obj->o_type && op->o_which != obj->o_which)
		{
		    lp = op;
		    if (next(op) == NULL)
			break;
		    else
			op = next(op);
		}
		if (op->o_type == obj->o_type && op->o_which == obj->o_which)
		{
		    if (ISMULT(op->o_type))
		    {
			if (!pack_room(rs,from_floor, obj))
			    return;
			op->o_count++;
dump_it:
			discard(obj);
			obj = op;
			lp = NULL;
			goto out;
		    }
		    else if (obj->o_group)
		    {
			lp = op;
			while (op->o_type == obj->o_type
			    && op->o_which == obj->o_which
			    && op->o_group != obj->o_group)
			{
			    lp = op;
			    if (next(op) == NULL)
				break;
			    else
				op = next(op);
			}
			if (op->o_type == obj->o_type
			    && op->o_which == obj->o_which
			    && op->o_group == obj->o_group)
			{
				op->o_count += obj->o_count;
				inpack--;
				if (!pack_room(rs,from_floor, obj))
				    return;
				goto dump_it;
			}
		    }
		    else
			lp = op;
		}
out:
		break;
	    }
	}

	if (lp != NULL)
	{
	    if (!pack_room(rs,from_floor, obj))
		return;
	    else
	    {
		obj->o_packch = pack_char();
		next(obj) = next(lp);
		prev(obj) = lp;
		if (next(lp) != NULL)
		    prev(next(lp)) = obj;
		next(lp) = obj;
	    }
	}
    }

    obj->o_flags |= ISFOUND;

    /*
     * If this was the object of something's desire, that monster will
     * get mad and run at the hero.
     */
    for (op = mlist; op != NULL; op = next(op))
	if (op->t_dest == &obj->o_pos)
	    op->t_dest = &hero;

    if (obj->o_type == AMULET)
	amulet = TRUE;
    /*
     * Notify the user
     */
    if (!silent)
    {
	if (!terse)
	    addmsg(rs,"you now have ");
	msg(rs,"%s (%c)", inv_name(obj, !terse), obj->o_packch);
    }
}

int32_t num_packitems(struct rogue_state *rs)
{
    THING *list = pack;
    int32_t type = 0,n = 0,total = 0;
    for (; list != NULL; list = next(list))
    {
        if ( thing_find(list) < 0 )
        {
            fprintf(stderr,"num_packitems cant find %p\n",list);
            return(-1);
        }
        if ( list->o_packch != 0 )
        {
            n++;
            total += rogue_total(list);
        }
    }
    if ( rs->guiflag != 0 ) 
    {
        char str[MAXSTR];
        sprintf(str,"strength*2 %d vs total.%d vs %d inventory letters\n",ROGUE_MAXTOTAL,total,n);
        add_line(rs,"%s",str);
    }
    if ( total > ROGUE_MAXTOTAL )
        return(MAXPACK);
    return(n);
}

/*
 * pack_room:
 *	See if there's room in the pack.  If not, print out an
 *	appropriate message
 */
bool pack_room(struct rogue_state *rs,bool from_floor, THING *obj)
{
    inpack = num_packitems(rs);
    if ( ++inpack > MAXPACK )
    {
        if (!terse)
            addmsg(rs,"there's ");
        addmsg(rs,"no room");
        if (!terse)
            addmsg(rs," in your pack");
        endmsg(rs);
        if (from_floor)
            move_msg(rs,obj);
        inpack = MAXPACK;
        return FALSE;
    }
    //fprintf(stderr,"inpack.%d vs MAX.%d\n",inpack,MAXPACK), sleep(2);
    if ( from_floor != 0 )
    {
        detach(lvl_obj, obj);
        mvaddch(hero.y, hero.x, floor_ch());
        chat(hero.y, hero.x) = (proom->r_flags & ISGONE) ? PASSAGE : FLOOR;
    }
    return TRUE;
}

/*
 * leave_pack:
 *	take an item out of the pack
 */
THING *
leave_pack(struct rogue_state *rs,THING *obj, bool newobj, bool all)
{
    THING *nobj;
    
    inpack--;
    nobj = obj;
    if (obj->o_count > 1 && !all)
    {
        last_pick = obj;
        obj->o_count--;
        if (obj->o_group)
            inpack++;
        if (newobj)
        {
            nobj = new_item();
            *nobj = *obj;
            next(nobj) = NULL;
            prev(nobj) = NULL;
            nobj->o_count = 1;
        }
    }
    else
    {
        last_pick = NULL;
        pack_used[obj->o_packch - 'a'] = FALSE;
        detach(pack, obj);
    }
    return nobj;
}

/*
 * pack_char:
 *	Return the next unused pack character.
 */
char
pack_char()
{
    bool *bp;

    for (bp = pack_used; *bp; bp++)
	continue;
    *bp = TRUE;
    return (char)((int)(bp - pack_used) + 'a');
}

/*
 * inventory:
 *	List what is in the pack.  Return TRUE if there is something of
 *	the given type.
 */


bool
inventory(struct rogue_state *rs,THING *list, int type)
{
    char inv_temp[MAXSTR];

    n_objs = 0;
    for (; list != NULL; list = next(list))
    {
        if (type && type != list->o_type && !(type == CALLABLE &&
                                              list->o_type != FOOD && list->o_type != AMULET) &&
            !(type == R_OR_S && (list->o_type == RING || list->o_type == STICK)))
            continue;
        n_objs++;
#ifdef MASTER
        if (!list->o_packch)
            strcpy(inv_temp, "%s");
        else
#endif
            sprintf(inv_temp, "%c) %%s", list->o_packch);
        msg_esc = TRUE;
        if (add_line(rs,inv_temp, inv_name(list, FALSE)) == ESCAPE)
        {
            msg_esc = FALSE;
            msg(rs,"");
            return TRUE;
        }
        msg_esc = FALSE;
    }
    //if ( n_objs != inpack )
    //    fprintf(stderr,"n_objs.%d vs inpack.%d\n",n_objs,inpack), sleep(2);
    if (n_objs == 0)
    {
        if (terse)
            msg(rs,type == 0 ? (char *)"empty handed" : (char *)"nothing appropriate");
        else
            msg(rs,type == 0 ? (char *)"you are empty handed" : (char *)"you don't have anything appropriate");
        return FALSE;
    }
    end_line(rs);
    return TRUE;
}

/*
 * pick_up:
 *	Add something to characters pack.
 */

void
pick_up(struct rogue_state *rs,char ch)
{
    THING *obj;

    if (on(player, ISLEVIT))
	return;

    obj = find_obj(rs,hero.y, hero.x);
    if (move_on)
	move_msg(rs,obj);
    else
	switch (ch)
	{
	    case GOLD:
		if (obj == NULL)
		    return;
		money(rs,obj->o_goldval);
		detach(lvl_obj, obj);
		discard(obj);
		proom->r_goldval = 0;
		break;
	    default:
#ifdef MASTER
		debug("Where did you pick a '%s' up???", unctrl(ch));
#endif
	    case ARMOR:
	    case POTION:
	    case FOOD:
	    case WEAPON:
	    case SCROLL:	
	    case AMULET:
	    case RING:
	    case STICK:
		add_pack(rs,(THING *) NULL, FALSE);
		break;
	}
}

/*
 * move_msg:
 *	Print out the message if you are just moving onto an object
 */

void
move_msg(struct rogue_state *rs,THING *obj)
{
    if (!terse)
	addmsg(rs,"you ");
    msg(rs,"moved onto %s", inv_name(obj, TRUE));
}

/*
 * picky_inven:
 *	Allow player to inventory a single item
 */

void
picky_inven(struct rogue_state *rs)
{
    THING *obj;
    char mch;

    if (pack == NULL)
	msg(rs,"you aren't carrying anything");
    else if (next(pack) == NULL)
	msg(rs,"a) %s", inv_name(pack, FALSE));
    else
    {
	msg(rs,terse ? (char *)"item: " : (char *)"which item do you wish to inventory: ");
	mpos = 0;
	if ((mch = readchar(rs)) == ESCAPE)
	{
	    msg(rs,"");
	    return;
	}
	for (obj = pack; obj != NULL; obj = next(obj))
	    if (mch == obj->o_packch)
	    {
		msg(rs,"%c) %s", mch, inv_name(obj, FALSE));
		return;
	    }
	msg(rs,"'%s' not in pack", unctrl(mch));
    }
}

/*
 * get_item:
 *	Pick something out of a pack for a purpose
 */
THING *
get_item(struct rogue_state *rs,char *purpose, int type)
{
    THING *obj;
    char ch;
    
    if (pack == NULL)
        msg(rs,"you aren't carrying anything");
    else if (again)
        if (last_pick)
            return last_pick;
        else
            msg(rs,"you ran out");
        else
        {
            for (;;)
            {
                if ( rs->replaydone != 0 )
                    return(NULL);
                if (!terse)
                    addmsg(rs,"which object do you want to ");
                addmsg(rs,purpose);
                if (terse)
                    addmsg(rs," what");
                msg(rs,"? (* for list): ");
                ch = readchar(rs);
                mpos = 0;
                /*
                 * Give the poor player a chance to abort the command
                 */
                if (ch == ESCAPE)
                {
                    reset_last();
                    after = FALSE;
                    msg(rs,"");
                    return NULL;
                }
                n_objs = 1;		/* normal case: person types one char */
                if (ch == '*')
                {
                    mpos = 0;
                    if (inventory(rs,pack, type) == 0)
                    {
                        after = FALSE;
                        return NULL;
                    }
                    continue;
                }
                for (obj = pack; obj != NULL; obj = next(obj))
                    if (obj->o_packch == ch)
                        break;
                if (obj == NULL)
                {
                    //msg(rs,"'%s' is not a valid item",unctrl(ch));
                    //continue;
                    reset_last();
                    after = FALSE;
                    msg(rs,"'%s' is not a valid item",unctrl(ch));
                    return NULL;
                }
                else
                {
                    return obj;
                }
            }
        }
    return NULL;
}

/*
 * money:
 *	Add or subtract gold from the pack
 */

void
money(struct rogue_state *rs,int value)
{
    purse += value;
    mvaddch(hero.y, hero.x, floor_ch());
    chat(hero.y, hero.x) = (proom->r_flags & ISGONE) ? PASSAGE : FLOOR;
    if (value > 0)
    {
	if (!terse)
	    addmsg(rs,"you found ");
	msg(rs,"%d gold pieces", value);
    }
}

/*
 * floor_ch:
 *	Return the appropriate floor character for her room
 */
char
floor_ch()
{
    if (proom->r_flags & ISGONE)
	return PASSAGE;
    return (show_floor() ? FLOOR : ' ');
}

/*
 * floor_at:
 *	Return the character at hero's position, taking see_floor
 *	into account
 */
char
floor_at()
{
    char ch;

    ch = chat(hero.y, hero.x);
    if (ch == FLOOR)
	ch = floor_ch();
    return ch;
}

/*
 * reset_last:
 *	Reset the last command when the current one is aborted
 */

void
reset_last()
{
    last_comm = l_last_comm;
    last_dir = l_last_dir;
    last_pick = l_last_pick;
}
