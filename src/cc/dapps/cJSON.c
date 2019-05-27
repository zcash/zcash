
/*
 Copyright (c) 2009 Dave Gamble
 
 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:
 
 The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.
 
 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 THE SOFTWARE.
 */

/* cJSON */
/* JSON parser in C. */
#include <math.h>

#include "../includes/cJSON.h"

#ifndef DBL_EPSILON
#define DBL_EPSILON 2.2204460492503131E-16
#endif

static const char *ep;

long stripquotes(char *str)
{
    long len,offset;
    if ( str == 0 )
        return(0);
    len = strlen(str);
    if ( str[0] == '"' && str[len-1] == '"' )
        str[len-1] = 0, offset = 1;
    else offset = 0;
    return(offset);
}

const char *cJSON_GetErrorPtr(void) {return ep;}

static int32_t cJSON_strcasecmp(const char *s1,const char *s2)
{
	if (!s1) return (s1==s2)?0:1;if (!s2) return 1;
	for(; tolower((int32_t)(*s1)) == tolower((int32_t)(*s2)); ++s1, ++s2)	if(*s1 == 0)	return 0;
	return tolower((int32_t)(*(const unsigned char *)s1)) - tolower((int32_t)(*(const unsigned char *)s2));
}

void *LP_alloc(uint64_t len);
void LP_free(void *ptr);
static void *(*cJSON_malloc)(size_t sz) = (void *)malloc;//LP_alloc;
static void (*cJSON_free)(void *ptr) = free;//LP_free;

static void *cJSON_mallocstr(int32_t len)
{
    return(cJSON_malloc(len));
}

static char **cJSON_mallocptrs(int32_t num,char **space,int32_t max)
{
    if ( num < max )
        return(space);
    else return(cJSON_malloc(num * sizeof(char *)));
}

static void *cJSON_mallocnode()
{
    return(cJSON_malloc(sizeof(cJSON)));
}

static void cJSON_freeptrs(char **ptrs,int32_t num,char **space)
{
    if ( ptrs != space )
        cJSON_free(ptrs);
}

static void cJSON_freestr(char *str)
{
    cJSON_free(str);
}

static void cJSON_freenode(cJSON *item)
{
    cJSON_free(item);
}

static char* cJSON_strdup(const char* str)
{
    size_t len;
    char* copy;
    
    len = strlen(str) + 1;
    if (!(copy = (char*)cJSON_mallocstr((int32_t)len+1))) return 0;
    memcpy(copy,str,len);
    return copy;
}

void cJSON_InitHooks(cJSON_Hooks* hooks)
{
    if (!hooks) { /* Reset hooks */
        cJSON_malloc = malloc;
        cJSON_free = free;
        return;
    }
    
    cJSON_malloc = (hooks->malloc_fn)?hooks->malloc_fn:malloc;
    cJSON_free	 = (hooks->free_fn)?hooks->free_fn:free;
}

/* Internal constructor. */
static cJSON *cJSON_New_Item(void)
{
	cJSON* node = (cJSON*)cJSON_mallocnode();
	if (node) memset(node,0,sizeof(cJSON));
	return node;
}

/* Delete a cJSON structure. */
void cJSON_Delete(cJSON *c)
{
	cJSON *next;
	while (c)
	{
		next=c->next;
		if (!(c->type&cJSON_IsReference) && c->child) cJSON_Delete(c->child);
		if (!(c->type&cJSON_IsReference) && c->valuestring) cJSON_freestr(c->valuestring);
		if (c->string) cJSON_freestr(c->string);
		cJSON_freenode(c);
		c=next;
	}
}

/* Parse the input text to generate a number, and populate the result into item. */
static const char *parse_number(cJSON *item,const char *num)
{
	double n=0,sign=1,scale=0;int32_t subscale=0,signsubscale=1;
    
	if (*num=='-') sign=-1,num++;	/* Has sign? */
	if (*num=='0') num++;			/* is zero */
	if (*num>='1' && *num<='9')	do	n=(n*10.0)+(*num++ -'0');	while (*num>='0' && *num<='9');	/* Number? */
	if (*num=='.' && num[1]>='0' && num[1]<='9') {num++;		do	n=(n*10.0)+(*num++ -'0'),scale--; while (*num>='0' && *num<='9');}	/* Fractional part? */
	if (*num=='e' || *num=='E')		/* Exponent? */
	{	num++;if (*num=='+') num++;	else if (*num=='-') signsubscale=-1,num++;		/* With sign? */
		while (*num>='0' && *num<='9') subscale=(subscale*10)+(*num++ - '0');	/* Number? */
	}
    
	n=sign*n*pow(10.0,(scale+subscale*signsubscale));	/* number = +/- number.fraction * 10^+/- exponent */
	
	item->valuedouble=n;
	item->valueint=(int64_t)n;
	item->type=cJSON_Number;
	return num;
}

/* Render the number nicely from the given item into a string. */
static char *print_number(cJSON *item)
{
	char *str;
	double d = item->valuedouble;
        if ( fabs(((double)item->valueint) - d) <= DBL_EPSILON && d >= (1. - DBL_EPSILON) && d < (1LL << 62) )//d <= INT_MAX && d >= INT_MIN )
        {
		str = (char *)cJSON_mallocstr(24);	/* 2^64+1 can be represented in 21 chars + sign. */
		if ( str != 0 )
            sprintf(str,"%lld",(long long)item->valueint);
	}
	else
	{
		str = (char *)cJSON_mallocstr(66);	/* This is a nice tradeoff. */
		if ( str != 0 )
		{
			if ( fabs(floor(d) - d) <= DBL_EPSILON && fabs(d) < 1.0e60 )
                sprintf(str,"%.0f",d);
			//else if (fabs(d)<1.0e-6 || fabs(d)>1.0e9)			sprintf(str,"%e",d);
			else
                sprintf(str,"%.8f",d);
		}
	}
	return str;
}

static unsigned parse_hex4(const char *str)
{
	unsigned h=0;
	if (*str>='0' && *str<='9') h+=(*str)-'0'; else if (*str>='A' && *str<='F') h+=10+(*str)-'A'; else if (*str>='a' && *str<='f') h+=10+(*str)-'a'; else return 0;
	h=h<<4;str++;
	if (*str>='0' && *str<='9') h+=(*str)-'0'; else if (*str>='A' && *str<='F') h+=10+(*str)-'A'; else if (*str>='a' && *str<='f') h+=10+(*str)-'a'; else return 0;
	h=h<<4;str++;
	if (*str>='0' && *str<='9') h+=(*str)-'0'; else if (*str>='A' && *str<='F') h+=10+(*str)-'A'; else if (*str>='a' && *str<='f') h+=10+(*str)-'a'; else return 0;
	h=h<<4;str++;
	if (*str>='0' && *str<='9') h+=(*str)-'0'; else if (*str>='A' && *str<='F') h+=10+(*str)-'A'; else if (*str>='a' && *str<='f') h+=10+(*str)-'a'; else return 0;
	return h;
}

/* Parse the input text into an unescaped cstring, and populate item. */
static const unsigned char firstByteMark[7] = { 0x00, 0x00, 0xC0, 0xE0, 0xF0, 0xF8, 0xFC };
static const char *parse_string(cJSON *item,const char *str)
{
	const char *ptr=str+1;char *ptr2;char *out;int32_t len=0;unsigned uc,uc2;
	if (*str!='\"') {ep=str;return 0;}	/* not a string! */
	
	while (*ptr!='\"' && *ptr && ++len) if (*ptr++ == '\\') ptr++;	// Skip escaped quotes
	
	out=(char*)cJSON_mallocstr(len+2);	/* This is how long we need for the string, roughly. */
	if (!out) return 0;
	
	ptr=str+1;ptr2=out;
	while (*ptr!='\"' && *ptr)
	{
		if (*ptr!='\\')
        {
            if ( *ptr == '%' && is_hexstr((char *)&ptr[1],2) && isprint(_decode_hex((char *)&ptr[1])) != 0 )
                *ptr2++ = _decode_hex((char *)&ptr[1]), ptr += 3;
            else *ptr2++ = *ptr++;
        }
		else
		{
			ptr++;
			switch (*ptr)
			{
				case 'b': *ptr2++='\b';	break;
				case 'f': *ptr2++='\f';	break;
				case 'n': *ptr2++='\n';	break;
				case 'r': *ptr2++='\r';	break;
				case 't': *ptr2++='\t';	break;
				case 'u':	 // transcode utf16 to utf8
					uc=parse_hex4(ptr+1);ptr+=4;	// get the unicode char
                    
					if ((uc>=0xDC00 && uc<=0xDFFF) || uc==0)	break;	// check for invalid
                    
					if (uc>=0xD800 && uc<=0xDBFF)	// UTF16 surrogate pairs
					{
						if (ptr[1]!='\\' || ptr[2]!='u')	break;	// missing second-half of surrogate.
						uc2=parse_hex4(ptr+3);ptr+=6;
						if (uc2<0xDC00 || uc2>0xDFFF)		break;	// invalid second-half of surrogate
						uc=0x10000 + (((uc&0x3FF)<<10) | (uc2&0x3FF));
					}
                    
					len=4;if (uc<0x80) len=1;else if (uc<0x800) len=2;else if (uc<0x10000) len=3; ptr2+=len;
					
					switch (len) {
						case 4: *--ptr2 =((uc | 0x80) & 0xBF); uc >>= 6;
						case 3: *--ptr2 =((uc | 0x80) & 0xBF); uc >>= 6;
						case 2: *--ptr2 =((uc | 0x80) & 0xBF); uc >>= 6;
						case 1: *--ptr2 =(uc | firstByteMark[len]);
					}
					ptr2+=len;
					break;
				default:  *ptr2++=*ptr; break;
			}
			ptr++;
		}
	}
	*ptr2=0;
	if (*ptr=='\"') ptr++;
	item->valuestring=out;
	item->type=cJSON_String;
	return ptr;
}

/* Render the cstring provided to an escaped version that can be printed. */
static char *print_string_ptr(const char *str)
{
	const char *ptr;char *ptr2,*out;int32_t len=0;unsigned char token;
	
	if (!str) return cJSON_strdup("");
	ptr=str;while ((token=*ptr) && ++len) {if (strchr("\"\\\b\f\n\r\t",token)) len++; else if (token<32) len+=5;ptr++;}
	
	out=(char*)cJSON_mallocstr(len+3+1);
	if (!out) return 0;
    
	ptr2=out;ptr=str;
	*ptr2++='\"';
	while (*ptr)
	{
		if ((unsigned char)*ptr>31 && *ptr!='\"' && *ptr!='\\') *ptr2++=*ptr++;
		else
		{
			*ptr2++='\\';
			switch (token=*ptr++)
			{
				case '\\':	*ptr2++='\\';	break;
				case '\"':	*ptr2++='\"';	break;
				case '\b':	*ptr2++='b';	break;
				case '\f':	*ptr2++='f';	break;
				case '\n':	*ptr2++='n';	break;
				case '\r':	*ptr2++='r';	break;
				case '\t':	*ptr2++='t';	break;
				default: sprintf(ptr2,"u%04x",token);ptr2+=5;	break;	/* escape and print */
			}
		}
	}
	*ptr2++='\"';*ptr2++=0;
	return out;
}
/* Invote print_string_ptr (which is useful) on an item. */
static char *print_string(cJSON *item)	{return print_string_ptr(item->valuestring);}

/* Predeclare these prototypes. */
static const char *parse_value(cJSON *item,const char *value);
static char *print_value(cJSON *item,int32_t depth,int32_t fmt);
static const char *parse_array(cJSON *item,const char *value);
static char *print_array(cJSON *item,int32_t depth,int32_t fmt);
static const char *parse_object(cJSON *item,const char *value);
static char *print_object(cJSON *item,int32_t depth,int32_t fmt);

/* Utility to jump whitespace and cr/lf */
static const char *skip(const char *in) {while (in && *in && (unsigned char)*in<=32) in++; return in;}

/* Parse an object - create a new root, and populate. */
cJSON *cJSON_ParseWithOpts(const char *value,const char **return_parse_end,int32_t require_null_terminated)
{
	const char *end=0;
	cJSON *c=cJSON_New_Item();
	ep=0;
	if (!c) return 0;       /* memory fail */
    
	end=parse_value(c,skip(value));
	if (!end)	{cJSON_Delete(c);return 0;}	/* parse failure. ep is set. */
    
	/* if we require null-terminated JSON without appended garbage, skip and then check for a null terminator */
	if (require_null_terminated) {end=skip(end);if (*end) {cJSON_Delete(c);ep=end;return 0;}}
	if (return_parse_end) *return_parse_end=end;
	return c;
}
/* Default options for cJSON_Parse */
cJSON *cJSON_Parse(const char *value)
{
    return(cJSON_ParseWithOpts(value,0,0));
}

/* Render a cJSON item/entity/structure to text. */
char *cJSON_Print(cJSON *item)
{
    return(print_value(item,0,1));
}
char *cJSON_PrintUnformatted(cJSON *item)	{return print_value(item,0,0);}

/* Parser core - when encountering text, process appropriately. */
static const char *parse_value(cJSON *item,const char *value)
{
	if (!value)						return 0;	/* Fail on null. */
	if (!strncmp(value,"null",4))	{ item->type=cJSON_NULL;  return value+4; }
	if (!strncmp(value,"false",5))	{ item->type=cJSON_False; return value+5; }
	if (!strncmp(value,"true",4))	{ item->type=cJSON_True; item->valueint=1;	return value+4; }
	if (*value=='\"')				{ return parse_string(item,value); }
	if (*value=='-' || (*value>='0' && *value<='9'))	{ return parse_number(item,value); }
	if (*value=='[')				{ return parse_array(item,value); }
	if (*value=='{')				{ return parse_object(item,value); }
    
	ep=value;return 0;	/* failure. */
}

/* Render a value to text. */
static char *print_value(cJSON *item,int32_t depth,int32_t fmt)
{
	char *out=0;
	if (!item) return 0;
	switch ((item->type)&255)
	{
		case cJSON_NULL:	out=cJSON_strdup("null");	break;
		case cJSON_False:	out=cJSON_strdup("false");break;
		case cJSON_True:	out=cJSON_strdup("true"); break;
		case cJSON_Number:	out=print_number(item);break;
		case cJSON_String:	out=print_string(item);break;
		case cJSON_Array:	out=print_array(item,depth,fmt);break;
		case cJSON_Object:	out=print_object(item,depth,fmt);break;
	}
	return out;
}

/* Build an array from input text. */
static const char *parse_array(cJSON *item,const char *value)
{
	cJSON *child;
	if (*value!='[')	{ep=value;return 0;}	/* not an array! */
    
	item->type=cJSON_Array;
	value=skip(value+1);
	if (*value==']') return value+1;	/* empty array. */
    
	item->child=child=cJSON_New_Item();
	if (!item->child) return 0;		 /* memory fail */
	value=skip(parse_value(child,skip(value)));	/* skip any spacing, get the value. */
	if (!value) return 0;
    
	while (*value==',')
	{
		cJSON *new_item;
		if (!(new_item=cJSON_New_Item())) return 0; 	/* memory fail */
		child->next=new_item;new_item->prev=child;child=new_item;
		value=skip(parse_value(child,skip(value+1)));
		if (!value) return 0;	/* memory fail */
	}
    
	if (*value==']') return value+1;	/* end of array */
	ep=value;return 0;	/* malformed. */
}

/* Render an array to text */
static char *print_array(cJSON *item,int32_t depth,int32_t fmt)
{
	char **entries,*space_entries[512];
	char *out=0,*ptr,*ret;int32_t len=5;
	cJSON *child=item->child;
	int32_t numentries=0,i=0,fail=0;
	
	/* How many entries in the array? */
	while (child) numentries++,child=child->next;
	/* Explicitly handle numentries==0 */
	if (!numentries)
	{
		out=(char*)cJSON_mallocstr(3+1);
		if (out) strcpy(out,"[]");
		return out;
	}
	/* Allocate an array to hold the values for each */
	entries=cJSON_mallocptrs(1+numentries,space_entries,sizeof(space_entries)/sizeof(*space_entries));
	if (!entries) return 0;
	memset(entries,0,numentries*sizeof(char*));
	/* Retrieve all the results: */
	child=item->child;
	while (child && !fail)
	{
		ret=print_value(child,depth+1,fmt);
		entries[i++]=ret;
		if (ret) len+=strlen(ret)+2+(fmt?1:0); else fail=1;
		child=child->next;
	}
	
	/* If we didn't fail, try to malloc the output string */
	if (!fail) out=(char*)cJSON_mallocstr(len+1);
	/* If that fails, we fail. */
	if (!out) fail=1;
    
	/* Handle failure. */
	if (fail)
	{
		for (i=0;i<numentries;i++) if (entries[i]) cJSON_freestr(entries[i]);
		cJSON_freeptrs(entries,numentries,space_entries);
		return 0;
	}
	
	/* Compose the output array. */
	*out='[';
	ptr=out+1;*ptr=0;
	for (i=0;i<numentries;i++)
	{
		strcpy(ptr,entries[i]);ptr+=strlen(entries[i]);
		if (i!=numentries-1) {*ptr++=',';if(fmt)*ptr++=' ';*ptr=0;}
		cJSON_freestr(entries[i]);
	}
	cJSON_freeptrs(entries,numentries,space_entries);
	*ptr++=']';*ptr++=0;
	return out;
}

/* Build an object from the text. */
static const char *parse_object(cJSON *item,const char *value)
{
	cJSON *child;
	if (*value!='{')	{ep=value;return 0;}	/* not an object! */
	
	item->type=cJSON_Object;
	value=skip(value+1);
	if (*value=='}') return value+1;	/* empty array. */
	
	item->child=child=cJSON_New_Item();
	if (!item->child) return 0;
	value=skip(parse_string(child,skip(value)));
	if (!value) return 0;
	child->string=child->valuestring;child->valuestring=0;
	if (*value!=':') {ep=value;return 0;}	/* fail! */
	value=skip(parse_value(child,skip(value+1)));	/* skip any spacing, get the value. */
	if (!value) return 0;
	
	while (*value==',')
	{
		cJSON *new_item;
		if (!(new_item=cJSON_New_Item()))	return 0; /* memory fail */
		child->next=new_item;new_item->prev=child;child=new_item;
		value=skip(parse_string(child,skip(value+1)));
		if (!value) return 0;
		child->string=child->valuestring;child->valuestring=0;
		if (*value!=':') {ep=value;return 0;}	/* fail! */
		value=skip(parse_value(child,skip(value+1)));	/* skip any spacing, get the value. */
		if (!value) return 0;
	}
	
	if (*value=='}') return value+1;	/* end of array */
	ep=value;return 0;	/* malformed. */
}

/* Render an object to text. */
static char *print_object(cJSON *item,int32_t depth,int32_t fmt)
{
    char **entries=0,**names=0,*space_entries[512],*space_names[512];
	char *out=0,*ptr,*ret,*str;int32_t len=7,i=0,j;
	cJSON *child=item->child,*firstchild;
	int32_t numentries=0,fail=0;
	// Count the number of entries
    firstchild = child;
	while ( child )
    {
        numentries++;
        child = child->next;
        if ( child == firstchild )
        {
            printf("cJSON infinite loop detected\n");
            break;
        }
    }
	/* Explicitly handle empty object case */
	if (!numentries)
	{
		out=(char*)cJSON_mallocstr(fmt?depth+4+1:3+1);
		if (!out)	return 0;
		ptr=out;*ptr++='{';
		if (fmt) {*ptr++='\n';for (i=0;i<depth-1;i++) *ptr++='\t';}
		*ptr++='}';*ptr++=0;
		return out;
	}
	/* Allocate space for the names and the objects */
	entries=(char**)cJSON_mallocptrs(numentries,space_entries,sizeof(space_entries)/sizeof(*space_entries));
	if (!entries) return 0;
	names=(char**)cJSON_mallocptrs(numentries,space_names,sizeof(space_names)/sizeof(*space_names));
	if (!names) {cJSON_freeptrs(entries,numentries,space_entries);return 0;}
	memset(entries,0,sizeof(char*)*numentries);
	memset(names,0,sizeof(char*)*numentries);
    
	/* Collect all the results into our arrays: */
	child=item->child;depth++;if (fmt) len+=depth;
	while ( child )
	{
		names[i]=str=print_string_ptr(child->string);
		entries[i++]=ret=print_value(child,depth,fmt);
		if (str && ret) len+=strlen(ret)+strlen(str)+2+(fmt?2+depth:0); else fail=1;
		child=child->next;
        if ( child == firstchild )
            break;
	}
	
	/* Try to allocate the output string */
	if (!fail) out=(char*)cJSON_mallocstr(len+1);
	if (!out) fail=1;
    
	/* Handle failure */
	if (fail)
	{
		for (i=0;i<numentries;i++) {if (names[i]) cJSON_freestr(names[i]);if (entries[i]) cJSON_freestr(entries[i]);}
		cJSON_freeptrs(names,numentries,space_names);
        cJSON_freeptrs(entries,numentries,space_entries);
		return 0;
	}
	
	/* Compose the output: */
	*out='{';ptr=out+1;if (fmt)*ptr++='\n';*ptr=0;
	for (i=0;i<numentries;i++)
	{
		if (fmt) for (j=0;j<depth;j++) *ptr++='\t';
		strcpy(ptr,names[i]);ptr+=strlen(names[i]);
		*ptr++=':';if (fmt) *ptr++='\t';
		strcpy(ptr,entries[i]);ptr+=strlen(entries[i]);
		if (i!=numentries-1) *ptr++=',';
		if (fmt) *ptr++='\n';*ptr=0;
		cJSON_freestr(names[i]);cJSON_freestr(entries[i]);
	}
	
	cJSON_freeptrs(names,numentries,space_names);
    cJSON_freeptrs(entries,numentries,space_entries);
	if (fmt) for (i=0;i<depth-1;i++) *ptr++='\t';
	*ptr++='}';*ptr++=0;
	return out;
}

/* Get Array size/item / object item. */
int32_t    cJSON_GetArraySize(cJSON *array)							{cJSON *c; if ( array == 0 ) return(0); c=array->child;int32_t i=0;while(c)i++,c=c->next;return i;}
cJSON *cJSON_GetArrayItem(cJSON *array,int32_t item)				{cJSON *c=array->child;  while (c && item>0) item--,c=c->next; return c;}
cJSON *cJSON_GetObjectItem(cJSON *object,const char *string)	{ if ( object == 0 ) return(0); cJSON *c=object->child; while (c && cJSON_strcasecmp(c->string,string)) c=c->next; return c;}

/* Utility for array list handling. */
static void suffix_object(cJSON *prev,cJSON *item) {prev->next=item;item->prev=prev;}
/* Utility for handling references. */
static cJSON *create_reference(cJSON *item) {cJSON *ref=cJSON_New_Item();if (!ref) return 0;memcpy(ref,item,sizeof(cJSON));ref->string=0;ref->type|=cJSON_IsReference;ref->next=ref->prev=0;return ref;}

/* Add item to array/object. */
void   cJSON_AddItemToArray(cJSON *array, cJSON *item)						{cJSON *c=array->child;if (!item) return; if (!c) {array->child=item;} else {while (c && c->next) c=c->next; suffix_object(c,item);}}
void   cJSON_AddItemToObject(cJSON *object,const char *string,cJSON *item)	{if (!item) return; if (item->string) cJSON_free(item->string);item->string=cJSON_strdup(string);cJSON_AddItemToArray(object,item);}
void	cJSON_AddItemReferenceToArray(cJSON *array, cJSON *item)						{cJSON_AddItemToArray(array,create_reference(item));}
void	cJSON_AddItemReferenceToObject(cJSON *object,const char *string,cJSON *item)	{cJSON_AddItemToObject(object,string,create_reference(item));}

cJSON *cJSON_DetachItemFromArray(cJSON *array,int32_t which)			{cJSON *c=array->child;while (c && which>0) c=c->next,which--;if (!c) return 0;
	if (c->prev) c->prev->next=c->next;if (c->next) c->next->prev=c->prev;if (c==array->child) array->child=c->next;c->prev=c->next=0;return c;}
void   cJSON_DeleteItemFromArray(cJSON *array,int32_t which)			{cJSON_Delete(cJSON_DetachItemFromArray(array,which));}
cJSON *cJSON_DetachItemFromObject(cJSON *object,const char *string) {int32_t i=0;cJSON *c=object->child;while (c && cJSON_strcasecmp(c->string,string)) i++,c=c->next;if (c) return cJSON_DetachItemFromArray(object,i);return 0;}
void   cJSON_DeleteItemFromObject(cJSON *object,const char *string) {cJSON_Delete(cJSON_DetachItemFromObject(object,string));}

/* Replace array/object items with new ones. */
void   cJSON_ReplaceItemInArray(cJSON *array,int32_t which,cJSON *newitem)		{cJSON *c=array->child;while (c && which>0) c=c->next,which--;if (!c) return;
	newitem->next=c->next;newitem->prev=c->prev;if (newitem->next) newitem->next->prev=newitem;
	if (c==array->child) array->child=newitem; else newitem->prev->next=newitem;c->next=c->prev=0;cJSON_Delete(c);}
void   cJSON_ReplaceItemInObject(cJSON *object,const char *string,cJSON *newitem){int32_t i=0;cJSON *c=object->child;while(c && cJSON_strcasecmp(c->string,string))i++,c=c->next;if(c){newitem->string=cJSON_strdup(string);cJSON_ReplaceItemInArray(object,i,newitem);}}

/* Create basic types: */
cJSON *cJSON_CreateNull(void)					{cJSON *item=cJSON_New_Item();if(item)item->type=cJSON_NULL;return item;}
cJSON *cJSON_CreateTrue(void)					{cJSON *item=cJSON_New_Item();if(item)item->type=cJSON_True;return item;}
cJSON *cJSON_CreateFalse(void)					{cJSON *item=cJSON_New_Item();if(item)item->type=cJSON_False;return item;}
cJSON *cJSON_CreateBool(int32_t b)					{cJSON *item=cJSON_New_Item();if(item)item->type=b?cJSON_True:cJSON_False;return item;}
cJSON *cJSON_CreateNumber(double num)			{cJSON *item=cJSON_New_Item();if(item){item->type=cJSON_Number;item->valuedouble=num;item->valueint=(int64_t)num;}return item;}
cJSON *cJSON_CreateString(const char *string)	{cJSON *item=cJSON_New_Item();if(item){item->type=cJSON_String;item->valuestring=cJSON_strdup(string);}return item;}

/* Create Arrays: */
cJSON *cJSON_CreateIntArray(int64_t *numbers,int32_t count)		{int32_t i;cJSON *n=0,*p=0,*a=cJSON_CreateArray();for(i=0;a && i<count;i++){n=cJSON_CreateNumber((double)numbers[i]);if(!i)a->child=n;else suffix_object(p,n);p=n;}return a;}
cJSON *cJSON_CreateFloatArray(float *numbers,int32_t count)	{int32_t i;cJSON *n=0,*p=0,*a=cJSON_CreateArray();for(i=0;a && i<count;i++){n=cJSON_CreateNumber(numbers[i]);if(!i)a->child=n;else suffix_object(p,n);p=n;}return a;}
cJSON *cJSON_CreateDoubleArray(double *numbers,int32_t count)	{int32_t i;cJSON *n=0,*p=0,*a=cJSON_CreateArray();for(i=0;a && i<count;i++){n=cJSON_CreateNumber(numbers[i]);if(!i)a->child=n;else suffix_object(p,n);p=n;}return a;}
cJSON *cJSON_CreateStringArray(char **strings,int32_t count)	{int32_t i;cJSON *n=0,*p=0,*a=cJSON_CreateArray();for(i=0;a && i<count;i++){n=cJSON_CreateString(strings[i]);if(!i)a->child=n;else suffix_object(p,n);p=n;}return a;}

/* Duplication */
cJSON *cJSON_Duplicate(cJSON *item,int32_t recurse)
{
	cJSON *newitem,*cptr,*nptr=0,*newchild;
	/* Bail on bad ptr */
	if (!item) return 0;
	/* Create new item */
	newitem=cJSON_New_Item();
	if (!newitem) return 0;
	/* Copy over all vars */
	newitem->type=item->type&(~cJSON_IsReference),newitem->valueint=item->valueint,newitem->valuedouble=item->valuedouble;
	if (item->valuestring)	{newitem->valuestring=cJSON_strdup(item->valuestring);	if (!newitem->valuestring)	{cJSON_Delete(newitem);return 0;}}
	if (item->string)		{newitem->string=cJSON_strdup(item->string);			if (!newitem->string)		{cJSON_Delete(newitem);return 0;}}
	/* If non-recursive, then we're done! */
	if (!recurse) return newitem;
	/* Walk the ->next chain for the child. */
	cptr=item->child;
	while (cptr)
	{
		newchild=cJSON_Duplicate(cptr,1);		/* Duplicate (with recurse) each item in the ->next chain */
		if (!newchild) {cJSON_Delete(newitem);return 0;}
		if (nptr)	{nptr->next=newchild,newchild->prev=nptr;nptr=newchild;}	/* If newitem->child already set, then crosswire ->prev and ->next and move on */
		else		{newitem->child=newchild;nptr=newchild;}					/* Set newitem->child and move to it */
		cptr=cptr->next;
	}
	return newitem;
}

void cJSON_Minify(char *json)
{
	char *into=json;
	while (*json)
	{
		if (*json==' ') json++;
		else if (*json=='\t') json++;	// Whitespace characters.
		else if (*json=='\r') json++;
		else if (*json=='\n') json++;
		else if (*json=='/' && json[1]=='/')  while (*json && *json!='\n') json++;	// double-slash comments, to end of line.
		else if (*json=='/' && json[1]=='*') {while (*json && !(*json=='*' && json[1]=='/')) json++;json+=2;}	// multiline comments.
		else if (*json=='\"'){*into++=*json++;while (*json && *json!='\"'){if (*json=='\\') *into++=*json++;*into++=*json++;}*into++=*json++;} // string literals, which are \" sensitive.
		else *into++=*json++;			// All other characters.
	}
	*into=0;	// and null-terminate.
}

// the following written by jl777
/******************************************************************************
 * Copyright © 2014-2019 The SuperNET Developers.                             *
 *                                                                            *
 * See the AUTHORS, DEVELOPER-AGREEMENT and LICENSE files at                  *
 * the top-level directory of this distribution for the individual copyright  *
 * holder information and the developer policies on copyright and licensing.  *
 *                                                                            *
 * Unless otherwise agreed in a custom licensing agreement, no part of the    *
 * SuperNET software, including this file may be copied, modified, propagated *
 * or distributed except according to the terms contained in the LICENSE file *
 *                                                                            *
 * Removal or modification of this copyright notice is prohibited.            *
 *                                                                            *
 ******************************************************************************/

void copy_cJSON(struct destbuf *dest,cJSON *obj)
{
    char *str;
    int i;
    long offset;
    dest->buf[0] = 0;
    if ( obj != 0 )
    {
        str = cJSON_Print(obj);
        if ( str != 0 )
        {
            offset = stripquotes(str);
            //strcpy(dest,str+offset);
            for (i=0; i<MAX_JSON_FIELD-1; i++)
                if ( (dest->buf[i]= str[offset+i]) == 0 )
                    break;
            dest->buf[i] = 0;
            free(str);
        }
    }
}

void copy_cJSON2(char *dest,int32_t maxlen,cJSON *obj)
{
    struct destbuf tmp;
    maxlen--;
    dest[0] = 0;
    if ( maxlen > sizeof(tmp.buf) )
        maxlen = sizeof(tmp.buf);
    copy_cJSON(&tmp,obj);
    if ( strlen(tmp.buf) < maxlen )
        strcpy(dest,tmp.buf);
    else dest[0] = 0;
}

int64_t _get_cJSON_int(cJSON *json)
{
    struct destbuf tmp;
    if ( json != 0 )
    {
        copy_cJSON(&tmp,json);
        if ( tmp.buf[0] != 0 )
            return(calc_nxt64bits(tmp.buf));
    }
    return(0);
}

int64_t get_cJSON_int(cJSON *json,char *field)
{
    cJSON *numjson;
    if ( json != 0 )
    {
        numjson = cJSON_GetObjectItem(json,field);
        if ( numjson != 0 )
            return(_get_cJSON_int(numjson));
    }
    return(0);
}

int64_t _conv_cJSON_float(cJSON *json)
{
    int64_t conv_floatstr(char *);
    struct destbuf tmp;
    if ( json != 0 )
    {
        copy_cJSON(&tmp,json);
        return(conv_floatstr(tmp.buf));
    }
    return(0);
}

int64_t conv_cJSON_float(cJSON *json,char *field)
{
    if ( json != 0 )
        return(_conv_cJSON_float(cJSON_GetObjectItem(json,field)));
    return(0);
}

int32_t extract_cJSON_str(char *dest,int32_t max,cJSON *json,char *field)
{
    int32_t safecopy(char *dest,char *src,long len);
    char *str;
    cJSON *obj;
    int32_t len;
    long offset;
    dest[0] = 0;
    obj = cJSON_GetObjectItem(json,field);
    if ( obj != 0 )
    {
        str = cJSON_Print(obj);
        offset = stripquotes(str);
        len = safecopy(dest,str+offset,max);
        free(str);
        return(len);
    }
    return(0);
}

cJSON *gen_list_json(char **list)
{
    cJSON *array,*item;
    array = cJSON_CreateArray();
    while ( list != 0 && *list != 0 && *list[0] != 0 )
    {
        item = cJSON_CreateString(*list++);
        cJSON_AddItemToArray(array,item);
    }
    return(array);
}

uint64_t get_API_nxt64bits(cJSON *obj)
{
    uint64_t nxt64bits = 0;
    struct destbuf tmp;
    if ( obj != 0 )
    {
        if ( is_cJSON_Number(obj) != 0 )
            return((uint64_t)obj->valuedouble);
        copy_cJSON(&tmp,obj);
        nxt64bits = calc_nxt64bits(tmp.buf);
    }
    return(nxt64bits);
}
uint64_t j64bits(cJSON *json,char *field) { if ( field == 0 ) return(get_API_nxt64bits(json)); return(get_API_nxt64bits(cJSON_GetObjectItem(json,field))); }
uint64_t j64bitsi(cJSON *json,int32_t i) { return(get_API_nxt64bits(cJSON_GetArrayItem(json,i))); }

uint64_t get_satoshi_obj(cJSON *json,char *field)
{
    int32_t i,n;
    uint64_t prev,satoshis,mult = 1;
    struct destbuf numstr,checkstr;
    cJSON *numjson;
    numjson = cJSON_GetObjectItem(json,field);
    copy_cJSON(&numstr,numjson);
    satoshis = prev = 0; mult = 1; n = (int32_t)strlen(numstr.buf);
    for (i=n-1; i>=0; i--,mult*=10)
    {
        satoshis += (mult * (numstr.buf[i] - '0'));
        if ( satoshis < prev )
            printf("get_satoshi_obj numstr.(%s) i.%d prev.%llu vs satoshis.%llu\n",numstr.buf,i,(unsigned long long)prev,(unsigned long long)satoshis);
        prev = satoshis;
    }
    sprintf(checkstr.buf,"%llu",(long long)satoshis);
    if ( strcmp(checkstr.buf,numstr.buf) != 0 )
    {
        printf("SATOSHI GREMLIN?? numstr.(%s) -> %.8f -> (%s)\n",numstr.buf,dstr(satoshis),checkstr.buf);
    }
    return(satoshis);
}

void add_satoshis_json(cJSON *json,char *field,uint64_t satoshis)
{
    cJSON *obj;
    char numstr[64];
    sprintf(numstr,"%lld",(long long)satoshis);
    obj = cJSON_CreateString(numstr);
    cJSON_AddItemToObject(json,field,obj);
    if ( satoshis != get_satoshi_obj(json,field) )
        printf("error adding satoshi obj %ld -> %ld\n",(unsigned long)satoshis,(unsigned long)get_satoshi_obj(json,field));
}

char *cJSON_str(cJSON *json)
{
    if ( json != 0 && is_cJSON_String(json) != 0 )
        return(json->valuestring);
    return(0);
}

void jadd(cJSON *json,char *field,cJSON *item) { if ( json != 0 )cJSON_AddItemToObject(json,field,item); }
void jaddstr(cJSON *json,char *field,char *str) { if ( json != 0 && str != 0 ) cJSON_AddItemToObject(json,field,cJSON_CreateString(str)); }
void jaddnum(cJSON *json,char *field,double num) { if ( json != 0 )cJSON_AddItemToObject(json,field,cJSON_CreateNumber(num)); }
void jadd64bits(cJSON *json,char *field,uint64_t nxt64bits) { char numstr[64]; sprintf(numstr,"%llu",(long long)nxt64bits), jaddstr(json,field,numstr); }
void jaddi(cJSON *json,cJSON *item) { if ( json != 0 ) cJSON_AddItemToArray(json,item); }
void jaddistr(cJSON *json,char *str) { if ( json != 0 ) cJSON_AddItemToArray(json,cJSON_CreateString(str)); }
void jaddinum(cJSON *json,double num) { if ( json != 0 ) cJSON_AddItemToArray(json,cJSON_CreateNumber(num)); }
void jaddi64bits(cJSON *json,uint64_t nxt64bits) { char numstr[64]; sprintf(numstr,"%llu",(long long)nxt64bits), jaddistr(json,numstr); }
char *jstr(cJSON *json,char *field) { if ( json == 0 ) return(0); if ( field == 0 ) return(cJSON_str(json)); return(cJSON_str(cJSON_GetObjectItem(json,field))); }

char *jstri(cJSON *json,int32_t i) { return(cJSON_str(cJSON_GetArrayItem(json,i))); }
char *jprint(cJSON *json,int32_t freeflag)
{
    char *str;
    /*static portable_mutex_t mutex; static int32_t initflag;
    if ( initflag == 0 )
    {
        portable_mutex_init(&mutex);
        initflag = 1;
    }*/
    if ( json == 0 )
        return(clonestr((char *)"{}"));
    //portable_mutex_lock(&mutex);
    //usleep(5000);
    str = cJSON_Print(json), _stripwhite(str,' ');
    if ( freeflag != 0 )
        free_json(json);
    //portable_mutex_unlock(&mutex);
    return(str);
}

bits256 get_API_bits256(cJSON *obj)
{
    bits256 hash; char *str;
    memset(hash.bytes,0,sizeof(hash));
    if ( obj != 0 )
    {
        if ( is_cJSON_String(obj) != 0 && (str= obj->valuestring) != 0 && strlen(str) == 64 )
            decode_hex(hash.bytes,sizeof(hash),str);
    }
    return(hash);
}
bits256 jbits256(cJSON *json,char *field) { if ( field == 0 ) return(get_API_bits256(json)); return(get_API_bits256(json != 0 ? cJSON_GetObjectItem(json,field) : 0)); }
bits256 jbits256i(cJSON *json,int32_t i) { return(get_API_bits256(cJSON_GetArrayItem(json,i))); }
void jaddbits256(cJSON *json,char *field,bits256 hash) { char str[65]; bits256_str(str,hash), jaddstr(json,field,str); }
void jaddibits256(cJSON *json,bits256 hash) { char str[65]; bits256_str(str,hash), jaddistr(json,str); }

char *get_cJSON_fieldname(cJSON *obj)
{
    if ( obj != 0 )
    {
        if ( obj->string != 0 )
            return(obj->string);
		if ( obj->child != 0 && obj->child->string != 0 )
			return(obj->child->string);
    }
    return((char *)"<no cJSON string field>");
}

int32_t jnum(cJSON *obj,char *field)
{
    char *str; int32_t polarity = 1;
    if ( field != 0 )
        obj = jobj(obj,field);
    if ( obj != 0 )
    {
        if ( is_cJSON_Number(obj) != 0 )
            return(obj->valuedouble);
        else if ( is_cJSON_String(obj) != 0 && (str= jstr(obj,0)) != 0 )
        {
            if ( str[0] == '-' )
                polarity = -1, str++;
            return(polarity * (int32_t)calc_nxt64bits(str));
        }
    }
    return(0);
}

void ensure_jsonitem(cJSON *json,char *field,char *value)
{
    cJSON *obj;
    if ( json != 0 )
    {
        obj = cJSON_GetObjectItem(json,field);
        if ( obj == 0 )
            cJSON_AddItemToObject(json,field,cJSON_CreateString(value));
        else cJSON_ReplaceItemInObject(json,field,cJSON_CreateString(value));
    }
}

int32_t in_jsonarray(cJSON *array,char *value)
{
    int32_t i,n;
    struct destbuf remote;
    if ( array != 0 && is_cJSON_Array(array) != 0 )
    {
        n = cJSON_GetArraySize(array);
        for (i=0; i<n; i++)
        {
            if ( array == 0 || n == 0 )
                break;
            copy_cJSON(&remote,cJSON_GetArrayItem(array,i));
            if ( strcmp(remote.buf,value) == 0 )
                return(1);
        }
    }
    return(0);
}

int32_t myatoi(char *str,int32_t range)
{
    long x; char *ptr;
    x = strtol(str,&ptr,10);
    if ( range != 0 && x >= range )
        x = (range - 1);
    return((int32_t)x);
}

int32_t get_API_int(cJSON *obj,int32_t val)
{
    struct destbuf buf;
    if ( obj != 0 )
    {
        if ( is_cJSON_Number(obj) != 0 )
            return((int32_t)obj->valuedouble);
        copy_cJSON(&buf,obj);
        val = myatoi(buf.buf,0);
        if ( val < 0 )
            val = 0;
    }
    return(val);
}
int32_t jint(cJSON *json,char *field) { if ( json == 0 ) return(0); if ( field == 0 ) return(get_API_int(json,0)); return(get_API_int(cJSON_GetObjectItem(json,field),0)); }
int32_t jinti(cJSON *json,int32_t i) { if ( json == 0 ) return(0); return(get_API_int(cJSON_GetArrayItem(json,i),0)); }

uint32_t get_API_uint(cJSON *obj,uint32_t val)
{
    struct destbuf buf;
    if ( obj != 0 )
    {
        if ( is_cJSON_Number(obj) != 0 )
            return((uint32_t)obj->valuedouble);
        copy_cJSON(&buf,obj);
        val = myatoi(buf.buf,0);
    }
    return(val);
}
uint32_t juint(cJSON *json,char *field) { if ( json == 0 ) return(0); if ( field == 0 ) return(get_API_uint(json,0)); return(get_API_uint(cJSON_GetObjectItem(json,field),0)); }
uint32_t juinti(cJSON *json,int32_t i) { if ( json == 0 ) return(0); return(get_API_uint(cJSON_GetArrayItem(json,i),0)); }

double get_API_float(cJSON *obj)
{
    double val = 0.;
    struct destbuf buf;
    if ( obj != 0 )
    {
        if ( is_cJSON_Number(obj) != 0 )
            return(obj->valuedouble);
        copy_cJSON(&buf,obj);
        val = atof(buf.buf);
    }
    return(val);
}

double jdouble(cJSON *json,char *field)
{
    if ( json != 0 )
    {
        if ( field == 0 )
            return(get_API_float(json));
        else return(get_API_float(cJSON_GetObjectItem(json,field)));
    } else return(0.);
}

double jdoublei(cJSON *json,int32_t i)
{
    if ( json != 0 )
        return(get_API_float(cJSON_GetArrayItem(json,i)));
    else return(0.);
}

cJSON *jobj(cJSON *json,char *field) { if ( json != 0 ) return(cJSON_GetObjectItem(json,field)); return(0); }

void jdelete(cJSON *json,char *field)
{
    if ( jobj(json,field) != 0 )
        cJSON_DeleteItemFromObject(json,field);
}

cJSON *jduplicate(cJSON *json) { return(cJSON_Duplicate(json,1)); }

cJSON *jitem(cJSON *array,int32_t i) { if ( array != 0 && is_cJSON_Array(array) != 0 && cJSON_GetArraySize(array) > i ) return(cJSON_GetArrayItem(array,i)); return(0); }
cJSON *jarray(int32_t *nump,cJSON *json,char *field)
{
    cJSON *array;
    if ( json != 0 )
    {
        if ( field == 0 )
            array = json;
        else array = cJSON_GetObjectItem(json,field);
        if ( array != 0 && is_cJSON_Array(array) != 0 && (*nump= cJSON_GetArraySize(array)) > 0 )
            return(array);
    }
    *nump = 0;
    return(0);
}

int32_t expand_nxt64bits(char *NXTaddr,uint64_t nxt64bits)
{
    int32_t i,n;
    uint64_t modval;
    char rev[64];
    for (i=0; nxt64bits!=0; i++)
    {
        modval = nxt64bits % 10;
        rev[i] = (char)(modval + '0');
        nxt64bits /= 10;
    }
    n = i;
    for (i=0; i<n; i++)
        NXTaddr[i] = rev[n-1-i];
    NXTaddr[i] = 0;
    return(n);
}

char *nxt64str(uint64_t nxt64bits)
{
    static char NXTaddr[64];
    expand_nxt64bits(NXTaddr,nxt64bits);
    return(NXTaddr);
}

char *nxt64str2(uint64_t nxt64bits)
{
    static char NXTaddr[64];
    expand_nxt64bits(NXTaddr,nxt64bits);
    return(NXTaddr);
}

int32_t cmp_nxt64bits(const char *str,uint64_t nxt64bits)
{
    char expanded[64];
    if ( str == 0 )//|| str[0] == 0 || nxt64bits == 0 )
        return(-1);
    if ( nxt64bits == 0 && str[0] == 0 )
        return(0);
    expand_nxt64bits(expanded,nxt64bits);
    return(strcmp(str,expanded));
}

uint64_t calc_nxt64bits(const char *NXTaddr)
{
    int32_t c;
    int64_t n,i,polarity = 1;
    uint64_t lastval,mult,nxt64bits = 0;
    if ( NXTaddr == 0 )
    {
        printf("calling calc_nxt64bits with null ptr!\n");
        return(0);
    }
    n = strlen(NXTaddr);
    if ( n >= 22 )
    {
        printf("calc_nxt64bits: illegal NXTaddr.(%s) too long\n",NXTaddr);
        return(0);
    }
    else if ( strcmp(NXTaddr,"0") == 0 || strcmp(NXTaddr,"false") == 0 )
    {
        // printf("zero address?\n"); getchar();
        return(0);
    }
    if ( NXTaddr[0] == '-' )
        polarity = -1, NXTaddr++, n--;
    mult = 1;
    lastval = 0;
    for (i=n-1; i>=0; i--,mult*=10)
    {
        c = NXTaddr[i];
        if ( c < '0' || c > '9' )
        {
            //printf("calc_nxt64bits: illegal char.(%c %d) in (%s).%d\n",c,c,NXTaddr,(int32_t)i);
#ifdef __APPLE__
            //while ( 1 )
            {
                //sleep(60);
                //printf("calc_nxt64bits: illegal char.(%c %d) in (%s).%d\n",c,c,NXTaddr,(int32_t)i);
            }
#endif
            return(0);
        }
        nxt64bits += mult * (c - '0');
        if ( nxt64bits < lastval )
            printf("calc_nxt64bits: warning: 64bit overflow %llx < %llx\n",(long long)nxt64bits,(long long)lastval);
        lastval = nxt64bits;
    }
    while ( *NXTaddr == '0' && *NXTaddr != 0 )
        NXTaddr++;
    if ( cmp_nxt64bits(NXTaddr,nxt64bits) != 0 )
        printf("error calculating nxt64bits: %s -> %llx -> %s\n",NXTaddr,(long long)nxt64bits,nxt64str(nxt64bits));
    if ( polarity < 0 )
        return(-(int64_t)nxt64bits);
    return(nxt64bits);
}

cJSON *addrs_jsonarray(uint64_t *addrs,int32_t num)
{
    int32_t j; cJSON *array;
    array = cJSON_CreateArray();
    for (j=0; j<num; j++)
        jaddi64bits(array,addrs[j]);
    return(array);
}

cJSON *cJSON_CreateArray(void)
{
    cJSON *item = cJSON_New_Item();
    if ( item )
        item->type = cJSON_Array;
//#ifdef CJSON_GARBAGECOLLECTION
//    cJSON_register(item);
//#endif
    return(item);
}

cJSON *cJSON_CreateObject(void)
{
    cJSON *item = cJSON_New_Item();
    if ( item )
        item->type = cJSON_Object;
//#ifdef CJSON_GARBAGECOLLECTION
//    cJSON_register(item);
//#endif
    return item;
}

void free_json(cJSON *item)
{
//#ifdef CJSON_GARBAGECOLLECTION
//    cJSON_unregister(item);
//#endif
   if ( item != 0 )
        cJSON_Delete(item);
}
