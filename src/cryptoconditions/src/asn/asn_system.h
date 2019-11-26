/*-
 * Copyright (c) 2003, 2004, 2007 Lev Walkin <vlm@lionet.info>.
 * All rights reserved.
 * Redistribution and modifications are permitted subject to BSD license.
 */
/*
 * Miscellaneous system-dependent types.
 */
#ifndef	ASN_SYSTEM_H
#define	ASN_SYSTEM_H

#ifdef CRYPTOCONDITIONS_HAVE_CONFIG_H
#include "cryptoconditions-config.h"
#endif

#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE /* for snprintf() on some linux systems  */
#endif

#include <stdio.h>	/* For snprintf(3) */
#include <stdlib.h>	/* For *alloc(3) */
#include <string.h>	/* For memcpy(3) */
#include <sys/types.h>	/* For size_t */
#include <limits.h>	/* For LONG_MAX */
#include <stdarg.h>	/* For va_start */
#include <stddef.h>	/* for offsetof and ptrdiff_t */

#ifdef	HAVE_ALLOCA_H
#include <alloca.h>	/* For alloca(3) */
#endif

#ifdef	_WIN32

#include <malloc.h>
#define	 snprintf	_snprintf
#define	 vsnprintf	_vsnprintf

/* To avoid linking with ws2_32.lib, here's the definition of ntohl() */
#define sys_ntohl(l)	((((l) << 24)  & 0xff000000)	\
			| (((l) << 8) & 0xff0000)	\
			| (((l) >> 8)  & 0xff00)	\
			| ((l >> 24) & 0xff))

#ifdef _MSC_VER			/* MSVS.Net */
#ifndef __cplusplus
#define inline __inline
#endif
#ifndef	ASSUMESTDTYPES	/* Standard types have been defined elsewhere */
#define	ssize_t		SSIZE_T
typedef	char		int8_t;
typedef	short		int16_t;
typedef	int		int32_t;
typedef	unsigned char	uint8_t;
typedef	unsigned short	uint16_t;
typedef	unsigned int	uint32_t;
#endif	/* ASSUMESTDTYPES */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <float.h>
#define isnan _isnan
#define finite _finite
#define copysign _copysign
#define	ilogb	_logb
#else	/* !_MSC_VER */
#include <stdint.h>
#endif	/* _MSC_VER */

#else	/* !_WIN32 */

#if defined(__vxworks)
#include <types/vxTypes.h>
#else	/* !defined(__vxworks) */

#include <inttypes.h>	/* C99 specifies this file */
/*
 * 1. Earlier FreeBSD version didn't have <stdint.h>,
 * but <inttypes.h> was present.
 * 2. Sun Solaris requires <alloca.h> for alloca(3),
 * but does not have <stdint.h>.
 */
#if	(!defined(__FreeBSD__) || !defined(_SYS_INTTYPES_H_))
#if	defined(sun)
#include <alloca.h>	/* For alloca(3) */
#include <ieeefp.h>	/* for finite(3) */
#elif	defined(__hpux)
#ifdef	__GNUC__
#include <alloca.h>	/* For alloca(3) */
#else	/* !__GNUC__ */
#define inline
#endif	/* __GNUC__ */
#else
#include <stdint.h>	/* SUSv2+ and C99 specify this file, for uintXX_t */
#endif	/* defined(sun) */
#endif

#include <netinet/in.h> /* for ntohl() */
#define	sys_ntohl(foo)	ntohl(foo)

#endif	/* defined(__vxworks) */

#endif	/* _WIN32 */

#if	__GNUC__ >= 3
#ifndef	GCC_PRINTFLIKE
#define	GCC_PRINTFLIKE(fmt,var)	__attribute__((format(printf,fmt,var)))
#endif
#ifndef	GCC_NOTUSED
#define	GCC_NOTUSED		__attribute__((unused))
#endif
#else
#ifndef	GCC_PRINTFLIKE
#define	GCC_PRINTFLIKE(fmt,var)	/* nothing */
#endif
#ifndef	GCC_NOTUSED
#define	GCC_NOTUSED
#endif
#endif

/* Figure out if thread safety is requested */
#if !defined(ASN_THREAD_SAFE) && (defined(THREAD_SAFE) || defined(_REENTRANT))
#define	ASN_THREAD_SAFE
#endif	/* Thread safety */

#ifndef	offsetof	/* If not defined by <stddef.h> */
#define	offsetof(s, m)	((ptrdiff_t)&(((s *)0)->m) - (ptrdiff_t)((s *)0))
#endif	/* offsetof */

#endif	/* ASN_SYSTEM_H */
