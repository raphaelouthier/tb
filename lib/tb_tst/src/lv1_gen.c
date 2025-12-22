/* Copyright 2025 Raphael Outhier - confidential - proprietary - no copy - no diffusion. */

#include <tb_tst/tb_tst.all.h>

#include <tb_tst/lv1_utl.h>

/*
 * Add @obk to @ctx's heatmap at column @col_idx.
 */
static inline void _hmp_add(
	tb_tst_lv1_ctx *ctx,
	f64 *obk,
	u64 col_idx,
	u64 nxt_idx
)
{
	
	const u64 tck_nbr = ctx->tck_nbr;
	const u64 hmp_stt = col_idx * tck_nbr; 
	for (u64 tck_idx = tck_nbr; tck_idx--;) {
		ctx->hmp[hmp_stt + tck_idx] += obk[tck_idx];
	}
	if (col_idx != nxt_idx) {
		assert(nxt_idx == col_idx + 1);
		const u64 hmp_nxt = nxt_idx * tck_nbr; 
		for (u64 tck_idx = tck_nbr; tck_idx--;) {
			ctx->hmp_ini[hmp_nxt + tck_idx] += obk[tck_idx];
		}
	}
}

/*
 * Resize the updates arrays.
 */
static inline void _upd_rsz(
	tb_tst_lv1_ctx *ctx,
	u64 nb
)
{
	if (ctx->upd_max < nb) {
		const u64 _upd_max = 2 * ctx->upd_max;
		assert(nb == ctx->upd_max + 1);
		tb_tst_lv1_upd *_upds = nh_all(_upd_max * sizeof(tb_tst_lv1_upd));
		ns_mem_cpy(_upds, ctx->upds, ctx->upd_nbr * sizeof(tb_tst_lv1_upd));
		nh_fre(ctx->upds, ctx->upd_max * sizeof(tb_tst_lv1_upd));
		ctx->upds = _upds;
		ctx->upd_max = _upd_max;
		debug("rsz %U.\n", ctx->upd_max);
	}
}

/*
 * Let time pass, return 1 if end time is reached,
 * 0 if not.
 */
static inline u8 _gen_skp(
	tb_tst_lv1_ctx *ctx,
	tb_tst_lv1_gen *gen,
	u64 itr_idx,
	u64 *tim_curp,
	u64 tim_inc,
	u64 tim_end,
	f64 *obk_cur
)
{
	u64 tim_cur = *tim_curp;

	/* Determine if and how much iterations we should skip. */
	u64 skp_nb = (*(gen->skp))(gen, itr_idx);

	/* Iteratively add the current orderbook to
	 * the heatmap and add time increment. */
	while (skp_nb--) {
		const u64 aid_cur = _tim_to_aid(ctx, tim_cur);
		const u64 aid_nxt = _tim_to_aid(ctx, tim_cur + ctx->tim_stp);
		assert(aid_cur < ctx->unt_nbr);
		assert(aid_nxt < ctx->unt_nbr);
		_hmp_add(ctx, obk_cur, aid_cur, aid_nxt);
		tim_cur += tim_inc;
		assert(tim_cur <= tim_end);
		if (tim_cur == tim_end) {
			*tim_curp = tim_cur;
			return 1;
		}
	}

	*tim_curp = tim_cur;
	return 0;
}

/*
 * Find best bid / ask at current time,
 * update generation metadata.
 */
static inline void _bac_upd(
	tb_tst_lv1_ctx *ctx,
	f64 *obk_cur,
	f64 *obk_prv,
	u64 *prv_bst_bid,
	u64 *prv_bst_ask,
	u64 aid_cur,
	u64 aid_prv
)
{
	assert(!ns_mem_cmp(obk_cur, obk_prv, ctx->tck_nbr * sizeof(f64)));
	u64 bst_bid = 0;
	u64 bst_ask = (u64) -1;
	
	/* Determine the best bid and best ask. */
	tb_obk_bst_bat(
		obk_cur,
		ctx->tck_nbr,
		0, ctx->tck_nbr,
		ctx->tck_min,
		&bst_bid, &bst_ask,
		0, 0
	);
	assert(bst_bid < bst_ask);

	/* Incorporate the best bid and best ask. */
	if (ctx->bid_arr[aid_cur] < bst_bid) ctx->bid_arr[aid_cur] = bst_bid;
	if (ctx->ask_arr[aid_cur] > bst_ask) ctx->ask_arr[aid_cur] = bst_ask;
	if (aid_cur != aid_prv) {
		ctx->bid_ini[aid_cur] = bst_bid;
		ctx->ask_ini[aid_cur] = bst_ask;
	}
	*prv_bst_bid = bst_bid;
	*prv_bst_ask = bst_ask;
}

/*
 * Propagate BAC data until @aid_cur.
 * @aid cur may be equal to @ctx->unt_nbr.
 */
static inline void _bac_prp(
	tb_tst_lv1_ctx *ctx,
	u64 aid_cur,
	u64 aid_prv,
	u64 aid_lst,
	u64 prv_bst_bid,
	u64 prv_bst_ask,
	u64 *tck_refp
)
{

	const u64 aid_max = ctx->unt_nbr;
	assert((aid_cur != aid_max) || (aid_prv + 1 == aid_max));

	/* If we changed cell : */
	if (aid_lst != aid_cur) {

		/* Propagate best bid/ask to skipped cells. */
		if (aid_lst != aid_prv) {
			for (u64 idx = aid_lst + 1; idx <= aid_prv; idx++) {
				ctx->bid_arr[idx] = prv_bst_bid;
				ctx->ask_arr[idx] = prv_bst_ask;
				ctx->bid_ini[idx] = prv_bst_bid;
				ctx->ask_ini[idx] = prv_bst_ask;
			}
		}

		/* Initialize the current cell. */
		if ((aid_cur != aid_max) && (aid_prv != aid_cur)) {
			ctx->bid_arr[aid_cur] = 0;
			ctx->ask_arr[aid_cur] = (u64) -1;
		}

		/*
		 * The reference point of the heatmap at a given
		 * time uses the best bid and ask for the
		 * orderbook state before (<) this time. 
		 * If we are in a new cell, propagate the
		 * reference value up to this cell.
		 */ 
		for (u64 idx = aid_lst + 1; idx <= aid_cur; idx++) {
			if (idx < aid_max) {
				*tck_refp = ctx->ref_arr[idx] = tb_obk_anc(prv_bst_bid, prv_bst_ask, *tck_refp, ctx->hmp_dim_tck);
			}
		}

	} else {
		assert(aid_cur == aid_prv);
	}
}


static u64 _upd_gen_prms[8] = {
	1,
	3,
	19,
	31,
	59,
	241,
	701,
	947
};

static inline void _upd_gen(
	tb_tst_lv1_ctx *ctx,
	f64 *obk_cur,
	f64 *obk_prv,
	u8 *prm_idxp,
	u8 *bmp,
	u64 itr_idx,
	u64 tim_cur
)
{

	/* Get the update orderer, update the index. */
	u8 prm_idx = *prm_idxp;
	u64 prm = _upd_gen_prms[prm_idx];
	*prm_idxp = (u8) (prm_idx + 1) % 8;
	
	/* Traverse the orderbooks in a different order. */
	u64 tck_nbr = ctx->tck_nbr;
	ns_mem_rst(bmp, tck_nbr);
	check(prm % tck_nbr);
	check((prm == 1) || (tck_nbr % prm));
	for (u64 idx = 0; idx < tck_nbr; idx++) {

		/* Reorder. */
		const u64 tck_idx = (itr_idx + prm * idx) % tck_nbr; 

		/* Report tick processed. */
		assert(!bmp[tck_idx]);
		bmp[tck_idx] = 1;

		/* Read prev and current volume. */
		const f64 vol_prv = obk_prv[tck_idx];
		const f64 vol_cur = obk_cur[tck_idx];
		if (vol_prv == vol_cur) continue;
		obk_prv[tck_idx] = vol_cur;

		/* If required, generate a tick update. */ 
		u64 upd_nbr = ctx->upd_nbr;
		_upd_rsz(ctx, upd_nbr + 1);
		check(upd_nbr < ctx->upd_max);
		ctx->upds[upd_nbr].tim = tim_cur;	
		ctx->upds[upd_nbr].tck = _idx_to_tck(ctx, tck_idx);
		ctx->upds[upd_nbr].vol = vol_cur;
		ctx->upd_nbr++;
		
	}
	
	/* Verify that each tick was accounted for. */
	for (u64 idx = 0; idx < ctx->tck_nbr; idx++) {
		assert(bmp[idx] == 1);
	}

}

/*
 * Generate a sequence of orderbook updates as
 * specified by @ctx.
 */
void tb_tst_lv1_upds_gen(
	tb_tst_lv1_ctx *ctx,
	tb_tst_lv1_gen *gen
)
{
	assert(ctx->unt_nbr < 10000);

	/* Read request data. */
	const u64 unt_nbr = ctx->unt_nbr; 
	const u64 tim_stt = ctx->tim_stt; 
	const u64 tim_inc = ctx->tim_inc; 
	const f64 prc_min = ctx->prc_min;
	const u64 tck_nbr = ctx->tck_nbr;
	const u64 tim_stp = ctx->tim_stp;

	/* 100 ticks per price unit. */
	const f64 prc_cfc = (f64) ctx->tck_rat;
	const f64 prc_stp = 1 / prc_cfc;
	assert(prc_cfc == (f64) 1 / prc_stp);

	/* Allocate the updates array. */ 	
	ctx->upd_max = ctx->unt_nbr * tim_stp; 
	ctx->upds = nh_all(ctx->upd_max * sizeof(tb_tst_lv1_upd));
	ctx->upd_nbr = 0;

	/* Generate the range. */
	const u64 tck_min = ctx->tck_min = (u64) ((f64) (prc_min * prc_cfc));
	const u64 tck_max = ctx->tck_max = tck_min + tck_nbr;
	check(tck_min < tck_max);

	/* Allocate two orderbooks. */
	u64 obk_siz = tck_nbr * sizeof(f64);
	f64 *const obk_cur = ctx->obk = nh_all(obk_siz);
	f64 *const obk_prv = nh_all(obk_siz);

	/* Allocate the bitmap. */
	u64 bmp_siz = tck_nbr;
	u8 *bmp = nh_all(bmp_siz);

	/* Initialize the generator. */
	(*(gen->ini))(gen, ctx->sed, tck_min, tck_max, ctx->ref_vol);

	/* Generate the initial state. */
	for (u64 i = 0; i < tck_nbr; i++) obk_cur[i] = 0;
	(*(gen->obk_upd))(gen, obk_cur);
	ns_mem_cpy(obk_prv, obk_cur, obk_siz);

	/* Allocate the bid/ask arrays. */
	ctx->bid_arr = nh_all(unt_nbr * sizeof(u64));
	ctx->ask_arr = nh_all(unt_nbr * sizeof(u64));
	ctx->bid_ini = nh_all(unt_nbr * sizeof(u64));
	ctx->ask_ini = nh_all(unt_nbr * sizeof(u64));
	ctx->ref_arr = nh_all(unt_nbr * sizeof(u64));

	/* Allocate the heatmap and its initial copy. */
	const u64 hmp_nbr = unt_nbr * tck_nbr; 
	ctx->hmp = nh_all(hmp_nbr * sizeof(f64));
	ctx->hmp_ini = nh_all(hmp_nbr * sizeof(f64));
	ns_mem_rst(ctx->hmp, hmp_nbr * sizeof(f64));
	ns_mem_rst(ctx->hmp_ini, hmp_nbr * sizeof(f64));

	/* The first row of the hashmap init is never set by
	 * hmp_add. Do it now. */
	ns_mem_cpy(ctx->hmp_ini, obk_cur, obk_siz);

	u64 prv_bst_bid = 0;
	u64 prv_bst_ask = (u64) -1;
	ctx->bid_arr[0] = prv_bst_bid;
	ctx->ask_arr[0] = prv_bst_ask;
	_bac_upd(ctx, obk_cur, obk_prv, &prv_bst_bid, &prv_bst_ask, 0, (u64) -1);

	/* Tick update offsets. */
	u8 prm_idx = 0;

	/* Determine the initial anchoring. */
	u64 prv_ref = ctx->hmp_dim_tck >> 1;  
	ctx->ref_arr[0] = prv_ref = tb_obk_anc(prv_bst_bid, prv_bst_ask, prv_ref, ctx->hmp_dim_tck);
	//assert(0, "ini %U %U %U.\n", prv_bst_bid, prv_bst_ask, ctx->ref_arr[0]);

	/* Generate as many updates as requested. */ 
	u64 tim_lst = tim_stt;
	u64 tim_cur = tim_lst;
	const u64 tim_end = tim_stt + tim_inc * tim_stp * unt_nbr;
	assert(tim_stt == tim_cur);
	assert(tim_cur < tim_end);
	assert(!(tim_stt % (tim_stp * tim_inc)));
	assert(!(tim_end % (tim_stp * tim_inc)));
	u64 itr_idx = 0;
	while (tim_cur < tim_end) {

		/* Let some time pass. */
		const u8 end = _gen_skp(ctx, gen, itr_idx, &tim_cur, tim_inc, tim_end, obk_cur);

		/* Determine aids. */
		check(tim_cur != 0);
		assert(tim_cur >= tim_stt);
		const u64 aid_lst = _tim_to_aid(ctx, tim_lst);
		const u64 aid_prv = (tim_cur == tim_stt) ? 0 : _tim_to_aid(ctx, tim_cur - 1);
		const u64 aid_cur = _tim_to_aid(ctx, tim_cur);
		const u64 aid_nxt = _tim_to_aid(ctx, tim_cur + tim_stp);
		assert(aid_lst <= aid_prv);
		assert(aid_lst < unt_nbr);
		assert(aid_prv < unt_nbr);

		/* Propagate bid-ask to the current cells. */
		_bac_prp(ctx, aid_cur, aid_prv, aid_lst, prv_bst_bid, prv_bst_ask, &prv_ref);

		/* Update the last time. */
		tim_lst = tim_cur;

		if (end) {
			goto out;
		}

		assert(aid_nxt < unt_nbr);
		assert(aid_cur < unt_nbr);

		/* Generate the new current tick. */
		(*(gen->tck_upd))(gen);

		/* Generate a new orderbook state. */
		(*(gen->obk_upd))(gen, obk_cur);

		/* Generate updates from orderbook states. */
		_upd_gen(ctx, obk_cur, obk_prv, &prm_idx, bmp, itr_idx, tim_cur);

		/* Determine the current best bid and ask. */
		_bac_upd(ctx, obk_cur, obk_prv, &prv_bst_bid, &prv_bst_ask, aid_cur, aid_prv);

		/* Update the number of remaining updates. */
		assert(ctx->upd_nbr <= ctx->upd_max);

		/* Incorporate the current column to the heatmap,
		 * update the current time. */
		_hmp_add(ctx, obk_cur, aid_cur, aid_nxt);
		tim_cur += tim_inc;
		itr_idx++;

	}
	out:;
	assert(tim_cur == tim_end);

	/* Compute the actual average of hashmap cells. */
	for (u64 hmp_idx = hmp_nbr; hmp_idx--;) {
		ctx->hmp[hmp_idx] /= (f64) tim_stp;
	}

	/* Release. */
	nh_fre(obk_prv, obk_siz);
	nh_fre(bmp, bmp_siz);

	/* Transmit generation data. */
	assert(tim_cur > tim_stt);
	assert(tim_cur - tim_inc >= tim_stt);
	ctx->tim_end = tim_cur - tim_inc;

}

/*
 * Free all generated tick data.
 */
void tb_tst_lv1_upds_del(
	tb_tst_lv1_ctx *ctx
)
{
	nh_fre(ctx->upds, ctx->upd_max * sizeof(tb_tst_lv1_upd));
	nh_fre(ctx->obk, ctx->tck_nbr * sizeof(f64));
	nh_fre(ctx->bid_arr, ctx->unt_nbr * sizeof(u64));
	nh_fre(ctx->ask_arr, ctx->unt_nbr * sizeof(u64));
	nh_fre(ctx->bid_ini, ctx->unt_nbr * sizeof(u64));
	nh_fre(ctx->ask_ini, ctx->unt_nbr * sizeof(u64));
	nh_fre(ctx->ref_arr, ctx->unt_nbr * sizeof(u64));
	const u64 hmp_nbr = ctx->unt_nbr * ctx->tck_nbr; 
	nh_fre(ctx->hmp, hmp_nbr * sizeof(f64));
	nh_fre(ctx->hmp_ini, hmp_nbr * sizeof(f64));
}
