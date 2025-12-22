/* Copyright 2025 Raphael Outhier - confidential - proprietary - no copy - no diffusion. */

#include <tb_cor/tb_cor.all.h>

/****************
 * Provider API *
 ****************/

/*
 * Construct and return a provider.
 */
tb_prv *tb_prv_ctr(
	const char *pth,
	u8 sim
)
{
	tb_stg_ini(pth);
	tb_stg_sys *stg = assert(tb_stg_ctr(pth, 0)); 
	nh_all__(tb_prv, prv);
	prv->stg = stg;
	prv->tim_max = (sim) ? (u64) -1 : 0; 
	return prv;
}

/*
 * Delete @prv.
 */
void tb_prv_dtr(
	tb_prv *prv
)
{
	tb_stg_dtr(prv->stg);
	nh_fre_(prv);	
}

/**************
 * Stream API *
 **************/

/*
 * Create a stream.
 */
tb_prv_stm *tb_prv_opn(
	tb_prv *prv,
	const char *mkp,
	const char *ist,
	u8 lvl,
	u64 tim_stt
)
{

	/* Get the number of arrays. */
	const u8 dat_nbr = tb_lvl_arr_nbr(lvl);
	assert(dat_nbr <= TB_ANB_MAX);

	/* Construct, open the index, get the block
	 * covering @tim. */
	assert(lvl < 3);
	nh_all__(tb_prv_stm, stm);
	tb_str_cpy(stm->mkp, mkp);
	tb_str_cpy(stm->ist, ist);
	stm->lvl = lvl;
	stm->idx = assert(tb_stg_opn(prv->stg, mkp, ist, lvl, 0, 0));
	stm->blk = assert(tb_stg_lod_tim(stm->idx, tim_stt));

	/* Get the first blocks metadata. */
	const u64 elm_nbr = tb_blk_arr(stm->blk, (const void **) stm->dats, stm->dat_nbr, &stm->sizs);
	assert(elm_nbr);
	assert(stm->sizs);
	const u64 elm_max = stm->elm_max = tb_stg_blk_max(stm->blk);
	const u64 elm_idx = stm->elm_idx = tb_stg_elm_sch((u64 *) stm->dats[0], elm_nbr, 0, tim_stt); 
	assert(elm_idx < elm_nbr);
	assert(elm_nbr < elm_max);
	stm->blk_end = ((u64 *) stm->dats[0])[elm_nbr - 1];
	stm->dat_nbr = dat_nbr;

	/* Complete. */
	return stm;

}

/*
 * Delete a stream.
 */
void tb_prv_cls(
	tb_prv_stm *stm
)
{
	tb_stg_unl(stm->blk);
	tb_stg_cls(stm->idx, 0);
	nh_fre_(stm);
}

/*
 * If data is available, store its location at @dstp,
 * return the number of available elements.
 * Otherwise, return 0.
 */
u64 tb_prv_red(
	tb_prv_stm *stm,
	const void **dsts,
	u8 dsts_nbr
)
{
	assert(dsts_nbr == stm->dat_nbr);

	/* First, update the block if required.
	 * If none, fail. */
	if (stm->elm_idx == stm->elm_max) {

		/* Query next, do nothing if none. */
		tb_stg_blk *blk = tb_stg_red_nxt(stm->idx, stm->blk, (u64) -1, 0);
		if (!blk) return 0;

		/* Unload previous. */
		tb_stg_unl(stm->blk);
		stm->blk = blk;

		/* Update metadata. */
		stm->elm_nbr = tb_blk_arr(blk, stm->dats, dsts_nbr, &stm->sizs);
		stm->elm_max = tb_stg_blk_max(stm->blk);
		stm->elm_idx = 0;
		stm->blk_end = ((u64 *) stm->dats[0])[stm->elm_nbr - 1];

	}

	/* If no time limit, or if time limit is after the
	 * current block's end, provide as much data as possible.
	 * If time limit within the current block, find the max index. */
	assert(stm->elm_idx < stm->elm_nbr);
	const u64 elm_nbr = stm->elm_nbr = tb_stg_elm_nbr(stm->blk);
	const u64 tim_max = stm->prv->tim_max;
	const u64 shf = stm->elm_idx;
	u64 nbr = stm->elm_nbr - shf; 
	if ((tim_max != (u64) -1) && (tim_max <= stm->blk_end)) {
		const u64 max = stm->elm_idx = tb_stg_elm_sch((u64 *) stm->dats[0], elm_nbr, 0, tim_max); 
		assert(max < elm_nbr);
		assert(shf < max);
		nbr = max - shf;
	}

	/* Provide data. */
	tb_stg_shf(dsts, stm->dats, stm->sizs, dsts_nbr, shf);
	return nbr;

}
