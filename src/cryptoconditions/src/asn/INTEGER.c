/*-
 * Copyright (c) 2003-2014 Lev Walkin <vlm@lionet.info>.
 * All rights reserved.
 * Redistribution and modifications are permitted subject to BSD license.
 */
#include <asn_internal.h>
#include <INTEGER.h>
#include <asn_codecs_prim.h>	/* Encoder and decoder of a primitive type */
#include <errno.h>

/*
 * INTEGER basic type description.
 */
static const ber_tlv_tag_t asn_DEF_INTEGER_tags[] = {
	(ASN_TAG_CLASS_UNIVERSAL | (2 << 2))
};
asn_TYPE_descriptor_t asn_DEF_INTEGER = {
	"INTEGER",
	"INTEGER",
	ASN__PRIMITIVE_TYPE_free,
	INTEGER_print,
	asn_generic_no_constraint,
	ber_decode_primitive,
	INTEGER_encode_der,
	INTEGER_decode_xer,
	INTEGER_encode_xer,
#ifdef	ASN_DISABLE_PER_SUPPORT
	0,
	0,
#else
	INTEGER_decode_uper,	/* Unaligned PER decoder */
	INTEGER_encode_uper,	/* Unaligned PER encoder */
#endif	/* ASN_DISABLE_PER_SUPPORT */
	0, /* Use generic outmost tag fetcher */
	asn_DEF_INTEGER_tags,
	sizeof(asn_DEF_INTEGER_tags) / sizeof(asn_DEF_INTEGER_tags[0]),
	asn_DEF_INTEGER_tags,	/* Same as above */
	sizeof(asn_DEF_INTEGER_tags) / sizeof(asn_DEF_INTEGER_tags[0]),
	0,	/* No PER visible constraints */
	0, 0,	/* No members */
	0	/* No specifics */
};

/*
 * Encode INTEGER type using DER.
 */
asn_enc_rval_t
INTEGER_encode_der(asn_TYPE_descriptor_t *td, void *sptr,
	int tag_mode, ber_tlv_tag_t tag,
	asn_app_consume_bytes_f *cb, void *app_key) {
	INTEGER_t *st = (INTEGER_t *)sptr;

	ASN_DEBUG("%s %s as INTEGER (tm=%d)",
		cb?"Encoding":"Estimating", td->name, tag_mode);

	/*
	 * Canonicalize integer in the buffer.
	 * (Remove too long sign extension, remove some first 0x00 bytes)
	 */
	if(st->buf) {
		uint8_t *buf = st->buf;
		uint8_t *end1 = buf + st->size - 1;
		int shift;

		/* Compute the number of superfluous leading bytes */
		for(; buf < end1; buf++) {
			/*
			 * If the contents octets of an integer value encoding
			 * consist of more than one octet, then the bits of the
			 * first octet and bit 8 of the second octet:
			 * a) shall not all be ones; and
			 * b) shall not all be zero.
			 */
			switch(*buf) {
			case 0x00: if((buf[1] & 0x80) == 0)
					continue;
				break;
			case 0xff: if((buf[1] & 0x80))
					continue;
				break;
			}
			break;
		}

		/* Remove leading superfluous bytes from the integer */
		shift = buf - st->buf;
		if(shift) {
			uint8_t *nb = st->buf;
			uint8_t *end;

			st->size -= shift;	/* New size, minus bad bytes */
			end = nb + st->size;

			for(; nb < end; nb++, buf++)
				*nb = *buf;
		}

	} /* if(1) */

	return der_encode_primitive(td, sptr, tag_mode, tag, cb, app_key);
}

static const asn_INTEGER_enum_map_t *INTEGER_map_enum2value(asn_INTEGER_specifics_t *specs, const char *lstart, const char *lstop);

/*
 * INTEGER specific human-readable output.
 */
static ssize_t
INTEGER__dump(const asn_TYPE_descriptor_t *td, const INTEGER_t *st, asn_app_consume_bytes_f *cb, void *app_key, int plainOrXER) {
	asn_INTEGER_specifics_t *specs=(asn_INTEGER_specifics_t *)td->specifics;
	char scratch[32];	/* Enough for 64-bit integer */
	uint8_t *buf = st->buf;
	uint8_t *buf_end = st->buf + st->size;
	signed long value;
	ssize_t wrote = 0;
	char *p;
	int ret;

	if(specs && specs->field_unsigned)
		ret = asn_INTEGER2ulong(st, (unsigned long *)&value);
	else
		ret = asn_INTEGER2long(st, &value);

	/* Simple case: the integer size is small */
	if(ret == 0) {
		const asn_INTEGER_enum_map_t *el;
		size_t scrsize;
		char *scr;

		el = (value >= 0 || !specs || !specs->field_unsigned)
			? INTEGER_map_value2enum(specs, value) : 0;
		if(el) {
			scrsize = el->enum_len + 32;
			scr = (char *)alloca(scrsize);
			if(plainOrXER == 0)
				ret = snprintf(scr, scrsize,
					"%ld (%s)", value, el->enum_name);
			else
				ret = snprintf(scr, scrsize,
					"<%s/>", el->enum_name);
		} else if(plainOrXER && specs && specs->strict_enumeration) {
			ASN_DEBUG("ASN.1 forbids dealing with "
				"unknown value of ENUMERATED type");
			errno = EPERM;
			return -1;
		} else {
			scrsize = sizeof(scratch);
			scr = scratch;
			ret = snprintf(scr, scrsize,
				(specs && specs->field_unsigned)
				?"%lu":"%ld", value);
		}
		assert(ret > 0 && (size_t)ret < scrsize);
		return (cb(scr, ret, app_key) < 0) ? -1 : ret;
	} else if(plainOrXER && specs && specs->strict_enumeration) {
		/*
		 * Here and earlier, we cannot encode the ENUMERATED values
		 * if there is no corresponding identifier.
		 */
		ASN_DEBUG("ASN.1 forbids dealing with "
			"unknown value of ENUMERATED type");
		errno = EPERM;
		return -1;
	}

	/* Output in the long xx:yy:zz... format */
	/* TODO: replace with generic algorithm (Knuth TAOCP Vol 2, 4.3.1) */
	for(p = scratch; buf < buf_end; buf++) {
		const char * const h2c = "0123456789ABCDEF";
		if((p - scratch) >= (ssize_t)(sizeof(scratch) - 4)) {
			/* Flush buffer */
			if(cb(scratch, p - scratch, app_key) < 0)
				return -1;
			wrote += p - scratch;
			p = scratch;
		}
		*p++ = h2c[*buf >> 4];
		*p++ = h2c[*buf & 0x0F];
		*p++ = 0x3a;	/* ":" */
	}
	if(p != scratch)
		p--;	/* Remove the last ":" */

	wrote += p - scratch;
	return (cb(scratch, p - scratch, app_key) < 0) ? -1 : wrote;
}

/*
 * INTEGER specific human-readable output.
 */
int
INTEGER_print(asn_TYPE_descriptor_t *td, const void *sptr, int ilevel,
	asn_app_consume_bytes_f *cb, void *app_key) {
	const INTEGER_t *st = (const INTEGER_t *)sptr;
	ssize_t ret;

	(void)td;
	(void)ilevel;

	if(!st || !st->buf)
		ret = cb("<absent>", 8, app_key);
	else
		ret = INTEGER__dump(td, st, cb, app_key, 0);

	return (ret < 0) ? -1 : 0;
}

struct e2v_key {
	const char *start;
	const char *stop;
	const asn_INTEGER_enum_map_t *vemap;
	const unsigned int *evmap;
};
static int
INTEGER__compar_enum2value(const void *kp, const void *am) {
	const struct e2v_key *key = (const struct e2v_key *)kp;
	const asn_INTEGER_enum_map_t *el = (const asn_INTEGER_enum_map_t *)am;
	const char *ptr, *end, *name;

	/* Remap the element (sort by different criterion) */
	el = key->vemap + key->evmap[el - key->vemap];

	/* Compare strings */
	for(ptr = key->start, end = key->stop, name = el->enum_name;
			ptr < end; ptr++, name++) {
		if(*ptr != *name)
			return *(const unsigned char *)ptr
				- *(const unsigned char *)name;
	}
	return name[0] ? -1 : 0;
}

static const asn_INTEGER_enum_map_t *
INTEGER_map_enum2value(asn_INTEGER_specifics_t *specs, const char *lstart, const char *lstop) {
	const asn_INTEGER_enum_map_t *el_found;
	int count = specs ? specs->map_count : 0;
	struct e2v_key key;
	const char *lp;

	if(!count) return NULL;

	/* Guaranteed: assert(lstart < lstop); */
	/* Figure out the tag name */
	for(lstart++, lp = lstart; lp < lstop; lp++) {
		switch(*lp) {
		case 9: case 10: case 11: case 12: case 13: case 32: /* WSP */
		case 0x2f: /* '/' */ case 0x3e: /* '>' */
			break;
		default:
			continue;
		}
		break;
	}
	if(lp == lstop) return NULL;	/* No tag found */
	lstop = lp;

	key.start = lstart;
	key.stop = lstop;
	key.vemap = specs->value2enum;
	key.evmap = specs->enum2value;
	el_found = (asn_INTEGER_enum_map_t *)bsearch(&key,
		specs->value2enum, count, sizeof(specs->value2enum[0]),
		INTEGER__compar_enum2value);
	if(el_found) {
		/* Remap enum2value into value2enum */
		el_found = key.vemap + key.evmap[el_found - key.vemap];
	}
	return el_found;
}

static int
INTEGER__compar_value2enum(const void *kp, const void *am) {
	long a = *(const long *)kp;
	const asn_INTEGER_enum_map_t *el = (const asn_INTEGER_enum_map_t *)am;
	long b = el->nat_value;
	if(a < b) return -1;
	else if(a == b) return 0;
	else return 1;
}

const asn_INTEGER_enum_map_t *
INTEGER_map_value2enum(asn_INTEGER_specifics_t *specs, long value) {
	int count = specs ? specs->map_count : 0;
	if(!count) return 0;
	return (asn_INTEGER_enum_map_t *)bsearch(&value, specs->value2enum,
		count, sizeof(specs->value2enum[0]),
		INTEGER__compar_value2enum);
}

static int
INTEGER_st_prealloc(INTEGER_t *st, int min_size) {
	void *p = MALLOC(min_size + 1);
	if(p) {
		void *b = st->buf;
		st->size = 0;
		st->buf = p;
		FREEMEM(b);
		return 0;
	} else {
		return -1;
	}
}

/*
 * Decode the chunk of XML text encoding INTEGER.
 */
static enum xer_pbd_rval
INTEGER__xer_body_decode(asn_TYPE_descriptor_t *td, void *sptr, const void *chunk_buf, size_t chunk_size) {
	INTEGER_t *st = (INTEGER_t *)sptr;
	long dec_value;
	long hex_value = 0;
	const char *lp;
	const char *lstart = (const char *)chunk_buf;
	const char *lstop = lstart + chunk_size;
	enum {
		ST_LEADSPACE,
		ST_SKIPSPHEX,
		ST_WAITDIGITS,
		ST_DIGITS,
		ST_DIGITS_TRAILSPACE,
		ST_HEXDIGIT1,
		ST_HEXDIGIT2,
		ST_HEXDIGITS_TRAILSPACE,
		ST_HEXCOLON,
		ST_END_ENUM,
		ST_UNEXPECTED
	} state = ST_LEADSPACE;
	const char *dec_value_start = 0; /* INVARIANT: always !0 in ST_DIGITS */
	const char *dec_value_end = 0;

	if(chunk_size)
		ASN_DEBUG("INTEGER body %ld 0x%2x..0x%2x",
			(long)chunk_size, *lstart, lstop[-1]);

	if(INTEGER_st_prealloc(st, (chunk_size/3) + 1))
		return XPBD_SYSTEM_FAILURE;

	/*
	 * We may have received a tag here. It will be processed inline.
	 * Use strtoul()-like code and serialize the result.
	 */
	for(lp = lstart; lp < lstop; lp++) {
		int lv = *lp;
		switch(lv) {
		case 0x09: case 0x0a: case 0x0d: case 0x20:
			switch(state) {
			case ST_LEADSPACE:
			case ST_DIGITS_TRAILSPACE:
			case ST_HEXDIGITS_TRAILSPACE:
			case ST_SKIPSPHEX:
				continue;
			case ST_DIGITS:
				dec_value_end = lp;
				state = ST_DIGITS_TRAILSPACE;
				continue;
			case ST_HEXCOLON:
				state = ST_HEXDIGITS_TRAILSPACE;
				continue;
			default:
				break;
			}
			break;
		case 0x2d:	/* '-' */
			if(state == ST_LEADSPACE) {
				dec_value = 0;
				dec_value_start = lp;
				state = ST_WAITDIGITS;
				continue;
			}
			break;
		case 0x2b:	/* '+' */
			if(state == ST_LEADSPACE) {
				dec_value = 0;
				dec_value_start = lp;
				state = ST_WAITDIGITS;
				continue;
			}
			break;
		case 0x30: case 0x31: case 0x32: case 0x33: case 0x34:
		case 0x35: case 0x36: case 0x37: case 0x38: case 0x39:
			switch(state) {
			case ST_DIGITS: continue;
			case ST_SKIPSPHEX:	/* Fall through */
			case ST_HEXDIGIT1:
				hex_value = (lv - 0x30) << 4;
				state = ST_HEXDIGIT2;
				continue;
			case ST_HEXDIGIT2:
				hex_value += (lv - 0x30);
				state = ST_HEXCOLON;
				st->buf[st->size++] = (uint8_t)hex_value;
				continue;
			case ST_HEXCOLON:
				return XPBD_BROKEN_ENCODING;
			case ST_LEADSPACE:
				dec_value = 0;
				dec_value_start = lp;
				/* FALL THROUGH */
			case ST_WAITDIGITS:
				state = ST_DIGITS;
				continue;
			default:
				break;
			}
			break;
		case 0x3c:	/* '<', start of XML encoded enumeration */
			if(state == ST_LEADSPACE) {
				const asn_INTEGER_enum_map_t *el;
				el = INTEGER_map_enum2value(
					(asn_INTEGER_specifics_t *)
					td->specifics, lstart, lstop);
				if(el) {
					ASN_DEBUG("Found \"%s\" => %ld",
						el->enum_name, el->nat_value);
					dec_value = el->nat_value;
					state = ST_END_ENUM;
					lp = lstop - 1;
					continue;
				}
				ASN_DEBUG("Unknown identifier for INTEGER");
			}
			return XPBD_BROKEN_ENCODING;
		case 0x3a:	/* ':' */
			if(state == ST_HEXCOLON) {
				/* This colon is expected */
				state = ST_HEXDIGIT1;
				continue;
			} else if(state == ST_DIGITS) {
				/* The colon here means that we have
				 * decoded the first two hexadecimal
				 * places as a decimal value.
				 * Switch decoding mode. */
				ASN_DEBUG("INTEGER re-evaluate as hex form");
				state = ST_SKIPSPHEX;
				dec_value_start = 0;
				lp = lstart - 1;
				continue;
			} else {
				ASN_DEBUG("state %d at %ld", state, (long)(lp - lstart));
				break;
			}
		/* [A-Fa-f] */
		case 0x41:case 0x42:case 0x43:case 0x44:case 0x45:case 0x46:
		case 0x61:case 0x62:case 0x63:case 0x64:case 0x65:case 0x66:
			switch(state) {
			case ST_SKIPSPHEX:
			case ST_LEADSPACE: /* Fall through */
			case ST_HEXDIGIT1:
				hex_value = lv - ((lv < 0x61) ? 0x41 : 0x61);
				hex_value += 10;
				hex_value <<= 4;
				state = ST_HEXDIGIT2;
				continue;
			case ST_HEXDIGIT2:
				hex_value += lv - ((lv < 0x61) ? 0x41 : 0x61);
				hex_value += 10;
				st->buf[st->size++] = (uint8_t)hex_value;
				state = ST_HEXCOLON;
				continue;
			case ST_DIGITS:
				ASN_DEBUG("INTEGER re-evaluate as hex form");
				state = ST_SKIPSPHEX;
				dec_value_start = 0;
				lp = lstart - 1;
				continue;
			default:
				break;
			}
			break;
		}

		/* Found extra non-numeric stuff */
		ASN_DEBUG("INTEGER :: Found non-numeric 0x%2x at %ld",
			lv, (long)(lp - lstart));
		state = ST_UNEXPECTED;
		break;
	}

	switch(state) {
	case ST_END_ENUM:
		/* Got a complete and valid enumeration encoded as a tag. */
		break;
	case ST_DIGITS:
		dec_value_end = lstop;
		/* FALL THROUGH */
	case ST_DIGITS_TRAILSPACE:
		/* The last symbol encountered was a digit. */
		switch(asn_strtol_lim(dec_value_start, &dec_value_end, &dec_value)) {
		case ASN_STRTOL_OK:
			break;
		case ASN_STRTOL_ERROR_RANGE:
			return XPBD_DECODER_LIMIT;
		case ASN_STRTOL_ERROR_INVAL:
		case ASN_STRTOL_EXPECT_MORE:
		case ASN_STRTOL_EXTRA_DATA:
			return XPBD_BROKEN_ENCODING;
		}
		break;
	case ST_HEXCOLON:
	case ST_HEXDIGITS_TRAILSPACE:
		st->buf[st->size] = 0;	/* Just in case termination */
		return XPBD_BODY_CONSUMED;
	case ST_HEXDIGIT1:
	case ST_HEXDIGIT2:
	case ST_SKIPSPHEX:
		return XPBD_BROKEN_ENCODING;
	case ST_LEADSPACE:
		/* Content not found */
		return XPBD_NOT_BODY_IGNORE;
	case ST_WAITDIGITS:
	case ST_UNEXPECTED:
		ASN_DEBUG("INTEGER: No useful digits (state %d)", state);
		return XPBD_BROKEN_ENCODING;	/* No digits */
	}

	/*
	 * Convert the result of parsing of enumeration or a straight
	 * decimal value into a BER representation.
	 */
	if(asn_long2INTEGER(st, dec_value))
		return XPBD_SYSTEM_FAILURE;

	return XPBD_BODY_CONSUMED;
}

asn_dec_rval_t
INTEGER_decode_xer(asn_codec_ctx_t *opt_codec_ctx,
	asn_TYPE_descriptor_t *td, void **sptr, const char *opt_mname,
		const void *buf_ptr, size_t size) {

	return xer_decode_primitive(opt_codec_ctx, td,
		sptr, sizeof(INTEGER_t), opt_mname,
		buf_ptr, size, INTEGER__xer_body_decode);
}

asn_enc_rval_t
INTEGER_encode_xer(asn_TYPE_descriptor_t *td, void *sptr,
	int ilevel, enum xer_encoder_flags_e flags,
		asn_app_consume_bytes_f *cb, void *app_key) {
	const INTEGER_t *st = (const INTEGER_t *)sptr;
	asn_enc_rval_t er;

	(void)ilevel;
	(void)flags;
	
	if(!st || !st->buf)
		ASN__ENCODE_FAILED;

	er.encoded = INTEGER__dump(td, st, cb, app_key, 1);
	if(er.encoded < 0) ASN__ENCODE_FAILED;

	ASN__ENCODED_OK(er);
}

#ifndef	ASN_DISABLE_PER_SUPPORT

asn_dec_rval_t
INTEGER_decode_uper(asn_codec_ctx_t *opt_codec_ctx, asn_TYPE_descriptor_t *td,
	asn_per_constraints_t *constraints, void **sptr, asn_per_data_t *pd) {
	asn_INTEGER_specifics_t *specs=(asn_INTEGER_specifics_t *)td->specifics;
	asn_dec_rval_t rval = { RC_OK, 0 };
	INTEGER_t *st = (INTEGER_t *)*sptr;
	asn_per_constraint_t *ct;
	int repeat;

	(void)opt_codec_ctx;

	if(!st) {
		st = (INTEGER_t *)(*sptr = CALLOC(1, sizeof(*st)));
		if(!st) ASN__DECODE_FAILED;
	}

	if(!constraints) constraints = td->per_constraints;
	ct = constraints ? &constraints->value : 0;

	if(ct && ct->flags & APC_EXTENSIBLE) {
		int inext = per_get_few_bits(pd, 1);
		if(inext < 0) ASN__DECODE_STARVED;
		if(inext) ct = 0;
	}

	FREEMEM(st->buf);
	st->buf = 0;
	st->size = 0;
	if(ct) {
		if(ct->flags & APC_SEMI_CONSTRAINED) {
			st->buf = (uint8_t *)CALLOC(1, 2);
			if(!st->buf) ASN__DECODE_FAILED;
			st->size = 1;
		} else if(ct->flags & APC_CONSTRAINED && ct->range_bits >= 0) {
			size_t size = (ct->range_bits + 7) >> 3;
			st->buf = (uint8_t *)MALLOC(1 + size + 1);
			if(!st->buf) ASN__DECODE_FAILED;
			st->size = size;
		}
	}

	/* X.691-2008/11, #13.2.2, constrained whole number */
	if(ct && ct->flags != APC_UNCONSTRAINED) {
		/* #11.5.6 */
		ASN_DEBUG("Integer with range %d bits", ct->range_bits);
		if(ct->range_bits >= 0) {
			if((size_t)ct->range_bits > 8 * sizeof(unsigned long))
				ASN__DECODE_FAILED;

			if(specs && specs->field_unsigned) {
				unsigned long uvalue;
				if(uper_get_constrained_whole_number(pd,
					&uvalue, ct->range_bits))
					ASN__DECODE_STARVED;
				ASN_DEBUG("Got value %lu + low %ld",
					uvalue, ct->lower_bound);
				uvalue += ct->lower_bound;
				if(asn_ulong2INTEGER(st, uvalue))
					ASN__DECODE_FAILED;
			} else {
				unsigned long svalue;
				if(uper_get_constrained_whole_number(pd,
					&svalue, ct->range_bits))
					ASN__DECODE_STARVED;
				ASN_DEBUG("Got value %ld + low %ld",
					svalue, ct->lower_bound);
				svalue += ct->lower_bound;
				if(asn_long2INTEGER(st, svalue))
					ASN__DECODE_FAILED;
			}
			return rval;
		}
	} else {
		ASN_DEBUG("Decoding unconstrained integer %s", td->name);
	}

	/* X.691, #12.2.3, #12.2.4 */
	do {
		ssize_t len;
		void *p;
		int ret;

		/* Get the PER length */
		len = uper_get_length(pd, -1, &repeat);
		if(len < 0) ASN__DECODE_STARVED;

		p = REALLOC(st->buf, st->size + len + 1);
		if(!p) ASN__DECODE_FAILED;
		st->buf = (uint8_t *)p;

		ret = per_get_many_bits(pd, &st->buf[st->size], 0, 8 * len);
		if(ret < 0) ASN__DECODE_STARVED;
		st->size += len;
	} while(repeat);
	st->buf[st->size] = 0;	/* JIC */

	/* #12.2.3 */
	if(ct && ct->lower_bound) {
		/*
		 * TODO: replace by in-place arithmetics.
		 */
		long value;
		if(asn_INTEGER2long(st, &value))
			ASN__DECODE_FAILED;
		if(asn_long2INTEGER(st, value + ct->lower_bound))
			ASN__DECODE_FAILED;
	}

	return rval;
}

asn_enc_rval_t
INTEGER_encode_uper(asn_TYPE_descriptor_t *td,
	asn_per_constraints_t *constraints, void *sptr, asn_per_outp_t *po) {
	asn_INTEGER_specifics_t *specs=(asn_INTEGER_specifics_t *)td->specifics;
	asn_enc_rval_t er;
	INTEGER_t *st = (INTEGER_t *)sptr;
	const uint8_t *buf;
	const uint8_t *end;
	asn_per_constraint_t *ct;
	long value = 0;
	unsigned long v = 0;

	if(!st || st->size == 0) ASN__ENCODE_FAILED;

	if(!constraints) constraints = td->per_constraints;
	ct = constraints ? &constraints->value : 0;

	er.encoded = 0;

	if(ct) {
		int inext = 0;
		if(specs && specs->field_unsigned) {
			unsigned long uval;
			if(asn_INTEGER2ulong(st, &uval))
				ASN__ENCODE_FAILED;
			/* Check proper range */
			if(ct->flags & APC_SEMI_CONSTRAINED) {
				if(uval < (unsigned long)ct->lower_bound)
					inext = 1;
			} else if(ct->range_bits >= 0) {
				if(uval < (unsigned long)ct->lower_bound
				|| uval > (unsigned long)ct->upper_bound)
					inext = 1;
			}
			ASN_DEBUG("Value %lu (%02x/%d) lb %lu ub %lu %s",
				uval, st->buf[0], st->size,
				ct->lower_bound, ct->upper_bound,
				inext ? "ext" : "fix");
			value = uval;
		} else {
			if(asn_INTEGER2long(st, &value))
				ASN__ENCODE_FAILED;
			/* Check proper range */
			if(ct->flags & APC_SEMI_CONSTRAINED) {
				if(value < ct->lower_bound)
					inext = 1;
			} else if(ct->range_bits >= 0) {
				if(value < ct->lower_bound
				|| value > ct->upper_bound)
					inext = 1;
			}
			ASN_DEBUG("Value %ld (%02x/%d) lb %ld ub %ld %s",
				value, st->buf[0], st->size,
				ct->lower_bound, ct->upper_bound,
				inext ? "ext" : "fix");
		}
		if(ct->flags & APC_EXTENSIBLE) {
			if(per_put_few_bits(po, inext, 1))
				ASN__ENCODE_FAILED;
			if(inext) ct = 0;
		} else if(inext) {
			ASN__ENCODE_FAILED;
		}
	}


	/* X.691-11/2008, #13.2.2, test if constrained whole number */
	if(ct && ct->range_bits >= 0) {
		/* #11.5.6 -> #11.3 */
		ASN_DEBUG("Encoding integer %ld (%lu) with range %d bits",
			value, value - ct->lower_bound, ct->range_bits);
		v = value - ct->lower_bound;
		if(uper_put_constrained_whole_number_u(po, v, ct->range_bits))
			ASN__ENCODE_FAILED;
		ASN__ENCODED_OK(er);
	}

	if(ct && ct->lower_bound) {
		ASN_DEBUG("Adjust lower bound to %ld", ct->lower_bound);
		/* TODO: adjust lower bound */
		ASN__ENCODE_FAILED;
	}

	for(buf = st->buf, end = st->buf + st->size; buf < end;) {
		ssize_t mayEncode = uper_put_length(po, end - buf);
		if(mayEncode < 0)
			ASN__ENCODE_FAILED;
		if(per_put_many_bits(po, buf, 8 * mayEncode))
			ASN__ENCODE_FAILED;
		buf += mayEncode;
	}

	ASN__ENCODED_OK(er);
}

#endif	/* ASN_DISABLE_PER_SUPPORT */

int
asn_INTEGER2long(const INTEGER_t *iptr, long *lptr) {
	uint8_t *b, *end;
	size_t size;
	long l;

	/* Sanity checking */
	if(!iptr || !iptr->buf || !lptr) {
		errno = EINVAL;
		return -1;
	}

	/* Cache the begin/end of the buffer */
	b = iptr->buf;	/* Start of the INTEGER buffer */
	size = iptr->size;
	end = b + size;	/* Where to stop */

	if(size > sizeof(long)) {
		uint8_t *end1 = end - 1;
		/*
		 * Slightly more advanced processing,
		 * able to >sizeof(long) bytes,
		 * when the actual value is small
		 * (0x0000000000abcdef would yield a fine 0x00abcdef)
		 */
		/* Skip out the insignificant leading bytes */
		for(; b < end1; b++) {
			switch(*b) {
			case 0x00: if((b[1] & 0x80) == 0) continue; break;
			case 0xff: if((b[1] & 0x80) != 0) continue; break;
			}
			break;
		}

		size = end - b;
		if(size > sizeof(long)) {
			/* Still cannot fit the long */
			errno = ERANGE;
			return -1;
		}
	}

	/* Shortcut processing of a corner case */
	if(end == b) {
		*lptr = 0;
		return 0;
	}

	/* Perform the sign initialization */
	/* Actually l = -(*b >> 7); gains nothing, yet unreadable! */
	if((*b >> 7)) l = -1; else l = 0;

	/* Conversion engine */
	for(; b < end; b++)
		l = (l << 8) | *b;

	*lptr = l;
	return 0;
}

int
asn_INTEGER2ulong(const INTEGER_t *iptr, unsigned long *lptr) {
	uint8_t *b, *end;
	unsigned long l;
	size_t size;

	if(!iptr || !iptr->buf || !lptr) {
		errno = EINVAL;
		return -1;
	}

	b = iptr->buf;
	size = iptr->size;
	end = b + size;

	/* If all extra leading bytes are zeroes, ignore them */
	for(; size > sizeof(unsigned long); b++, size--) {
		if(*b) {
			/* Value won't fit unsigned long */
			errno = ERANGE;
			return -1;
		}
	}

	/* Conversion engine */
	for(l = 0; b < end; b++)
		l = (l << 8) | *b;

	*lptr = l;
	return 0;
}

int
asn_ulong2INTEGER(INTEGER_t *st, unsigned long value) {
	uint8_t *buf;
	uint8_t *end;
	uint8_t *b;
	int shr;

	if(value <= LONG_MAX)
		return asn_long2INTEGER(st, value);

	buf = (uint8_t *)MALLOC(1 + sizeof(value));
	if(!buf) return -1;

	end = buf + (sizeof(value) + 1);
	buf[0] = 0;
	for(b = buf + 1, shr = (sizeof(long)-1)*8; b < end; shr -= 8, b++)
		*b = (uint8_t)(value >> shr);

	if(st->buf) FREEMEM(st->buf);
	st->buf = buf;
	st->size = 1 + sizeof(value);

	return 0;
}

int
asn_long2INTEGER(INTEGER_t *st, long value) {
	uint8_t *buf, *bp;
	uint8_t *p;
	uint8_t *pstart;
	uint8_t *pend1;
	int littleEndian = 1;	/* Run-time detection */
	int add;

	if(!st) {
		errno = EINVAL;
		return -1;
	}

	buf = (uint8_t *)MALLOC(sizeof(value));
	if(!buf) return -1;

	if(*(char *)&littleEndian) {
		pstart = (uint8_t *)&value + sizeof(value) - 1;
		pend1 = (uint8_t *)&value;
		add = -1;
	} else {
		pstart = (uint8_t *)&value;
		pend1 = pstart + sizeof(value) - 1;
		add = 1;
	}

	/*
	 * If the contents octet consists of more than one octet,
	 * then bits of the first octet and bit 8 of the second octet:
	 * a) shall not all be ones; and
	 * b) shall not all be zero.
	 */
	for(p = pstart; p != pend1; p += add) {
		switch(*p) {
		case 0x00: if((*(p+add) & 0x80) == 0)
				continue;
			break;
		case 0xff: if((*(p+add) & 0x80))
				continue;
			break;
		}
		break;
	}
	/* Copy the integer body */
	for(pstart = p, bp = buf, pend1 += add; p != pend1; p += add)
		*bp++ = *p;

	if(st->buf) FREEMEM(st->buf);
	st->buf = buf;
	st->size = bp - buf;

	return 0;
}

/*
 * This function is going to be DEPRECATED soon.
 */
enum asn_strtol_result_e
asn_strtol(const char *str, const char *end, long *lp) {
    const char *endp = end;

    switch(asn_strtol_lim(str, &endp, lp)) {
    case ASN_STRTOL_ERROR_RANGE:
        return ASN_STRTOL_ERROR_RANGE;
    case ASN_STRTOL_ERROR_INVAL:
        return ASN_STRTOL_ERROR_INVAL;
    case ASN_STRTOL_EXPECT_MORE:
        return ASN_STRTOL_ERROR_INVAL;  /* Retain old behavior */
    case ASN_STRTOL_OK:
        return ASN_STRTOL_OK;
    case ASN_STRTOL_EXTRA_DATA:
        return ASN_STRTOL_ERROR_INVAL;  /* Retain old behavior */
    }

    return ASN_STRTOL_ERROR_INVAL;  /* Retain old behavior */
}

/*
 * Parse the number in the given string until the given *end position,
 * returning the position after the last parsed character back using the
 * same (*end) pointer.
 * WARNING: This behavior is different from the standard strtol(3).
 */
enum asn_strtol_result_e
asn_strtol_lim(const char *str, const char **end, long *lp) {
	int sign = 1;
	long l;

	const long upper_boundary = LONG_MAX / 10;
	long last_digit_max = LONG_MAX % 10;

	if(str >= *end) return ASN_STRTOL_ERROR_INVAL;

	switch(*str) {
	case '-':
		last_digit_max++;
		sign = -1;
		/* FALL THROUGH */
	case '+':
		str++;
		if(str >= *end) {
			*end = str;
			return ASN_STRTOL_EXPECT_MORE;
		}
	}

	for(l = 0; str < (*end); str++) {
		switch(*str) {
		case 0x30: case 0x31: case 0x32: case 0x33: case 0x34:
		case 0x35: case 0x36: case 0x37: case 0x38: case 0x39: {
			int d = *str - '0';
			if(l < upper_boundary) {
				l = l * 10 + d;
			} else if(l == upper_boundary) {
				if(d <= last_digit_max) {
					if(sign > 0) {
						l = l * 10 + d;
					} else {
						sign = 1;
						l = -l * 10 - d;
					}
				} else {
					*end = str;
					return ASN_STRTOL_ERROR_RANGE;
				}
			} else {
				*end = str;
				return ASN_STRTOL_ERROR_RANGE;
			}
		    }
		    continue;
		default:
		    *end = str;
		    *lp = sign * l;
		    return ASN_STRTOL_EXTRA_DATA;
		}
	}

	*end = str;
	*lp = sign * l;
	return ASN_STRTOL_OK;
}

