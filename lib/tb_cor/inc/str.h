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

#endif /* TB_STR_H */
