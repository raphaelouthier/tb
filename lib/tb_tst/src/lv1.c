/* Copyright 2025 Raphael Outhier - confidential - proprietary - no copy - no diffusion. */

#include <tb_tst/tb_tst.all.h>

/*
 * Update @ctx's current tick.
 */
static inline void _tck_upd(
	tb_tst_lv1_ctx *ctx
)
{
	u64 tck_cur = ctx->tck_cur; 
	u64 gen_sed = ctx->gen_sed;
	ctx->tck_prv = ctx->tck_cur;

	/* Determine if we should go up or down. */
	u8 up_ok = tck_cur + 1 < ctx->tck_max;
	u8 dwn_ok = ctx->tck_min < tck_cur;
	assert(up_ok || dwn_ok);
	u8 is_dwn;
	if (!up_ok) {
		is_dwn = 1;
	} else if (!dwn_ok) {
		is_dwn = 0;
	} else {
		is_dwn = ((gen_sed >> 32) & 0xff) < 0x80;
	}
	gen_sed = ns_hsh_mas_gen(gen_sed);

	/* Determine the price increment logarithmically. */
	u8 inc_sed = gen_sed & 0xff;
	u8 inc_ord = 0;
	if (inc_sed < (1 << 1)) {
		inc_ord = 7;
	} else if (inc_sed < (1 << 2)) {
		inc_ord = 6;
	} else if (inc_sed < (1 << 3)) {
		inc_ord = 5;
	} else if (inc_sed < (1 << 4)) {
		inc_ord = 4;
	} else if (inc_sed < (1 << 5)) {
		inc_ord = 3;
	} else {
		inc_ord = 2;
	}
	check(inc_ord);
	u64 inc_max = 1 << inc_ord;
	assert(inc_max < ctx->tck_min); 
	assert(inc_max < inc_max + ctx->tck_max);
	gen_sed = ctx->gen_sed = ns_hsh_mas_gen(gen_sed);

	/* Determine the actual increment. */
	const u64 inc_val = ns_hsh_u64_rng(gen_sed, 0, inc_max, 1);

	/* Determine the new target price. */
	if (is_dwn) {
		tck_cur = tck_cur - inc_val;
		if (tck_cur < ctx->tck_min) tck_cur = ctx->tck_min;
	} else {
		tck_cur = tck_cur + inc_val;
		if (tck_cur >= ctx->tck_max) tck_cur = ctx->tck_max - 1;
	}
	ctx->gen_sed = ns_hsh_mas_gen(gen_sed);
	ctx->tck_cur = tck_cur;

}

/*
 * Update @obk after @ctx's price was updated.
 */
static inline void _obk_upd(
	tb_tst_lv1_ctx *ctx,
	f64 *obk
)
{

	/* Flags. */
	ctx->obk_rst = 0;

	/* Constants. */
	const f64 gen_vol = ctx->gen_vol; 

	/* Read the current and previous prices. */
	const u64 tck_cur = ctx->tck_cur;
	const u64 tck_prv = ctx->tck_prv;
	debug("tck_cur %U.\n", tck_cur);
	debug("tck_prv %U.\n", tck_prv);
	const u64 tck_min = ctx->tck_min;
	const u64 tck_max = ctx->tck_max;
	assert(tck_min > 400);
	#define OBK(idx) (obk[((idx) - tck_min)])
	#define OBK_WRT(idx, val) ({ \
		if ((tck_min <= idx) && (idx < tck_max)) { \
			OBK(idx) = (val); \
		} \
	})

	/* Read the seed. */
	u64 gen_sed = ctx->gen_sed;
	u64 gen_idx = ctx->gen_idx;

	/* Reset ticks in between. */
	{
		assert(!(OBK(tck_prv)));
		u64 rst_stt = tck_cur; 
		u64 rst_end = tck_prv; 
		if (rst_end < rst_stt) {
			_swap(rst_end, rst_stt);
		}
		for (;rst_stt <= rst_end; rst_stt++) {
			OBK(rst_stt) = 0;
		}
		assert(!(OBK(tck_cur)));
	}

	/* Reset around the current tick. */
	{
		if (gen_sed & (1 << 20)) {
			u8 rst_up = (gen_sed & 0x7);
			u8 rst_dwn = ((gen_sed >> 3) & 0x7);
			for (
				u64 rst_stt = (tck_cur - rst_dwn), rst_end = (tck_cur + rst_up);
				rst_stt <= rst_end;
				rst_stt++
			) {
				if ((tck_min <= rst_stt) && (rst_stt < tck_max)) {
					OBK(rst_stt) = 0;
				}	
			}
		}
	}
	gen_sed = ns_hsh_mas_gen(gen_sed);

	/* Reset the orderbook if required. */ 
	if (gen_idx && (!(gen_idx % ctx->obk_rst_prd))) {
		for (u64 idx = tck_min; idx < tck_max; idx++) {
			OBK(idx) = 0;
		}

		/* Skip all other generation steps. */
		goto end;
	}

	/* Normal and exceptional steps. */
	static u64 nrm_stps[4] = {
		1,
		1,
		5,
		2		
	};

	/* Apply normal bids and ask updates on 10 ticks around the current price. */
	if (1) { //!(gen_idx % ctx->obk_ask_nrm_prd)) {
		f64 ask_max = ns_hsh_f64_rng(gen_sed, gen_vol / 2, gen_vol, gen_vol / 100);
		const u64 stp = nrm_stps[(gen_idx) % 4];
		for (u64 idx = tck_cur, ctr = 0; ((idx += stp) < tck_max) && (ctr++ < 10);) {
			OBK(idx) = ask_max;
			ask_max /= 1.1;
		}	
		gen_sed = ns_hsh_mas_gen(gen_sed);
	}
	if (1) { //!(gen_idx % ctx->obk_bid_nrm_prd)) {
		f64 bid_max = -ns_hsh_f64_rng(gen_sed, gen_vol / 2, gen_vol, gen_vol / 100);
		const u64 stp = nrm_stps[(gen_idx) % 4];
		for (u64 idx = tck_cur, ctr = 0; (tck_min <= (idx -= stp)) && (ctr++ < 10);) {
			OBK(idx) = bid_max;
			bid_max /= 1.1;
		}	
		gen_sed = ns_hsh_mas_gen(gen_sed);
	}

	/* Skip resets if first iteration. */
	if (!gen_idx) goto end;

	static u64 exc_stps[3] = {
		100,
		200,
		400
	};
	/* Apply exceptional bids and asks on the whole orderbook. */
	if (!(gen_idx % ctx->obk_ask_exc_prd)) {
		f64 ask_max = ns_hsh_f64_rng(gen_sed, gen_vol / 2, gen_vol, gen_vol / 100);
		const u64 stp = exc_stps[(gen_idx / ctx->obk_ask_exc_prd) % 3];
		for (u64 idx = tck_cur; (idx += stp) < tck_max;) {
			OBK(idx) = ask_max;
			ask_max /= 1.1;
		}	
		gen_sed = ns_hsh_mas_gen(gen_sed);
	}
	if (!(gen_idx % ctx->obk_bid_exc_prd)) {
		f64 bid_max = -ns_hsh_f64_rng(gen_sed, gen_vol / 2, gen_vol, gen_vol / 100);
		const u64 stp = exc_stps[(gen_idx / ctx->obk_bid_exc_prd) % 3];
		for (u64 idx = tck_cur; tck_min <= (idx -= stp);) {
			OBK(idx) = bid_max;
			bid_max /= 1.1;
		}	
		gen_sed = ns_hsh_mas_gen(gen_sed);
	}

	static u64 rst_stps[3] = {
		12,
		29,
		51
	};
	/* Apply resets on the whole orderbook. */
	if (gen_idx && !(gen_idx % ctx->obk_ask_rst_prd)) {
		const u64 stp = rst_stps[(gen_idx / ctx->obk_ask_rst_prd) % 3];
		for (u64 idx = tck_cur; (idx += stp) < tck_max;) {
			OBK(idx) = 0;
		}	
		gen_sed = ns_hsh_mas_gen(gen_sed);
	}
	if (gen_idx && !(gen_idx % ctx->obk_bid_rst_prd)) {
		const u64 stp = rst_stps[(gen_idx / ctx->obk_bid_rst_prd) % 3];
		for (u64 idx = tck_cur; tck_min <= (idx -= stp);) {
			OBK(idx) = 0;
		}	
		gen_sed = ns_hsh_mas_gen(gen_sed);
	}

	/* Save post generation params. */	
	end:;
	ctx->gen_sed = gen_sed;
	ctx->gen_idx = gen_idx + 1;

}

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

#define TCK_TO_PRC(tck_idx, min, stp) \
	((f64) min + (f64) (stp) * (f64) (tck_idx))

/*
 * Determine the current tick reference given the current
 * bid-ask, the previous tick reference and
 * the heatmap tick dimension.
 */
static inline u64 _tck_ref(
	u64 bid,
	u64 ask,
	u64 prv,
	u64 dim
)
{
	u64 ref = 0;
	u64 mid = dim >> 1;
	if ((bid != 0) && (ask != (u64) -1)) {
		ref = (bid + ask) / 2;
	} else {
		if (bid != 0) {
			assert(ask == (u64) -1);
			ref = bid;
		} else if (ask != (u64) -1) {
			assert(bid == 0);
			ref = ask;
		} else {
			assert(ask == (u64) -1);
			assert(bid == 0);
			ref = prv;
			assert(ref >= mid);
		}
	}
	return (ref < mid) ? mid : ref; 
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
 * Time -> AID.
 */
static inline u64 _tim_to_aid(
	tb_tst_lv1_ctx *ctx,
	u64 tim
)
{
	return (tim - ctx->tim_stt) / ctx->aid_wid;
}

static inline u64 _prc_to_tck(
	tb_tst_lv1_ctx *ctx,
	f64 prc
)
{
	assert(prc >= ctx->prc_min);
	return (u64) (f64) ((prc - ctx->prc_min) * ((f64) ctx->tck_rat + 0.1));
}


/*
 * Let time pass, return 1 if end time is reached,
 * 0 if not.
 */
static inline u8 _gen_skp(
	tb_tst_lv1_ctx *ctx,
	u64 itr_idx,
	u64 *tim_curp,
	u64 tim_inc,
	u64 tim_end,
	f64 *obk_cur
)
{
	u64 tim_cur = *tim_curp;

	/* Periodically let some time pass, up to 10 columns. */
	if (!(itr_idx % ctx->skp_prd)) {

		/* Determine how much time should pass. */
		u64 skp_nb = ns_hsh_u8_rng(ctx->gen_sed, 1, 30, 1);
		if (ctx->obk_rst) {
			SAFE_INCR(ctx->obk_rst_dly);
			if (skp_nb > 20) {
				SAFE_INCR(ctx->obk_rst_skp);
			}
		}

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
	u64 bst_bid = 0;
	u64 bst_ask = (u64) -1;
	for (u64 idx = 0; idx < ctx->tck_nbr; idx++) {
		const f64 vol = obk_cur[idx];
		assert(obk_prv[idx] == vol);
		if (!vol) continue;
		const u8 is_bid = vol < 0;
		const u64 tck_idx = ctx->tck_min + idx;
		assert(tck_idx);
		assert(tck_idx != (u64) -1);
		if (is_bid) {
			assert(bst_ask == (u64) -1); // Bid found at tick below first ask.
			if (bst_bid < tck_idx) bst_bid = tck_idx;
		} else {
			if (bst_ask != (u64) -1) bst_ask = tck_idx;
		}
	}
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
		if (aid_prv != aid_cur) {
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
			*tck_refp = ctx->ref_arr[idx] = _tck_ref(prv_bst_bid, prv_bst_ask, *tck_refp, ctx->hmp_dim_tck);
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
	u64 tim_cur,
	f64 prc_min,
	f64 prc_stp
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
		ctx->upds[upd_nbr].val = TCK_TO_PRC(tck_idx, prc_min, prc_stp);
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
static inline void _tck_gen(
	tb_tst_lv1_ctx *ctx
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
	f64 *const obk0 = nh_all(obk_siz);
	f64 *const obk1 = nh_all(obk_siz);

	/* Allocate the bitmap. */
	u64 bmp_siz = tck_nbr;
	u8 *bmp = nh_all(bmp_siz);

	/* Choose the initial tick. */
	ctx->gen_sed = ns_hsh_mas_gen(ctx->sed);
	ctx->tck_cur = ctx->tck_prv = ns_hsh_u64_rng(ctx->gen_sed, tck_min, tck_max + 1, 1);
	ctx->gen_idx = 0;

	/* Generate the initial state. */
	f64 *obk_prv = obk0;
	f64 *obk_cur = obk1;
	for (u64 i = 0; i < tck_nbr; i++) obk_prv[i] = 0;
	_obk_upd(ctx, obk_prv);
	ns_mem_cpy(obk_cur, obk_prv, obk_siz);

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

	/* Start with the heatmap on ticks bottom. */
	u64 prv_ref = ctx->hmp_dim_tck >> 1;  
	ctx->ref_arr[0] = prv_ref;

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
		debug("itr %U.\n", itr_idx);

		/* Let some time pass. */
		const u8 end = _gen_skp(ctx, itr_idx, &tim_cur, tim_inc, tim_end, obk_cur);
		if (end) {
			goto out;
		}

		/* Determine aids. */
		check(tim_cur != 0);
		assert(tim_cur > tim_stt);
		const u64 aid_lst = _tim_to_aid(ctx, tim_lst);
		const u64 aid_prv = _tim_to_aid(ctx, tim_cur - 1);
		const u64 aid_cur = _tim_to_aid(ctx, tim_cur);
		const u64 aid_nxt = _tim_to_aid(ctx, tim_cur + tim_stp);
		assert(aid_lst <= aid_prv);
		assert(aid_lst < unt_nbr);
		assert(aid_prv < unt_nbr);
		assert(aid_nxt < unt_nbr);
		assert(aid_cur < unt_nbr);

		/* Update the last time. */
		tim_lst = tim_cur;

		/* Propagate bid-ask to the current cells. */
		_bac_prp(ctx, aid_cur, aid_prv, aid_lst, prv_bst_bid, prv_bst_ask, &prv_ref);

		/* Generate the new current tick. */
		_tck_upd(ctx);

		/* Generate a new orderbook state. */
		_obk_upd(ctx, obk_cur);

		/* Generate updates from orderbook states. */
		_upd_gen(ctx, obk_cur, obk_prv, &prm_idx, bmp, itr_idx, tim_cur, prc_min, prc_stp); 

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
	nh_fre(obk0, obk_siz);
	nh_fre(obk1, obk_siz);
	nh_fre(bmp, bmp_siz);

	/* Transmit generation data. */
	assert(tim_cur > tim_stt);
	assert(tim_cur - tim_inc >= tim_stt);
	ctx->tim_end = tim_cur - tim_inc;

}

/*
 * Free all generated tick data.
 */
static inline void _tck_del(
	tb_tst_lv1_ctx *ctx
)
{
	nh_fre(ctx->upds, ctx->upd_max * sizeof(tb_tst_lv1_upd));
	nh_fre(ctx->bid_arr, ctx->unt_nbr * sizeof(u64));
	nh_fre(ctx->ask_arr, ctx->unt_nbr * sizeof(u64));
	nh_fre(ctx->bid_ini, ctx->unt_nbr * sizeof(u64));
	nh_fre(ctx->ask_ini, ctx->unt_nbr * sizeof(u64));
	nh_fre(ctx->ref_arr, ctx->unt_nbr * sizeof(u64));
	const u64 hmp_nbr = ctx->unt_nbr * ctx->tck_nbr; 
	nh_fre(ctx->hmp, hmp_nbr * sizeof(f64));
	nh_fre(ctx->hmp_ini, hmp_nbr * sizeof(f64));
}

/*
 * Export tick data.
 */
static inline void _tck_exp(
	tb_tst_lv1_ctx *ctx
)
{
	const u64 siz_x = ctx->unt_nbr;
	const u64 siz_y = ctx->tck_nbr;
	const u64 elm_nb = siz_x * siz_y;
	debug("exporting heatmap of size %Ux%U (volume ref %d) at /tmp/hmp.\n", siz_x, siz_y, ctx->gen_vol);
	nh_res res;
	ns_stg *stg = nh_stg_opn(NH_FIL_ATT_RWC, &res, "/tmp/hmp");	
	ns_stg_rsz(stg, elm_nb * sizeof(f64)); 
	f64 *dst = ns_stg_map(stg, 0, 0, elm_nb * sizeof(f64), NS_STG_ATT_RWS); 
	ns_mem_cpy(dst, ctx->hmp, elm_nb * sizeof(f64));
	ns_stg_syn(stg, dst, elm_nb * sizeof(f64));
	ns_stg_ump(stg, dst, elm_nb * sizeof(f64));
	nh_stg_cls(&res);
}

/*
 * Shortcut to incorporate into best bid / ask.
 */
static inline void _bac_add(
	tb_tst_lv1_ctx *ctx,
	u64 tck,
	f64 vol,
	u64 tim,
	u64 *bst_bac_aidp,
	u64 *bst_bidp,
	u64 *bst_askp
)
{
	const u64 aid_bac = (tim - ctx->tim_stt) / ctx->aid_wid;
	u64 bst_bac_aid = *bst_bac_aidp;
	assert(bst_bac_aid <= aid_bac);
	if (aid_bac != bst_bac_aid) {
		*bst_bac_aidp = bst_bac_aid = aid_bac;
		*bst_bidp = ctx->bid_ini[bst_bac_aid];
		*bst_askp = ctx->ask_ini[bst_bac_aid];
	}
	if ((vol < 0) && (tck > *bst_bidp)) {
		*bst_bidp = tck;
	} else if ((vol > 0) && (tck < *bst_askp)) {
		*bst_askp = tck;
	}
}

static inline void _upds_add_ini(
	tb_tst_lv1_ctx *ctx,
	tb_lv1_hst *hst,
	u64 *tims,
	f64 *prcs,
	f64 *vols,
	u64 tim_add,
	u64 *add_idxp,
	u64 *bst_bac_aidp,
	u64 *bst_bidp,
	u64 *bst_askp
)
{
	debug("Adding initial orders.\n");
	tb_tst_lv1_upd *upds = ctx->upds;
	u64 add_idx = *add_idxp;
	u64 i = 0;
	u64 tim;
	while (
		(add_idx < ctx->upd_nbr) &&
		((tim = upds[add_idx].tim) <= tim_add)
	) {

		/* Add. */
		tims[i] = tim;
		const f64 prc = prcs[i] = upds[add_idx].val;
		const f64 vol = vols[i] = upds[add_idx].vol;
		const u64 tck = _prc_to_tck(ctx, prc); 

		/* Incorporate in best bid / ask. */
		_bac_add(ctx, tck, vol, tim, bst_bac_aidp, bst_bidp, bst_askp);

		/* Update counters. */
		add_idx++;
		i++;

		/* If buffer full, flush. */
		if (i == 2 * ctx->tck_nbr) {
			tb_lv1_add(hst, i, tims, prcs, vols);
			i = 0;
		}

	}

	/* Add all non-flushed updates. */
	if (i) {
		tb_lv1_add(hst, i, tims, prcs, vols);
	}

	*add_idxp = add_idx;

}


#define UPD_GEN_STPS_NB 24
static u64 _upds_gen_stps[UPD_GEN_STPS_NB] = {
	1, 2, 1, 5, 1, 1,
	2, 1, 10, 1, 3, 2,
	5, 1, 43, 1, 1, 2,
	1, 10, 1, 1, 2, 127
};

/*
 * Generate a random number of updates.
 */
static inline u64 _upds_gen(
	tb_tst_lv1_ctx *ctx,
	tb_lv1_hst *hst,
	u64 itr_idx,
	u64 *tims,
	f64 *prcs,
	f64 *vols,
	u64 *tim_addp,
	u64 *add_idxp,
	u64 *bst_bac_aidp,
	u64 *bst_bidp,
	u64 *bst_askp
)
{
	u64 add_idx = *add_idxp;
	u64 tim_add = *tim_addp;

	/* Add the required number of updates, keep track of the bac time. */
	const u64 _add_nb = _upds_gen_stps[itr_idx % UPD_GEN_STPS_NB];
	const u64 add_nb = (_add_nb < ctx->tck_nbr) ? _add_nb : ctx->tck_nbr;
	assert(add_idx + 1 < ctx->upd_nbr);
	assert(tim_add < ctx->upds[add_idx + 1].tim); 
	u64 gen_nbr = 0;
	for (; (gen_nbr < add_nb) && (add_idx < ctx->upd_nbr); (gen_nbr++), (add_idx++)) {

		/* Add. */
		const f64 prc = prcs[gen_nbr] = ctx->upds[add_idx].val;
		const f64 vol = vols[gen_nbr] = ctx->upds[add_idx].vol;
		const u64 tim = tims[gen_nbr] = ctx->upds[add_idx].tim;
		const u64 tck = _prc_to_tck(ctx, prc); 
		assert(tim_add <= tim);
		tim_add = tim;

		/* Incorporate in best bid / ask. */
		_bac_add(ctx, tck, vol, tim, bst_bac_aidp, bst_bidp, bst_askp);
	}

	/* Update. */
	*add_idxp = add_idx;
	*tim_addp = tim_add;

	/* Return the number of generated updates. */
	return gen_nbr;

}

/*
 * Generate updates at the current tick.
 */
static inline u64 _upds_gen_crt(
	tb_tst_lv1_ctx *ctx,
	tb_lv1_hst *hst,
	u64 *tims,
	f64 *prcs,
	f64 *vols,
	u64 tim_add,
	u64 *add_idxp,
	u64 *bst_bac_aidp,
	u64 *bst_bidp,
	u64 *bst_askp
)
{
	u64 add_idx = *add_idxp; 

	/* Also add all successor updates at the same time. */
	u64 gen_nbr = 0;
	while (add_idx < ctx->upd_nbr) {
		const u64 tim = ctx->upds[add_idx].tim; 
		assert(tim_add <= tim);
		if (tim_add != tim) break;

		/* Add. */
		tims[gen_nbr] = tim;
		const f64 prc = prcs[gen_nbr] = ctx->upds[add_idx].val;
		const f64 vol = vols[gen_nbr] = ctx->upds[add_idx].vol;
		const u64 tck = _prc_to_tck(ctx, prc); 

		/* Incorporate in best bid / ask. */
		_bac_add(ctx, tck, vol, tim, bst_bac_aidp, bst_bidp, bst_askp);

		add_idx++;
		gen_nbr++;
	}

	*add_idxp = add_idx;
	return gen_nbr;

}

static inline void _upd_inc(
	tb_tst_lv1_ctx *ctx,
	tb_lv1_hst *hst,
	u64* cur_idxp,
	u64 tim_cur,
	u64 *chc_tims,
	f64 *chc_sums,
	f64 *chc_vols,
	u64 *chc_aidp
)
{
	u64 cur_idx = *cur_idxp;

	while (1) {

		/* Check. */
		const u64 tim = ctx->upds[cur_idx].tim; 
		if (tim >= tim_cur) break;

		/* Read. */
		const f64 prc = ctx->upds[cur_idx].val; 
		const f64 vol = ctx->upds[cur_idx].vol; 
		const u64 tck = _prc_to_tck(ctx, prc); 

		/* Incorporate. */
		const u64 aid = _tim_to_aid(ctx, tim);
		assert(aid >= *chc_aidp);
		if (aid != *chc_aidp) {
			*chc_aidp = aid;
			u64 aid_tim = aid * ctx->aid_wid + ctx->tim_stt;
			for (u32 rst_idx = 0; rst_idx < ctx->tck_nbr; rst_idx++) {
				chc_tims[rst_idx] = aid_tim;
				chc_sums[rst_idx] = 0;
			}
		}
		assert(tim >= chc_tims[tck]);
		if (tim != chc_tims[tck]) {
			chc_sums[tck] += (f64) (tim - chc_tims[tck]) * vol;
		}
		chc_vols[tck] = vol;
		chc_tims[tck] = tim;

		/* Increment. */
		cur_idx++;
	}

	*cur_idxp = cur_idx;
}

/*
 * Verify the result produced by @hst.
 */
static inline void _vrf(
	tb_tst_lv1_ctx *ctx,
	tb_lv1_hst *hst,
	u64 tim_cur,
	u64 bst_bid,
	u64 bst_ask,
	u64 *chc_tims,
	f64 *chc_sums,
	f64 *chc_vols
)
{

#define SKP_CHK

#ifndef SKP_CHK

		/* Verify the heatmap reference. */
		const u64 ref = hst->tck_ref;
		const u64 aid_cur = (tim_cur + 1 - ctx->tim_stt) / ctx->aid_wid;
		assert(ref == ctx->ref_arr[aid_cur], "reference mismatch at time %U cell %U : expected %U, got %U.\n",
			tim_cur + 1, aid_cur, ctx->ref_arr[aid_cur], ref); 

		
//#error following computations seem to assume that tim_cur is the preparation time.
		//	It is not, tim_cur + 1 is the preparation time.

		/* Determine the heatmap anchor point. */
		assert(hst->hmp_tck_max == hst->hmp_tck_min + ctx->hmp_dim_tck);
		const u64 hmp_min = hst->hmp_tck_min;
		assert(hmp_min >= ctx->tck_min);
		const u64 src_off = hmp_min - ctx->tck_min;

		/* Verify all heatmap columns except the current
		 * against the reference. */
		assert(!(ctx->tim_stt % ctx->aid_wid));
		assert(tim_cur >= ctx->tim_stt);
		const u64 aid_bac = (tim_cur + ctx->aid_wid - ctx->tim_stt) / ctx->aid_wid;
		const s64 aid_hmp = (s64) aid_bac - (s64) ctx->hmp_dim_tim;  
		const f64 *hmp = tb_lv1_hmp(hst);
		for (u64 cnt = 0; cnt < ctx->hmp_dim_tim - 1; cnt++) {
			const s64 col_idx = aid_hmp + (s64) cnt; 

			/* If column is before start point, it must be equal
			 * to the history's reset prices.
			 * If not, it must be equal to the actual history prices. */
			const u64 src_idx = (col_idx < 0) ? 0 : (u64) col_idx;
			const f64 *src = (col_idx < 0) ? ctx->hmp_ini : ctx->hmp;
			const f64 *src_cmp = src + src_idx * ctx->tck_nbr + src_off;
			const f64 *hmp_cmp = hmp + cnt * ctx->hmp_dim_tck;

			/* Compare. */
			for (u64 row_idx = 0; row_idx < ctx->hmp_dim_tck; row_idx++) {
				assert(hmp_cmp[row_idx] == src_cmp[row_idx],
					"heatmap mismatch at column %U/%U row %U/%U (tick %U/%U) : expected %d got %d.\n",
					col_idx, ctx->hmp_dim_tim - 1, row_idx, ctx->hmp_dim_tck, row_idx + src_off, ctx->tck_nbr,  
					src_cmp[row_idx], hmp_cmp[row_idx]
				);
			}

		}

		/* Verify the heatmap current column agains the
		 * currently updated column. */
		const f64 *hmp_cmp = hmp + (ctx->hmp_dim_tim - 1) * ctx->hmp_dim_tck;
		const u64 div = (tim_cur - ctx->tim_stt) % ctx->aid_wid;
		for (u64 row_idx = 0; row_idx < ctx->hmp_dim_tck; row_idx++) {
			assert(tim_cur >= chc_tims[row_idx]);
			const f64 avg = (div) ? 
				(chc_sums[row_idx] + (f64) (tim_cur - chc_tims[row_idx]) * chc_vols[row_idx]) / (f64) div :
				chc_vols[row_idx];

			assert(hmp_cmp[row_idx] == avg,
				"heatmap mismatch at current column row %U/%U (tick %U/%U) : expected %d got %d.\n",
				ctx->hmp_dim_tim - 1, ctx->hmp_dim_tim - 1, row_idx, ctx->hmp_dim_tck,  
				avg, hmp_cmp[row_idx]
			);
		}

		/* Verify all values of the bid-ask curves except the current one. */
		const u64 *bid = tb_lv1_bid(hst);
		const u64 *ask = tb_lv1_ask(hst);
		for (u64 chk_idx = 0; chk_idx < ctx->bac_siz - 1; chk_idx++) {
			assert(bid[chk_idx] == ctx->bid_arr[aid_bac + chk_idx],
				"bid curve mismatch at index %U/%U : expected %d got %d.\n",
				chk_idx, ctx->bac_siz - 1, ctx->bid_arr[aid_bac + chk_idx], bid[chk_idx]
			);
			assert(ask[chk_idx] == ctx->ask_arr[aid_bac + chk_idx],
				"ask curve mismatch at index %U/%U : expected %d got %d.\n",
				chk_idx, ctx->bac_siz - 1, ctx->ask_arr[aid_bac + chk_idx], ask[chk_idx]
			);
		}

		/* Verify the current best bid and ask. */
		assert(bid[ctx->bac_siz - 1] == bst_bid,
			"bid curve mismatch at index %U/%U : expected %d got %d.\n",
			ctx->bac_siz - 1, ctx->bac_siz - 1, bst_bid, bid[ctx->bac_siz - 1]
		);
		assert(ask[ctx->bac_siz - 1] == bst_ask,
			"ask curve mismatch at index %U/%U : expected %d got %d.\n",
			ctx->bac_siz - 1, ctx->bac_siz - 1, bst_ask, ask[ctx->bac_siz - 1]
		);
#endif

}

/*
 * Entrypoint for lv1 tests.
 */
void tb_tst_lv1(
	nh_tst_sys *sys,
	u64 sed,
	u8 wrk_nb,
	u8 run_prc
)
{

	/* Generation parameters. */
	#define TIM_INC 1000000
	#define TCK_PER_UNT 100
	#define UNT_NBR 1000
	#define TCK_NBR 37
	#define PRC_MIN 10000
	#define HMP_DIM_TCK 100
	#define HMP_DIM_TIM 100
	#define BAC_SIZ 200
	#define TIM_STP 10
	const u64 aid_wid = TIM_INC * TIM_STP;
	const f64 prc_stp = (f64) 1 / (f64) TCK_PER_UNT;
	const u64 tim_stt = ns_hsh_u32_rng(sed, (HMP_DIM_TIM + 1) * TIM_INC * TIM_STP, (u32) -1, TIM_INC * TIM_STP);
	const u64 tck_min = PRC_MIN * HMP_DIM_TCK;

	/* Generation settings. */ 
	nh_all__(tb_tst_lv1_ctx, ctx);
	ctx->sed = sed;
	ctx->unt_nbr = UNT_NBR;
	ctx->tim_stt = tim_stt;
	assert(!(ctx->tim_stt % 100));
	ctx->prc_min = PRC_MIN;
	ctx->tck_nbr = TCK_NBR;
	ctx->tim_inc = TIM_INC;
	ctx->tim_stp = TIM_STP;
	ctx->aid_wid = ctx->tim_inc * ctx->tim_stp;
	ctx->tck_rat = TCK_PER_UNT;
	ctx->hmp_dim_tck = HMP_DIM_TCK;
	ctx->hmp_dim_tim = HMP_DIM_TIM;
	ctx->bac_siz = BAC_SIZ;
	ctx->gen_vol = 10000;
	ctx->skp_prd = 43;
	ctx->obk_rst_prd = 100;
	ctx->obk_vol_rst_prd = 43;
	ctx->obk_vol_rst_prd = 47;
	ctx->obk_bid_exc_prd = 8;
	ctx->obk_ask_exc_prd = 9;
	ctx->obk_bid_rst_prd = 23;
	ctx->obk_ask_rst_prd = 27;

	/* Generate tick data. */
	debug("Generating updates.\n");
	_tck_gen(ctx);
	assert(tck_min == ctx->tck_min);

	/* Read post generation parameters. */
	const u64 tim_end = ctx->tim_end;

	/* Allocate buffers to contain two entire orderbooks,
	 * which prevents us from overflowing when adding
	 * updates as the current time (not more than TCK_NBR). */
	u64 *tims = nh_all(2 * TCK_NBR * sizeof(u64));
	f64 *prcs = nh_all(2 * TCK_NBR * sizeof(f64));
	f64 *vols = nh_all(2 * TCK_NBR * sizeof(f64));

	/* Export tick data. */
	//_tck_exp(ctx);

	/*
	 * Create a reconstructor covering a 
	 * 100 * 100 heatmap window
	 * and a 200 ticks bid-ask prediction.
	 */
	tb_lv1_hst *hst = tb_lv1_ctr(
		aid_wid,
		TCK_PER_UNT,
		HMP_DIM_TCK,
		HMP_DIM_TIM,
		BAC_SIZ
	);

	/* Set initial volumes in groups of 19 by step of 19. */
	debug("Adding initial volumes.\n");
	assert(TCK_NBR % 19);
	assert(19 % TCK_NBR);
	u64 ttl_non_nul = 0;
	for (u32 i = 0; i < TCK_NBR;) {
		u64 nb_non_nul = 0;
		for (u32 j = 0; (j < 19) && (i < TCK_NBR); i++, j++) {
			u32 ri = (19 * i) % TCK_NBR;
			const f64 vol = ctx->hmp_ini[ri];
			if (!vol) continue;
			prcs[nb_non_nul] = TCK_TO_PRC(ri, PRC_MIN, prc_stp); 
			assert(prcs[nb_non_nul]);
			vols[nb_non_nul] = ctx->hmp_ini[ri];
			nb_non_nul++;
		}
		if (nb_non_nul) tb_lv1_add(hst, nb_non_nul, 0, prcs, vols);
		ttl_non_nul += nb_non_nul;
	}
	assert(ttl_non_nul, "init with no volume.\n");

	/* Determine the initial times. */
	assert(tim_stt > HMP_DIM_TIM * aid_wid);
	u64 tim_cur = tim_stt;
	u64 tim_cln = tim_stt - HMP_DIM_TIM * aid_wid;
	u64 tim_add = tim_stt + BAC_SIZ * aid_wid;

	/* Index of orders to be inserted in the heatmap and bac region. */
	u64 cur_idx = 0;
	u64 add_idx = 0;
	
	/*
	 * The way we generate orderbook updates is fundamentally
	 * prone to causing incoherence in the bid-ask spread.
	 * Since we generate a new orderbook, then traverse ticks
	 * in a randomized way to create updates, then it is possible
	 * that if we only apply half of those updates, we end up
	 * with alternating bid and asks.
	 * We do this anyway because it adds randomness, and because
	 * since all those updates have the same time, they must
	 * be added altogether before processing, which will
	 * allow the processing stage to see all of them, and hence
	 * to see a coherent bid/ask spread.
	 * One nuance is that best bids and best asks are done in
	 * a step by step manner, which causes the intermediate
	 * states to be incoherent.
	 * For this reason, TODO mute debug messages about
	 * alternating bid and ask levels during testing.
	 */

	/*
	 * The heatmap of @ctx is only relevant for columns
	 * with all updates accounted for, which in practice
	 * comprises all columns except the current one.
	 * To verify @hst's result for the current column,
	 * we need arrays giving the current volume for each
	 * tick, the weighted sum at last update, and the
	 * time of last update.
	 */
	u64 *chc_tims = nh_all(TCK_NBR * sizeof(u64));
	f64 *chc_sums = nh_all(TCK_NBR * sizeof(f64));
	f64 *chc_vols = nh_all(TCK_NBR * sizeof(f64));
	u64 chc_aid = 0;

	/* Current best bid / asks. */
	u64 bst_bac_aid = 0;
	u64 bst_bid = ctx->bid_ini[0];
	u64 bst_ask = ctx->ask_ini[0];

	/* Prepare for insertion until bac end. */
	tb_lv1_prp(hst, tim_cur + 1);

	_upds_add_ini(ctx, hst, tims, prcs, vols, tim_add, &add_idx, &bst_bac_aid, &bst_bid, &bst_ask);

	/* Generate and compare the heatmap and bac. */
	u64 itr_idx = 0;
	debug("Adding updates.\n");
	while (tim_add < tim_end) {

		debug("Iteration %U\n", itr_idx);
		
		/* Generate updates to add. */
		u64 gen_nbr = _upds_gen(
			ctx, hst,
			itr_idx,
			tims, prcs, vols,
			&tim_add, &add_idx,
			&bst_bac_aid, &bst_bid, &bst_ask
		);

		/* Update the current time and clean time. */
		assert(tim_cur < tim_add - BAC_SIZ * aid_wid); 
		tim_cur = tim_add - BAC_SIZ * aid_wid;
		assert(tim_cln < tim_cur - HMP_DIM_TIM * aid_wid);
		tim_cln = tim_cur - HMP_DIM_TIM * aid_wid;
		
		/* Prepare to add. */
		tb_lv1_prp(hst, tim_cur + 1);

		/* Add all updates. */
		tb_lv1_add(hst, gen_nbr, tims, prcs, vols);

		/* Generate updates to add. */
		gen_nbr = _upds_gen_crt(
			ctx, hst,
			tims, prcs, vols,
			tim_add, &add_idx,
			&bst_bac_aid, &bst_bid, &bst_ask
		);

		/* Add extra updates. */
		if (gen_nbr) {
			tb_lv1_add(hst, gen_nbr, tims, prcs, vols);
		}

		/* Process. */
		tb_lv1_prc(hst);


		/* Incorporate all orders < current time in the
		 * heatmap data. */
		_upd_inc(ctx, hst, &cur_idx, tim_cur, chc_tims, chc_sums, chc_vols, &chc_aid);

		/* Verify. */
		_vrf(ctx, hst, tim_cur, bst_bid, bst_ask, chc_tims, chc_sums, chc_vols);

		/* Cleanup one time out of 10. */
		if (!itr_idx % 20) {
			tb_lv1_cln(hst);
		}

		itr_idx++;
	}	


	/* Delete the history. */
	tb_lv1_dtr(hst);

	/* Free buffers. */ 
	nh_fre(tims, 2 * TCK_NBR * sizeof(u64));
	nh_fre(prcs, 2 * TCK_NBR * sizeof(f64));
	nh_fre(vols, 2 * TCK_NBR * sizeof(f64));
	nh_fre(chc_tims, TCK_NBR * sizeof(u64));
	nh_fre(chc_sums, TCK_NBR * sizeof(f64));
	nh_fre(chc_vols, TCK_NBR * sizeof(f64));

	/* Delete tick data. */
	_tck_del(ctx);

	/* Free settings. */ 
	nh_fre_(ctx);

}
