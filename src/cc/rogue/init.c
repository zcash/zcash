/*
 * global variable initializaton
 *
 * @(#)init.c	4.31 (Berkeley) 02/05/99
 *
 * Rogue: Exploring the Dungeons of Doom
 * Copyright (C) 1980-1983, 1985, 1999 Michael Toy, Ken Arnold and Glenn Wichman
 * All rights reserved.
 *
 * See the file LICENSE.TXT for full copyright and licensing information.
 */

//#include <stdlib.h>
//#include <curses.h>
//#include <ctype.h>
//#include <string.h>
#include "rogue.h"

/*
 * init_player:
 *	Roll her up
 */
void rogue_restoreobject(THING *o,struct rogue_packitem *item);

int32_t rogue_total(THING *o)
{
    if ( (o->o_flags & ISMANY) != 0 )
        return(1);
    else return(o->o_count);
}

void restore_player(struct rogue_state *rs)
{
    int32_t i,total = 0; THING *obj;
    //rs->P.gold = purse;
    max_hp = rs->P.hitpoints;
    //pstats.s_hpt = max_hp;
    pstats.s_str = rs->P.strength & 0xffff;
    if ( (max_stats.s_str= (rs->P.strength >> 16) & 0xffff) == 0 )
        max_stats.s_str = 16;
    if ( pstats.s_str > max_stats.s_str )
        pstats.s_str = max_stats.s_str;
    pstats.s_lvl = rs->P.level;
    pstats.s_exp = rs->P.experience;
    for (i=0; i<rs->P.packsize&&i<MAXPACK; i++)
    {
        obj = new_item();
        rogue_restoreobject(obj,&rs->P.roguepack[i]);
        total += rogue_total(obj);
        if ( total > ROGUE_MAXTOTAL )
            break;
        add_pack(rs,obj,TRUE);
    }
}

void init_player(struct rogue_state *rs)
{
    register THING *obj; int32_t i;
    pstats = max_stats;
    food_left = HUNGERTIME;

    if ( rs->restoring != 0 )
    {
        // duplicate rng usage of normal case
        obj = new_item();
        init_weapon(obj, MACE);
        free(obj);
        obj = new_item();
        init_weapon(obj, BOW);
        free(obj);
        obj = new_item();
        init_weapon(obj, ARROW);
        obj->o_count = rnd(15) + 25;
        free(obj);
    }
    else
    {
        /*
         * Give him some food
         */
        obj = new_item();
        obj->o_type = FOOD;
        obj->o_count = 1;
        add_pack(rs,obj, TRUE);
        /*
         * And his suit of armor
         */
        obj = new_item();
        obj->o_type = ARMOR;
        obj->o_which = RING_MAIL;
        obj->o_arm = a_class[RING_MAIL] - 1;
        obj->o_flags |= ISKNOW;
        obj->o_count = 1;
        cur_armor = obj;
        add_pack(rs,obj, TRUE);
        /*
         * Give him his weaponry.  First a mace.
         */
        obj = new_item();
        init_weapon(obj, MACE);
        obj->o_hplus = 1;
        obj->o_dplus = 1;
        obj->o_flags |= ISKNOW;
        add_pack(rs,obj, TRUE);
        cur_weapon = obj;
        /*
         * Now a +1 bow
         */
        obj = new_item();
        init_weapon(obj, BOW);
        obj->o_hplus = 1;
        obj->o_flags |= ISKNOW;
        add_pack(rs,obj, TRUE);
        /*
         * Now some arrows
         */
        obj = new_item();
        init_weapon(obj, ARROW);
        obj->o_count = rnd(15) + 25;
        obj->o_flags |= ISKNOW;
        add_pack(rs,obj, TRUE);
        //fprintf(stderr,"initial o_count.%d\n",obj->o_count); sleep(3);
    }
}

/*
 * Contains defintions and functions for dealing with things like
 * potions and scrolls
 */

const char *rainbow[] = {
    "amber",
    "aquamarine",
    "black",
    "blue",
    "brown",
    "clear",
    "crimson",
    "cyan",
    "ecru",
    "gold",
    "green",
    "grey",
    "magenta",
    "orange",
    "pink",
    "plaid",
    "purple",
    "red",
    "silver",
    "tan",
    "tangerine",
    "topaz",
    "turquoise",
    "vermilion",
    "violet",
    "white",
    "yellow",
};

#define NCOLORS (sizeof rainbow / sizeof (char *))
int cNCOLORS = NCOLORS;

static const char *sylls[] = {
    "a", "ab", "ag", "aks", "ala", "an", "app", "arg", "arze", "ash",
    "bek", "bie", "bit", "bjor", "blu", "bot", "bu", "byt", "comp",
    "con", "cos", "cre", "dalf", "dan", "den", "do", "e", "eep", "el",
    "eng", "er", "ere", "erk", "esh", "evs", "fa", "fid", "fri", "fu",
    "gan", "gar", "glen", "gop", "gre", "ha", "hyd", "i", "ing", "ip",
    "ish", "it", "ite", "iv", "jo", "kho", "kli", "klis", "la", "lech",
    "mar", "me", "mi", "mic", "mik", "mon", "mung", "mur", "nej",
    "nelg", "nep", "ner", "nes", "nes", "nih", "nin", "o", "od", "ood",
    "org", "orn", "ox", "oxy", "pay", "ple", "plu", "po", "pot",
    "prok", "re", "rea", "rhov", "ri", "ro", "rog", "rok", "rol", "sa",
    "san", "sat", "sef", "seh", "shu", "ski", "sna", "sne", "snik",
    "sno", "so", "sol", "sri", "sta", "sun", "ta", "tab", "tem",
    "ther", "ti", "tox", "trol", "tue", "turs", "u", "ulk", "um", "un",
    "uni", "ur", "val", "viv", "vly", "vom", "wah", "wed", "werg",
    "wex", "whon", "wun", "xo", "y", "yot", "yu", "zant", "zeb", "zim",
    "zok", "zon", "zum",
};

const STONE stones[] = {
    { "agate",		 25},
    { "alexandrite",	 40},
    { "amethyst",	 50},
    { "carnelian",	 40},
    { "diamond",	300},
    { "emerald",	300},
    { "germanium",	225},
    { "granite",	  5},
    { "garnet",		 50},
    { "jade",		150},
    { "kryptonite",	300},
    { "lapis lazuli",	 50},
    { "moonstone",	 50},
    { "obsidian",	 15},
    { "onyx",		 60},
    { "opal",		200},
    { "pearl",		220},
    { "peridot",	 63},
    { "ruby",		350},
    { "sapphire",	285},
    { "stibotantalite",	200},
    { "tiger eye",	 50},
    { "topaz",		 60},
    { "turquoise",	 70},
    { "taaffeite",	300},
    { "zircon",	 	 80},
};

#define NSTONES (sizeof stones / sizeof (STONE))
int cNSTONES = NSTONES;

const char *wood[] = {
    "avocado wood",
    "balsa",
    "bamboo",
    "banyan",
    "birch",
    "cedar",
    "cherry",
    "cinnibar",
    "cypress",
    "dogwood",
    "driftwood",
    "ebony",
    "elm",
    "eucalyptus",
    "fall",
    "hemlock",
    "holly",
    "ironwood",
    "kukui wood",
    "mahogany",
    "manzanita",
    "maple",
    "oaken",
    "persimmon wood",
    "pecan",
    "pine",
    "poplar",
    "redwood",
    "rosewood",
    "spruce",
    "teak",
    "walnut",
    "zebrawood",
};

#define NWOOD (sizeof wood / sizeof (char *))
int cNWOOD = NWOOD;

const char *metal[] = {
    "aluminum",
    "beryllium",
    "bone",
    "brass",
    "bronze",
    "copper",
    "electrum",
    "gold",
    "iron",
    "lead",
    "magnesium",
    "mercury",
    "nickel",
    "pewter",
    "platinum",
    "steel",
    "silver",
    "silicon",
    "tin",
    "titanium",
    "tungsten",
    "zinc",
};

#define NMETAL (sizeof metal / sizeof (char *))
int cNMETAL = NMETAL;
#define MAX3(a,b,c)	(a > b ? (a > c ? a : c) : (b > c ? b : c))

static bool used[MAX3(NCOLORS, NSTONES, NWOOD)];

/*
 * init_colors:
 *	Initialize the potion color scheme for this time
 */
void
init_colors()
{
    register int i, j;
    memset(used,0,sizeof(used));
    for (i = 0; i < NCOLORS; i++)
        used[i] = FALSE;
    for (i = 0; i < MAXPOTIONS; i++)
    {
        do
            j = rnd(NCOLORS);
        until (!used[j]);
        used[j] = TRUE;
        p_colors[i] = rainbow[j];
    }
}

/*
 * init_names:
 *	Generate the names of the various scrolls
 */
#define MAXNAME	40	/* Max number of characters in a name */

void
init_names()
{
    register int nsyl;
    register char *cp; const char *sp;
    register int i, nwords;

    for (i = 0; i < MAXSCROLLS; i++)
    {
	cp = prbuf;
	nwords = rnd(3) + 2;
	while (nwords--)
	{
	    nsyl = rnd(3) + 1;
	    while (nsyl--)
	    {
		sp = sylls[rnd((sizeof sylls) / (sizeof (char *)))];
		if (&cp[strlen(sp)] > &prbuf[MAXNAME])
			break;
		while (*sp)
		    *cp++ = *sp++;
	    }
	    *cp++ = ' ';
	}
	*--cp = '\0';
	s_names[i] = (char *) malloc((unsigned) strlen(prbuf)+1);
	strcpy(s_names[i], prbuf);
    }
}

/*
 * init_stones:
 *	Initialize the ring stone setting scheme for this time
 */
void
init_stones()
{
    register int i, j;
    for (i = 0; i < NSTONES; i++)
        used[i] = FALSE;
    for (i = 0; i < MAXRINGS; i++)
    {
        do
            j = rnd(NSTONES);
        until (!used[j]);
        used[j] = TRUE;
        r_stones[i] = stones[j].st_name;
        ring_info[i].oi_worth += stones[j].st_value;
    }
}

/*
 * init_materials:
 *	Initialize the construction materials for wands and staffs
 */
void
init_materials()
{
    register int i, j;
    register const char *str;
    static bool metused[NMETAL];
    memset(metused,0,sizeof(metused));
    for (i = 0; i < NWOOD; i++)
        used[i] = FALSE;
    for (i = 0; i < NMETAL; i++)
        metused[i] = FALSE;
    for (i = 0; i < MAXSTICKS; i++)
    {
        for (;;)
            if (rnd(2) == 0)
            {
                j = rnd(NMETAL);
                if (!metused[j])
                {
                    ws_type[i] = "wand";
                    str = metal[j];
                    metused[j] = TRUE;
                    break;
                }
            }
            else
            {
                j = rnd(NWOOD);
                if (!used[j])
                {
                    ws_type[i] = "staff";
                    str = wood[j];
                    used[j] = TRUE;
                    break;
                }
            }
        ws_made[i] = str;
    }
}

#ifdef MASTER
# define	NT	NUMTHINGS, "things"
# define	MP	MAXPOTIONS, "potions"
# define	MS	MAXSCROLLS, "scrolls"
# define	MR	MAXRINGS, "rings"
# define	MWS	MAXSTICKS, "sticks"
# define	MW	MAXWEAPONS, "weapons"
# define	MA	MAXARMORS, "armor"
#else
# define	NT	NUMTHINGS
# define	MP	MAXPOTIONS
# define	MS	MAXSCROLLS
# define	MR	MAXRINGS
# define	MWS	MAXSTICKS
# define	MW	MAXWEAPONS
# define	MA	MAXARMORS
#endif

/*
 * sumprobs:
 *	Sum up the probabilities for items appearing
 */
void
sumprobs(struct obj_info *info, int bound
#ifdef MASTER
         , char *name
#endif
)
{
#ifdef MASTER
    struct obj_info *start = info;
#endif
    struct obj_info *endp;
    endp = info + bound;
    while (++info < endp)
        info->oi_prob += (info - 1)->oi_prob;
#ifdef MASTER
    badcheck(name, start, bound);
#endif
}

/*
 * init_probs:
 *	Initialize the probabilities for the various items
 */
void
init_probs()
{
    sumprobs(things, NT);
    sumprobs(pot_info, MP);
    sumprobs(scr_info, MS);
    sumprobs(ring_info, MR);
    sumprobs(ws_info, MWS);
    sumprobs(weap_info, MW);
    sumprobs(arm_info, MA);
}

#ifdef MASTER
/*
 * badcheck:
 *	Check to see if a series of probabilities sums to 100
 */
void
badcheck(char *name, struct obj_info *info, int bound)
{
    register struct obj_info *end;

    if (info[bound - 1].oi_prob == 100)
	return;
    printf("\nBad percentages for %s (bound = %d):\n", name, bound);
    for (end = &info[bound]; info < end; info++)
	printf("%3d%% %s\n", info->oi_prob, info->oi_name);
    printf("[hit RETURN to continue]");
    fflush(stdout);
    while (getchar() != '\n')
	continue;
}
#endif

/*
 * pick_color:
 *	If he is halucinating, pick a random color name and return it,
 *	otherwise return the given color.
 */
char *
pick_color(char *col)
{
    return (on(player, ISHALU) ? (char *)rainbow[rnd(NCOLORS)] : col);
}
