/*
 * Read and execute the user commands
 *
 * @(#)command.c	4.73 (Berkeley) 08/06/83
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
 * command:
 *	Process the user commands
 */
void
command(struct rogue_state *rs)
{
    register char ch;
    register int ntimes = 1;			/* Number of player moves */
    char *fp;
    THING *mp;
    static char countch, direction, newcount = FALSE;
    if (on(player, ISHASTE))
        ntimes++;
    /*
     * Let the daemons start up
     */
    do_daemons(rs,BEFORE);
    do_fuses(rs,BEFORE);
    while (ntimes--)
    {
        if ( rs->replaydone != 0 )
            return;
        again = FALSE;
        if (has_hit)
        {
            endmsg(rs);
            has_hit = FALSE;
        }
        /*
         * these are illegal things for the player to be, so if any are
         * set, someone's been poking in memeory
         */
        if (on(player, ISSLOW|ISGREED|ISINVIS|ISREGEN|ISTARGET))
            exit(1);
        
        look(rs,TRUE);
        if (!running)
            door_stop = FALSE;
        status(rs);
        lastscore = purse;
        move(hero.y, hero.x);
        if ( rs->sleeptime != 0 )
        {
            if (!((running || count) && jump))
                refresh();			/* Draw screen */
        }
        take = 0;
        after = TRUE;
        /*
         * Read command or continue run
         */
#ifdef MASTER
        if (wizard)
            noscore = TRUE;
#endif
        if (!no_command)
        {
            if (running || to_death)
                ch = runch;
            else if (count)
                ch = countch;
            else
            {
                ch = readchar(rs);
                move_on = FALSE;
                if (mpos != 0)		/* Erase message if its there */
                    msg(rs,"");
            }
        }
        else
            ch = '.';
        if (no_command)
        {
            if (--no_command == 0)
            {
                player.t_flags |= ISRUN;
                msg(rs,"you can move again");
            }
        }
        else
        {
            /*
             * check for prefixes
             */
            newcount = FALSE;
            if (isdigit(ch))
            {
                count = 0;
                newcount = TRUE;
                while (isdigit(ch))
                {
                    count = count * 10 + (ch - '0');
                    if (count > 255)
                        count = 255;
                    ch = readchar(rs);
                }
                countch = ch;
                /*
                 * turn off count for commands which don't make sense
                 * to repeat
                 */
                if ( rs->guiflag == 0 && rs->replaydone != 0 )
                    ch = 'Q';
		switch (ch)
		{
		    case CTRL('B'): case CTRL('H'): case CTRL('J'):
		    case CTRL('K'): case CTRL('L'): case CTRL('N'):
		    case CTRL('U'): case CTRL('Y'):
		    case '.': case 'a': case 'b': case 'h': case 'j':
		    case 'k': case 'l': case 'm': case 'n': case 'q':
		    case 'r': case 's': case 't': case 'u': case 'y':
		    case 'z': case 'B': case 'C': case 'H': case 'I':
		    case 'J': case 'K': case 'L': case 'N': case 'U':
		    case 'Y':
#ifdef MASTER
		    case CTRL('D'): case CTRL('A'):
#endif
			break;
		    default:
			count = 0;
		}
	    }
	    /*
	     * execute a command
	     */
	    if (count && !running)
		count--;
	    if (ch != 'a' && ch != ESCAPE && !(running || count || to_death))
	    {
		l_last_comm = last_comm;
		l_last_dir = last_dir;
		l_last_pick = last_pick;
		last_comm = ch;
		last_dir = '\0';
		last_pick = NULL;
	    }
over:
	    switch (ch)
	    {
		case ',': {
		    THING *obj = NULL;
		    int found = 0;
		    for (obj = lvl_obj; obj != NULL; obj = next(obj))
    			{
			    if (obj->o_pos.y == hero.y && obj->o_pos.x == hero.x)
			    {
				found=1;
				break;
			    }
    			}

		    if (found) {
			if (levit_check(rs))
			    ;
			else
			    pick_up(rs,(char)obj->o_type);
		    }
		    else {
			if (!terse)
			    addmsg(rs,"there is ");
			addmsg(rs,"nothing here");
                        if (!terse)
                            addmsg(rs," to pick up");
                        endmsg(rs);
		    }
		}
		when '!': shell(rs);
		when 'h': do_move(rs,0, -1);
		when 'j': do_move(rs,1, 0);
		when 'k': do_move(rs,-1, 0);
		when 'l': do_move(rs,0, 1);
		when 'y': do_move(rs,-1, -1);
		when 'u': do_move(rs,-1, 1);
		when 'b': do_move(rs,1, -1);
		when 'n': do_move(rs,1, 1);
		when 'H': do_run('h');
		when 'J': do_run('j');
		when 'K': do_run('k');
		when 'L': do_run('l');
		when 'Y': do_run('y');
		when 'U': do_run('u');
		when 'B': do_run('b');
		when 'N': do_run('n');
		when CTRL('H'): case CTRL('J'): case CTRL('K'): case CTRL('L'):
		case CTRL('Y'): case CTRL('U'): case CTRL('B'): case CTRL('N'):
		{
		    if (!on(player, ISBLIND))
		    {
			door_stop = TRUE;
			firstmove = TRUE;
		    }
		    if (count && !newcount)
			ch = direction;
		    else
		    {
			ch += ('A' - CTRL('A'));
			direction = ch;
		    }
		    goto over;
		}
		when 'F':
		    kamikaze = TRUE;
		    /* FALLTHROUGH */
		case 'f':
		    if (!get_dir(rs))
		    {
			after = FALSE;
			break;
		    }
		    delta.y += hero.y;
		    delta.x += hero.x;
		    if ( ((mp = moat(delta.y, delta.x)) == NULL)
			|| ((!see_monst(mp)) && !on(player, SEEMONST)))
		    {
			if (!terse)
			    addmsg(rs,"I see ");
			msg(rs,"no monster there");
			after = FALSE;
		    }
		    else if (diag_ok(&hero, &delta))
		    {
			to_death = TRUE;
			max_hit = 0;
			mp->t_flags |= ISTARGET;
			runch = ch = dir_ch;
			goto over;
		    }
		when 't':
		    if (!get_dir(rs))
			after = FALSE;
		    else
			missile(rs,delta.y, delta.x);
		when 'a':
		    if (last_comm == '\0')
		    {
			msg(rs,"you haven't typed a command yet");
			after = FALSE;
		    }
		    else
		    {
			ch = last_comm;
			again = TRUE;
			goto over;
		    }
		case 'q': quaff(rs);
                break;
		when 'Q':
                after = FALSE;
                q_comm = TRUE;
                if ( _quit() > 0 )
                {
                    if ( rs->guiflag != 0 )
                    {
                        if (rs->needflush == 0 )
                            rs->needflush = (uint32_t)time(NULL);
                        rogue_bailout(rs);
                    } else rs->replaydone = (uint32_t)time(NULL);
                }
                q_comm = FALSE;
                return;
		when 'i': after = FALSE; inventory(rs,pack, 0);
		when 'I': after = FALSE; picky_inven(rs);
		when 'd': drop(rs);
		when 'r': read_scroll(rs);
		when 'e': eat(rs);
		when 'w': wield(rs);
		when 'W': wear(rs);
		when 'T': take_off(rs);
		when 'P': ring_on(rs);
		when 'R': ring_off(rs);
		when 'o': option(rs); after = FALSE;
		when 'c': call(rs); after = FALSE;
                
        when '>': after = FALSE; d_level(rs);
                if ( rs->guiflag != 0 && rs->needflush == 0 )
                    rs->needflush = (uint32_t)time(NULL);
                
		when '<': after = FALSE; u_level(rs);
                if ( rs->guiflag != 0 && rs->needflush == 0 )
                    rs->needflush = (uint32_t)time(NULL);
               
		when '?': after = FALSE; help(rs);
		when '/': after = FALSE; identify(rs);
		when 's': search(rs);
		when 'z':
		    if (get_dir(rs))
			do_zap(rs);
		    else
			after = FALSE;
		when 'D': after = FALSE; discovered(rs);
		when CTRL('P'): after = FALSE; msg(rs,huh);
		when CTRL('R'):
		    after = FALSE;
		    clearok(curscr,TRUE);
		    wrefresh(curscr);
		when 'v':
		    after = FALSE;
		    msg(rs,"version %s. (mctesq was here)", release);
		when 'S': 
            after = FALSE;
#ifdef STANDALONE
            save_game(rs);
#else
            msg(rs,"Saving is disabled, use bailout rpc");
#endif
		when '.': ;			/* Rest command */
		when ' ': after = FALSE;	/* "Legal" illegal command */
		when '^':
		    after = FALSE;
		    if (get_dir(rs)) {
			delta.y += hero.y;
			delta.x += hero.x;
			fp = &flat(delta.y, delta.x);
                        if (!terse)
                            addmsg(rs,"You have found ");
			if (chat(delta.y, delta.x) != TRAP)
			    msg(rs,"no trap there");
			else if (on(player, ISHALU))
			    msg(rs,(char *)tr_name[rnd(NTRAPS)]);
			else {
			    msg(rs,(char *)tr_name[*fp & F_TMASK]);
			    *fp |= F_SEEN;
			}
		    }
#ifdef MASTER
		when '+':
		    after = FALSE;
		    if (wizard)
		    {
			wizard = FALSE;
			turn_see(rs,TRUE);
			msg(rs,"not wizard any more");
		    }
		    else
		    {
			wizard = passwd();
			if (wizard) 
			{
			    noscore = TRUE;
			    turn_see(rs,FALSE);
			    msg(rs,"you are suddenly as smart as Ken Arnold in dungeon #%d", dnum);
			}
			else
			    msg(rs,"sorry");
		    }
#endif
		when ESCAPE:	/* Escape */
		    door_stop = FALSE;
		    count = 0;
		    after = FALSE;
		    again = FALSE;
		when 'm':
		    move_on = TRUE;
		    if (!get_dir(rs))
			after = FALSE;
		    else
		    {
			ch = dir_ch;
			countch = dir_ch;
			goto over;
		    }
		when ')': current(rs,cur_weapon, "wielding", NULL);
		when ']': current(rs,cur_armor, "wearing", NULL);
		when '=':
		    current(rs,cur_ring[LEFT], "wearing",
					    terse ? (char *)"(L)" : (char *)"on left hand");
		    current(rs,cur_ring[RIGHT], "wearing",
					    terse ? (char *)"(R)" : (char *)"on right hand");
		when '@':
		    stat_msg = TRUE;
		    status(rs);
		    stat_msg = FALSE;
		    after = FALSE;
		otherwise:
		    after = FALSE;
#ifdef MASTER
		    if (wizard) switch (ch)
		    {
			case '|': msg(rs,"@ %d,%d", hero.y, hero.x);
			when 'C': create_obj();
			when '$': msg(rs,"inpack = %d", inpack);
			when CTRL('G'): inventory(rs,lvl_obj, 0);
			when CTRL('W'): whatis(rs,FALSE, 0);
			when CTRL('D'): level++; new_level();
			when CTRL('A'): level--; new_level();
			when CTRL('F'): show_map();
			when CTRL('T'): teleport();
			when CTRL('E'): msg(rs,"food left: %d", food_left);
			when CTRL('C'): add_pass();
			when CTRL('X'): turn_see(rs,on(player, SEEMONST));
			when CTRL('~'):
			{
			    THING *item;

			    if ((item = get_item(rs,"charge", STICK)) != NULL)
				item->o_charges = 10000;
			}
			when CTRL('I'):
			{
			    int i;
			    THING *obj;

			    for (i = 0; i < 9; i++)
				raise_level(rs);
			    /*
			     * Give him a sword (+1,+1)
			     */
			    obj = new_item();
			    init_weapon(obj, TWOSWORD);
			    obj->o_hplus = 1;
			    obj->o_dplus = 1;
			    add_pack(rs,obj, TRUE);
			    cur_weapon = obj;
			    /*
			     * And his suit of armor
			     */
			    obj = new_item();
			    obj->o_type = ARMOR;
			    obj->o_which = PLATE_MAIL;
			    obj->o_arm = -5;
			    obj->o_flags |= ISKNOW;
			    obj->o_count = 1;
			    obj->o_group = 0;
			    cur_armor = obj;
			    add_pack(rs,obj, TRUE);
			}
			when '*' :
			    pr_list();
			otherwise:
			    illcom(rs,ch);
		    }
		    else
#endif
			illcom(rs,ch);
	    }
	    /*
	     * turn off flags if no longer needed
	     */
	    if (!running)
		door_stop = FALSE;
	}
/*
	 * If he ran into something to take, let him pick it up.
	 */
        if (take != 0)
            pick_up(rs,take);
        if (!running)
            door_stop = FALSE;
        if (!after)
            ntimes++;
    }
    do_daemons(rs,AFTER);
    do_fuses(rs,AFTER);
    if (ISRING(LEFT, R_SEARCH))
        search(rs);
    else if (ISRING(LEFT, R_TELEPORT) && rnd(50) == 0)
        teleport(rs);
    if (ISRING(RIGHT, R_SEARCH))
        search(rs);
    else if (ISRING(RIGHT, R_TELEPORT) && rnd(50) == 0)
        teleport(rs);
}

/*
 * illcom:
 *	What to do with an illegal command
 */
void
illcom(struct rogue_state *rs,int ch)
{
    save_msg = FALSE;
    count = 0;
    msg(rs,"illegal command '%s'", unctrl(ch));
    save_msg = TRUE;
}

/*
 * search:
 *	player gropes about him to find hidden things.
 */
void
search(struct rogue_state *rs)
{
    register int y, x;
    register char *fp;
    register int ey, ex;
    int probinc;
    bool found;

    ey = hero.y + 1;
    ex = hero.x + 1;
    probinc = (on(player, ISHALU) ? 3 : 0);
    probinc += (on(player, ISBLIND) ? 2 : 0);
    found = FALSE;
    for (y = hero.y - 1; y <= ey; y++) 
	for (x = hero.x - 1; x <= ex; x++)
	{
	    if (y == hero.y && x == hero.x)
		continue;
	    fp = &flat(y, x);
	    if (!(*fp & F_REAL))
		switch (chat(y, x))
		{
		    case '|':
		    case '-':
			if (rnd(5 + probinc) != 0)
			    break;
			chat(y, x) = DOOR;
                        msg(rs,"a secret door");
foundone:
			found = TRUE;
			*fp |= F_REAL;
			count = FALSE;
			running = FALSE;
			break;
		    case FLOOR:
			if (rnd(2 + probinc) != 0)
			    break;
			chat(y, x) = TRAP;
			if (!terse)
			    addmsg(rs,"you found ");
			if (on(player, ISHALU))
			    msg(rs,(char *)tr_name[rnd(NTRAPS)]);
			else {
			    msg(rs,(char *)tr_name[*fp & F_TMASK]);
			    *fp |= F_SEEN;
			}
			goto foundone;
			break;
		    case ' ':
			if (rnd(3 + probinc) != 0)
			    break;
			chat(y, x) = PASSAGE;
			goto foundone;
		}
	}
    if (found)
	look(rs,FALSE);
}

/*
 * help:
 *	Give single character help, or the whole mess if he wants it
 */
void
help(struct rogue_state *rs)
{
    register const struct h_list *strp;
    register char helpch;
    register int numprint, cnt;
    msg(rs,"character you want help for (* for all): ");
    helpch = readchar(rs);
    mpos = 0;
    /*
     * If its not a *, print the right help string
     * or an error if he typed a funny character.
     */
    if (helpch != '*')
    {
	move(0, 0);
	for (strp = helpstr; strp->h_desc != NULL; strp++)
	    if (strp->h_ch == helpch)
	    {
		lower_msg = TRUE;
		msg(rs,"%s%s", unctrl(strp->h_ch), strp->h_desc);
		lower_msg = FALSE;
		return;
	    }
	msg(rs,"unknown character '%s'", unctrl(helpch));
	return;
    }
    /*
     * Here we print help for everything.
     * Then wait before we return to command mode
     */
    numprint = 0;
    for (strp = helpstr; strp->h_desc != NULL; strp++)
	if (strp->h_print)
	    numprint++;
    if (numprint & 01)		/* round odd numbers up */
	numprint++;
    numprint /= 2;
    if (numprint > LINES - 1)
	numprint = LINES - 1;

    wclear(hw);
    cnt = 0;
    for (strp = helpstr; strp->h_desc != NULL; strp++)
	if (strp->h_print)
	{
	    wmove(hw, cnt % numprint, cnt >= numprint ? COLS / 2 : 0);
	    if (strp->h_ch)
		waddstr(hw, unctrl(strp->h_ch));
	    waddstr(hw, strp->h_desc);
	    if (++cnt >= numprint * 2)
		break;
	}
    wmove(hw, LINES - 1, 0);
    waddstr(hw, "--Press space to continue--");
    wrefresh(hw);
    wait_for(rs,' ');
    clearok(stdscr, TRUE);
/*
    refresh();
*/
    msg(rs,"");
    touchwin(stdscr);
    wrefresh(stdscr);
}

/*
 * identify:
 *	Tell the player what a certain thing is.
 */
void
identify(struct rogue_state *rs)
{
    register int ch;
    register const struct h_list *hp;
    register char *str;
    static const struct h_list ident_list[] = {
	{'|',		"wall of a room",		FALSE},
	{'-',		"wall of a room",		FALSE},
	{GOLD,		"gold",				FALSE},
	{STAIRS,	"a staircase",			FALSE},
	{DOOR,		"door",				FALSE},
	{FLOOR,		"room floor",			FALSE},
	{PLAYER,	"you",				FALSE},
	{PASSAGE,	"passage",			FALSE},
	{TRAP,		"trap",				FALSE},
	{POTION,	"potion",			FALSE},
	{SCROLL,	"scroll",			FALSE},
	{FOOD,		"food",				FALSE},
	{WEAPON,	"weapon",			FALSE},
	{' ',		"solid rock",			FALSE},
	{ARMOR,		"armor",			FALSE},
	{AMULET,	"the Amulet of Yendor",		FALSE},
	{RING,		"ring",				FALSE},
	{STICK,		"wand or staff",		FALSE},
	{'\0'}
    };

    msg(rs,"what do you want identified? ");
    ch = readchar(rs);
    mpos = 0;
    if (ch == ESCAPE)
    {
	msg(rs,"");
	return;
    }
    if (isupper(ch))
	str = monsters[ch-'A'].m_name;
    else
    {
	str = "unknown character";
	for (hp = ident_list; hp->h_ch != '\0'; hp++)
	    if (hp->h_ch == ch)
	    {
		str = hp->h_desc;
		break;
	    }
    }
    msg(rs,"'%s': %s", unctrl(ch), str);
}

/*
 * d_level:
 *	He wants to go down a level
 */
void
d_level(struct rogue_state *rs)
{
    if (levit_check(rs))
	return;
    if (chat(hero.y, hero.x) != STAIRS)
	msg(rs,"I see no way down");
    else
    {
	level++;
	seenstairs = FALSE;
	new_level(rs);
    }
}

/*
 * u_level:
 *	He wants to go up a level
 */
void
u_level(struct rogue_state *rs)
{
    if (levit_check(rs))
	return;
    if (chat(hero.y, hero.x) == STAIRS)
	if (amulet)
	{
	    level--;
	    if (level == 0)
		total_winner(rs);
	    new_level(rs);
	    msg(rs,"you feel a wrenching sensation in your gut");
	}
	else
	    msg(rs,"your way is magically blocked");
    else
	msg(rs,"I see no way up");
}

/*
 * levit_check:
 *	Check to see if she's levitating, and if she is, print an
 *	appropriate message.
 */
bool
levit_check(struct rogue_state *rs)
{
    if (!on(player, ISLEVIT))
	return FALSE;
    msg(rs,"You can't.  You're floating off the ground!");
    return TRUE;
}

/*
 * call:
 *	Allow a user to call a potion, scroll, or ring something
 */
void
call(struct rogue_state *rs)
{
    register THING *obj;
    register const struct obj_info *op = NULL;
    register char **guess; const char *elsewise = NULL;
    register const bool *know;

    obj = get_item(rs,"call", CALLABLE);
    /*
     * Make certain that it is somethings that we want to wear
     */
    if (obj == NULL)
	return;
    switch (obj->o_type)
    {
	case RING:
	    op = &ring_info[obj->o_which];
	    elsewise = r_stones[obj->o_which];
	    goto norm;
	when POTION:
	    op = &pot_info[obj->o_which];
	    elsewise = p_colors[obj->o_which];
	    goto norm;
	when SCROLL:
	    op = &scr_info[obj->o_which];
	    elsewise = s_names[obj->o_which];
	    goto norm;
	when STICK:
	    op = &ws_info[obj->o_which];
	    elsewise = ws_made[obj->o_which];
norm:
	    know = &op->oi_know;
	    guess = (char **)&op->oi_guess;
	    if (*guess != NULL)
		elsewise = *guess;
	when FOOD:
	    msg(rs,"you can't call that anything");
	    return;
	otherwise:
	    guess = &obj->o_label;
	    know = NULL;
	    elsewise = obj->o_label;
    }
    if (know != NULL && *know)
    {
	msg(rs,"that has already been identified");
	return;
    }
    if (elsewise != NULL && elsewise == *guess)
    {
	if (!terse)
	    addmsg(rs,"Was ");
	msg(rs,"called \"%s\"", elsewise);
    }
    if (terse)
	msg(rs,"call it: ");
    else
	msg(rs,"what do you want to call it? ");

    if (elsewise == NULL)
	strcpy(prbuf, "");
    else
	strcpy(prbuf, elsewise);
    if (get_str(rs,prbuf, stdscr) == NORM)
    {
	if (*guess != NULL)
	    free(*guess);
	*guess = (char *)malloc((unsigned int) strlen(prbuf) + 1);
	strcpy(*guess, prbuf);
    }
}

/*
 * current:
 *	Print the current weapon/armor
 */
void
current(struct rogue_state *rs,THING *cur, char *how, char *where)
{
    after = FALSE;
    if (cur != NULL)
    {
	if (!terse)
	    addmsg(rs,"you are %s (", how);
	inv_describe = FALSE;
	addmsg(rs,"%c) %s", cur->o_packch, inv_name(cur, TRUE));
	inv_describe = TRUE;
	if (where)
	    addmsg(rs," %s", where);
	endmsg(rs);
    }
    else
    {
	if (!terse)
	    addmsg(rs,"you are ");
	addmsg(rs,"%s nothing", how);
	if (where)
	    addmsg(rs," %s", where);
	endmsg(rs);
    }
}
