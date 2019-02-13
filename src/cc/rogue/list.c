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
#include <memory.h>
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

int32_t itemcounter;
THING *thingptrs[100000];
int32_t numptrs;

void
discard(THING *item)
{
#ifdef MASTER
    total--;
#endif
    if ( 0 )
    {
        int32_t i;
        for (i=0; i<numptrs; i++)
            if ( item == thingptrs[i] )
            {
                thingptrs[i] = thingptrs[--numptrs];
                thingptrs[numptrs] = 0;
                break;
            }
    }
    itemcounter--;
    free((char *) item);
}

void garbage_collect()
{
    return;
    int32_t i;
    fprintf(stderr,"numptrs.%d free them\n",numptrs);
    for (i=0; i<numptrs; i++)
    {
        //fprintf(stderr,"%p _t_type.%d otype.%d (%c)\n",thingptrs[i],thingptrs[i]->_t._t_type,thingptrs[i]->o_type,thingptrs[i]->o_type);
        free(thingptrs[i]);
    }
    memset(thingptrs,0,sizeof(thingptrs));
    numptrs = 0;
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
    if ( 0 )
    {
        thingptrs[numptrs++] = item;
        if ( (++itemcounter % 100) == 0 )
            fprintf(stderr,"itemcounter.%d\n",itemcounter);
    }
    item->l_next = NULL;
    item->l_prev = NULL;
    return item;
}
