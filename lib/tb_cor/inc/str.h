/* Copyright 2025 Raphael Outhier - confidential - proprietary - no copy - no diffusion. */

#ifndef SP_COR_STR_H
#define SP_COR_STR_H

/*
 * String buffers are embedded into multiple structs.
 * Their default size and copy operations are defined
 * here.
 */

/*
 * Size and len.
 */
#define SP_STR_SIZ 64
#define SP_STR_LEN 63

/*
 * Type.
 */
typedef char sp_str[64];

/*
 * Copy operation.
 */
static inline void sp_str_cpy(
	sp_str dst,
	const char *src
) {
	assert(ns_str_len(src) < 64);
	char *d = dst;
	const char *s = src;
	char c;
	do {c = *(d++) = *(s++);} while (c);	
}

#define sp_str_ct_imp(dst, val) stra_ct_imp(dst, val, SP_STR_SIZ)
#define sp_str_ct_exp(val) stra_ct_exp(val, SP_STR_SIZ)

#endif /* SP_COR_STR_H */
