
/* Copyright 2025 Raphael Outhier - confidential - proprietary - no copy - no diffusion. */

#ifndef TB_COR_STG_H
#define TB_COR_STG_H

/*******
 * Doc *
 *******/

/*
 * The storage library stores historical trading 
 * data of different levels for instruments.
 *
 * The storage system is single-threaded,
 * but is designed to allow multiple instances
 * to operate in parallel on disk data.
 *
 * Multiple readers and writers are supported,
 * with a theoretical limit of at most one writer
 * per segment. In practice, this restriction is
 * extended to at most one writer per level of data
 * of a given instrument. 
 *
 * It relies on the segment library to store data,
 * and handles the organization of those segments
 * in the file system.
 *
 * A storage instance operates (in coordination with
 * other potential instances) on a storage directory
 * structured as follows :
 * - (root)/ : root directory.
 *   - stg : flag file reporting storage directory.
 *     Content unused.
 *   - (mkp)/ : marketplace directory.
 *     - (ist)/ : instrument directory.
 *       - (level)/ : level directory.
 *         - idx : index file : synchronizes readers
 *           and writers, indexes time and segments.
 *         - (sgm_id) : data segment, described in the index
 *
 * Levels 1 2 3 (as described in the design doc) are
 * supported. Level 0 may be supported in the future.
 */

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
 * A data block is composed of first tier data fetched
 * directly from a provider, and of second tier data,
 * derived from first tier data of this block, or from
 * first or second tier data of earlier blocks. 
 * Each block has a shard syn page that reports
 * the status of second tier data.
 */
struct tb_stg_blk_syn {

	/* Is the second tier data initialized ? */
	volatile aad scd_ini;

	/* Is someone currently initializing second-tier data ? */
	volatile aad scd_wip;

};

/*
 * The storage block contains historical data for a
 * time range.
 */
struct tb_stg_blk {

	/* Blocks of the same index indexed by identifier. */
	ns_mapn_u64 blks;

	/* Segment. */
	tb_sgm *sgm;

	/* Block sync data. */
	tb_stg_blk_syn *syn;
	
};

/*
 * Storage index.
 */
struct tb_stg_idx {

	/* Indexes of the same system indexed by identifier. */
	ns_mapn_str idxs;

	/* Blocks. */
	ns_map_u64 blks;

	/* System. */
	tb_stg_sys *sys;

	/* Level. */
	u8 lvl;

	/* Index segment. */
	tb_sgm *sgm;

	/* Usage counter. */
	u32 uctr;

	/* Identifier : "mkp:ist:lvl" */
	sp_str idt;

	/* Marketplace. */
	sp_str mkp;

	/* Instrument. */
	sp_str ist;

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

};

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
_own_ tb_tg_blk *tb_stg_lod(
	tb_stg_idx *idx
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
	tb_sgm_blk *blk
);

/*
 * Attempt to acquire the ownership of @blk's validation.
 * If success, return 0.
 * If someone already acquired it, return 1.
 */
uerr tb_stg_val_ini(
	tb_sgm_blk *blk
);

/*
 * Report @blk validated.
 */
uerr tb_stg_val_set(
	tb_sgm_blk *blk
);

/*
 * Initialize @dsts with @blk's arrays, set
 * write the time of @blk's first data element at *@timp,
 * write the number of elements of the arrays at *@nbp.
 */
void tb_sgm_arr(
	tb_sgm_blk *blk,
	void **dsts,
	u8 dst_nb,
	u64 *timp,
	u64 *nbp
);

/*
 * Return @blk's second tier data.
 */
void tb_sgm_std(
	tb_sgm_blk *blk
);

/*************
 * Write API *
 *************/

/*
 * Write functions conform to the following behavior :
 * - they write exactly @nb elements at the end of the
 *   stored data.
 * - @stt is the time of the firt element. If the storage
 *   is empty, it becomes the time of the first element.
 */

/*
 * Generic data write.
 */
void tb_stg_wrt(
	tb_stg_idx *idx,
	u64 stt,
	u64 nb,
	void **dat
);

/*
 * Level 0 data write.
 */
void tb_stg_wrt_lv0(
	tb_stg_idx *idx,
	u64 stt,
	u64 nb,
	const f64 *avg,
	const f64 *vol
)
{
	tb_stg_wrt(
		idx,
		stt,
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
void tb_stg_wrt_lv1(
	tb_stg_idx *idx,
	u64 stt,
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
		stt,
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
void tb_stg_wrt_lv2(
	tb_stg_idx *idx,
	u64 stt,
	u64 nb,
	const u64 *tim,
	const f64 *prc,
	const f64 *vol
)
{
	tb_stg_wrt(
		idx,
		stt,
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
void tb_stg_wrt_lv3(
	tb_stg_idx *idx,
	u64 stt,
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
		stt,
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
