/*
 * Functions for dealing with linked lists of goodies
 *
 * @(#)list.c	4.12 (Berkeley) 02/05/99
 *
 * Rogue: Exploring the Dungeons of Doom
 * Copyright (C) 1980-1983, 1985, 1999 Michael Toy, Ken Arnold and Glenn Wichman
 * All rights reserved.
 *
 * See the file LICENSE.TXT for full copyright and licensing information.
 */

#include <stdlib.h>
#include <curses.h>
#include "rogue.h"

#ifdef MASTER
int total = 0;			/* total dynamic memory bytes */
#endif

/*
 * detach:
 *	takes an item out of whatever linked list it might be in
 */

void
_detach(THING **list, THING *item)
{
    if (*list == item)
	*list = next(item);
    if (prev(item) != NULL)
	item->l_prev->l_next = next(item);
    if (next(item) != NULL)
	item->l_next->l_prev = prev(item);
    item->l_next = NULL;
    item->l_prev = NULL;
}

/*
 * _attach:
 *	add an item to the head of a list
 */

void
_attach(THING **list, THING *item)
{
    if (*list != NULL)
    {
	item->l_next = *list;
	(*list)->l_prev = item;
	item->l_prev = NULL;
    }
    else
    {
	item->l_next = NULL;
	item->l_prev = NULL;
    }
    *list = item;
}

/*
 * _free_list:
 *	Throw the whole blamed thing away
 */

void
_free_list(THING **ptr)
{
    THING *item;

    while (*ptr != NULL)
    {
	item = *ptr;
	*ptr = next(item);
	discard(item);
    }
}

/*
 * discard:
 *	Free up an item
 */

void
discard(THING *item)
{
#ifdef MASTER
    total--;
#endif
    free((char *) item);
}

/*
 * new_item
 *	Get a new item with a specified size
 */
THING *
new_item(void)
{
    THING *item;

#ifdef MASTER
    if ((item = (THING *)calloc(1, sizeof *item)) == NULL)
	msg(rs,"ran out of memory after %d items", total);
    else
	total++;
#else
    item = (THING *)calloc(1, sizeof *item);
#endif
    item->l_next = NULL;
    item->l_prev = NULL;
    return item;
}
