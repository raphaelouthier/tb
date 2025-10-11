/* Copyright 2025 Raphael Outhier - confidential - proprietary - no copy - no diffusion. */

#include <tb_stg/tb_stg.all.h>

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
 * Search @idx's table for a block covering @tim.
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
	for (u64 i = 0; i + 1 < blk_nb; i++) {
		assert(_itb_blk_end(tbl, blk_nb, i) < _itb_blk_stt(tbl, blk_nb, i + 1));
	}

	/* If outside boundaries, stop. */
	if (
		(blk_nb == 0) ||
		(tim < _itb_blk_stt(tbl, blk_nb, 0)) ||
		(tim > _itb_blk_end(tbl, blk_nb, blk_nb - 1))
	) return 1;

	/* Bisect based on end time. */
	u64 min = 0;
	u64 siz = blk_nb;
	while (siz > 1) {

		/* Get mid IDX. */
		const u64 hsiz = siz >> 1;
		assert(hsiz);
		const u64 mid = min + hsiz;

		/* Bisect iter. */
		if (tim > _itb_blk_end(tbl, blk_nb, mid)) min = mid;
		siz = hsiz;

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
	ns_map_str_rem(&idx->blks, &blk->blks);
	tb_sgm_cls(blk->sgm);
	nh_fre_(blk);
}

/*
 * Construct or load the block with number @nbr from storage.
 */
static inline void _blk_ctr_lod(
	u8 ctr,
	tb_stg_idx *idx,
	u64 blk_nbr
)
{

	/* Generate the segment initializer. */
	void *imp_ini = _blk_sgm_ini(mkp, ist, lvl);
	uad elm_max = _blk_elm_max(lvl);
	u8 elm_siz = _blk_elm_siz(lvl);

	/* Open the block segment, it should already exist. */
	tb_sgm *sgm = tb_sgm_fopn(
		ctr,
		imp_ini,
		imp_siz,
		1,
		elm_max,
		(u8 []) {elm_siz},  
		"%s/%s/%s/%u/%U", sys->pth, mkp, ist, lvl, blk_nbr
	);
	assert(sgm, "segment %s/%s/%s/%u/%U open failed.\n", sys->pth, mkp, ist, lvl, blk_nbr);

	/* Get the sync page. */
	tb_stg_blk_syn *syn = tb_sgm_rgn(sgm, 0);

	/* Allocate. */
	nh_all__(tb_stg_blk, blk);
	assert(!ns_map_u64_put(&idx->blks, &blk->blks, blk_nbr));
	blk->sgm = sgm;
	blk->syn = syn;
	blk->uctr = 0;
	
	/* Complete. */
	return blk;
	
}

/*
 * Load the block with number @nbr from storage.
 */
static inline void _blk_lod(
	tb_stg_idx *idx,
	u64 blk_nbr
) {_blk_ctr_lod(0, idx, blk_nbr);}

/*
 * Construct a block and insert it at the end of
 * the index table.
 * Only called by the write sequence.
 */
static inline void _blk_ctr(
	tb_stg_idx *idx,
	u64 stt_tim
)
{

	assert(idx->key, "Index not writeable.\n");
	
	/* Determine the next block number. */
	const u64 itb_max = _itb_max(idx); 
	const u64 itb_nb = _itb_nb(idx); 
	assert(itb_nb != itb_max, "index table full.\n");

	/* Determine the block number. */
	_blk_ctr_lod(1, idx, itb_nb);

	/* Do not report the block yet in the index table,
	 * as it has not data. */

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

/*******************
 * Index internals *
 *******************/

/*
 * If @idx has a block covering @tim,
 * load it and return it.
 * Otherwise, return 0.
 */
static inline _own_ tb_stg_blk *_idx_lod(
	tb_stg_idx *idx,
	u64 tim
)
{

	/* Read the index. If no block covers the section, fail. */
	u64 blk_nbr = 0;
	const uerr err = _itb_sch(idx, tim, &blk_nbr);
	if (err) return 0;
	
	/* Check if the block is loaded. */
	tb_stg_blk *blk = ns_map_sch(&idx->blks, blk_nbr, u64, tb_stg_blk, blks);

	/* Load the block if not. */
	if (!blk) blk = _blk_lod(idx, blk, blk_nbr, tim);
	assert(blk);

	/* Once a block is found, take it and return it. */
	return _blk_tak(blk);

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
	ns_map_fe(blk, &idx->blks, blks, str, in) {
		_blk_dtr(idx, blk);
	}
	assert(ns_map_str_empty(&idx->blks));

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
 * Return 0 if success, 1 if failure, 2 if already
 * initialized.
 */
uerr tb_stg_ini(
	const char *pth
)
{

	/*
	 * If the directory already exists, check that
	 * the lock file exists.
	 */
	nh_stt stt;
	if (!nh_fs_tst(NH_FIL_TYP_DIR, 0, &stt, pth)) {
		return
		if (nh_fs_ftst(NH_FIL_TYP_STM, 0, &stt, "%s/stg", pth)) {
			debug("Storage flag not present failed.\n");
			return 1;
		}
		return 2;
	}

	/* Create the directory. */
	if (nh_fs_crt_dir(pth)) {
		debug("Directory creation failed.\n");
		return 1;
	}

	/* Create the storage flag. */
	if (nh_fs_fcrt_stg(NH_FIL_ATT_RW, "%s/stg", pth)) {
		debug("Storage flag creation failed.\n");
		return 1;
	}

	/* Complete. */
	return 0;

}

/*
 * Construct and return a data system operating
 * on the storage directory at @pth.
 * If an error occurs, return 0.
 */ 
tb_stg_sys *tb_stg_ctr(
	const char *pth
)
{


	/* Check that the storage layout is valid. */
	nh_stt stt;
	assert(
		(!nh_fs_tst(NH_FIL_TYP_DIR, 0, &stt, pth)) &&
		(!nh_fs_ftst(NH_FIL_TYP_STM, 0, &stt, "%s/stg", pth))
		"%s : not a storage directory.\n", pth
	);
	
	/* Allocate. */	
	nh_all_(sys);
	sys->pth = nh_sall(pth);
	ns_map_str_ini(&sys->idxs);
	sys->itf_nb = 0;

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
	assert(ns_map_str_empty(&sys->idxs));

	/* Free. */
	nh_sfre(&sys->pth);
	nh_fre_(sys);
}


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
)
{

	/* Generate the identifier. */ 
	tb_str idt;
	_gen_idt(idt, mkp, ist, lvl);

	/* Search. */
	tb_stg_idx *idx = ns_map_sch(&sys->idxs, str, idt, tb_stg_idx, idxs);
	if (idx) {
		SAFE_INCR(idx->uctr);
		SAFE_INCR(sys->itf_nb);
		goto end;
	}

	/* Allocate. */
	nh_all_(idx);

	/*
	 * Create the mkp dir, the instrument dir,
	 * and the level dir.
	 * Ignore errors.
	 */
	nh_fs_crt_dir("%s/%s", sys->pth, mkp);
	nh_fs_crt_dir("%s/%s/%s", sys->pth, mkp, ist);
	nh_fs_crt_dir("%s/%s/%s/%u", sys->pth, mkp, ist, lvl);

	/* Generate the segment initializer. */
	void *imp_ini = _idx_sgm_ini(mkp, ist, lvl);
	uad elm_max = _idx_elm_max(lvl);

	/* Open the index segment. */
	tb_sgm *sgm = tb_sgm_fopn(
		1,
		imp_ini,
		imp_siz,
		1,
		elm_max,
		(u8 []) {2 * sizeof(u64)},  
		"%s/%s/%s/%u/idx", sys->pth, mkp, ist, lvl
	);
	assert(sgm, "segment %s/%s/%s/%u/idx failed.\n", sys->pth, mkp, ist, lvl);

	/* Initialize. */
	tb_str_cpy(idx->idt, idt);
	assert(!ns_map_str_put(&sys->idxs, &idx->idxs, idx->idt);
	ns_map_u64_ini(&sys->blks);
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
		blk_nb,
		(void **) &idx->tbl,
		1
	);

	/* End section.
	 * Reached when we find or allocate an index. */
	end:;

	/* Attempt to acquire write privileges if required. */
	if (wrt) {
		assert(!tb_sgm_wrt_get(sgm), "%s/%s/%u/idx : failed to acquire write privileges.\n", mkp, ist, lvl);
		assert(!idx->key);
		*key = sys->key = nh_run_tim();
		assert(sys->key);
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
		assert(key == sys->key);
		sys->key = 0;
		tb_sgm_wrt_cpl(sys->sgm);
	}

	/* Unuse. */
	SAFE_DECR(idx->uctr);
	if (!idx->uctr) {
		assert(idx->key);
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
_own_ tb_stg_blk *tb_stg_lod(
	tb_stg_idx *idx,
	u64 tim
) {return _idx_lod(idx);}

/*
 * Unload @blk.
 */
void tb_stg_unl(
	_own_ tb_stg_blk *blk
) {_blk_rel(blk);}

/*
 * If @blk is validated, return 0.
 * If @blk is not validated, return 1.
 */
uerr tb_stg_val_get(
	tb_stg_blk *blk
) {return !!ns_atm(acq, red, acq, &blk->syn->scd_ini);}

/*
 * Attempt to acquire the ownership of @blk's validation.
 * If success, return 0.
 * If someone already acquired it, return 1.
 */
uerr tb_stg_val_ini(
	tb_stg_blk *blk
) {return !!ns_atm(aad, xch, aar, &blk->syn->scd_wip, 1);}

/*
 * Report @blk validated.
 */
uerr tb_stg_val_set(
	tb_stg_blk *blk
)
{
	assert(ns_atm(acq, red, acq, &blk->syn->scd_wip, 1));
	assert(!ns_atm(acq, red, acq, &blk->syn->scd_ini));
	ns_atm(aad, wrt, rel, &blk->syn->scd_wip, 1);
}

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
)
{
	const u64 nb = *nbp = tb_sgm_arr_nb(sgm);
	*timp = _blk_stt(blk);
	return tb_sgm_red_rng(
		blk->sgm, 	
		0,
		nb,
		dst_nb,
		_arr_nb(blk->idx->lvl)
	);
}

/*
 * Return @blk's second tier data.
 */
void *tb_sgm_std(
	tb_stg_blk *blk
) {return _blk_std(blk);}

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
	u64 nb,
	void **dat,
	u8 arr_nb
)
{
	assert(idx->key, "not a writeable index.\n");

	/* Fetch the index table. */
	const u64 blk_nb = _itb_nb(idx);
	volatile u64 (*tbl)[2] = idx->tbl;

	/* Determine the last block. */
	tb_stg_blk *blk = _blk_lst(idx);

	/* Write while data is available.
	 * Check that block is always null when reiterating. */
	for (;nb;{(assert(!blk);})) {

		/* Create a block if required. */
		const u8 crt = (!blk);
		if (crt) blk = _blk_ctr(idx);

		/* Determine the number of writeable elements. */
		const uad blk_max = _blk_max(blk);
		const uad blk_stt = _blk_nb(blk);
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

		/* Get the start time of the data that we will write. */
		const u64 stt = _dat_stt(idx, dat);
		
		/* Perform the write, update locations, return
		 * the time of the last element. */
		u64 end = _blk_wrt(
			blk,
			blk_stt,
			wrt_nb,
			dat,
			arr_nb
		);

		/* If the block was just created, report
		 * it as existing now that its data is populated. */
		if (crt) {

			/* Reference a new block in the index table. */
			_itb_blk_stt_set, tbl, itb_nb + 1, stt);

			/* Report a new element in the index table. */
			const uad itb_nxt = tb_sgm_wrt_don(idx->sgm, 1);
			assert(itb_nxt == itb_nb + 1);
			itb_nb = itb_nxt; 

		}

		/* Report the block's end in the index table. */ 
		_itb_blk_end_set(tbl, itb_nb, end);

		/* Reiterate. */ 
		blk = 0;
	}

}

