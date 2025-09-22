/* Copyright 2025 Raphael Outhier - confidential - proprietary - no copy - no diffusion. */

#ifndef TB_STR_H
#define TB_STR_H

/*
 * String buffers are embedded into multiple structs.
 * Their default size and copy operations are defined
 * here.
 */

/*
 * Size and len.
 */
#define TB_SIZ 64
#define TB_LEN 63

/*
 * Type.
 */
typedef char tb_str[64];

/*
 * Copy operation.
 */
static inline void tb_str_cpy(
	tb_str dst,
	const char *src
)
{
	assert(ns_str_len(src) < 64);
	char *d = dst;
	const char *s = src;
	char c;
	do {c = *(d++) = *(s++);} while (c);	
}

/***************************
 * Multi string generation *
 ***************************/

/*
 * Initialize @dst with @src0 and @src1 of known
 * lengths.
 */
static inline void tb_str_gen2(
	char *dst,
	const char *src0,
	const uad src0_len,
	const char *src1,
	const uad src1_len
)
{
	ns_mem_cpy(dst, src0, src0_len);
	dst[src0_len] = ':';
	ns_mem_cpy(&dst[src0_len + 1], src1, src1_len);
	dst[src0_len + 1 + src1_len] = 0;
}

#define TB_STR_GEN2(dst, src0, src1) \
char dst[64]; \
{ \
	const uad __src0_len = ns_str_len(src0); \
	const uad __src1_len = ns_str_len(src1); \
	assert(__src0_len < 21); \
	assert(__src1_len < 21); \
	tb_str_gen2(dst, src0, __src0_len, src1, __src1_len); \
}


#endif /* TB_STR_H */
