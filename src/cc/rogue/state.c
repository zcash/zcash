/*
    state.c - Portable Rogue Save State Code

    Copyright (C) 1999, 2000, 2005 Nicholas J. Kisseberth
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions
    are met:
    1. Redistributions of source code must retain the above copyright
       notice, this list of conditions and the following disclaimer.
    2. Redistributions in binary form must reproduce the above copyright
       notice, this list of conditions and the following disclaimer in the
       documentation and/or other materials provided with the distribution.
    3. Neither the name(s) of the author(s) nor the names of other contributors
       may be used to endorse or promote products derived from this software
       without specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) AND CONTRIBUTORS ``AS IS'' AND
    ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
    IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
    ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR(S) OR CONTRIBUTORS BE LIABLE
    FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
    DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
    OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
    HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
    LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
    OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
    SUCH DAMAGE.
*/

//#include <stdlib.h>
//#include <string.h>
//#include <curses.h>
#include "rogue.h"

/************************************************************************/
/* Save State Code                                                      */
/************************************************************************/

#define RSID_STATS        0xABCD0001
#define RSID_THING        0xABCD0002
#define RSID_THING_NULL   0xDEAD0002
#define RSID_OBJECT       0xABCD0003
#define RSID_MAGICITEMS   0xABCD0004
#define RSID_KNOWS        0xABCD0005
#define RSID_GUESSES      0xABCD0006
#define RSID_OBJECTLIST   0xABCD0007
#define RSID_BAGOBJECT    0xABCD0008
#define RSID_MONSTERLIST  0xABCD0009
#define RSID_MONSTERSTATS 0xABCD000A
#define RSID_MONSTERS     0xABCD000B
#define RSID_TRAP         0xABCD000C
#define RSID_WINDOW       0xABCD000D
#define RSID_DAEMONS      0xABCD000E
#define RSID_IWEAPS       0xABCD000F
#define RSID_IARMOR       0xABCD0010
#define RSID_SPELLS       0xABCD0011
#define RSID_ILIST        0xABCD0012
#define RSID_HLIST        0xABCD0013
#define RSID_DEATHTYPE    0xABCD0014
#define RSID_CTYPES       0XABCD0015
#define RSID_COORDLIST    0XABCD0016
#define RSID_ROOMS        0XABCD0017

#define READSTAT (format_error || read_error )
#define WRITESTAT (write_error)

static int read_error   = FALSE;
static int write_error  = FALSE;
static int format_error = FALSE;
static int endian = 0x01020304;
#define  big_endian ( *((char *)&endian) == 0x01 )

int
rs_write(FILE *savef, void *ptr, size_t size)
{
    if (write_error)
        return(WRITESTAT);

    if (encwrite((char *)ptr, size, savef) != size)
        write_error = 1;

    return(WRITESTAT);
}

int
rs_read(FILE *inf, void *ptr, size_t size)
{
    if (read_error || format_error)
        return(READSTAT);

    if (encread((char *)ptr, size, inf) != size)
        read_error = 1;
       
    return(READSTAT);
}

int
rs_write_int(FILE *savef, int32_t c)
{
    unsigned char bytes[4];
    unsigned char *buf = (unsigned char *) &c;

    if (write_error)
        return(WRITESTAT);

    if (big_endian)
    {
        bytes[3] = buf[0];
        bytes[2] = buf[1];
        bytes[1] = buf[2];
        bytes[0] = buf[3];
        buf = bytes;
    }
    
    rs_write(savef, buf, 4);

    return(WRITESTAT);
}

int
rs_read_int(FILE *inf, int32_t *i)
{
    unsigned char bytes[4];
    int input = 0;
    unsigned char *buf = (unsigned char *)&input;
    
    if (read_error || format_error)
        return(READSTAT);

    rs_read(inf, &input, 4);

    if (big_endian)
    {
        bytes[3] = buf[0];
        bytes[2] = buf[1];
        bytes[1] = buf[2];
        bytes[0] = buf[3];
        buf = bytes;
    }
    
    *i = *((int *) buf);

    return(READSTAT);
}

int
rs_write_char(FILE *savef, char c)
{
    if (write_error)
        return(WRITESTAT);

    rs_write(savef, &c, 1);

    return(WRITESTAT);
}

int
rs_read_char(FILE *inf, char *c)
{
    if (read_error || format_error)
        return(READSTAT);

    rs_read(inf, c, 1);

    return(READSTAT);
}

int
rs_write_chars(FILE *savef, char *c, int count)
{
    if (write_error)
        return(WRITESTAT);

    rs_write_int(savef, count);
    rs_write(savef, c, count);

    return(WRITESTAT);
}

int
rs_read_chars(FILE *inf, char *i, int count)
{
    int value = 0;
    
    if (read_error || format_error)
        return(READSTAT);

    rs_read_int(inf, &value);
    
    if (value != count)
        format_error = TRUE;

    rs_read(inf, i, count);
    
    return(READSTAT);
}

int
rs_write_ints(FILE *savef, int32_t *c, int count)
{
    int n = 0;

    if (write_error)
        return(WRITESTAT);

    rs_write_int(savef, count);

    for(n = 0; n < count; n++)
        if( rs_write_int(savef,c[n]) != 0)
            break;

    return(WRITESTAT);
}

int
rs_read_ints(FILE *inf, int32_t *i, int count)
{
    int n, value;
    
    if (read_error || format_error)
        return(READSTAT);

    rs_read_int(inf,&value);

    if (value != count)
        format_error = TRUE;

    for(n = 0; n < count; n++)
        if (rs_read_int(inf, &i[n]) != 0)
            break;
    
    return(READSTAT);
}

int
rs_write_boolean(FILE *savef, int c)
{
    unsigned char buf = (c == 0) ? 0 : 1;
    
    if (write_error)
        return(WRITESTAT);

    rs_write(savef, &buf, 1);

    return(WRITESTAT);
}

int
rs_read_boolean(FILE *inf, bool *i)
{
    unsigned char buf = 0;
    
    if (read_error || format_error)
        return(READSTAT);

    rs_read(inf, &buf, 1);

    *i = (buf != 0);
    
    return(READSTAT);
}

int
rs_write_booleans(FILE *savef, bool *c, int count)
{
    int n = 0;

    if (write_error)
        return(WRITESTAT);

    rs_write_int(savef, count);

    for(n = 0; n < count; n++)
        if (rs_write_boolean(savef, c[n]) != 0)
            break;

    return(WRITESTAT);
}

int
rs_read_booleans(FILE *inf, bool *i, int count)
{
    int n = 0, value = 0;
    
    if (read_error || format_error)
        return(READSTAT);

    rs_read_int(inf,&value);

    if (value != count)
        format_error = TRUE;

    for(n = 0; n < count; n++)
        if (rs_read_boolean(inf, &i[n]) != 0)
            break;
    
    return(READSTAT);
}

int
rs_write_short(FILE *savef, int16_t c)
{
    unsigned char bytes[2];
    unsigned char *buf = (unsigned char *) &c;

    if (write_error)
        return(WRITESTAT);

    if (big_endian)
    {
        bytes[1] = buf[0];
        bytes[0] = buf[1];
        buf = bytes;
    }

    rs_write(savef, buf, 2);

    return(WRITESTAT);
}

int
rs_read_short(FILE *inf, int16_t *i)
{
    unsigned char bytes[2];
    short  input;
    unsigned char *buf = (unsigned char *)&input;
    
    if (read_error || format_error)
        return(READSTAT);

    rs_read(inf, &input, 2);

    if (big_endian)
    {
        bytes[1] = buf[0];
        bytes[0] = buf[1];
        buf = bytes;
    }
    
    *i = *((short *) buf);

    return(READSTAT);
} 

int
rs_write_shorts(FILE *savef, int16_t *c, int count)
{
    int n = 0;

    if (write_error)
        return(WRITESTAT);

    rs_write_int(savef, count);

    for(n = 0; n < count; n++)
        if (rs_write_short(savef, c[n]) != 0)
            break; 

    return(WRITESTAT);
}

int
rs_read_shorts(FILE *inf, int16_t *i, int count)
{
    int n = 0, value = 0;

    if (read_error || format_error)
        return(READSTAT);

    rs_read_int(inf,&value);

    if (value != count)
        format_error = TRUE;

    for(n = 0; n < value; n++)
        if (rs_read_short(inf, &i[n]) != 0)
            break;
    
    return(READSTAT);
}

int
rs_write_ushort(FILE *savef, uint16_t c)
{
    unsigned char bytes[2];
    unsigned char *buf = (unsigned char *) &c;

    if (write_error)
        return(WRITESTAT);

    if (big_endian)
    {
        bytes[1] = buf[0];
        bytes[0] = buf[1];
        buf = bytes;
    }

    rs_write(savef, buf, 2);

    return(WRITESTAT);
}

int
rs_read_ushort(FILE *inf, uint16_t *i)
{
    unsigned char bytes[2];
    unsigned short  input;
    unsigned char *buf = (unsigned char *)&input;
    
    if (read_error || format_error)
        return(READSTAT);

    rs_read(inf, &input, 2);

    if (big_endian)
    {
        bytes[1] = buf[0];
        bytes[0] = buf[1];
        buf = bytes;
    }
    
    *i = *((unsigned short *) buf);

    return(READSTAT);
} 

int
rs_write_uint(FILE *savef, uint32_t c)
{
    unsigned char bytes[4];
    unsigned char *buf = (unsigned char *) &c;

    if (write_error)
        return(WRITESTAT);

    if (big_endian)
    {
        bytes[3] = buf[0];
        bytes[2] = buf[1];
        bytes[1] = buf[2];
        bytes[0] = buf[3];
        buf = bytes;
    }
    
    rs_write(savef, buf, 4);

    return(WRITESTAT);
}

int
rs_read_uint(FILE *inf, uint32_t *i)
{
    unsigned char bytes[4];
    int  input;
    unsigned char *buf = (unsigned char *)&input;
    
    if (read_error || format_error)
        return(READSTAT);

    rs_read(inf, &input, 4);

    if (big_endian)
    {
        bytes[3] = buf[0];
        bytes[2] = buf[1];
        bytes[1] = buf[2];
        bytes[0] = buf[3];
        buf = bytes;
    }
    
    *i = *((unsigned int *) buf);

    return(READSTAT);
}

int
rs_write_marker(FILE *savef, int32_t id)
{
    if (write_error)
        return(WRITESTAT);

    rs_write_int(savef, id);

    return(WRITESTAT);
}

int 
rs_read_marker(FILE *inf, int32_t id)
{
    int nid;

    if (read_error || format_error)
        return(READSTAT);

    if (rs_read_int(inf, &nid) == 0)
        if (id != nid)
            format_error = 1;
    
    return(READSTAT);
}



/******************************************************************************/

int
rs_write_string(FILE *savef, char *s)
{
    int len = 0;

    if (write_error)
        return(WRITESTAT);

    len = (s == NULL) ? 0 : (int) strlen(s) + 1;

    rs_write_int(savef, len);
    rs_write_chars(savef, s, len);
            
    return(WRITESTAT);
}

int
rs_read_string(FILE *inf, char *s, int max)
{
    int len = 0;

    if (read_error || format_error)
        return(READSTAT);

    rs_read_int(inf, &len);

    if (len > max)
        format_error = TRUE;

    rs_read_chars(inf, s, len);
    
    return(READSTAT);
}

int
rs_read_new_string(FILE *inf, char **s)
{
    int len=0;
    char *buf=0;

    if (read_error || format_error)
        return(READSTAT);

    rs_read_int(inf, &len);

    if (len == 0)
        buf = NULL;
    else
    { 
        buf = (char *)malloc(len);

        if (buf == NULL)            
            read_error = TRUE;
    }

    rs_read_chars(inf, buf, len);

    *s = buf;

    return(READSTAT);
}

int
rs_write_strings(FILE *savef, char *s[], int count)
{
    int n = 0;

    if (write_error)
        return(WRITESTAT);

    rs_write_int(savef, count);

    for(n = 0; n < count; n++)
        if (rs_write_string(savef, s[n]) != 0)
            break;
    
    return(WRITESTAT);
}

int
rs_read_strings(FILE *inf, char **s, int count, int max)
{
    int n     = 0;
    int value = 0;
    
    if (read_error || format_error)
        return(READSTAT);

    rs_read_int(inf, &value);

    if (value != count)
        format_error = TRUE;

    for(n = 0; n < count; n++)
        if (rs_read_string(inf, s[n], max) != 0)
            break;
    
    return(READSTAT);
}

int
rs_read_new_strings(FILE *inf, char **s, int count)
{
    int n     = 0;
    int value = 0;
    
    if (read_error || format_error)
        return(READSTAT);

    rs_read_int(inf, &value);

    if (value != count)
        format_error = TRUE;

    for(n = 0; n < count; n++)
        if (rs_read_new_string(inf, &s[n]) != 0)
            break;
    
    return(READSTAT);
}

int
rs_write_string_index(FILE *savef, char *master[], int max, const char *str)
{
    int i;

    if (write_error)
        return(WRITESTAT);

    for(i = 0; i < max; i++)
        if (str == master[i])
            return( rs_write_int(savef, i) );

    return( rs_write_int(savef,-1) );
}

int
rs_read_string_index(FILE *inf, char *master[], int maxindex, char **str)
{
    int i;

    if (read_error || format_error)
        return(READSTAT);

    rs_read_int(inf, &i);

    if (i > maxindex)
        format_error = TRUE;
    else if (i >= 0)
        *str = master[i];
    else
        *str = NULL;

    return(READSTAT);
}

int
rs_write_str_t(FILE *savef, str_t st)
{
    if (write_error)
        return(WRITESTAT);

    rs_write_uint(savef, st);

    return( WRITESTAT );
}

int
rs_read_str_t(FILE *inf, str_t *st)
{
    if (read_error || format_error)
        return(READSTAT);

    rs_read_uint(inf, st);

    return(READSTAT);
}

int
rs_write_coord(FILE *savef, coord c)
{
    if (write_error)
        return(WRITESTAT);

    rs_write_int(savef, c.x);
    rs_write_int(savef, c.y);
    
    return(WRITESTAT);
}

int
rs_read_coord(FILE *inf, coord *c)
{
    coord in;

    if (read_error || format_error)
        return(READSTAT);

    rs_read_int(inf,&in.x);
    rs_read_int(inf,&in.y);

    if (READSTAT == 0) 
    {
        c->x = in.x;
        c->y = in.y;
    }

    return(READSTAT);
}

int
rs_write_window(FILE *savef, WINDOW *win)
{
    int row,col,height,width;

    if (write_error)
        return(WRITESTAT);

    width  = getmaxx(win);
    height = getmaxy(win);

    rs_write_marker(savef,RSID_WINDOW);
    rs_write_int(savef,height);
    rs_write_int(savef,width);

    for(row=0;row<height;row++)
        for(col=0;col<width;col++)
            if (rs_write_int(savef, mvwinch(win,row,col)) != 0)
                return(WRITESTAT);

    return(WRITESTAT);
}

int
rs_read_window(FILE *inf, WINDOW *win)
{
    int row,col,maxlines,maxcols,value,width,height;
    
    if (read_error || format_error)
        return(READSTAT);

    width  = getmaxx(win);
    height = getmaxy(win);

    rs_read_marker(inf, RSID_WINDOW);

    rs_read_int(inf, &maxlines);
    rs_read_int(inf, &maxcols);

    for(row = 0; row < maxlines; row++)
        for(col = 0; col < maxcols; col++)
        {
            if (rs_read_int(inf, &value) != 0)
                return(READSTAT);

            if ((row < height) && (col < width))
                mvwaddch(win,row,col,value);
        }
        
    return(READSTAT);
}

/******************************************************************************/

void *
get_list_item(THING *l, int i)
{
    int count;

    for(count = 0; l != NULL; count++, l = l->l_next)
        if (count == i)
            return(l);
    
    return(NULL);
}

int
find_list_ptr(THING *l, void *ptr)
{
    int count;

    for(count = 0; l != NULL; count++, l = l->l_next)
        if (l == ptr)
            return(count);
    
    return(-1);
}

int
list_size(THING *l)
{
    int count;
    
    for(count = 0; l != NULL; count++, l = l->l_next)
        ;
    
    return(count);
}

/******************************************************************************/

int
rs_write_stats(FILE *savef, struct stats *s)
{
    if (write_error)
        return(WRITESTAT);

    rs_write_marker(savef, RSID_STATS);
    rs_write_str_t(savef, s->s_str);
    rs_write_int(savef, s->s_exp);
    rs_write_int(savef, s->s_lvl);
    rs_write_int(savef, s->s_arm);
    rs_write_int(savef, s->s_hpt);
    rs_write_chars(savef, s->s_dmg, sizeof(s->s_dmg));
    rs_write_int(savef,s->s_maxhp);

    return(WRITESTAT);
}

int
rs_read_stats(FILE *inf, struct stats *s)
{
    if (read_error || format_error)
        return(READSTAT);

    rs_read_marker(inf, RSID_STATS);
    rs_read_str_t(inf,&s->s_str);
    rs_read_int(inf,&s->s_exp);
    rs_read_int(inf,&s->s_lvl);
    rs_read_int(inf,&s->s_arm);
    rs_read_int(inf,&s->s_hpt);
    rs_read_chars(inf,s->s_dmg,sizeof(s->s_dmg));
    rs_read_int(inf,&s->s_maxhp);

    return(READSTAT);
}

int
rs_write_stone_index(FILE *savef, STONE master[], int max, const char *str)
{
    int i;

    if (write_error)
        return(WRITESTAT);

    for(i = 0; i < max; i++)
        if (str == master[i].st_name)
        {
            rs_write_int(savef,i);
            return(WRITESTAT);
        }

    rs_write_int(savef,-1);

    return(WRITESTAT);
}

int
rs_read_stone_index(FILE *inf, STONE master[], int maxindex, char **str)
{
    int i = 0;

    if (read_error || format_error)
        return(READSTAT);

    rs_read_int(inf,&i);

    if (i > maxindex)
        format_error = TRUE;
    else if (i >= 0)
        *str = master[i].st_name;
    else
        *str = NULL;

    return(READSTAT);
}

int
rs_write_scrolls(FILE *savef)
{
    int i;

    if (write_error)
        return(WRITESTAT);

    for(i = 0; i < MAXSCROLLS; i++)
        rs_write_string(savef, (char *)s_names[i]);

    return(READSTAT);
}

int
rs_read_scrolls(FILE *inf)
{
    int i;

    if (read_error || format_error)
        return(READSTAT);

    for(i = 0; i < MAXSCROLLS; i++)
        rs_read_new_string(inf, (char **)&s_names[i]);

    return(READSTAT);
}

int
rs_write_potions(FILE *savef)
{
    int i;

    if (write_error)
        return(WRITESTAT);

    for(i = 0; i < MAXPOTIONS; i++)
        rs_write_string_index(savef, (char **)rainbow, cNCOLORS, (char *)p_colors[i]);

    return(WRITESTAT);
}

int
rs_read_potions(FILE *inf)
{
    int i;

    if (read_error || format_error)
        return(READSTAT);

    for(i = 0; i < MAXPOTIONS; i++)
        rs_read_string_index(inf, (char **)rainbow, cNCOLORS, (char **)&p_colors[i]);

    return(READSTAT);
}

int
rs_write_rings(FILE *savef)
{
    int i;

    if (write_error)
        return(WRITESTAT);

    for(i = 0; i < MAXRINGS; i++)
        rs_write_stone_index(savef, (STONE *)stones, cNSTONES, (char *)r_stones[i]);

    return(WRITESTAT);
}

int
rs_read_rings(FILE *inf)
{
    int i;

    if (read_error || format_error)
        return(READSTAT);

    for(i = 0; i < MAXRINGS; i++)
        rs_read_stone_index(inf, (STONE *)stones, cNSTONES, (char **)&r_stones[i]);

    return(READSTAT);
}

int
rs_write_sticks(FILE *savef)
{
    int i;

    if (write_error)
        return(WRITESTAT);

    for (i = 0; i < MAXSTICKS; i++)
    {
        if (strcmp(ws_type[i],"staff") == 0)
        {
            rs_write_int(savef,0);
            rs_write_string_index(savef, (char **)wood, cNWOOD, (char *)ws_made[i]);
        }
        else
        {
            rs_write_int(savef,1);
            rs_write_string_index(savef, (char **)metal, cNMETAL, (char *)ws_made[i]);
        }
    }
 
    return(WRITESTAT);
}
        
int
rs_read_sticks(FILE *inf)
{
    int i = 0, list = 0;

    if (read_error || format_error)
        return(READSTAT);

    for(i = 0; i < MAXSTICKS; i++)
    { 
        rs_read_int(inf,&list);

        if (list == 0)
        {
            rs_read_string_index(inf, (char **)wood, cNWOOD, (char **)&ws_made[i]);
            ws_type[i] = "staff";
        }
        else 
        {
            rs_read_string_index(inf, (char **)metal, cNMETAL, (char **)&ws_made[i]);
            ws_type[i] = "wand";
        }
    }

    return(READSTAT);
}

int
rs_write_daemons(FILE *savef, struct delayed_action *d_list, int count)
{
    int i = 0;
    int func = 0;
        
    if (write_error)
        return(WRITESTAT);

    rs_write_marker(savef, RSID_DAEMONS);
    rs_write_int(savef, count);
        
    for(i = 0; i < count; i++)
    {
        if (d_list[i].d_func == rollwand)
            func = 1;
        else if (d_list[i].d_func == doctor)
            func = 2;
        else if (d_list[i].d_func == stomach)
            func = 3;
        else if (d_list[i].d_func == runners)
            func = 4;
        else if (d_list[i].d_func == swander)
            func = 5;
        else if (d_list[i].d_func == nohaste)
            func = 6;
        else if (d_list[i].d_func == unconfuse)
            func = 7;
        else if (d_list[i].d_func == unsee)
            func = 8;
        else if (d_list[i].d_func == sight)
            func = 9;
        else if (d_list[i].d_func == NULL)
            func = 0;
        else
            func = -1;

        rs_write_int(savef, d_list[i].d_type);
        rs_write_int(savef, func);
        rs_write_int(savef, d_list[i].d_arg);
        rs_write_int(savef, d_list[i].d_time);
    }
    
    return(WRITESTAT);
}       

int
rs_read_daemons(FILE *inf, struct delayed_action *d_list, int count)
{
    int i = 0;
    int func = 0;
    int value = 0;
    
    if (read_error || format_error)
        return(READSTAT);

    rs_read_marker(inf, RSID_DAEMONS);
    rs_read_int(inf, &value);

    if (value > count)
        format_error = TRUE;

    for(i=0; i < count; i++)
    {
        func = 0;
        rs_read_int(inf, &d_list[i].d_type);
        rs_read_int(inf, &func);
        rs_read_int(inf, &d_list[i].d_arg);
        rs_read_int(inf, &d_list[i].d_time);
                    
        switch(func)
        {
            case 1: d_list[i].d_func = rollwand;
                    break;
            case 2: d_list[i].d_func = doctor;
                    break;
            case 3: d_list[i].d_func = stomach;
                    break;
            case 4: d_list[i].d_func = runners;
                    break;
            case 5: d_list[i].d_func = swander;
                    break;
            case 6: d_list[i].d_func = nohaste;
                    break;
            case 7: d_list[i].d_func = unconfuse;
                    break;
            case 8: d_list[i].d_func = unsee;
                    break;
            case 9: d_list[i].d_func = sight;
                    break;
            default:d_list[i].d_func = NULL;
                    break;
        }
    }

    if (d_list[i].d_func == NULL)
    {
        d_list[i].d_type = 0;
        d_list[i].d_arg = 0;
        d_list[i].d_time = 0;
    }
    
    return(READSTAT);
}       
        
int
rs_write_obj_info(FILE *savef, struct obj_info *i, int count)
{
    int n;
    
    if (write_error)
        return(WRITESTAT);

    rs_write_marker(savef, RSID_MAGICITEMS);
    rs_write_int(savef, count);

    for(n = 0; n < count; n++)
    {
        /* mi_name is constant, defined at compile time in all cases */
        rs_write_int(savef,i[n].oi_prob);
        rs_write_int(savef,i[n].oi_worth);
        rs_write_string(savef,i[n].oi_guess);
        rs_write_boolean(savef,i[n].oi_know);
    }
    
    return(WRITESTAT);
}

int
rs_read_obj_info(FILE *inf, struct obj_info *mi, int count)
{
    int n;
    int value;

    if (read_error || format_error)
        return(READSTAT);

    rs_read_marker(inf, RSID_MAGICITEMS);

    rs_read_int(inf, &value);

    if (value > count)
        format_error = TRUE;

    for(n = 0; n < value; n++)
    {
        /* mi_name is const, defined at compile time in all cases */
        rs_read_int(inf,&mi[n].oi_prob);
        rs_read_int(inf,&mi[n].oi_worth);
        rs_read_new_string(inf,&mi[n].oi_guess);
        rs_read_boolean(inf,&mi[n].oi_know);
    }
    
    return(READSTAT);
}

int
rs_write_room(FILE *savef, struct room *r)
{
    if (write_error)
        return(WRITESTAT);

    rs_write_coord(savef, r->r_pos);
    rs_write_coord(savef, r->r_max);
    rs_write_coord(savef, r->r_gold);
    rs_write_int(savef,   r->r_goldval);
    rs_write_short(savef, r->r_flags);
    rs_write_int(savef, r->r_nexits);
    rs_write_coord(savef, r->r_exit[0]);
    rs_write_coord(savef, r->r_exit[1]);
    rs_write_coord(savef, r->r_exit[2]);
    rs_write_coord(savef, r->r_exit[3]);
    rs_write_coord(savef, r->r_exit[4]);
    rs_write_coord(savef, r->r_exit[5]);
    rs_write_coord(savef, r->r_exit[6]);
    rs_write_coord(savef, r->r_exit[7]);
    rs_write_coord(savef, r->r_exit[8]);
    rs_write_coord(savef, r->r_exit[9]);
    rs_write_coord(savef, r->r_exit[10]);
    rs_write_coord(savef, r->r_exit[11]);
    
    return(WRITESTAT);
}

int
rs_read_room(FILE *inf, struct room *r)
{
    if (read_error || format_error)
        return(READSTAT);

    rs_read_coord(inf,&r->r_pos);
    rs_read_coord(inf,&r->r_max);
    rs_read_coord(inf,&r->r_gold);
    rs_read_int(inf,&r->r_goldval);
    rs_read_short(inf,&r->r_flags);
    rs_read_int(inf,&r->r_nexits);
    rs_read_coord(inf,&r->r_exit[0]);
    rs_read_coord(inf,&r->r_exit[1]);
    rs_read_coord(inf,&r->r_exit[2]);
    rs_read_coord(inf,&r->r_exit[3]);
    rs_read_coord(inf,&r->r_exit[4]);
    rs_read_coord(inf,&r->r_exit[5]);
    rs_read_coord(inf,&r->r_exit[6]);
    rs_read_coord(inf,&r->r_exit[7]);
    rs_read_coord(inf,&r->r_exit[8]);
    rs_read_coord(inf,&r->r_exit[9]);
    rs_read_coord(inf,&r->r_exit[10]);
    rs_read_coord(inf,&r->r_exit[11]);

    return(READSTAT);
}

int
rs_write_rooms(FILE *savef, struct room r[], int count)
{
    int n = 0;

    if (write_error)
        return(WRITESTAT);
    //fprintf(stderr,"rooms %ld -> ",ftell(savef));
    rs_write_int(savef, count);
    
    for(n = 0; n < count; n++)
        rs_write_room(savef, &r[n]);
    //fprintf(stderr,"%ld\n",ftell(savef));
 
    return(WRITESTAT);
}

int
rs_read_rooms(FILE *inf, struct room *r, int count)
{
    int value = 0, n = 0;

    if (read_error || format_error)
        return(READSTAT);

    rs_read_int(inf,&value);

    if (value > count)
        format_error = TRUE;

    for(n = 0; n < value; n++)
        rs_read_room(inf,&r[n]);

    return(READSTAT);
}

int
rs_write_room_reference(FILE *savef, struct room *rp)
{
    int i, room = -1;
    
    if (write_error)
        return(WRITESTAT);

    for (i = 0; i < MAXROOMS; i++)
        if (&rooms[i] == rp)
            room = i;

    rs_write_int(savef, room);

    return(WRITESTAT);
}

int
rs_read_room_reference(FILE *inf, struct room **rp)
{
    int i;
    
    if (read_error || format_error)
        return(READSTAT);

    rs_read_int(inf, &i);

    *rp = &rooms[i];
            
    return(READSTAT);
}

int
rs_write_monsters(FILE *savef, struct monster *m, int count)
{
    int n;
    
    if (write_error)
        return(WRITESTAT);

    rs_write_marker(savef, RSID_MONSTERS);
    rs_write_int(savef, count);

    for(n=0;n<count;n++)
        rs_write_stats(savef, &m[n].m_stats);
    
    return(WRITESTAT);
}

int
rs_read_monsters(FILE *inf, struct monster *m, int count)
{
    int value = 0, n = 0;

    if (read_error || format_error)
        return(READSTAT);

    rs_read_marker(inf, RSID_MONSTERS);

    rs_read_int(inf, &value);

    if (value != count)
        format_error = TRUE;

    for(n = 0; n < count; n++)
        rs_read_stats(inf, &m[n].m_stats);
    
    return(READSTAT);
}

void rogue_restoreobject(THING *o,struct rogue_packitem *item)
{
    int32_t i;
    if ( item->type != AMULET )
    {
        o->_o._o_type = item->type;
        o->_o._o_launch = item->launch;
        memcpy(o->_o._o_damage,item->damage,sizeof(item->damage));
        memcpy(o->_o._o_hurldmg,item->hurldmg,sizeof(item->hurldmg));
        o->_o._o_count = item->count;
        o->_o._o_which = item->which;
        o->_o._o_hplus = item->hplus;
        o->_o._o_dplus = item->dplus;
        o->_o._o_arm = item->arm;
        o->_o._o_flags = item->flags;
        o->_o._o_group = item->group;
        o->o_flags |= ISKNOW;
        o->o_flags &= ~ISFOUND;
        switch ( item->type )
        {
            case SCROLL:
                if ( item->which < MAXSCROLLS )
                    scr_info[item->which].oi_know = TRUE;
                break;
            case POTION:
                if ( item->which < MAXPOTIONS )
                    pot_info[item->which].oi_know = TRUE;
                break;
            case RING:
                if ( item->which < MAXRINGS )
                    ring_info[item->which].oi_know = TRUE;
                break;
            case STICK:
                if ( item->which < MAXSTICKS )
                    ws_info[item->which].oi_know = TRUE;
                break;
            // cur_armor and cur_weapon should be set
        }
        //char packitemstr[256];
        //strcpy(packitemstr,inv_name(o,FALSE));
        //fprintf(stderr,"packitem.(%s)\n",packitemstr);
    }
}

void rogue_packitemstr(char *packitemstr,struct rogue_packitem *item)
{
    static int32_t didinit; int32_t i;
    if ( didinit == 0 )
    {
        struct rogue_state R; char keystrokes[3];
        memset(&R,0,sizeof(R));
        keystrokes[0] = 'Q';
        keystrokes[1] = 'y';
        keystrokes[2] = 0;
        R.keystrokes = keystrokes;
        R.numkeys = 2;
        rogueiterate(&R);
        didinit = 1;
    }
    THING *obj = new_item();
    rogue_restoreobject(obj,item);
    strcpy(packitemstr,inv_name(obj,FALSE));
    //fprintf(stderr,"packitem.(%s)\n",packitemstr);
    free(obj);
}

int32_t packsave(struct rogue_packitem *item,int32_t type,int32_t launch,char *damage,int32_t damagesize,char *hurldmg,int32_t hurlsize,int32_t count,int32_t which,int32_t hplus,int32_t dplus,int32_t arm,int32_t flags,int32_t group)
{
    if ( damagesize != 8 || hurlsize != 8 )
    {
        fprintf(stderr,"unexpected damagesize.%d or hurlsize.%d != 8\n",damagesize,hurlsize);
        return(-1);
    }
    item->type = type;
    item->launch = launch;
    memcpy(item->damage,damage,damagesize);
    memcpy(item->hurldmg,hurldmg,hurlsize);
    item->count = count;
    item->which = which;
    item->hplus = hplus;
    item->dplus = dplus;
    item->arm = arm;
    item->flags = flags;
    item->group = group;
    return(0);
}

int
rs_write_object(struct rogue_state *rs,FILE *savef, THING *o)
{
    struct rogue_packitem *item;
    if (write_error)
        return(WRITESTAT);
    if ( thing_find(o) < 0 )
    {
        fprintf(stderr,"cant find thing.%p (%s) in list\n",o,inv_name(o,FALSE)); //sleep(3);
        return(0);
    }
    if ( o->_o._o_packch != 0 )
    {
        item = &rs->P.roguepack[rs->P.packsize];
        if ( 1 && pstats.s_hpt <= 0 )
        {
            //fprintf(stderr,"KILLED\n");
            rs->P.gold = -1;
            rs->P.hitpoints = -1;
            rs->P.strength = 0;
            rs->P.level = -1;
            rs->P.experience = -1;
            rs->P.dungeonlevel = -1;
        }
        else
        {
            if ( rs->P.packsize == 0 )
            {
                rs->P.gold = purse;
                rs->P.hitpoints = max_hp;
                rs->P.strength = (pstats.s_str & 0xffff) | (max_stats.s_str << 16);
                rs->P.level = pstats.s_lvl;
                rs->P.experience = pstats.s_exp;
                rs->P.dungeonlevel = level;
                rs->P.amulet = amulet;
                //fprintf(stderr,"%ld gold.%d hp.%d strength.%d/%d level.%d exp.%d %d\n",ftell(savef),purse,max_hp,pstats.s_str,max_stats.s_str,pstats.s_lvl,pstats.s_exp,level);
            }
            //fprintf(stderr,"object (%s) x.%d y.%d type.%d pack.(%c:%d)\n",inv_name(o,FALSE),o->_o._o_pos.x,o->_o._o_pos.y,o->_o._o_type,o->_o._o_packch,o->_o._o_packch);
            if ( rs->P.packsize < MAXPACK && o->o_type != AMULET )
            {
                packsave(item,o->_o._o_type,o->_o._o_launch,o->_o._o_damage,sizeof(o->_o._o_damage),o->_o._o_hurldmg,sizeof(o->_o._o_hurldmg),o->_o._o_count,o->_o._o_which,o->_o._o_hplus,o->_o._o_dplus,o->_o._o_arm,o->_o._o_flags,o->_o._o_group);
                rs->P.packsize++;
            }
        }
    }
    rs_write_marker(savef, RSID_OBJECT);
    rs_write_int(savef, o->_o._o_type); 
    rs_write_coord(savef, o->_o._o_pos); 
    rs_write_int(savef, o->_o._o_launch);
    rs_write_char(savef, o->_o._o_packch);
    rs_write_chars(savef, o->_o._o_damage, sizeof(o->_o._o_damage));
    rs_write_chars(savef, o->_o._o_hurldmg, sizeof(o->_o._o_hurldmg));
    rs_write_int(savef, o->_o._o_count);
    rs_write_int(savef, o->_o._o_which);
    rs_write_int(savef, o->_o._o_hplus);
    rs_write_int(savef, o->_o._o_dplus);
    rs_write_int(savef, o->_o._o_arm);
    rs_write_int(savef, o->_o._o_flags);
    rs_write_int(savef, o->_o._o_group);
    rs_write_string(savef, o->_o._o_label);
    return(WRITESTAT);
}

int
rs_read_object(FILE *inf, THING *o)
{
    if (read_error || format_error)
        return(READSTAT);

    rs_read_marker(inf, RSID_OBJECT);
    rs_read_int(inf, &o->_o._o_type);
    rs_read_coord(inf, &o->_o._o_pos);
    rs_read_int(inf, &o->_o._o_launch);
    rs_read_char(inf, &o->_o._o_packch);
    rs_read_chars(inf, o->_o._o_damage, sizeof(o->_o._o_damage));
    rs_read_chars(inf, o->_o._o_hurldmg, sizeof(o->_o._o_hurldmg));
    rs_read_int(inf, &o->_o._o_count);
    rs_read_int(inf, &o->_o._o_which);
    rs_read_int(inf, &o->_o._o_hplus);
    rs_read_int(inf, &o->_o._o_dplus);
    rs_read_int(inf, &o->_o._o_arm);
    rs_read_int(inf, &o->_o._o_flags);
    rs_read_int(inf, &o->_o._o_group);
    rs_read_new_string(inf, &o->_o._o_label);
    
    return(READSTAT);
}

int
rs_write_object_list(struct rogue_state *rs,FILE *savef, THING *l)
{
    if (write_error)
        return(WRITESTAT);
    //fprintf(stderr,"list %ld -> ",ftell(savef));

    rs_write_marker(savef, RSID_OBJECTLIST);
    rs_write_int(savef, list_size(l));

    for( ;l != NULL; l = l->l_next)
        rs_write_object(rs,savef, l);
    //fprintf(stderr,"%ld\n",ftell(savef));

    return(WRITESTAT);
}

int
rs_read_object_list(FILE *inf, THING **list)
{
    int i, cnt;
    THING *l = NULL, *previous = NULL, *head = NULL;

    if (read_error || format_error)
        return(READSTAT);

    rs_read_marker(inf, RSID_OBJECTLIST);
    rs_read_int(inf, &cnt);

    for (i = 0; i < cnt; i++) 
    {
        l = new_item();//,sizeof(THING));

        memset(l,0,sizeof(THING));

        l->l_prev = previous;

        if (previous != NULL)
            previous->l_next = l;

        rs_read_object(inf,l);

        if (previous == NULL)
            head = l;

        previous = l;
    }
            
    if (l != NULL)
        l->l_next = NULL;
    
    *list = head;

    return(READSTAT);
}

int
rs_write_object_reference(FILE *savef, THING *list, THING *item)
{
    int i;
    
    if (write_error)
        return(WRITESTAT);

    i = find_list_ptr(list, item);

    rs_write_int(savef, i);

    return(WRITESTAT);
}

int
rs_read_object_reference(FILE *inf, THING *list, THING **item)
{
    int i;
    
    if (read_error || format_error)
        return(READSTAT);

    rs_read_int(inf, &i);

    *item = (THING *)get_list_item(list,i);
            
    return(READSTAT);
}

int
find_room_coord(struct room *rmlist, coord *c, int n)
{
    int i = 0;
    
    for(i = 0; i < n; i++)
        if(&rmlist[i].r_gold == c)
            return(i);
    
    return(-1);
}

int
find_thing_coord(THING *monlist, coord *c)
{
    THING *mitem;
    THING *tp;
    int i = 0;

    for(mitem = monlist; mitem != NULL; mitem = mitem->l_next)
    {
        tp = mitem;

        if (c == &tp->t_pos)
            return(i);

        i++;
    }

    return(-1);
}

int
find_object_coord(THING *objlist, coord *c)
{
    THING *oitem;
    THING *obj;
    int i = 0;

    for(oitem = objlist; oitem != NULL; oitem = oitem->l_next)
    {
        obj = oitem;

        if (c == &obj->o_pos)
            return(i);

        i++;
    }

    return(-1);
}

int
rs_write_thing(struct rogue_state *rs,FILE *savef, THING *t)
{
    int i = -1;
    
    if (write_error)
        return(WRITESTAT);
    //fprintf(stderr,"thing %ld -> ",ftell(savef));

    rs_write_marker(savef, RSID_THING);

    if (t == NULL)
    {
        rs_write_int(savef, 0);
        return(WRITESTAT);
    }
    
    rs_write_int(savef, 1);
    rs_write_coord(savef, t->_t._t_pos);
    rs_write_boolean(savef, t->_t._t_turn);
    rs_write_char(savef, t->_t._t_type);
    rs_write_char(savef, t->_t._t_disguise);
    rs_write_char(savef, t->_t._t_oldch);

    /* 
        t_dest can be:
        0,0: NULL
        0,1: location of hero
        1,i: location of a thing (monster)
        2,i: location of an object
        3,i: location of gold in a room

        We need to remember what we are chasing rather than 
        the current location of what we are chasing.
    */

    if (t->t_dest == &hero)
    {
        rs_write_int(savef,0);
        rs_write_int(savef,1);
    }
    else if (t->t_dest != NULL)
    {
        i = find_thing_coord(mlist, t->t_dest);
            
        if (i >=0 )
        {
            rs_write_int(savef,1);
            rs_write_int(savef,i);
        }
        else
        {
            i = find_object_coord(lvl_obj, t->t_dest);
            
            if (i >= 0)
            {
                rs_write_int(savef,2);
                rs_write_int(savef,i);
            }
            else
            {
                i = find_room_coord(rooms, t->t_dest, MAXROOMS);
        
                if (i >= 0) 
                {
                    rs_write_int(savef,3);
                    rs_write_int(savef,i);
                }
                else 
                {
                    rs_write_int(savef, 0);
                    rs_write_int(savef,1); /* chase the hero anyway */
                }
            }
        }
    }
    else
    {
        rs_write_int(savef,0);
        rs_write_int(savef,0);
    }
    
    rs_write_short(savef, t->_t._t_flags);
    rs_write_stats(savef, &t->_t._t_stats);
    rs_write_room_reference(savef, t->_t._t_room);
    rs_write_object_list(rs,savef, t->_t._t_pack);
    //fprintf(stderr,"%ld\n",ftell(savef));

    return(WRITESTAT);
}

int
rs_read_thing(FILE *inf, THING *t)
{
    int listid = 0, index = -1;
    THING *item;

    if (read_error || format_error)
        return(READSTAT);

    rs_read_marker(inf, RSID_THING);

    rs_read_int(inf, &index);

    if (index == 0)
        return(READSTAT);

    rs_read_coord(inf,&t->_t._t_pos);
    rs_read_boolean(inf,&t->_t._t_turn);
    rs_read_char(inf,&t->_t._t_type);
    rs_read_char(inf,&t->_t._t_disguise);
    rs_read_char(inf,&t->_t._t_oldch);
            
    /* 
        t_dest can be (listid,index):
        0,0: NULL
        0,1: location of hero
        1,i: location of a thing (monster)
        2,i: location of an object
        3,i: location of gold in a room

        We need to remember what we are chasing rather than 
        the current location of what we are chasing.
    */
            
    rs_read_int(inf, &listid);
    rs_read_int(inf, &index);
    t->_t._t_reserved = -1;

    if (listid == 0) /* hero or NULL */
    {
        if (index == 1)
            t->_t._t_dest = &hero;
        else
            t->_t._t_dest = NULL;
    }
    else if (listid == 1) /* monster/thing */
    {
        t->_t._t_dest     = NULL;
        t->_t._t_reserved = index;
    }
    else if (listid == 2) /* object */
    {
        THING *obj;

        item = (THING *)get_list_item(lvl_obj, index);

        if (item != NULL)
        {
            obj = item;
            t->_t._t_dest = &obj->o_pos;
        }
    }
    else if (listid == 3) /* gold */
    {
        t->_t._t_dest = &rooms[index].r_gold;
    }
    else
        t->_t._t_dest = NULL;
            
    rs_read_short(inf,&t->_t._t_flags);
    rs_read_stats(inf,&t->_t._t_stats);
    rs_read_room_reference(inf, &t->_t._t_room);
    rs_read_object_list(inf,&t->_t._t_pack);
    
    return(READSTAT);
}

void
rs_fix_thing(THING *t)
{
    THING *item;
    THING *tp;

    if (t->t_reserved < 0)
        return;

    item = (THING *)get_list_item(mlist,t->t_reserved);

    if (item != NULL)
    {
        tp = item;
        t->t_dest = &tp->t_pos;
    }
}

int
rs_write_thing_list(struct rogue_state *rs,FILE *savef, THING *l)
{
    int cnt = 0;
    
    if (write_error)
        return(WRITESTAT);

    rs_write_marker(savef, RSID_MONSTERLIST);

    cnt = list_size(l);

    rs_write_int(savef, cnt);

    if (cnt < 1)
        return(WRITESTAT);

    while (l != NULL) {
        rs_write_thing(rs,savef, l);
        l = l->l_next;
    }
    
    return(WRITESTAT);
}

int
rs_read_thing_list(FILE *inf, THING **list)
{
    int i, cnt;
    THING *l = NULL, *previous = NULL, *head = NULL;

    if (read_error || format_error)
        return(READSTAT);

    rs_read_marker(inf, RSID_MONSTERLIST);

    rs_read_int(inf, &cnt);

    for (i = 0; i < cnt; i++) 
    {
        l = new_item();

        l->l_prev = previous;
            
        if (previous != NULL)
            previous->l_next = l;

        rs_read_thing(inf,l);

        if (previous == NULL)
            head = l;

        previous = l;
    }
        
    if (l != NULL)
        l->l_next = NULL;

    *list = head;
    
    return(READSTAT);
}

void
rs_fix_thing_list(THING *list)
{
    THING *item;

    for(item = list; item != NULL; item = item->l_next)
        rs_fix_thing(item);
}

int
rs_write_thing_reference(FILE *savef, THING *list, THING *item)
{
    int i;

    if (write_error)
        return(WRITESTAT);

    if (item == NULL)
        rs_write_int(savef,-1);
    else
    {
        i = find_list_ptr(list, item);

        rs_write_int(savef, i);
    }

    return(WRITESTAT);
}

int
rs_read_thing_reference(FILE *inf, THING *list, THING **item)
{
    int i;
    
    if (read_error || format_error)
        return(READSTAT);

    rs_read_int(inf, &i);

    if (i == -1)
        *item = NULL;
    else
        *item = (THING *)get_list_item(list,i);

    return(READSTAT);
}

int
rs_write_thing_references(FILE *savef, THING *list, THING *items[], int count)
{
    int i;

    if (write_error)
        return(WRITESTAT);

    for(i = 0; i < count; i++)
        rs_write_thing_reference(savef,list,items[i]);

    return(WRITESTAT);
}

int
rs_read_thing_references(FILE *inf, THING *list, THING *items[], int count)
{
    int i;

    if (read_error || format_error)
        return(READSTAT);

    for(i = 0; i < count; i++)
        rs_read_thing_reference(inf,list,&items[i]);

    return(WRITESTAT);
}

int 
rs_write_places(FILE *savef, PLACE *places, int count)
{
    int i = 0;
    //fprintf(stderr,"places %ld -> ",ftell(savef));

    if (write_error)
        return(WRITESTAT);

    for(i = 0; i < count; i++) 
    {
        rs_write_char(savef, places[i].p_ch);
        rs_write_char(savef, places[i].p_flags);
        rs_write_thing_reference(savef, mlist, places[i].p_monst);
    }
    //fprintf(stderr,"%ld\n",ftell(savef));

    return(WRITESTAT);
}

int 
rs_read_places(FILE *inf, PLACE *places, int count)
{
    int i = 0;
    
    if (read_error || format_error)
        return(READSTAT);

    for(i = 0; i < count; i++) 
    {
        rs_read_char(inf,&places[i].p_ch);
        rs_read_char(inf,&places[i].p_flags);
        rs_read_thing_reference(inf, mlist, &places[i].p_monst);
    }

    return(READSTAT);
}

int
rs_save_file(struct rogue_state *rs,FILE *savef)
{
    if (write_error)
    {
        fprintf(stderr,"write error\n");
        return(WRITESTAT);
    }

    rs_write_boolean(savef, after);                 /* 1  */    /* extern.c */
    rs_write_boolean(savef, again);                 /* 2  */
    rs_write_int(savef, noscore);	                /* 3  */
    rs_write_boolean(savef, seenstairs);            /* 4  */
    rs_write_boolean(savef, amulet);                /* 5  */
    rs_write_boolean(savef, door_stop);             /* 6  */
    rs_write_boolean(savef, fight_flush);           /* 7  */
    rs_write_boolean(savef, firstmove);             /* 8  */
    rs_write_boolean(savef, got_ltc);               /* 9  */
    rs_write_boolean(savef, has_hit);               /* 10 */
    rs_write_boolean(savef, in_shell);              /* 11 */
    rs_write_boolean(savef, inv_describe);          /* 12 */
    rs_write_boolean(savef, jump);                  /* 13 */
    rs_write_boolean(savef, kamikaze);              /* 14 */
    rs_write_boolean(savef, lower_msg);             /* 15 */
    rs_write_boolean(savef, move_on);               /* 16 */
    rs_write_boolean(savef, msg_esc);               /* 17 */
    rs_write_boolean(savef, passgo);                /* 18 */
    rs_write_boolean(savef, playing);               /* 19 */
    rs_write_boolean(savef, q_comm);                /* 20 */
    rs_write_boolean(savef, running);               /* 21 */
    rs_write_boolean(savef, save_msg);              /* 22 */
    rs_write_boolean(savef, see_floor);             /* 23 */
    rs_write_boolean(savef, stat_msg);              /* 24 */
    rs_write_boolean(savef, terse);                 /* 25 */
    rs_write_boolean(savef, to_death);              /* 26 */
    rs_write_boolean(savef, tombstone);             /* 27 */
#ifdef MASTER
    rs_write_int(savef, wizard);                    /* 28 */
#else
    rs_write_int(savef, 0);                         /* 28 */
#endif
    rs_write_booleans(savef, pack_used, 26);        /* 29 */
    rs_write_char(savef, dir_ch);
    //rs_write_chars(savef, file_name, MAXSTR);
    //rs_write_chars(savef, huh, MAXSTR);
    rs_write_potions(savef);
    //rs_write_chars(savef,prbuf,2*MAXSTR);
    rs_write_rings(savef);
    rs_write_string(savef,release);
    rs_write_char(savef, runch);
    rs_write_scrolls(savef);
    rs_write_char(savef, take);
    //rs_write_chars(savef, whoami, MAXSTR);
    rs_write_sticks(savef);
    rs_write_int(savef,orig_dsusp);
    rs_write_chars(savef, fruit, MAXSTR);
    //rs_write_chars(savef, home, MAXSTR);
    rs_write_strings(savef,(char **)inv_t_name,3);
    rs_write_char(savef,l_last_comm);
    rs_write_char(savef,l_last_dir);
    rs_write_char(savef,last_comm);
    rs_write_char(savef,last_dir);
    rs_write_strings(savef,(char **)tr_name,8);
    rs_write_int(savef,n_objs);
    rs_write_int(savef, ntraps);
    rs_write_int(savef, hungry_state);
    rs_write_int(savef, inpack);
    rs_write_int(savef, inv_type);
    rs_write_int(savef, level);
    rs_write_int(savef, max_level);
    rs_write_int(savef, mpos);
    rs_write_int(savef, no_food);
    rs_write_ints(savef,(int32_t *)a_class,MAXARMORS);
    rs_write_int(savef, count);
    rs_write_int(savef, food_left);
    rs_write_int(savef, lastscore);
    rs_write_int(savef, no_command);
    rs_write_int(savef, no_move);
    rs_write_int(savef, purse);
    rs_write_int(savef, quiet);
    rs_write_int(savef, vf_hit);
    //rs_write_int(savef, dnum);
    rs_write_int(savef, (int32_t)(seed&0xffffffff));
    rs_write_int(savef, (int32_t)((seed>>32)&0xffffffff));
    rs_write_ints(savef, (int32_t *)e_levels, 21);
    rs_write_coord(savef, delta);
    rs_write_coord(savef, oldpos);
    rs_write_coord(savef, stairs);
    rs_write_thing(rs,savef, &player);
    rs_write_object_reference(savef, player.t_pack, cur_armor);
    rs_write_object_reference(savef, player.t_pack, cur_ring[0]);
    rs_write_object_reference(savef, player.t_pack, cur_ring[1]); 
    rs_write_object_reference(savef, player.t_pack, cur_weapon); 
    rs_write_object_reference(savef, player.t_pack, l_last_pick); 
    rs_write_object_reference(savef, player.t_pack, last_pick); 

    rs_write_object_list(rs,savef, lvl_obj);
    rs_write_thing_list(rs,savef, mlist);

    rs_write_places(savef,places,MAXLINES*MAXCOLS);
    
    rs_write_stats(savef,&max_stats); 
    rs_write_rooms(savef, rooms, MAXROOMS);             
    rs_write_room_reference(savef, oldrp);              
    rs_write_rooms(savef, passages, MAXPASS);

    rs_write_monsters(savef,monsters,26);
    rs_write_obj_info(savef, things,   NUMTHINGS);
    rs_write_obj_info(savef, arm_info,  MAXARMORS);  
    rs_write_obj_info(savef, pot_info,  MAXPOTIONS);  
    rs_write_obj_info(savef, ring_info,  MAXRINGS);    
    rs_write_obj_info(savef, scr_info,  MAXSCROLLS);  
    rs_write_obj_info(savef, weap_info,  MAXWEAPONS+1);  
    rs_write_obj_info(savef, ws_info, MAXSTICKS);      

    
    rs_write_daemons(savef, &d_list[0], 20);            /* 5.4-daemon.c */
#ifdef MASTER
    rs_write_int(savef,total);                          /* 5.4-list.c   */
#else
    rs_write_int(savef, 0);
#endif
    rs_write_int(savef,between);                        /* 5.4-daemons.c*/
    rs_write_coord(savef, nh);                          /* 5.4-move.c    */
    rs_write_int(savef, group);                         /* 5.4-weapons.c */
    //fprintf(stderr,"fifth %ld\n",ftell(savef));

    rs_write_window(savef,stdscr);
    //fprintf(stderr,"done %ld\n",ftell(savef));

    return(WRITESTAT);
}

int
rs_restore_file(FILE *inf)
{
    int dummyint;

    if (read_error || format_error)
        return(READSTAT);

    rs_read_boolean(inf, &after);               /* 1  */    /* extern.c */
    rs_read_boolean(inf, &again);               /* 2  */
    rs_read_int(inf, &noscore);                 /* 3  */
    rs_read_boolean(inf, &seenstairs);          /* 4  */
    rs_read_boolean(inf, &amulet);              /* 5  */
    rs_read_boolean(inf, &door_stop);           /* 6  */
    rs_read_boolean(inf, &fight_flush);         /* 7  */
    rs_read_boolean(inf, &firstmove);           /* 8  */
    rs_read_boolean(inf, &got_ltc);             /* 9  */
    rs_read_boolean(inf, &has_hit);             /* 10 */
    rs_read_boolean(inf, &in_shell);            /* 11 */
    rs_read_boolean(inf, &inv_describe);        /* 12 */
    rs_read_boolean(inf, &jump);                /* 13 */
    rs_read_boolean(inf, &kamikaze);            /* 14 */
    rs_read_boolean(inf, &lower_msg);           /* 15 */
    rs_read_boolean(inf, &move_on);             /* 16 */
    rs_read_boolean(inf, &msg_esc);             /* 17 */
    rs_read_boolean(inf, &passgo);              /* 18 */
    rs_read_boolean(inf, &playing);             /* 19 */
    rs_read_boolean(inf, &q_comm);              /* 20 */
    rs_read_boolean(inf, &running);             /* 21 */
    rs_read_boolean(inf, &save_msg);            /* 22 */
    rs_read_boolean(inf, &see_floor);           /* 23 */
    rs_read_boolean(inf, &stat_msg);            /* 24 */
    rs_read_boolean(inf, &terse);               /* 25 */
    rs_read_boolean(inf, &to_death);            /* 26 */
    rs_read_boolean(inf, &tombstone);           /* 27 */
#ifdef MASTER
    rs_read_int(inf, &wizard);                  /* 28 */
#else
    rs_read_int(inf, &dummyint);                /* 28 */
#endif
    rs_read_booleans(inf, pack_used, 26);       /* 29 */
    rs_read_char(inf, &dir_ch);
    //rs_read_chars(inf, file_name, MAXSTR);
    //rs_read_chars(inf, huh, MAXSTR);
    rs_read_potions(inf);
    //rs_read_chars(inf, prbuf, 2*MAXSTR);
    rs_read_rings(inf);
    rs_read_new_string(inf,&release);
    rs_read_char(inf, &runch);
    rs_read_scrolls(inf);
    rs_read_char(inf, &take);
    //rs_read_chars(inf, whoami, MAXSTR);
    rs_read_sticks(inf);
    rs_read_int(inf,&orig_dsusp);
    rs_read_chars(inf, fruit, MAXSTR);
    //rs_read_chars(inf, home, MAXSTR);
    rs_read_new_strings(inf,(char **)inv_t_name,3);
    rs_read_char(inf, &l_last_comm);
    rs_read_char(inf, &l_last_dir);
    rs_read_char(inf, &last_comm);
    rs_read_char(inf, &last_dir);
    rs_read_new_strings(inf,(char **)tr_name,8);
    rs_read_int(inf, &n_objs);
    rs_read_int(inf, &ntraps);
    rs_read_int(inf, &hungry_state);
    rs_read_int(inf, &inpack);
    rs_read_int(inf, &inv_type);
    rs_read_int(inf, &level);
    rs_read_int(inf, &max_level);
    rs_read_int(inf, &mpos);
    rs_read_int(inf, &no_food);
    rs_read_ints(inf,(int32_t *)a_class,MAXARMORS);
    rs_read_int(inf, &count);
    rs_read_int(inf, &food_left);
    rs_read_int(inf, &lastscore);
    rs_read_int(inf, &no_command);
    rs_read_int(inf, &no_move);
    rs_read_int(inf, &purse);
    rs_read_int(inf, &quiet);
    rs_read_int(inf, &vf_hit);
    //rs_read_int(inf, &dnum);
    int32_t lownum,highnum;
    rs_read_int(inf, &lownum);
    rs_read_int(inf, &highnum);
    seed = ((uint64_t)highnum<<32) | (lownum&0xffffffff);
    rs_read_ints(inf,(int32_t *)e_levels,21);
    rs_read_coord(inf, &delta);
    rs_read_coord(inf, &oldpos);
    rs_read_coord(inf, &stairs);

    rs_read_thing(inf, &player); 
    rs_read_object_reference(inf, player.t_pack, &cur_armor);
    rs_read_object_reference(inf, player.t_pack, &cur_ring[0]);
    rs_read_object_reference(inf, player.t_pack, &cur_ring[1]);
    rs_read_object_reference(inf, player.t_pack, &cur_weapon);
    rs_read_object_reference(inf, player.t_pack, &l_last_pick);
    rs_read_object_reference(inf, player.t_pack, &last_pick);

    rs_read_object_list(inf, &lvl_obj);                 
    rs_read_thing_list(inf, &mlist);                  
    rs_fix_thing(&player);
    rs_fix_thing_list(mlist);

    rs_read_places(inf,places,MAXLINES*MAXCOLS);

    rs_read_stats(inf, &max_stats);
    rs_read_rooms(inf, rooms, MAXROOMS);
    rs_read_room_reference(inf, &oldrp);
    rs_read_rooms(inf, passages, MAXPASS);

    rs_read_monsters(inf,monsters,26);                  
    rs_read_obj_info(inf, things,   NUMTHINGS);         
    rs_read_obj_info(inf, arm_info,   MAXARMORS);         
    rs_read_obj_info(inf, pot_info,  MAXPOTIONS);       
    rs_read_obj_info(inf, ring_info,  MAXRINGS);         
    rs_read_obj_info(inf, scr_info,  MAXSCROLLS);       
    rs_read_obj_info(inf, weap_info, MAXWEAPONS+1);       
    rs_read_obj_info(inf, ws_info, MAXSTICKS);       

    rs_read_daemons(inf, d_list, 20);                   /* 5.4-daemon.c     */
    rs_read_int(inf,&dummyint);  /* total */            /* 5.4-list.c    */
    rs_read_int(inf,&between);                          /* 5.4-daemons.c    */
    rs_read_coord(inf, &nh);                            /* 5.4-move.c       */
    rs_read_int(inf,&group);                            /* 5.4-weapons.c    */
    
    rs_read_window(inf,stdscr);

    return(READSTAT);
}
