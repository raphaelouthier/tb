/* Copyright 2025 Raphael Outhier - confidential - proprietary - no copy - no diffusion. */

#include <tb_cor/tb_cor.all.h>

/*************************
 * Segments initializers *
 *************************/

/*
 * Generate the initializer for @idx.
 * Return its length.
 */
static inline uad _ini_idx(
	tb_stg_sys *sys,
	const char *mkp,
	const char *ist,
	u8 lvl
)
{
	char *ini = sys->ini;
	uad nb = ns_str_cpy(ini, mkp);
	ini[nb++] = '/';
	nb += ns_str_cpy(ini + nb, ist);
	ini[nb++] = '/';
	assert(lvl < 3);
	ini[nb++] = '0' + lvl;
	ini[nb++] = '/';
	return nb;
}

/*
 * Generate the initializer for @blk.
 * Return its length.
 */
static inline uad _ini_blk(
	tb_stg_idx *idx,
	u64 blk_nbr
)
{
	char *ini = idx->sys->ini;
	uad nb = _ini_idx(idx->sys, idx->mkp, idx->ist, idx->lvl); 
	char *end = ns_u64_to_str_hex(blk_nbr, ini + nb, 16); 
	return ns_psub(end, ini);
}

/*******************
 * Level constants *
 *******************/

/*
 * Determine the index of the timestamp array
 * in the arrays of level @lvl.
 */
static inline u8 _dat_tim_idx(
	u8 lvl
)
{
	/* Always at index 0. */
	return 0;
}

/*
 * Determine the maximal number of elements
 * that an index table of level @lvl has.
 */
static const uad _lvl_to_idx_max[3] = {
	22000,
	22000,
	22000,
};
static inline u64 _idx_elm_max(
	u8 tst,
	u8 lvl
)
{
	/* See design.md. */
	assert(lvl < 3);
	return tst ? 2000 : _lvl_to_idx_max[lvl];
}

/*
 * Determine the element sizeof elements that a block
 * of level @lvl should contain.
 */
static const uad _lvl_to_blk_max[3] = {
	(1 << 19),
	(1 << 26),
	(1 << 26),
};
static inline inline uad _blk_elm_max(
	u8 tst,
	u8 lvl
)
{
	/* See design.md. */
	assert(lvl < 3);
	return tst ? 3 : _lvl_to_blk_max[lvl];
}

/*
 * Determine the number of regions that a block
 * of level @lvl should contain.
 */
static const u8 _lvl_to_rgn_nb[3] = {
	1, /* Syn. */
	2, /* Syn + Orderbook state. */
	2, /* Syn + Orderbook state. */
};
static inline inline u8 _blk_rgn_nb(
	u8 lvl
)
{
	/* See design.md. */
	assert(lvl < 3);
	return _lvl_to_rgn_nb[lvl];
}

/*
 * Determine the region sizes that a block
 * of level @lvl should contain.
 */
static const u64 *const (_lvl_to_rgn_sizs[3]) = {
	(u64 []) {TB_SGM_PAG_SIZ},
	(u64 []) {TB_SGM_PAG_SIZ, TB_STG_RGN_SIZ_OBS},
	(u64 []) {TB_SGM_PAG_SIZ, TB_STG_RGN_SIZ_OBS},
};
static inline const u64 *_blk_rgn_sizs(
	u8 lvl
)
{
	/* See design.md. */
	assert(lvl < 3);
	return _lvl_to_rgn_sizs[lvl];
}

/*
 * Determine the number of arrays that a block
 * of level @lvl should contain.
 */
static const u8 _lvl_to_arr_nb[3] = {
	5, /* time, bid, ask, vol, val. */
	3, /* time, prc, vol. */
	6, /* time, ord_id, trd_id, ord_typ, ord_val, ord_vol. */
};
static inline inline u8 _blk_arr_nb(
	u8 lvl
)
{
	/* See design.md. */
	assert(lvl < 3);
	return _lvl_to_arr_nb[lvl];
}

/*
 * Determine the maximal number of elements that a block
 * of level @lvl should contain.
 */
static const u8 *const (_lvl_to_elm_sizs[3]) = {
	(u8 []) {8, 8, 8, 8, 8},
	(u8 []) {8, 8, 8},
	(u8 []) {8, 8, 8, 1, 8, 8},
};
static inline const u8 *_blk_elm_sizs(
	u8 lvl
)
{
	/* See design.md. */
	assert(lvl < 3);
	return _lvl_to_elm_sizs[lvl];
}

/*******************
 * Data extraction *
 *******************/

/*
 * Most of the historical data is not used by
 * the storage library, with the exception of
 * timestamps.
 * As the data provider does not know at which element
 * the storage will be full, we cannot expect it
 * to provide the end timestamp of a block.
 * On the other side, we need this timestamp,
 * as blocks are indexed in the index table by
 * their start and end times.
 * Data is hence read with the only objective of
 * finding an element's timestamp.
 */

/*
 * Get the timestamp of the element of data of level
 * @lvl stored at @srcs.
 */
static inline u64 _dat_tim(
	u8 lvl,
	const void **srcs,
	u64 idx

)
{
	const u8 tid = _dat_tim_idx(lvl);
	return ((const u64 *) srcs[tid])[idx];
}

/*************************
 * Index table internals *
 *************************/

/*
 * The index table is atomically accessed.
 * This API does the proper access sequences.
 */

/*
 * Return the size of the index table.
 */
static inline u64 _itb_nb(
	tb_stg_idx *idx
) {return tb_sgm_elm_nb(idx->sgm);}

/*
 * Get the start time of a block.
 */
static inline u64 _itb_blk_stt(
	volatile u64 (*tbl)[2],
	u64 blk_nb,
	u64 blk_idx
)
{
	assert(blk_idx < blk_nb);
	return ns_atm(a64, red, acq, &tbl[blk_idx][0]);
}

/*
 * Get the end time of a block.
 */
static inline u64 _itb_blk_end(
	volatile u64 (*tbl)[2],
	u64 blk_nb,
	u64 blk_idx
)
{
	assert(blk_idx < blk_nb);
	return ns_atm(a64, red, acq, &tbl[blk_idx][1]);
}

/*
 * Set the start time of a block.
 */
static inline void _itb_blk_stt_set(
	volatile u64 (*tbl)[2],
	u64 blk_nb,
	u64 blk_idx,
	u64 stt
)
{
	assert(blk_idx < blk_nb);
	ns_atm(a64, wrt, rel, &tbl[blk_idx][0], stt);
}

/*
 * Set the end time of a block.
 */
static inline void _itb_blk_end_set(
	volatile u64 (*tbl)[2],
	u64 blk_nb,
	u64 blk_idx,
	u64 end
)
{
	assert(blk_idx < blk_nb);
	ns_atm(a64, wrt, rel, &tbl[blk_idx][1], end);
}

/*
 * Search @idx's table for the first block covering @tim.
 * If it is found, store its index at *@blk_nbrp
 * and return 0.
 * If it is in between two blocks, it is covered by the
 * successor block.
 * Otherwise, return 1.
 */ 
static inline uerr _itb_sch(
	tb_stg_idx *idx,
	u64 tim,
	u64 *blk_nbrp
)
{
	
	/* Read the number of blocks. */
	const u64 blk_nb = _itb_nb(idx);
	volatile u64 (*tbl)[2] = idx->tbl;

	/* Verify the table. */
#ifdef DEBUG
	for (u64 i = 0; i + 1 < blk_nb; i++) {
		check(_itb_blk_end(tbl, blk_nb, i) <= _itb_blk_stt(tbl, blk_nb, i + 1));
	}
#endif

	/* If outside boundaries, stop. */
	if (
		(blk_nb == 0) ||
		(tim < _itb_blk_stt(tbl, blk_nb, 0)) ||
		(tim > _itb_blk_end(tbl, blk_nb, blk_nb - 1))
	) return 1;

	/* Bisect based on end time. */
	u64 min = 0;
	u64 max = blk_nb;
	while (min != max) {

		/* Get mid IDX. */
		const u64 mid = min + ((max - min) >> 1);

		/* Bisect iter. */
		if (tim > _itb_blk_end(tbl, blk_nb, mid)) min = mid + 1;
		else max = mid;
		assert(min <= max);

	}

	/* Verify that we found a valid block,
	 * that spans @tim wrt its predecessor if any. */
	assert(tim <= _itb_blk_end(tbl, blk_nb, min));
	assert((_itb_blk_stt(tbl, blk_nb, min) <= tim) || (min > 0));
	assert((min == 0) || (_itb_blk_end(tbl, blk_nb, min - 1) < tim));

	/* Complete. */
	*blk_nbrp = min;
	return 0;
	
}

/*******************
 * Block internals *
 *******************/

/*
 * Delete @blk.
 */
static inline void _blk_dtr(
	tb_stg_idx *idx,
	tb_stg_blk *blk
)
{
	assert(!blk->uctr);
	ns_map_u64_rem(&idx->blks, &blk->blks);
	tb_sgm_cls(blk->sgm);
	nh_fre_(blk);
}

/*
 * Construct or load the block with number @nbr from storage.
 */
static inline tb_stg_blk *_blk_ctr_lod(
	u8 ctr,
	tb_stg_idx *idx,
	u64 blk_nbr
)
{

	/* Generate the segment initializer. */
	const uad imp_siz = _ini_blk(idx, blk_nbr);
	const uad elm_max = _blk_elm_max(idx->sys->tst, idx->lvl);
	const u8 rgn_nb = _blk_rgn_nb(idx->lvl);
	const u64 *rgn_sizs = _blk_rgn_sizs(idx->lvl);
	const u8 arr_nb = _blk_arr_nb(idx->lvl);
	const u8 *elm_sizs = _blk_elm_sizs(idx->lvl);


	/* Open the block segment, it should already exist. */
	tb_sgm *sgm = tb_sgm_fopn(
		ctr,
		idx->sys->ini,
		imp_siz,
		rgn_nb,
		rgn_sizs,
		arr_nb,
		elm_max,
		elm_sizs,
		"%s/%s/%s/%u/%U", idx->sys->pth, idx->mkp, idx->ist, idx->lvl, blk_nbr
	);
	assert(sgm, "segment %s/%s/%s/%u/%U open failed.\n", idx->sys->pth, idx->mkp, idx->ist, idx->lvl, blk_nbr);

	/* Get the sync page. */
	tb_stg_blk_syn *syn = tb_sgm_rgn(sgm, 0);

	/* Allocate. */
	nh_all__(tb_stg_blk, blk);
	check(!ns_map_sch(&idx->blks, blk_nbr, u64, tb_stg_blk, blks));
	assert(!ns_map_u64_put(&idx->blks, &blk->blks, blk_nbr));
	blk->idx = idx;
	blk->sgm = sgm;
	blk->syn = syn;
	blk->uctr = 0;

	/* Complete. */
	return blk;
	
}

/*
 * Load the block with number @nbr from storage.
 */
static inline tb_stg_blk *_blk_lod(
	tb_stg_idx *idx,
	u64 blk_nbr
) {return _blk_ctr_lod(0, idx, blk_nbr);}

/*
 * Construct a block and insert it at the end of
 * the index table.
 * Only called by the write sequence.
 */
static inline tb_stg_blk *_blk_ctr(
	tb_stg_idx *idx,
	u64 stt_tim
)
{

	assert(idx->key, "Index not writeable.\n");
	
	/* Determine the next block number. */
	const u64 itb_max = _idx_elm_max(idx->sys->tst, idx->lvl); 
	assert(itb_max == tb_sgm_elm_max(idx->sgm));
	const u64 itb_nb = _itb_nb(idx); 
	assert(itb_nb != itb_max, "index table full.\n");

	/* Determine the block number. */
	tb_stg_blk *blk = _blk_ctr_lod(1, idx, itb_nb);

	/* Do not report the block yet in the index table,
	 * as it has not data. */

	/* Complete. */
	return blk;

}

/*
 * Take and return @blk.
 */
static inline _own_ tb_stg_blk *_blk_tak(
	tb_stg_blk *blk
)
{
	SAFE_INCR(blk->uctr);
	return blk;
}

/*
 * Release @blk.
 */
static inline void _blk_rel(
	tb_stg_blk *blk
)
{
	SAFE_DECR(blk->uctr);
}

/*
 * Return @blk's block number.
 */
static inline u64 _blk_nbr(
	tb_stg_blk *blk
) {return blk->blks.val;}

/*
 * Return @blk's start time.
 */
static inline u64 _blk_stt(
	tb_stg_blk *blk
)
{
	volatile u64 (*tbl)[2] = blk->idx->tbl;
	const u64 blk_nb = _itb_nb(blk->idx);
	return _itb_blk_stt(tbl, blk_nb, _blk_nbr(blk));
}

/*
 * Return @blk's current end time.
 */
static inline u64 _blk_end(
	tb_stg_blk *blk
)
{
	volatile u64 (*tbl)[2] = blk->idx->tbl;
	const u64 blk_nb = _itb_nb(blk->idx);
	return _itb_blk_end(tbl, blk_nb, _blk_nbr(blk));
}

/*
 * Write @wrt_nb elements into @blk's @arr_nb arrays
 * from the locations specified in @srcs.  
 * Update @srcs with the refs of the next copy sources.
 */
static inline u64 _blk_wrt(
	tb_stg_idx *idx,
	tb_stg_blk *blk,
	u64 wrt_nb,
	const void **srcs,
	u8 arr_nb
)
{
	assert(idx->key, "not a writeable index.\n");
	check(arr_nb == _blk_arr_nb(idx->lvl));

	/* Cache the timestamp of the last values. */
	const u64 tim_end = _dat_tim(idx->lvl, srcs, wrt_nb - 1);

	/* Cache dests. */
	void *dst[6];
	uad off = 0;
	assert(!tb_sgm_wrt_get(blk->sgm, &off));
	tb_sgm_wrt_loc(blk->sgm, wrt_nb, dst, arr_nb);

	/* Get arrays sizes. */
	const u8 *elm_sizs = _blk_elm_sizs(idx->lvl);

	/* Copy, update locations. */
	for (u8 i = 0; i < arr_nb; i++) {
		const void *src = srcs[i];
		const u64 siz = elm_sizs[i] * wrt_nb;
		ns_mem_cpy(dst[i], src, siz);
		srcs[i] = ns_psum(src, siz);
	}

	/* Report the write. */
	tb_sgm_wrt_don(blk->sgm, wrt_nb);
	tb_sgm_wrt_cpl(blk->sgm);

	/* Return the end timestamp. */ 
	return tim_end;

}


/*******************
 * Index internals *
 *******************/


/*
 * Load and return the block at index @blk_nbr of @idx.
 */
static inline _own_ tb_stg_blk *_idx_lod_nbr(
	tb_stg_idx *idx,
	u64 blk_nbr
)
{

	/* Check if the block is loaded. */
	tb_stg_blk *blk = ns_map_sch(&idx->blks, blk_nbr, u64, tb_stg_blk, blks);

	/* Load the block if not. */
	if (!blk) blk = _blk_lod(idx, blk_nbr);
	assert(blk);

	/* Once a block is found, take it and return it. */
	return blk;

}

/*
 * If @idx has a block covering @tim,
 * load it and return it.
 * Otherwise, return 0.
 */
static inline _own_ tb_stg_blk *_idx_lod_tim(
	tb_stg_idx *idx,
	u64 tim
)
{

	/* Read the index. If no block covers the section, fail. */
	u64 blk_nbr = 0;
	const uerr err = _itb_sch(idx, tim, &blk_nbr);
	if (err) return 0;
	
	/* Load. */
	return _idx_lod_nbr(idx, blk_nbr);

}

/*
 * Return @idx's last block.
 * If it doesn't exist, return 0.
 * @idx must be opened with write privs.
 */
static inline tb_stg_blk *_idx_lst(
	tb_stg_idx *idx
)
{
	assert(idx->key);

	/* Get the block number. */
	const u64 blk_nbr = _itb_nb(idx);
	if (!blk_nbr) return 0;

	/* Load. */
	return _idx_lod_nbr(idx, blk_nbr - 1);

}

/*
 * Delete @idx.
 */
static inline void _idx_dtr(
	tb_stg_sys *sys,
	tb_stg_idx *idx
)
{
	assert(!idx->uctr);

	/* Delete blocks. */
	tb_stg_blk *blk;
	ns_map_fe(blk, &idx->blks, blks, u64, in) {
		_blk_dtr(idx, blk);
	}
	assert(ns_map_u64_emp(&idx->blks));

	/* Delete. */
	ns_map_str_rem(&sys->idxs, &idx->idxs);
	tb_sgm_cls(idx->sgm);
	nh_fre_(idx);
}


/*
 * Clean @idx.
 */
static inline void _idx_cln(
	tb_stg_sys *sys,
	tb_stg_idx *idx
)
{

	/* If no usage, delete blocks. */
	if (!idx->uctr) {
		_idx_dtr(sys, idx);
	}
}

/****************
 * System utils *
 ****************/

/*
 * Clean @idx.
 */
static inline void _sys_cln(
	tb_stg_sys *sys
)
{
	tb_stg_idx *idx;
	ns_map_fe(idx, &sys->idxs, idxs, str, in) {
		_idx_cln(sys, idx);
	}
}

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
)
{

	/* Create the directory. */
	nh_fs_crt_dir(pth);

	/* Create the storage flag. */
	nh_fs_fcrt_stg(NH_FIL_ATT_RW, "%s/stg", pth);

	/*
	 * If the directory already exists, check that
	 * the lock file exists.
	 */
	nh_stt stt;
	assert(!nh_fs_tst(NH_FIL_TYP_DIR, 0, &stt, pth));
	assert(!nh_fs_ftst(NH_FIL_TYP_STM, 0, &stt, "%s/stg", pth));

}

/*
 * Construct and return a data system operating
 * on the storage directory at @pth.
 * If an error occurs, return 0.
 */ 
tb_stg_sys *tb_stg_ctr(
	const char *pth,
	u8 tst
)
{

	/* Check that the storage layout is valid. */
	nh_stt stt;
	assert(
		(!nh_fs_tst(NH_FIL_TYP_DIR, 0, &stt, pth)) &&
		(!nh_fs_ftst(NH_FIL_TYP_STM, 0, &stt, "%s/stg", pth)),
		"%s : not a storage directory.\n", pth
	);
	
	/* Allocate. */	
	nh_all__(tb_stg_sys, sys);
	sys->pth = nh_sall(pth);
	ns_map_str_ini(&sys->idxs);
	sys->itf_nb = 0;
	sys->ini = nh_all(1024);
	sys->tst = !!tst;
	sys->sgm = 0;

	/* Load the syn segment. */
	if (tst) {
		sys->sgm = tb_sgm_fopn(
			1,
			0,
			0,
			1,
			(u64 []) {4096},
			0,
			0,
			0,
			"%s/syn", 
			pth
		);
		assert(sys->sgm);
	}

	/* Complete. */
	return sys;

}

/*
 * Delete @sys.
 * It must have no active data interface.
 */
void tb_stg_dtr(
	tb_stg_sys *sys
)
{

	/* No user must be active. */
	assert(!sys->itf_nb);

	/* Clean. */
	_sys_cln(sys);

	/* No index should remain. */
	assert(ns_map_str_emp(&sys->idxs));

	/* Delete the segment. */
	tb_sgm_cls(sys->sgm);

	/* Free. */
	nh_fre(sys->ini, 1024);
	nh_sfre(sys->pth);
	nh_fre_(sys);
}

/*
 * Return @sys's test sync page.
 */ 
void *tb_stg_tst_syn(
	tb_stg_sys *sys
)
{
	return (sys->sgm) ? tb_sgm_rgn(sys->sgm, 0) : 0;
}

/*************
 * Index API *
 *************/

static inline void _gen_idt(
	char *dst,
	const char *mkp,
	const char *ist,
	u8 lvl
)
{
	const uad mkp_len = ns_str_len(mkp);
	const uad ist_len = ns_str_len(ist);
	assert(mkp_len < 21);
	assert(ist_len < 21);
	assert(lvl < 3);
	ns_mem_cpy(dst, mkp, mkp_len);
	dst[mkp_len] = '/';
	ns_mem_cpy(dst + mkp_len + 1, ist, ist_len);
	dst[mkp_len + ist_len + 1] = '/';
	dst[mkp_len + ist_len + 2] = '0' + lvl; 
	dst[mkp_len + ist_len + 3] = 0;
}

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
)
{

	/* Generate the identifier. */ 
	tb_str idt;
	_gen_idt(idt, mkp, ist, lvl);

	/* Search. */
	tb_stg_idx *idx = ns_map_sch(&sys->idxs, idt, str, tb_stg_idx, idxs);
	if (idx) {
		SAFE_INCR(idx->uctr);
		goto end;
	}

	/* Allocate. */
	nh_all_(idx);

	/*
	 * Create the mkp dir, the instrument dir,
	 * and the level dir.
	 * Ignore errors.
	 */
	nh_fs_fcrt_dir("%s/%s", sys->pth, mkp);
	nh_fs_fcrt_dir("%s/%s/%s", sys->pth, mkp, ist);
	nh_fs_fcrt_dir("%s/%s/%s/%u", sys->pth, mkp, ist, lvl);

	/* Generate the segment initializer. */
	uad imp_siz = _ini_idx(sys, mkp, ist, lvl);
	uad elm_max = _idx_elm_max(sys->tst, lvl);

	/* Open the index segment. */
	tb_sgm *sgm = tb_sgm_fopn(
		1,
		sys->ini,
		imp_siz,
		0,
		0,
		1,
		elm_max,
		(u8 []) {2 * sizeof(u64)},  
		"%s/%s/%s/%u/idx", sys->pth, mkp, ist, lvl
	);
	assert(sgm, "segment %s/%s/%s/%u/idx failed.\n", sys->pth, mkp, ist, lvl);

	/* Initialize. */
	tb_str_cpy(idx->idt, idt);
	assert(!ns_map_str_put(&sys->idxs, &idx->idxs, idx->idt));
	ns_map_u64_ini(&idx->blks);
	idx->sys = sys; 
	idx->lvl = lvl;
	idx->sgm = sgm;
	idx->uctr = 1;
	idx->key = 0;
	tb_str_cpy(idx->mkp, mkp);
	tb_str_cpy(idx->ist, ist);
	tb_sgm_red_rng(
		idx->sgm,
		0,
		0,
		(const void **) &idx->tbl,
		1
	);

	/* End section.
	 * Reached when we find or allocate an index. */
	end:;

	/* Track one more use. */	
	SAFE_INCR(sys->itf_nb);

	/* Attempt to acquire write privileges if required.
	 * Bail out if failure. */
	if (wrt) {
		uad off = 0;
		const uerr err = tb_sgm_wrt_get(sgm, &off);
		if (err) {
			tb_stg_cls(idx, 0);
			return 0;
		}
		assert(!idx->key);
		*key = idx->key = nh_run_tim();
		assert(idx->key);
	}

	/* Complete. */
	return idx;

}

/*
 * Close @idx.
 * If @key is set, release its write privileges; in this
 * case, @key must match the key that was provided when
 * @idx was opened with write privileges.
 */
void tb_stg_cls(
	tb_stg_idx *idx,
	u64 key
)
{

	/* If required, release write priv. */
	if (key) {
		assert(key == idx->key);
		idx->key = 0;
		tb_sgm_wrt_cpl(idx->sgm);
	}

	/* Unuse. */
	SAFE_DECR(idx->uctr);
	SAFE_DECR(idx->sys->itf_nb);
	if (!idx->uctr) {
		assert(!idx->key);
	}
	
}

/************
 * Read API *
 ************/

/*
 * If @idx has no block covering @tim, return 0.
 * If @idx has a block covering @tim, load it in memory,
 * and return it.
 */
_own_ tb_stg_blk *tb_stg_lod_tim(
	tb_stg_idx *idx,
	u64 tim
)
{
	tb_stg_blk *blk = _idx_lod_tim(idx, tim);
	return blk ? _blk_tak(blk) : 0;
}

/*
 * Unload @blk.
 */
void tb_stg_unl(
	_own_ tb_stg_blk *blk
) {_blk_rel(blk);}

/*
 * If @idx has a block for @tim, store its number
 * at @blk_nbrp and return 0.
 * Otherwise, return 1.
 */
uerr tb_stg_sch(
	tb_stg_idx *idx,
	u64 tim,
	u64 *blk_nbrp
) {return _itb_sch(idx, tim, blk_nbrp);}

/*
 * Iteration next.
 */
tb_stg_blk *tb_stg_red_nxt(
	tb_stg_idx *idx,
	tb_stg_blk *blk,
	u64 end
)
{

	/* Release. */
	assert(blk);
	const u64 blk_nbr = blk->blks.val;
	_blk_rel(blk);

	/* Read the index table.
	 * If next block is after end time, nothing to do. */
	const u64 itb_nbr = _itb_nb(idx);
	check(blk_nbr < itb_nbr);
	const u64 nxt_nbr = blk_nbr + 1;
	if (((nxt_nbr) == itb_nbr) || (end < _itb_blk_end(idx->tbl, itb_nbr, nxt_nbr))) {
		return 0;
	}

	/* If next block is in iteration range, load it. */
	return _blk_tak(_idx_lod_nbr(idx, nxt_nbr));
	
}

/*
 * If @blk is validated, return 0.
 * If @blk is not validated, return 1.
 */
uerr tb_stg_val_get(
	tb_stg_blk *blk
) {return !!ns_atm(a64, red, acq, &blk->syn->scd_ini);}

/*
 * Attempt to acquire the ownership of @blk's validation.
 * If success, return 0.
 * If someone already acquired it, return 1.
 */
uerr tb_stg_val_ini(
	tb_stg_blk *blk
) {return !!ns_atm(a64, xch, aar, &blk->syn->scd_wip, 1);}

/*
 * Report @blk validated.
 */
void tb_stg_val_set(
	tb_stg_blk *blk
)
{
	assert(ns_atm(a64, red, acq, &blk->syn->scd_wip));
	assert(!ns_atm(a64, red, acq, &blk->syn->scd_ini));
	ns_atm(a64, wrt, rel, &blk->syn->scd_wip, 1);
}

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
)
{
	const u64 nb = tb_sgm_elm_nb(blk->sgm);
	assert(dst_nb == tb_sgm_arr_nb(blk->sgm));
	assert(dst_nb == _blk_arr_nb(blk->idx->lvl));
	*sizsp = _blk_elm_sizs(blk->idx->lvl);
	tb_sgm_red_rng(
		blk->sgm, 	
		0,
		nb,
		dsts,
		dst_nb
	);
	return nb;
}

/*************
 * Write API *
 *************/

/*
 * Write functions conform to the following behavior :
 * - they write exactly @nb elements at the end of the
 *   stored data.
 * - @stt is the time of the firt element. If the storage
 *   is empty, it becomes the time of the first element.
 * - srcs are updated to the locations of the first
 *   non-written elements.
 */

/*
 * Generic data write.
 */
void tb_stg_wrt(
	tb_stg_idx *idx,
	u64 nb,
	const void **srcs,
	u8 arr_nb
)
{
	assert(idx->key, "not a writeable index.\n");

	/* Fetch the index table. */
	const u64 itb_max = _idx_elm_max(idx->sys->tst, idx->lvl);
	assert(itb_max == tb_sgm_elm_max(idx->sgm));
	u64 itb_nb = _itb_nb(idx);
	assert(itb_nb <= itb_max);
	volatile u64 (*tbl)[2] = idx->tbl;

	/* Determine the last block. */
	tb_stg_blk *blk = _idx_lst(idx);

	/* Write while data is available.
	 * Check that block is always null when reiterating. */
	u64 prv_end = 0;
	for (;nb;({assert(!blk);})) {

		/* Get the start time of the data that we will write. */
		const u64 stt = _dat_tim(idx->lvl, srcs, 0);
		assert(prv_end <= stt);
		
		/* Create a block if required. */
		const u8 crt = (!blk);
		if (crt) blk = _blk_ctr(idx, stt);

		/* Determine the number of writeable elements. */
		const uad blk_max = tb_sgm_elm_max(blk->sgm);
		const uad blk_stt = tb_sgm_elm_nb(blk->sgm);
		assert(blk_stt <= blk_max);
		const uad blk_avl = blk_max - blk_stt;
		
		/* If no more space, push a new block.
		 * Not hit if we just created the block. */
		if (!blk_avl) {
			assert(!crt);
			blk = 0;
			continue;
		}

		/* Determine the write size. */
		const u64 wrt_nb = (nb < blk_avl) ? nb : blk_avl;

		/* Perform the write, update locations, return
		 * the time of the last element. */
		const u64 end = prv_end = _blk_wrt(
			idx,
			blk,
			wrt_nb,
			srcs,
			arr_nb
		);
		assert(stt <= end);

		/* If the block was just created, report
		 * it as existing now that its data is populated. */
		if (crt) {

			/* Reference a new block in the index table. */
			_itb_blk_stt_set(tbl, itb_nb + 1, itb_nb, stt);

			/* Report a new element in the index table. */
			const uad itb_nxt = tb_sgm_wrt_don(idx->sgm, 1);
			assert(itb_nxt == itb_nb + 1);
			itb_nb = itb_nxt; 
			assert(itb_nb == _itb_nb(idx));

		}

		/* Report the block's end in the index table. */ 
		_itb_blk_end_set(tbl, itb_nb, itb_nb - 1, end);

		/* Report write. */
		SAFE_SUB(nb, wrt_nb);

		/* Reiterate. */ 
		blk = 0;
	}

	assert(itb_nb == _itb_nb(idx));

}

