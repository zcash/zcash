#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <string.h>
#include <stdio.h>

int32_t safecopy(char *dest,const char *src,long len);

long _stripwhite(char *buf,int accept);

char *clonestr(char *str);

#ifdef __cplusplus
}
#endif