/*-
 * Copyright (c) 2003, 2004, 2005, 2007 Lev Walkin <vlm@lionet.info>.
 * All rights reserved.
 * Redistribution and modifications are permitted subject to BSD license.
 */
/*
 * Declarations internally useful for the ASN.1 support code.
 */
#ifndef	ASN_INTERNAL_H
#define	ASN_INTERNAL_H

#include "asn_application.h"	/* Application-visible API */

#ifndef	__NO_ASSERT_H__		/* Include assert.h only for internal use. */
#include <assert.h>		/* for assert() macro */
#endif

#ifdef	__cplusplus
extern "C" {
#endif

/* Environment version might be used to avoid running with the old library */
#define	ASN1C_ENVIRONMENT_VERSION	923	/* Compile-time version */
int get_asn1c_environment_version(void);	/* Run-time version */

#define	CALLOC(nmemb, size)	calloc(nmemb, size)
#define	MALLOC(size)		malloc(size)
#define	REALLOC(oldptr, size)	realloc(oldptr, size)
#define	FREEMEM(ptr)		free(ptr)

#define	asn_debug_indent	0
#define ASN_DEBUG_INDENT_ADD(i) do{}while(0)

/*
 * A macro for debugging the ASN.1 internals.
 * You may enable or override it.
 */
#ifndef	ASN_DEBUG	/* If debugging code is not defined elsewhere... */
#if	EMIT_ASN_DEBUG == 1	/* And it was asked to emit this code... */
#ifdef	__GNUC__
#ifdef	ASN_THREAD_SAFE
/* Thread safety requires sacrifice in output indentation:
 * Retain empty definition of ASN_DEBUG_INDENT_ADD. */
#else	/* !ASN_THREAD_SAFE */
#undef  ASN_DEBUG_INDENT_ADD
#undef  asn_debug_indent
int asn_debug_indent;
#define ASN_DEBUG_INDENT_ADD(i) do { asn_debug_indent += i; } while(0)
#endif	/* ASN_THREAD_SAFE */
#define	ASN_DEBUG(fmt, args...)	do {			\
		int adi = asn_debug_indent;		\
		while(adi--) fprintf(stderr, " ");	\
		fprintf(stderr, fmt, ##args);		\
		fprintf(stderr, " (%s:%d)\n",		\
			__FILE__, __LINE__);		\
	} while(0)
#else	/* !__GNUC__ */
void ASN_DEBUG_f(const char *fmt, ...);
#define	ASN_DEBUG	ASN_DEBUG_f
#endif	/* __GNUC__ */
#else	/* EMIT_ASN_DEBUG != 1 */
static void ASN_DEBUG(const char *fmt, ...) { (void)fmt; }
#endif	/* EMIT_ASN_DEBUG */
#endif	/* ASN_DEBUG */

/*
 * Invoke the application-supplied callback and fail, if something is wrong.
 */
#define	ASN__E_cbc(buf, size)	(cb((buf), (size), app_key) < 0)
#define	ASN__E_CALLBACK(foo)	do {					\
		if(foo)	goto cb_failed;					\
	} while(0)
#define	ASN__CALLBACK(buf, size)					\
	ASN__E_CALLBACK(ASN__E_cbc(buf, size))
#define	ASN__CALLBACK2(buf1, size1, buf2, size2)			\
	ASN__E_CALLBACK(ASN__E_cbc(buf1, size1) || ASN__E_cbc(buf2, size2))
#define	ASN__CALLBACK3(buf1, size1, buf2, size2, buf3, size3)		\
	ASN__E_CALLBACK(ASN__E_cbc(buf1, size1)			\
		|| ASN__E_cbc(buf2, size2)				\
		|| ASN__E_cbc(buf3, size3))

#define	ASN__TEXT_INDENT(nl, level) do {            \
        int tmp_level = (level);                    \
        int tmp_nl = ((nl) != 0);                   \
        int tmp_i;                                  \
        if(tmp_nl) ASN__CALLBACK("\n", 1);          \
        if(tmp_level < 0) tmp_level = 0;            \
        for(tmp_i = 0; tmp_i < tmp_level; tmp_i++)  \
            ASN__CALLBACK("    ", 4);               \
        er.encoded += tmp_nl + 4 * tmp_level;       \
    } while(0)

#define	_i_INDENT(nl)	do {                        \
        int tmp_i;                                  \
        if((nl) && cb("\n", 1, app_key) < 0)        \
            return -1;                              \
        for(tmp_i = 0; tmp_i < ilevel; tmp_i++)     \
            if(cb("    ", 4, app_key) < 0)          \
                return -1;                          \
    } while(0)

/*
 * Check stack against overflow, if limit is set.
 */
#define	ASN__DEFAULT_STACK_MAX	(30000)
static int __attribute__((unused))
ASN__STACK_OVERFLOW_CHECK(asn_codec_ctx_t *ctx) {
	if(ctx && ctx->max_stack_size) {

		/* ctx MUST be allocated on the stack */
		ptrdiff_t usedstack = ((char *)ctx - (char *)&ctx);
		if(usedstack > 0) usedstack = -usedstack; /* grows up! */

		/* double negative required to avoid int wrap-around */
		if(usedstack < -(ptrdiff_t)ctx->max_stack_size) {
			ASN_DEBUG("Stack limit %ld reached",
				(long)ctx->max_stack_size);
			return -1;
		}
	}
	return 0;
}

#ifdef	__cplusplus
}
#endif

#endif	/* ASN_INTERNAL_H */
