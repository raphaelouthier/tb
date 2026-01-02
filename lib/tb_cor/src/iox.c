/* Copyright 2025 Raphael Outhier - confidential - proprietary - no copy - no diffusion. */

#include <tb_cor/tb_cor.all.h>

/****************
 * Level 1 read *
 ****************/

/*
 * If data is available, store its location at @dstp,
 * return the number of available elements.
 * Otherwise, return 0.
 */
static inline u64 _lv1_blk_red(
	tb_dr1 *dr1,
	const u64 tim_cur,
	const void **dsts,
	u8 dsts_nbr,
	u8 *donp,
	u8 *endp
)
{
	assert(dsts_nbr == dr1->dat_nbr);
	assert(tim_cur != 0);
	assert(tim_cur != (u64) -1);

	/* Stream flags. */
	u8 don = 0;
	u8 end = 0;

	/* First, update the block if required.
	 * If none, fail. */
	if (dr1->elm_idx == dr1->elm_max) {

		/* Query next, do nothing if none. */
		tb_stg_blk *blk = tb_stg_red_nxt(dr1->idx, dr1->blk, (u64) -1, 0);
		if (!blk) {
			don = 1;
			end = 1;
			goto end;
		}

		/* Unload previous. */
		tb_stg_unl(dr1->blk);
		dr1->blk = blk;

		/* Update metadata. */
		dr1->elm_nbr = tb_blk_arr(blk, dr1->dats, dsts_nbr, &dr1->sizs);
		dr1->elm_max = tb_stg_blk_max(dr1->blk);
		dr1->elm_idx = 0;
		dr1->blk_end = ((u64 *) dr1->dats[0])[dr1->elm_nbr - 1];

	}

	/* If no time limit, or if time limit is after the
	 * current block's end, provide as much data as possible.
	 * If time limit within the current block, find the max index. */
	assert(dr1->elm_idx < dr1->elm_nbr);
	const u64 elm_nbr = dr1->elm_nbr = tb_stg_elm_nbr(dr1->blk);
	const u64 shf = dr1->elm_idx;
	u64 nbr = dr1->elm_nbr - shf; 
	if (tim_cur <= dr1->blk_end) {
		don = 1;
		const u64 max = dr1->elm_idx = tb_stg_elm_sch((u64 *) dr1->dats[0], elm_nbr, 0, tim_cur); 
		assert(max < elm_nbr);
		assert(shf < max);
		nbr = max - shf;
	}
	
	/* If current block is not full, the end of
	 * the stream is reached. */
	else if (elm_nbr != dr1->elm_max) {
		don = 1;
		end = 1;
	}

	/* Provide data. */
	end:;
	*donp = don;
	*endp = end;
	tb_stg_shf(dsts, dr1->dats, dr1->sizs, dsts_nbr, shf);
	return nbr;

}

/*
 * Add the orderbook snapshot before @dr1's current block.
 */ 
static inline tb_stg_blk *_add_obs(
	tb_dr1 *dr1,
	u64 tim_stt
)
{

	/* Load the initial block. */
	tb_stg_blk *blk = assert(tb_stg_lod_tim(dr1->idx, tim_stt), "no data for initial block.\n");

	/* Load the initial block's predecessor, if any. */
	tb_stg_blk *prv = tb_stg_red_prv(dr1->idx, blk);

	/* If no predecessor, nothing to do. */
	if (!prv) goto end;

	/*
	 * If a predecessor exists, we need to only forward
	 * non-null volumes to the history.
	 * Allocate two temporary arrays.
	 * Fine, as only done during init.
	 */
	u64 *tcks = nh_all(TB_LVL_OBS_NB * sizeof(u64));
	f64 *vols = nh_all(TB_LVL_OBS_NB * sizeof(f64));

	/*
	 * Extract ticks and volumes from @prv's orderbook
	 * snapshot.
	 */
	void *prv_obs = tb_stg_std(prv);
	u64 tck_stt = tb_obs_stt(prv_obs);
	f64 *vols_src = tb_obs_arr(prv_obs);
	u64 upd_nbr = 0;
	for (u64 idx = 0; idx < TB_LVL_OBS_NB; idx++) {
		const f64 vol = vols_src[idx];
		if (!vol) continue;
		tcks[upd_nbr] = tck_stt + idx;
		vols[upd_nbr] = vol;
		upd_nbr++;
	}

	/*
	 * If any updates (non-empty snapshot), initialize
	 * the history.
	 * No preparation or processing required, as we're
	 * only adding resting volumes.
	 */
	if (upd_nbr) {
		tb_lv1_add(
			dr1->hst,
			upd_nbr,
			0,
			tcks,
			vols
		);
	}

	/* Cleanup. */
	nh_fre(tcks, TB_LVL_OBS_NB * sizeof(u64));
	nh_fre(vols, TB_LVL_OBS_NB * sizeof(f64));

	/* Complete. */
	end:;
	return blk;

}

/*
 * Add all updates from @blk's start (containing @tim_stt)
 * until @tim_cur (<) to @dr1's history.
 */
static inline void _add_upds(
	tb_dr1 *dr1,
	tb_stg_blk *blk,
	u64 tim_stt,
	u64 tim_cur,
	u8 end_ok
)
{
	/* Get the number of arrays. */
	const u8 dat_nbr = 3;

	/* Initialize the read environment to read starting
	 * at @blk's first element. */
	dr1->blk = blk;
	const u64 elm_nbr = tb_blk_arr(dr1->blk, (const void **) dr1->dats, dat_nbr, &dr1->sizs);
	assert(elm_nbr);
	assert(dr1->sizs);
	const u64 elm_max = dr1->elm_max = tb_stg_blk_max(dr1->blk);
	const u64 elm_idx = dr1->elm_idx = 0;
	assert(elm_idx < elm_nbr);
	assert(elm_nbr < elm_max);
	const u64 blk_stt = ((u64 *) dr1->dats[0])[0];
	const u64 blk_end = ((u64 *) dr1->dats[0])[elm_nbr - 1];
	assert(blk_stt <= tim_stt);
	assert(tim_stt <= blk_end);
	dr1->blk_end = blk_end;
	dr1->dat_nbr = dat_nbr;
	dr1->tim_lst = tim_stt;

	/* Incorporate all data until @tim_cur. */
	tb_dr1_add(dr1, tim_cur, end_ok);

}

/*
 * Construct a level 1 data reconstructor for (@mkp, @ist)
 * read through @sys, initialized with data up to @tim_cur.
 * The reconstructor has the entire heatmap data
 * populated until @tim_cur. 
 */
tb_dr1 *tb_dr1_ctr(
	tb_stg_sys *sys,
	const char *mkp,
	const char *ist,
	u64 tim_res,
	u64 hmp_dim_tck,
	u64 hmp_dim_tim,
	u64 bac_nb,
	u64 tim_cur
)
{

	/* Construct, open the index, get the block
	 * covering @tim. */
	nh_all__(tb_dr1, dr1);
	tb_str_cpy(dr1->mkp, mkp);
	tb_str_cpy(dr1->ist, ist);
	dr1->stg = sys;
	dr1->idx = assert(tb_stg_opn(dr1->stg, mkp, ist, 1, 0, 0));

	/* Construct a level 1 reconstructor. */
	dr1->hst = tb_lv1_ctr(
		tim_res,
		hmp_dim_tck,
		hmp_dim_tim,
		bac_nb
	);

	/* Determine the total heatmap length and the
	 * heatmap start. */
	assert(hmp_dim_tck < (u64) (u32) -1);
	assert(tim_res < (u64) (u32) -1);
	const u64 hmp_len = hmp_dim_tck * tim_res;
	assert(hmp_len / tim_res == hmp_dim_tck);
	assert(tim_cur > hmp_len);
	const u64 tim_stt = tim_cur - hmp_len;

	/* Add the first block's orderbook snapshot. */
	tb_stg_blk *blk = assert(_add_obs(dr1, tim_stt));

	/* Add all updates until @tim_cur. */
	_add_upds(dr1, blk, tim_stt, tim_cur, 0);

	/* Complete. */
	return dr1;

}

/*
 * Delete @dr1.
 */
void tb_dr1_dtr(
	tb_dr1 *dr1
)
{
	tb_lv1_dtr(dr1->hst);
	tb_stg_unl(dr1->blk);
	tb_stg_cls(dr1->idx, 0);
	nh_fre_(dr1);
}

/*
 * Add data in @dr1 until @tim_cur, process it
 * and update the heatmap and bid-ask curves, if any.
 * If @end_ok is set, tolerate reaching the end of the index.
 * If @end_ok is clear, abort if the end of the index is
 * reached.
 */
void tb_dr1_add(
	tb_dr1 *dr1,
	u64 tim_cur,
	u8 end_ok
)
{

	/* Ensure monotonicity. */
	assert(dr1->tim_lst <= tim_cur);
	dr1->tim_lst = tim_cur;

	/* Prepare until @tim_cur. */
	tb_lv1_prp(dr1->hst, tim_cur);
	
	/* Read iteratively. */
	u8 don = 0;
	u8 end = 0;
	const void *dsts[3];
	while (!don) {

		/* Read. */
		const u64 upd_nbr = _lv1_blk_red(
			dr1,
			tim_cur,
			dsts,
			3,
			&don,
			&end
		);

		/* Verify that end makes sense and is only
		 * encountered when expected. */
		assert((!end) || don);
		assert((!end) || (end_ok), "unexpected end of data.\n");

		/* Add. */
		tb_lv1_add(
			dr1->hst,
			upd_nbr,
			dsts[0],
			dsts[1],
			dsts[2]
		);

	}

	/* Process all updates. */
	tb_lv1_prc(dr1->hst);

}

/**************
 * Validation *
 **************/

/*
 * Level 1 data validation.
 */
static void _val_lv1(
	tb_stg_blk *blk,
	tb_stg_blk *prv,
	void *arg
)
{

	/* Expect a giga orderbook snapshot as arg, and @blk
	 * non-null. @prv can be null when @blk is the
	 * first block. */
	f64 *const gos = arg;
	assert(blk);
	
	/* Get orderbook snapshots. */
	const void *src = (prv) ? tb_stg_std(prv) : 0;
	void *dst = tb_stg_std(blk);

	/* Get arrays. */
	const void *arrs[3];
	const u8 *sizs;
	const u64 upd_nbr = tb_blk_arr(blk, arrs, 3, &sizs); 

	/* Generate the new snapshot. */
	tb_obs_gen(
		dst, src,
		gos, 
		upd_nbr,
		arrs[1], arrs[2]
	);

}

/*********
 * Write *
 *********/

/*
 * Level 0 data write.
 */
void tb_io0_wrt(
	tb_stg_idx *idx,
	u64 nb,
	const u64 *tim,
	const f64 *bid,
	const f64 *ask,
	const f64 *avg,
	const f64 *vol,
	void (*val_fnc)(tb_stg_blk *blk, tb_stg_blk *prv, void *val_arg),
	void *val_arg
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
		5,
		0, 0
	);
}

/*
 * Level 1 data write.
 * Receives a giga orderbook snapshot to compute the
 * new block's orderbook snapshot.
 */
void tb_io1_wrt(
	tb_stg_idx *idx,
	u64 nb,
	const u64 *tim,
	const f64 *prc,
	const f64 *vol,
	f64 *gos
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
		3,
		&_val_lv1, (void *) gos
	);
}
	
/*
 * Level 2 data write.
 */
void tb_io2_wrt(
	tb_stg_idx *idx,
	u64 nb,
	const u64 *tim,
	const u64 *ord,
	const u64 *trd,
	const u8 *typ,
	const f64 *prc,
	const f64 *vol,
	void (*val_fnc)(tb_stg_blk *blk, tb_stg_blk *prv, void *val_arg),
	void *val_arg
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
		6,
		0, 0
	);
}

