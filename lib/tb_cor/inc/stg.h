/* Copyright 2025 Raphael Outhier - confidential - proprietary - no copy - no diffusion. */

#ifndef TB_COR_STG_H
#define TB_COR_STG_H

/*********
 * Types *
 *********/

types(
	tb_stg_blk_syn,
	tb_stg_blk,
	tb_stg_idx,
	tb_stg_sys
);

/**************
 * Structures *
 **************/

/*
 * Levels 1 and 2 contain a snapshot of the orderbook
 * at the end of the block, so that orderbook
 * reconstruction does not need to go many blocks behind.
 * The snapshot of the orderbook is composed of :
 * - nb, the number of tick levels saved (constant).
 * - prc, the tick of the first volume element.
 * - the volumes of the @nb ticks >= @prc, stored in
 *   incremental tick levels starting at @prc.  
 */

/*
 * Number of tick levels saved in orderbook snapshot.
 * TODO review this.
 */ 
#define TB_STG_OBS_NB 1024 

/* Number of bytes of an orderbook snapshot region. */
#define TB_STG_RGN_SIZ_OBS ((sizeof(u64)) + ((TB_STG_OBS_NB) * sizeof(f64))) 

/*
 * A data block is composed of first tier data fetched
 * directly from a provider, and of second tier data,
 * derived from first tier data of this block, or from
 * first or second tier data of earlier blocks. 
 * Each block has a shard syn page that reports
 * the status of second tier data.
 */
struct tb_stg_blk_syn {

	/* Is someone currently initializing second-tier data ? */
	volatile aad scd_wip;
	
	/* Is the second tier data initialized ? */
	volatile aad scd_ini;

};

/*
 * The storage block contains historical data for a
 * time range.
 */
struct tb_stg_blk {

	/* Blocks of the same index indexed by block number */
	ns_mapn_u64 blks;

	/* Index. */
	tb_stg_idx *idx;

	/* Segment. */
	tb_sgm *sgm;

	/* Block sync data. */
	tb_stg_blk_syn *syn;

	/* Usage counter. */
	u32 uctr;
	
};

/*
 * Storage index.
 */
struct tb_stg_idx {

	/* Indexes of the same system indexed by identifier. */
	ns_mapn_str idxs;

	/* Blocks indexed by start time. */
	ns_map_u64 blks;

	/* System. */
	tb_stg_sys *sys;

	/* Level. */
	u8 lvl;

	/* Write key. */
	u64 key;

	/* Index segment.
	 * One array of u64 couples. */
	tb_sgm *sgm;

	/* Block table. */
	volatile u64 (*tbl)[2];

	/* Usage counter. */
	u32 uctr;

	/* Identifier : "mkp:ist:lvl" */
	tb_str idt;

	/* Marketplace. */
	tb_str mkp;

	/* Instrument. */
	tb_str ist;

};

/*
 * Storage system.
 */
struct tb_stg_sys {

	/* Storage directory path. */
	const char *pth;

	/* Indexes. */
	ns_map_str idxs;

	/* Number of active data interfaces. */
	u32 itf_nb;

	/* Segments initializer scratch. */
	void *ini;

	/* Set <=> we are in test scenario and blocks only
	 * have three elements. */
	u8 tst;

	/* Storage segment for testing. */
	tb_sgm *sgm;

};

/**************
 * System API *
 **************/

/*
 * Create and initialize the storage directory @pth
 * to contain actual storage data.
 * Abort on failure.
 */
void tb_stg_ini(
	const char *pth
);

/*
 * Construct and return a data system operating
 * on the storage directory at @pth.
 * If an error occurs, return 0.
 */ 
tb_stg_sys *tb_stg_ctr(
	const char *pth,
	u8 tst
);

/*
 * Delete @sys.
 * It must have no active data interface.
 */
void tb_stg_dtr(
	tb_stg_sys *sys
);

/*
 * Return @sys's test sync page.
 */ 
void *tb_stg_tst_syn(
	tb_stg_sys *sys
);

/*************
 * Index API *
 *************/

/*
 * Open and return the index for "@mkp:@ist:@lvl". 
 * If @wrt is set, attempt to open with write privileges,
 * and if success, store the write key (non 0) at @keyp,
 * and return the index; if failure, return 0.
 * Otherwise, return the index (always succeeds).
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
 * Initialize @dsts with @blk's arrays, set *@sizsp with
 * the array containing its element sizes, return its
 * number of elements.
 */
u64 tb_sgm_arr_(
	tb_stg_blk *blk,
	const void **dsts,
	u8 dst_nb,
	const u8 **sizsp
);

/*
 * Same as above but initialize @dsts and the return
 * value to point to the elements at time @tim.
 * @tim must be valid.
 */ 
static inline u64 tb_sgm_arr(
	tb_stg_blk *blk,
	u64 tim,
	const void **dsts,
	u8 dst_nb
)
{
	assert(dst_nb);
	const u8 *sizs = 0;
	u64 nb = tb_sgm_arr_(blk, dsts, dst_nb, &sizs);
	assert(nb);
	assert(sizs);
	const u64 *tims = dsts[0];
	assert(tim <= tims[nb - 1]); /* Wrong block. */
	for (u64 elm_id = 0; elm_id < nb; elm_id++) {
		if (tim <= tims[elm_id]) {
			for (u8 arr_id = 0; arr_id < dst_nb; arr_id++) {
				dsts[arr_id] += sizs[arr_id] * elm_id;
			}
			return nb - elm_id;
		}
	}

	/* Element not found. Not possible. */
	assert(0);
	return 0;
	
}


/*
 * Unload @blk.
 */
void tb_stg_unl(
	_own_ tb_stg_blk *blk
);

#define ns_def_stt() for (u8 __ns_def_flg = 1;__ns_def_flg;) 
#define ns_def_end() for(;__ns_def_flg;__ns_def_flg = 0)
#define ns_def_(typ, nam, val) for (typ nam = val;__ns_def_flg;) 
#define ns_def(typ, nam, val) ns_def_stt() ns_def_(typ, nam, val) ns_def_end()


/*
 * Stream values of @idx in [@tim_stt, @tim_end].
 */
#define tb_stg_red(idx, tim_stt, tim_end, dsts, dst_nb) \
	ns_def_stt() \
	ns_def_(u64, __tim, tim_stt) \
	ns_def_(u64, __end, tim_end) \
	ns_def_(const u64 *, __tims, 0) \
	ns_def_end() \
	for ( \
		tb_stg_blk *__blk = 0; \
		(({if (__blk) tb_stg_unl(__blk);}),__tim <= __end) && (__blk = tb_stg_lod(idx, __tim)); \
	) \
	for ( \
		u64 __blk_nb = ({u64 __nb = tb_sgm_arr(__blk, __tim, dsts, dst_nb); __tims = dsts[0]; __nb;}), blk_id = 0; \
	   __tim = __tims[blk_id], (__tim <= __end) && (blk_id < __blk_nb); \
	   blk_id++ \
	)

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
 * - srcs are updated to the locations of the first
 *   non-written elements.
 */

/*
 * Generic data write.
 * @stt is optional
 */
void tb_stg_wrt(
	tb_stg_idx *idx,
	u64 nb,
	const void **srcs,
	u8 arr_nb
);

/*
 * Level 0 data write.
 */
static inline void tb_stg_wrt_lv0(
	tb_stg_idx *idx,
	u64 nb,
	const u64 *tim,
	const f64 *bid,
	const f64 *ask,
	const f64 *avg,
	const f64 *vol
)
{
	assert(idx->lvl == 0);
	tb_stg_wrt(
		idx,
		nb,
		(const void *[]) {
			(const void *) tim,
			(const void *) bid,
			(const void *) ask,
			(const void *) avg,
			(const void *) vol,
		},
		5
	);
}

/*
 * Level 1 data write.
 */
static inline void tb_stg_wrt_lv1(
	tb_stg_idx *idx,
	u64 nb,
	const u64 *tim,
	const f64 *prc,
	const f64 *vol
)
{
	assert(idx->lvl == 1);
	tb_stg_wrt(
		idx,
		nb,
		(const void *[]) {
			(const void *) tim,
			(const void *) prc,
			(const void *) vol,
		},
		3
	);
}
	
/*
 * Level 2 data write.
 */
static inline void tb_stg_wrt_lv2(
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
	assert(idx->lvl == 2);
	tb_stg_wrt(
		idx,
		nb,
		(const void *[]) {
			(const void *) tim,
			(const void *) ord,
			(const void *) trd,
			(const void *) typ,
			(const void *) prc,
			(const void *) vol,
		},
		6
	);
}

#endif /* TB_COR_STG_H */
