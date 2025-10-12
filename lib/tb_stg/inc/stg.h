/* Copyright 2025 Raphael Outhier - confidential - proprietary - no copy - no diffusion. */

#ifndef TB_COR_STG_H
#define TB_COR_STG_H

/**************
 * System API *
 **************/

/*
 * Create and initialize the storage directory @pth
 * to contain actual storage data.
 * Return 0 if success, 1 if failure, 2 if already
 * initialized.
 */
uerr tb_stg_ini(
	const char *pth
);

/*
 * Construct and return a data system operating
 * on the storage directory at @pth.
 * If an error occurs, return 0.
 */ 
tb_stg_sys *tb_stg_ctr(
	const char *pth
);

/*
 * Delete @sys.
 * It must have no active data interface.
 */
void tb_stg_dtr(
	tb_stg_sys *sys
);

/*************
 * Index API *
 *************/

/*
 * Open and return the index for "@mkp:@ist:@lvl". 
 * If @wrt is set, attempt to open with write privileges,
 * and if success, store the write key (non 0) at @keyp,
 * and return 0; if failure, return 1.
 * Otherwise, return 0 (always succeeds).
 */
tb_stg_idx *tb_stg_opn(
	tb_stg_sys *sys,
	const char *mkp,
	const char *ist,
	u8 lvl,
	u8 wrt,
	u64 *key
);

/*
 * Close @idx.
 * If @key is set, release its write privileges; in this
 * case, @key must match the key that was provided when
 * @idx was opened with write privileges.
 */
void tb_stg_cls(
	tb_stg_idx *idx,
	u64 key
);

/************
 * Read API *
 ************/

/*
 * If @idx has no block covering @tim, return 0.
 * If @idx has a block covering @tim, load it in memory,
 * and return it.
 */
_own_ tb_stg_blk *tb_stg_lod(
	tb_stg_idx *idx,
	u64 tim
);

/*
 * Unload @blk.
 */
void tb_stg_unl(
	_own_ tb_stg_blk *blk
);

/*
 * If @blk is validated, return 0.
 * If @blk is not validated, return 1.
 */
uerr tb_stg_val_get(
	tb_stg_blk *blk
);

/*
 * Attempt to acquire the ownership of @blk's validation.
 * If success, return 0.
 * If someone already acquired it, return 1.
 */
uerr tb_stg_val_ini(
	tb_stg_blk *blk
);

/*
 * Report @blk validated.
 */
void tb_stg_val_set(
	tb_stg_blk *blk
);

/*
 * Initialize @dsts with @blk's arrays, set
 * write the time of @blk's first data element at *@timp,
 * write the number of elements of the arrays at *@nbp.
 */
void tb_sgm_arr(
	tb_stg_blk *blk,
	void **dsts,
	u8 dst_nb,
	u64 *timp,
	u64 *nbp
);

/*
 * Return @blk's second tier data.
 */
static inline void *tb_sgm_std(
	tb_stg_blk *blk
) {return tb_sgm_rgn(blk->sgm, 1);}

/*************
 * Write API *
 *************/

/*
 * Write functions conform to the following behavior :
 * - they write exactly @nb elements at the end of the
 *   stored data.
 */

/*
 * Generic data write.
 * @stt is optional
 */
void tb_stg_wrt(
	tb_stg_idx *idx,
	u64 nb,
	void **srcs,
	u8 arr_nb
);

/*
 * Level 0 data write.
 */
static inline void tb_stg_wrt_lv0(
	tb_stg_idx *idx,
	u64 nb,
	const f64 *avg,
	const f64 *vol
)
{
	tb_stg_wrt(
		idx,
		nb,
		(void *[]) {
			(void *) avg,
			(void *) vol,
		},
		2
	);
}

/*
 * Level 1 data write.
 */
static inline void tb_stg_wrt_lv1(
	tb_stg_idx *idx,
	u64 nb,
	const u64 *tim,
	const f64 *bid,
	const f64 *ask,
	const f64 *avg,
	const f64 *vol
)
{
	tb_stg_wrt(
		idx,
		nb,
		(void *[]) {
			(void *) tim,
			(void *) bid,
			(void *) ask,
			(void *) avg,
			(void *) vol,
		},
		5
	);
}

/*
 * Level 2 data write.
 */
static inline void tb_stg_wrt_lv2(
	tb_stg_idx *idx,
	u64 nb,
	const u64 *tim,
	const f64 *prc,
	const f64 *vol
)
{
	tb_stg_wrt(
		idx,
		nb,
		(void *[]) {
			(void *) tim,
			(void *) prc,
			(void *) vol,
		},
		3
	);
}
	
/*
 * Level 3 data write.
 */
static inline void tb_stg_wrt_lv3(
	tb_stg_idx *idx,
	u64 nb,
	const u64 *tim,
	const u64 *ord,
	const u64 *trd,
	const u8 *typ,
	const f64 *prc,
	const f64 *vol
)
{
	tb_stg_wrt(
		idx,
		nb,
		(void *[]) {
			(void *) tim,
			(void *) ord,
			(void *) trd,
			(void *) typ,
			(void *) prc,
			(void *) vol,
		},
		6
	);
}

#endif /* TB_COR_STG_H */
