/*
 * Rogue definitions and variable declarations
 *
 * @(#)rogue.h	5.42 (Berkeley) 08/06/83
 *
 * Rogue: Exploring the Dungeons of Doom
 * Copyright (C) 1980-1983, 1985, 1999 Michael Toy, Ken Arnold and Glenn Wichman
 * All rights reserved.
 *
 * See the file LICENSE.TXT for full copyright and licensing information.
 */

#ifndef H_ROGUE_H
#define H_ROGUE_H
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <memory.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>	/* we need va_list */
#include <stddef.h>	/* we want wchar_t */
#include <stdbool.h>
#include <ctype.h>

#include <time.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>

#ifndef BUILD_ROGUE
#include <curses.h>
#else
#include "cursesd.h"
#endif

#ifdef LINES
#undef LINES
#endif
#ifdef COLS
#undef COLS
#endif

#define	LINES	24
#define	COLS	80

#include "extern.h"


#undef lines 

#define NOOP(x) (x += 0)
#define CCHAR(x) ( (char) (x & A_CHARTEXT) )
/*
 * Maximum number of different things
 */
#define MAXROOMS	9
#define MAXTHINGS	9
#define MAXOBJ		9
#define MAXPACK		23
#define MAXTRAPS	10
#define AMULETLEVEL	26
#define	NUMTHINGS	7	/* number of types of things */
#define MAXPASS		13	/* upper limit on number of passages */
#define	NUMLINES	24
#define	NUMCOLS		80
#define STATLINE		(NUMLINES - 1)
#define BORE_LEVEL	50

/*
 * return values for get functions
 */
#define	NORM	0	/* normal exit */
#define	QUIT	1	/* quit option setting */
#define	MINUS	2	/* back up one option */

/*
 * inventory types
 */
#define	INV_OVER	0
#define	INV_SLOW	1
#define	INV_CLEAR	2

/*
 * All the fun defines
 */
#define when		break;case
#define otherwise	break;default
#define until(expr)	while(!(expr))
#define next(ptr)	(*ptr).l_next
#define prev(ptr)	(*ptr).l_prev
#define winat(y,x)	(moat(y,x) != NULL ? moat(y,x)->t_disguise : chat(y,x))
#define ce(a,b)		((a).x == (b).x && (a).y == (b).y)
#define hero		player.t_pos
#define pstats		player.t_stats
#define pack		player.t_pack
#define proom		player.t_room
#define max_hp		player.t_stats.s_maxhp
#define attach(a,b)	_attach(&a,b)
#define detach(a,b)	_detach(&a,b)
#define free_list(a)	_free_list(&a)
#undef max
#define max(a,b)	((a) > (b) ? (a) : (b))
#define on(thing,flag)	((bool)(((thing).t_flags & (flag)) != 0))
#define GOLDCALC	(rnd(50 + 10 * level) + 2)
#define ISRING(h,r)	(cur_ring[h] != NULL && cur_ring[h]->o_which == r)
#define ISWEARING(r)	(ISRING(LEFT, r) || ISRING(RIGHT, r))
#define ISMULT(type) 	(type == POTION || type == SCROLL || type == FOOD)
#define INDEX(y,x)	(&places[((x) << 5) + (y)])
#define chat(y,x)	(places[((x) << 5) + (y)].p_ch)
#define flat(y,x)	(places[((x) << 5) + (y)].p_flags)
#define moat(y,x)	(places[((x) << 5) + (y)].p_monst)
#define unc(cp)		(cp).y, (cp).x
#ifdef MASTER
#define debug		if (wizard) msg
#endif

/*
 * things that appear on the screens
 */
#define PASSAGE		'#'
#define DOOR		'+'
#define FLOOR		'.'
#define PLAYER		'@'
#define TRAP		'^'
#define STAIRS		'%'
#define GOLD		'*'
#define POTION		'!'
#define SCROLL		'?'
#define MAGIC		'$'
#define FOOD		':'
#define WEAPON		')'
#define ARMOR		']'
#define AMULET		','
#define RING		'='
#define STICK		'/'
#define CALLABLE	-1
#define R_OR_S		-2

/*
 * Various constants
 */
#define BEARTIME	spread(3)
#define SLEEPTIME	spread(5)
#define HOLDTIME	spread(2)
#define WANDERTIME	spread(70)
#define BEFORE		spread(1)
#define AFTER		spread(2)
#define HEALTIME	30
#define HUHDURATION	20
#define SEEDURATION	850
#define HUNGERTIME	1300
#define MORETIME	150
#define STOMACHSIZE	2000
#define STARVETIME	850
#define ESCAPE		27
#define LEFT		0
#define RIGHT		1
#define BOLT_LENGTH	6
#define LAMPDIST	3
#ifdef MASTER
#ifndef PASSWD
#define	PASSWD		"mTBellIQOsLNA"
#endif
#endif

/*
 * Save against things
 */
#define VS_POISON	00
#define VS_PARALYZATION	00
#define VS_DEATH	00
#define VS_BREATH	02
#define VS_MAGIC	03

/*
 * Various flag bits
 */
/* flags for rooms */
#define ISDARK	0000001		/* room is dark */
#define ISGONE	0000002		/* room is gone (a corridor) */
#define ISMAZE	0000004		/* room is gone (a corridor) */

/* flags for objects */
#define ISCURSED 000001		/* object is cursed */
#define ISKNOW	0000002		/* player knows details about the object */
#define ISMISL	0000004		/* object is a missile type */
#define ISMANY	0000010		/* object comes in groups */
/*	ISFOUND 0000020		...is used for both objects and creatures */
#define	ISPROT	0000040		/* armor is permanently protected */

/* flags for creatures */
#define CANHUH	0000001		/* creature can confuse */
#define CANSEE	0000002		/* creature can see invisible creatures */
#define ISBLIND	0000004		/* creature is blind */
#define ISCANC	0000010		/* creature has special qualities cancelled */
#define ISLEVIT	0000010		/* hero is levitating */
#define ISFOUND	0000020		/* creature has been seen (used for objects) */
#define ISGREED	0000040		/* creature runs to protect gold */
#define ISHASTE	0000100		/* creature has been hastened */
#define ISTARGET 000200		/* creature is the target of an 'f' command */
#define ISHELD	0000400		/* creature has been held */
#define ISHUH	0001000		/* creature is confused */
#define ISINVIS	0002000		/* creature is invisible */
#define ISMEAN	0004000		/* creature can wake when player enters room */
#define ISHALU	0004000		/* hero is on acid trip */
#define ISREGEN	0010000		/* creature can regenerate */
#define ISRUN	0020000		/* creature is running at the player */
#define SEEMONST 040000		/* hero can detect unseen monsters */
#define ISFLY	0040000		/* creature can fly */
#define ISSLOW	0100000		/* creature has been slowed */

/*
 * Flags for level map
 */
#define F_PASS		0x80		/* is a passageway */
#define F_SEEN		0x40		/* have seen this spot before */
#define F_DROPPED	0x20		/* object was dropped here */
#define F_LOCKED	0x20		/* door is locked */
#define F_REAL		0x10		/* what you see is what you get */
#define F_PNUM		0x0f		/* passage number mask */
#define F_TMASK		0x07		/* trap number mask */

/*
 * Trap types
 */
#define T_DOOR	00
#define T_ARROW	01
#define T_SLEEP	02
#define T_BEAR	03
#define T_TELEP	04
#define T_DART	05
#define T_RUST	06
#define T_MYST  07
#define NTRAPS	8

/*
 * Potion types
 */
#define P_CONFUSE	0
#define P_LSD		1
#define P_POISON	2
#define P_STRENGTH	3
#define P_SEEINVIS	4
#define P_HEALING	5
#define P_MFIND		6
#define	P_TFIND 	7
#define	P_RAISE		8
#define P_XHEAL		9
#define P_HASTE		10
#define P_RESTORE	11
#define P_BLIND		12
#define P_LEVIT		13
#define MAXPOTIONS	14

/*
 * Scroll types
 */
#define S_CONFUSE	0
#define S_MAP		1
#define S_HOLD		2
#define S_SLEEP		3
#define S_ARMOR		4
#define S_ID_POTION	5
#define S_ID_SCROLL	6
#define S_ID_WEAPON	7
#define S_ID_ARMOR	8
#define S_ID_R_OR_S	9
#define S_SCARE		10
#define S_FDET		11
#define S_TELEP		12
#define S_ENCH		13
#define S_CREATE	14
#define S_REMOVE	15
#define S_AGGR		16
#define S_PROTECT	17
#define MAXSCROLLS	18

/*
 * Weapon types
 */
#define MACE		0
#define SWORD		1
#define BOW		2
#define ARROW		3
#define DAGGER		4
#define TWOSWORD	5
#define DART		6
#define SHIRAKEN	7
#define SPEAR		8
#define FLAME		9	/* fake entry for dragon breath (ick) */
#define MAXWEAPONS	9	/* this should equal FLAME */

/*
 * Armor types
 */
#define LEATHER		0
#define RING_MAIL	1
#define STUDDED_LEATHER	2
#define SCALE_MAIL	3
#define CHAIN_MAIL	4
#define SPLINT_MAIL	5
#define BANDED_MAIL	6
#define PLATE_MAIL	7
#define MAXARMORS	8

/*
 * Ring types
 */
#define R_PROTECT	0
#define R_ADDSTR	1
#define R_SUSTSTR	2
#define R_SEARCH	3
#define R_SEEINVIS	4
#define R_NOP		5
#define R_AGGR		6
#define R_ADDHIT	7
#define R_ADDDAM	8
#define R_REGEN		9
#define R_DIGEST	10
#define R_TELEPORT	11
#define R_STEALTH	12
#define R_SUSTARM	13
#define MAXRINGS	14

/*
 * Rod/Wand/Staff types
 */
#define WS_LIGHT	0
#define WS_INVIS	1
#define WS_ELECT	2
#define WS_FIRE		3
#define WS_COLD		4
#define WS_POLYMORPH	5
#define WS_MISSILE	6
#define WS_HASTE_M	7
#define WS_SLOW_M	8
#define WS_DRAIN	9
#define WS_NOP		10
#define WS_TELAWAY	11
#define WS_TELTO	12
#define WS_CANCEL	13
#define MAXSTICKS	14

/*
 * Now we define the structures and types
 */

#define SMALLVAL 0.000000000000001
#define SATOSHIDEN ((uint64_t)100000000L)
#define dstr(x) ((double)(x) / SATOSHIDEN)

#ifndef _BITS256
#define _BITS256
union _bits256 { uint8_t bytes[32]; uint16_t ushorts[16]; uint32_t uints[8]; uint64_t ulongs[4]; uint64_t txid; };
typedef union _bits256 bits256;
#endif

#include "rogue_player.h" // interface to rpc

struct rogue_state
{
    uint64_t seed;
    char *keystrokes,*keystrokeshex;
    uint32_t needflush,replaydone;
    int32_t numkeys,ind,num,guiflag,counter,sleeptime,playersize,restoring,lastnum;
    FILE *logfp;
    struct rogue_player P;
    char buffered[10000];
    uint8_t playerdata[10000];
};
extern struct rogue_state globalR;

int rogue(int argc, char **argv, char **envp);
void rogueiterate(struct rogue_state *rs);
int32_t roguefname(char *fname,uint64_t seed,int32_t counter);
int32_t flushkeystrokes(struct rogue_state *rs,int32_t waitflag);
int32_t rogue_restorepack(struct rogue_state *rs);
void restore_player(struct rogue_state *rs);
int32_t rogue_replay2(uint8_t *newdata,uint64_t seed,char *keystrokes,int32_t num,struct rogue_player *player,int32_t sleepmillis);
void rogue_bailout(struct rogue_state *rs);
int32_t rogue_progress(struct rogue_state *rs,int32_t waitflag,uint64_t seed,char *keystrokes,int32_t num);
int32_t rogue_setplayerdata(struct rogue_state *rs,char *gametxidstr);

#define ROGUE_MAXTOTAL (pstats.s_str*2)

/*
 * Help list
 */
struct h_list {
    char h_ch;
    char *h_desc;
    bool h_print;
};

/*
 * Coordinate data type
 */
typedef struct {
    int x;
    int y;
} coord;

typedef unsigned int str_t;

/*
 * Stuff about objects
 */
struct obj_info {
    char *oi_name;
    int oi_prob;
    int oi_worth;
    char *oi_guess;
    bool oi_know;
};

/*
 * Room structure
 */
struct room {
    coord r_pos;			/* Upper left corner */
    coord r_max;			/* Size of room */
    coord r_gold;			/* Where the gold is */
    int r_goldval;			/* How much the gold is worth */
    short r_flags;			/* info about the room */
    int r_nexits;			/* Number of exits */
    coord r_exit[12];			/* Where the exits are */
};

/*
 * Structure describing a fighting being
 */
struct stats {
    str_t s_str;			/* Strength */
    int s_exp;				/* Experience */
    int s_lvl;				/* level of mastery */
    int s_arm;				/* Armor class */
    int s_hpt;			/* Hit points */
    char s_dmg[13];			/* String describing damage done */
    int  s_maxhp;			/* Max hit points */
};

/*
 * Structure for monsters and player
 */
union thing {
    struct {
	union thing *_l_next, *_l_prev;	/* Next pointer in link */
	coord _t_pos;			/* Position */
	bool _t_turn;			/* If slowed, is it a turn to move */
	char _t_type;			/* What it is */
	char _t_disguise;		/* What mimic looks like */
	char _t_oldch;			/* Character that was where it was */
	coord *_t_dest;			/* Where it is running to */
	short _t_flags;			/* State word */
	struct stats _t_stats;		/* Physical description */
	struct room *_t_room;		/* Current room for thing */
	union thing *_t_pack;		/* What the thing is carrying */
        int _t_reserved;
    } _t;
    struct {
	union thing *_l_next, *_l_prev;	/* Next pointer in link */
	int _o_type;			/* What kind of object it is */
	coord _o_pos;			/* Where it lives on the screen */
	char *_o_text;			/* What it says if you read it */
	int  _o_launch;			/* What you need to launch it */
	char _o_packch;			/* What character it is in the pack */
	char _o_damage[8];		/* Damage if used like sword */
	char _o_hurldmg[8];		/* Damage if thrown */
	int _o_count;			/* count for plural objects */
	int _o_which;			/* Which object of a type it is */
	int _o_hplus;			/* Plusses to hit */
	int _o_dplus;			/* Plusses to damage */
	int _o_arm;			/* Armor protection */
	int _o_flags;			/* information about objects */
	int _o_group;			/* group number for this object */
	char *_o_label;			/* Label for object */
    } _o;
};

typedef union thing THING;

#define l_next		_t._l_next
#define l_prev		_t._l_prev
#define t_pos		_t._t_pos
#define t_turn		_t._t_turn
#define t_type		_t._t_type
#define t_disguise	_t._t_disguise
#define t_oldch		_t._t_oldch
#define t_dest		_t._t_dest
#define t_flags		_t._t_flags
#define t_stats		_t._t_stats
#define t_pack		_t._t_pack
#define t_room		_t._t_room
#define t_reserved      _t._t_reserved
#define o_type		_o._o_type
#define o_pos		_o._o_pos
#define o_text		_o._o_text
#define o_launch	_o._o_launch
#define o_packch	_o._o_packch
#define o_damage	_o._o_damage
#define o_hurldmg	_o._o_hurldmg
#define o_count		_o._o_count
#define o_which		_o._o_which
#define o_hplus		_o._o_hplus
#define o_dplus		_o._o_dplus
#define o_arm		_o._o_arm
#define o_charges	o_arm
#define o_goldval	o_arm
#define o_flags		_o._o_flags
#define o_group		_o._o_group
#define o_label		_o._o_label

/*
 * describe a place on the level map
 */
typedef struct {
    char p_ch;
    char p_flags;
    THING *p_monst;
} PLACE;

/*
 * Array containing information on all the various types of monsters
 */
struct monster {
    char *m_name;			/* What to call the monster */
    int m_carry;			/* Probability of carrying something */
    short m_flags;			/* things about the monster */
    struct stats m_stats;		/* Initial stats */
};

/*
 * External variables
 */
extern const char *tr_name[],*inv_t_name[];
extern const int32_t a_class[], e_levels[];
extern const struct h_list	helpstr[];
extern const char *h_names[],*m_names[];


extern const struct monster	origmonsters[26];
extern const struct room origpassages[MAXPASS];
extern const struct obj_info origthings[NUMTHINGS],origring_info[MAXRINGS],origpot_info[MAXPOTIONS],origarm_info[MAXARMORS],origscr_info[MAXSCROLLS],origws_info[MAXSTICKS],origweap_info[MAXWEAPONS + 1];
extern struct monster monsters[26];
extern struct room passages[MAXPASS];
extern struct obj_info things[NUMTHINGS],ring_info[MAXRINGS],pot_info[MAXPOTIONS],arm_info[MAXARMORS],scr_info[MAXSCROLLS],weap_info[MAXWEAPONS + 1],ws_info[MAXSTICKS];

extern bool	after, again, allscore, amulet, door_stop, fight_flush,
		firstmove, has_hit, inv_describe, jump, kamikaze,
		lower_msg, move_on, msg_esc, pack_used[],
		passgo, playing, q_comm, running, save_msg, see_floor,
		seenstairs, stat_msg, terse, to_death, tombstone;

extern char	dir_ch, file_name[], home[], huh[],
		l_last_comm, l_last_dir, last_comm, last_dir, *Numname,
		outbuf[],  *release, *s_names[], runch, take;
extern const char *ws_made[], *r_stones[], *p_colors[], *ws_type[];

extern int	count, food_left, hungry_state, inpack,
		inv_type, lastscore, level, max_hit, max_level, mpos,
		n_objs, no_command, no_food, no_move, noscore, ntraps, purse,
		quiet, vf_hit;

extern unsigned int	numscores;

extern uint64_t seed;

extern WINDOW	*hw;

extern coord	delta, oldpos, stairs;

extern PLACE	places[];

extern THING	*cur_armor, *cur_ring[], *cur_weapon, *l_last_pick,
		*last_pick, *lvl_obj, *mlist, player;


extern struct room	*oldrp, rooms[];

extern struct stats	max_stats;


/*
 * Function types
 */
void	_attach(THING **list, THING *item);
void	_detach(THING **list, THING *item);
void	_free_list(THING **ptr);
void	addmsg(struct rogue_state *rs,char *fmt, ...);
bool	add_haste(struct rogue_state *rs,bool potion);
char	add_line(struct rogue_state *rs,char *fmt, char *arg);
void	add_pack(struct rogue_state *rs,THING *obj, bool silent);
void	add_pass(void);
void	add_str(str_t *sp, int amt);
void	accnt_maze(int y, int x, int ny, int nx);
void	aggravate(struct rogue_state *rs);
int	attack(struct rogue_state *rs,THING *mp);
void	badcheck(char *name, struct obj_info *info, int bound);
void	bounce(struct rogue_state *rs,THING *weap, char *mname, bool noend);
void	call(struct rogue_state *rs);
void	call_it(struct rogue_state *rs,struct obj_info *info);
bool	cansee(struct rogue_state *rs,int y, int x);
int	center(char *str);
void	chg_str(int amt);
void	check_level(struct rogue_state *rs);
void	conn(struct rogue_state *rs,int r1, int r2);
void	command(struct rogue_state *rs);
void	create_obj(struct rogue_state *rs);

void	current(struct rogue_state *rs,THING *cur, char *how, char *where);
void	d_level(struct rogue_state *rs);
void	death(struct rogue_state *rs,char monst);
char	death_monst(void);
void	dig(struct rogue_state *rs,int y, int x);
void	discard(THING *item);
void	discovered(struct rogue_state *rs);
int	dist(int y1, int x1, int y2, int x2);
int	dist_cp(coord *c1, coord *c2);
int	do_chase(struct rogue_state *rs,THING *th);
void	do_daemons(struct rogue_state *rs,int flag);
void	do_fuses(struct rogue_state *rs,int flag);
void	do_maze(struct rogue_state *rs,struct room *rp);
void	do_motion(struct rogue_state *rs,THING *obj, int ydelta, int xdelta);
void	do_move(struct rogue_state *rs,int dy, int dx);
void	do_passages(struct rogue_state *rs);
void	do_pot(struct rogue_state *rs,int type, bool knowit);
void	do_rooms(struct rogue_state *rs);
void	do_run(char ch);
void	do_zap(struct rogue_state *rs);
void	doadd(struct rogue_state *rs,char *fmt, va_list args);
void	door(struct room *rm, coord *cp);
void	door_open(struct rogue_state *rs,struct room *rp);
void	drain(struct rogue_state *rs);
void	draw_room(struct rogue_state *rs,struct room *rp);
void	drop(struct rogue_state *rs);
void	eat(struct rogue_state *rs);
size_t  encread(char *start, size_t size, FILE *inf);
size_t	encwrite(char *start, size_t size, FILE *outf);
int	endmsg(struct rogue_state *rs);
void	enter_room(struct rogue_state *rs,coord *cp);
void	erase_lamp(coord *pos, struct room *rp);
int	exp_add(THING *tp);
void	extinguish(void (*func)(struct rogue_state *rs,int));
void	fall(struct rogue_state *rs,THING *obj, bool pr);
void	fire_bolt(struct rogue_state *rs,coord *start, coord *dir, char *name);
char	floor_at(void);
void	flush_type(void);
int	fight(struct rogue_state *rs,coord *mp, THING *weap, bool thrown);
void	fix_stick(THING *cur);
void	fuse(void (*func)(struct rogue_state *rs,int), int arg, int time, int type);
bool	get_dir(struct rogue_state *rs);
int	gethand(struct rogue_state *rs);
void	give_pack(struct rogue_state *rs,THING *tp);
void	help(struct rogue_state *rs);
void	hit(struct rogue_state *rs,char *er, char *ee, bool noend);
void	horiz(struct room *rp, int starty);
void	leave_room(struct rogue_state *rs,coord *cp);
void	lengthen(void (*func)(struct rogue_state *rs,int), int xtime);
void	look(struct rogue_state *rs,bool wakeup);
int	hit_monster(struct rogue_state *rs,int y, int x, THING *obj);
void	identify(struct rogue_state *rs);
void	illcom(struct rogue_state *rs,int ch);
void	init_check(void);
void	init_colors(void);
void	init_materials(void);
void	init_names(void);
void	init_player(struct rogue_state *rs);
void	init_probs(void);
void	init_stones(void);
void	init_weapon(THING *weap, int which);
bool	inventory(struct rogue_state *rs,THING *list, int type);
void	invis_on(void);
void	killed(struct rogue_state *rs,THING *tp, bool pr);
void	kill_daemon(void (*func)(struct rogue_state *rs,int));
bool	lock_sc(void);
void	miss(struct rogue_state *rs,char *er, char *ee, bool noend);
void	missile(struct rogue_state *rs,int ydelta, int xdelta);
void	money(struct rogue_state *rs,int value);
int	move_monst(struct rogue_state *rs,THING *tp);
void	move_msg(struct rogue_state *rs,THING *obj);
int	msg(struct rogue_state *rs,char *fmt, ...);
void	nameit(THING *obj, const char *type, const char *which, struct obj_info *op, char *(*prfunc)(THING *));
void	new_level(struct rogue_state *rs);
void	new_monster(struct rogue_state *rs,THING *tp, char type, coord *cp);
void	numpass(int y, int x);
void	option(struct rogue_state *rs);
void	open_score(void);
void	parse_opts(char *str);
void 	passnum(void);
char	*pick_color(char *col);
int	pick_one(struct rogue_state *rs,struct obj_info *info, int nitems);
void	pick_up(struct rogue_state *rs,char ch);
void	picky_inven(struct rogue_state *rs);
void	pr_spec(struct obj_info *info, int nitems);
void	pr_list(void);
void	put_bool(void *b);
void	put_inv_t(void *ip);
void	put_str(void *str);
void	put_things(struct rogue_state *rs);
void	putpass(coord *cp);
void	print_disc(struct rogue_state *rs,char);
void	quaff(struct rogue_state *rs);
void	raise_level(struct rogue_state *rs);
char	randmonster(bool wander);
void	read_scroll(struct rogue_state *rs);
void    relocate(struct rogue_state *rs,THING *th, coord *new_loc);
void	remove_mon(struct rogue_state *rs,coord *mp, THING *tp, bool waskill);
void	reset_last(void);
bool	restore(struct rogue_state *rs,char *file, char **envp);
int	ring_eat(int hand);
void	ring_on(struct rogue_state *rs);
void	ring_off(struct rogue_state *rs);
int	rnd(int range);
int	rnd_room(void);
int	roll(int number, int sides);
int	rs_save_file(struct rogue_state *rs,FILE *savef);
int	rs_restore_file(FILE *inf);
void	runto(struct rogue_state *rs,coord *runner);
void	rust_armor(struct rogue_state *rs,THING *arm);
int	save(int which);
void	save_file(struct rogue_state *rs,FILE *savef,int32_t guiflag);
void	save_game(struct rogue_state *rs);
int	save_throw(int which, THING *tp);
void	score(struct rogue_state *rs,int amount, int flags, char monst);
void	search(struct rogue_state *rs);
void	set_know(THING *obj, struct obj_info *info);
void	set_oldch(THING *tp, coord *cp);
void	setup(void);
void	shell(struct rogue_state *rs);
bool	show_floor(void);
void	show_map(void);
void	show_win(struct rogue_state *rs,char *message);
int	sign(int nm);
int	spread(int nm);
void	start_daemon(void (*func)(struct rogue_state *rs,int), int arg, int type);
void	start_score(void);
void	status(struct rogue_state *rs);
int	step_ok(int ch);
void	strucpy(char *s1, char *s2, int len);
int	swing(int at_lvl, int op_arm, int wplus);
void	take_off(struct rogue_state *rs);
void	teleport(struct rogue_state *rs);
void	total_winner(struct rogue_state *rs);
void	thunk(struct rogue_state *rs,THING *weap, char *mname, bool noend);
void	treas_room(struct rogue_state *rs);
void	turnref(void);
void	u_level(struct rogue_state *rs);
void	uncurse(THING *obj);
void	unlock_sc(void);
void	vert(struct room *rp, int startx);
void	wait_for(struct rogue_state *rs,int ch);
THING  *wake_monster(struct rogue_state *rs,int y, int x);
void	wanderer(struct rogue_state *rs);
void	waste_time(struct rogue_state *rs);
void	wear(struct rogue_state *rs);
void	whatis(struct rogue_state *rs,bool insist, int type);
void	wield(struct rogue_state *rs);

bool	chase(THING *tp, coord *ee);
bool	diag_ok(coord *sp, coord *ep);
bool	dropcheck(struct rogue_state *rs,THING *obj);
bool	fallpos(coord *pos, coord *newpos);
bool	find_floor(struct rogue_state *rs,struct room *rp, coord *cp, int limit, bool monst);
bool	is_magic(THING *obj);
bool    is_symlink(char *sp); 
bool	levit_check(struct rogue_state *rs);
bool	pack_room(struct rogue_state *rs,bool from_floor, THING *obj);
bool	roll_em(THING *thatt, THING *thdef, THING *weap, bool hurl);
bool	see_monst(THING *mp);
bool	seen_stairs(void);
bool	turn_ok(int y, int x);
bool	turn_see(struct rogue_state *rs,bool turn_off);
bool	is_current(struct rogue_state *rs,THING *obj);
int	passwd(void);

char	be_trapped(struct rogue_state *rs,coord *tc);
char	floor_ch(void);
char	pack_char(void);
char	readchar(struct rogue_state *rs);
char	rnd_thing(void);

char	*charge_str(THING *obj);
char	*choose_str(char *ts, char *ns);
char	*inv_name(THING *obj, bool drop);
char	*nullstr(THING *ignored);
char	*num(int n1, int n2, char type);
char	*ring_num(THING *obj);
char	*set_mname(THING *tp);
char	*vowelstr(char *str);

int	get_bool(struct rogue_state *rs,void *vp, WINDOW *win);
int	get_inv_t(struct rogue_state *rs,void *vp, WINDOW *win);
int	get_num(struct rogue_state *rs,void *vp, WINDOW *win);
int	get_sf(struct rogue_state *rs,void *vp, WINDOW *win);
int	get_str(struct rogue_state *rs,void *vopt, WINDOW *win);
int	trip_ch(int y, int x, int ch);

coord	*find_dest(struct rogue_state *rs,THING *tp);
coord	*rndmove(THING *who);

THING	*find_obj(struct rogue_state *rs,int y, int x);
THING	*get_item(struct rogue_state *rs,char *purpose, int type);
THING	*leave_pack(struct rogue_state *rs,THING *obj, bool newobj, bool all);
THING	*new_item(void);
THING	*new_thing(struct rogue_state *rs);
void	end_line(struct rogue_state *rs);
int32_t num_packitems(struct rogue_state *rs);
int32_t rogue_total(THING *o);

void	runners(struct rogue_state *rs,int);
void	land(struct rogue_state *rs,int);
void	visuals(struct rogue_state *rs,int);
void	come_down(struct rogue_state *rs,int);
void	stomach(struct rogue_state *rs,int);
void	nohaste(struct rogue_state *rs,int);
void	sight(struct rogue_state *rs,int);
void	unconfuse(struct rogue_state *rs,int);
void	rollwand(struct rogue_state *rs,int);
void	unsee(struct rogue_state *rs,int);
void	swander(struct rogue_state *rs,int);
void	doctor(struct rogue_state *rs,int);

void	playit(struct rogue_state *rs);

struct room	*roomin(struct rogue_state *rs,coord *cp);
int32_t thing_find(THING *ptr);

#define MAXDAEMONS 20

extern struct delayed_action {
    int d_type;
    void (*d_func)(struct rogue_state *rs,int);
    int d_arg;
    int d_time;
} d_list[MAXDAEMONS];

typedef struct {
    char	*st_name;
    int		st_value;
} STONE;

extern int      total;
extern int      between;
extern int      group;
extern coord    nh;
extern const char     *rainbow[];
extern int      cNCOLORS;
extern const STONE    stones[];
extern int      cNSTONES;
extern const char     *wood[];
extern int      cNWOOD;
extern const char     *metal[];
extern int      cNMETAL;

//extern WINDOW *stdscr,*curscr;

#endif

