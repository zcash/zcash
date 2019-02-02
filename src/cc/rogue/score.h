/*
 * Score file structure
 *
 * @(#)score.h	4.6 (Berkeley) 02/05/99
 *
 * Rogue: Exploring the Dungeons of Doom
 * Copyright (C) 1980-1983, 1985, 1999 Michael Toy, Ken Arnold and Glenn Wichman
 * All rights reserved.
 *
 * See the file LICENSE.TXT for full copyright and licensing information.
 */

struct sc_ent {
    unsigned int sc_uid;
    int sc_score;
    unsigned int sc_flags;
    unsigned short sc_monster;
    char sc_name[MAXSTR];
    int sc_level;
    unsigned int sc_time;
};

typedef struct sc_ent SCORE;

void	rd_score(SCORE *top_ten);
void	wr_score(SCORE *top_ten);
