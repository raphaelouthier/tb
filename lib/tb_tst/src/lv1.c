/* Copyright 2025 Raphael Outhier - confidential - proprietary - no copy - no diffusion. */

#include <tb_tst/tb_tst.all.h>

/*********
 * Utils *
 *********/

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

/*
 * Price -> tick.
 */
static inline u64 _prc_to_tck(
	tb_tst_lv1_ctx *ctx,
	f64 prc
)
{
	assert(prc >= ctx->prc_min);
	return (u64) (f64) ((prc - ctx->prc_min) * ((f64) ctx->tck_rat + 0.1));
}

/*
 * Tick -> price.
 */
static inline f64 _tck_to_prc(
	tb_tst_lv1_ctx *ctx,
	u64 tck
) {return ctx->prc_min + ((f64) tck / (f64) ctx->tck_rat);}

/**********
 * Export *
 **********/

/*
 * Export tick data.
 */
static inline void _hmp_exp(
	tb_tst_lv1_ctx *ctx
)
{
	const u64 siz_x = ctx->unt_nbr;
	const u64 siz_y = ctx->tck_nbr;
	const u64 elm_nb = siz_x * siz_y;
	debug("exporting heatmap of size %Ux%U (volume ref %d) at /tmp/hmp.\n", siz_x, siz_y, ctx->ref_vol);
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
	u64 *uid_addp,
	u64 *bst_bac_aidp,
	u64 *bst_bidp,
	u64 *bst_askp
)
{
	debug("Adding initial orders.\n");
	tb_tst_lv1_upd *upds = ctx->upds;
	u64 uid_add = *uid_addp;
	u64 i = 0;
	u64 tim;
	while (
		(uid_add < ctx->upd_nbr) &&
		((tim = upds[uid_add].tim) <= tim_add)
	) {

		/* Add. */
		tims[i] = tim;
		const f64 prc = prcs[i] = upds[uid_add].prc;
		const f64 vol = vols[i] = upds[uid_add].vol;
		const u64 tck = _prc_to_tck(ctx, prc); 

		/* Incorporate in best bid / ask. */
		_bac_add(ctx, tck, vol, tim, bst_bac_aidp, bst_bidp, bst_askp);

		/* Update counters. */
		uid_add++;
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

	*uid_addp = uid_add;

}


#define UPD_GEN_STPS_NB 24
static u64 _upds_add_stps[UPD_GEN_STPS_NB] = {
	1, 2, 1, 5, 1, 1,
	2, 1, 10, 1, 3, 2,
	5, 1, 43, 1, 1, 2,
	1, 10, 1, 1, 2, 127
};

/*
 * Generate a random number of updates.
 */
static inline u64 _upds_add(
	tb_tst_lv1_ctx *ctx,
	tb_lv1_hst *hst,
	u64 itr_idx,
	u64 *tims,
	f64 *prcs,
	f64 *vols,
	u64 *tim_addp,
	u64 *uid_addp,
	u64 *bst_bac_aidp,
	u64 *bst_bidp,
	u64 *bst_askp
)
{
	u64 uid_add = *uid_addp;
	u64 tim_add = *tim_addp;

	/* Add the required number of updates, keep track of the bac time. */
	const u64 _add_nb = _upds_add_stps[itr_idx % UPD_GEN_STPS_NB];
	const u64 add_nb = (_add_nb < ctx->tck_nbr) ? _add_nb : ctx->tck_nbr;
	assert(uid_add + 1 < ctx->upd_nbr);
	assert(tim_add < ctx->upds[uid_add + 1].tim); 
	u64 gen_nbr = 0;
	for (; (gen_nbr < add_nb) && (uid_add < ctx->upd_nbr); (gen_nbr++), (uid_add++)) {

		/* Add. */
		const f64 prc = prcs[gen_nbr] = ctx->upds[uid_add].prc;
		const f64 vol = vols[gen_nbr] = ctx->upds[uid_add].vol;
		const u64 tim = tims[gen_nbr] = ctx->upds[uid_add].tim;
		const u64 tck = _prc_to_tck(ctx, prc); 
		assert(tim_add <= tim);
		tim_add = tim;

		/* Incorporate in best bid / ask. */
		_bac_add(ctx, tck, vol, tim, bst_bac_aidp, bst_bidp, bst_askp);
	}

	/* Update. */
	*uid_addp = uid_add;
	*tim_addp = tim_add;

	/* Return the number of generated updates. */
	return gen_nbr;

}

/*
 * Add updates at @tim_add.
 */
static inline u64 _upds_add_agn(
	tb_tst_lv1_ctx *ctx,
	tb_lv1_hst *hst,
	u64 *tims,
	f64 *prcs,
	f64 *vols,
	u64 tim_add,
	u64 *uid_addp,
	u64 *bst_bac_aidp,
	u64 *bst_bidp,
	u64 *bst_askp
)
{
	u64 uid_add = *uid_addp; 

	/* Also add all successor updates at the same time. */
	u64 gen_nbr = 0;
	while (uid_add < ctx->upd_nbr) {
		const u64 tim = ctx->upds[uid_add].tim; 
		assert(tim_add <= tim);
		if (tim_add != tim) break;

		/* Add. */
		tims[gen_nbr] = tim;
		const f64 prc = prcs[gen_nbr] = ctx->upds[uid_add].prc;
		const f64 vol = vols[gen_nbr] = ctx->upds[uid_add].vol;
		const u64 tck = _prc_to_tck(ctx, prc); 

		/* Incorporate in best bid / ask. */
		_bac_add(ctx, tck, vol, tim, bst_bac_aidp, bst_bidp, bst_askp);

		uid_add++;
		gen_nbr++;
	}

	*uid_addp = uid_add;
	return gen_nbr;

}

/*
 * Incorporate all updates until @tim_cur starting
 * at *@uid_curp.
 * Update @uid_curp.
 */
static inline void _upds_mov(
	tb_tst_lv1_ctx *ctx,
	tb_lv1_hst *hst,
	u64* uid_curp,
	u64 tim_cur,
	u64 *chc_tims,
	f64 *chc_sums,
	f64 *chc_vols,
	u64 *chc_aidp
)
{
	u64 uid_cur = *uid_curp;

	while (1) {

		/* Check. */
		const u64 tim = ctx->upds[uid_cur].tim; 
		if (tim >= tim_cur) break;

		/* Read. */
		const f64 prc = ctx->upds[uid_cur].prc; 
		const f64 vol = ctx->upds[uid_cur].vol; 
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
		uid_cur++;
	}

	*uid_curp = uid_cur;
}

/*
 * Update the clean index.
 */
static inline u64 _uid_cln_upd(
	tb_tst_lv1_ctx *ctx,
	u64 uid_cln,
	u64 tim_cln
)
{
	while (uid_cln < ctx->upd_nbr) {
		const u64 tim = ctx->upds[uid_cln].tim; 
		assert(tim_cln <= tim);
		if (tim_cln != tim) break;
		uid_cln++;
	}
	return uid_cln;
}

/*
 * Log all updates in @hst's update queue.
 */
static void _log_hst_upds(
	tb_lv1_hst *hst
)
{
	debug("Updates of hst %p.\n", hst);
	tb_lv1_upd *upd;
	ns_slsh_fe(upd, &hst->upds_hst, upds_hst) {
		debug("  upd %p : tim : %U, tck : %U, vol : %d.\n", upd, upd->tim, upd->tck->tcks.val, upd->vol); 
	}
}

/*
 * Log all updates in @hst's update queue at the tick
 * at value @tck_val.
 */
static void _log_hst_upds_at(
	tb_lv1_hst *hst,
	u64 tck_val
)
{
	debug("Updates of hst %p at %U.\n", hst, tck_val);
	tb_lv1_tck *tck = 0;
	tb_lv1_upd *upd;
	ns_slsh_fe(upd, &hst->upds_hst, upds_hst) {
		if (upd->tck->tcks.val == tck_val) {
			if (!tck) tck = upd->tck;
			assert(upd->tck == tck, "multiple ticks at the same value.\n");
			debug("  upd %p : tim : %U, tck : %U, vol : %d.\n", upd, upd->tim, upd->tck->tcks.val, upd->vol); 
		}
	}
}

/*
 * Log @hst's tick at @tck_val.
 */
static void _log_hst_tck_lst(
	tb_lv1_hst *hst,
	u64 tck_val
)
{
	debug("Tick of hst %p at %U : ", hst, tck_val);
	tb_lv1_tck *tck = ns_map_sch(&hst->tcks, tck_val, u64, tb_lv1_tck, tcks);
	if (!tck) {
		debug("not found.\n");
		return;
	}
	debug("vol_stt : %d, vol_cur : %d, vol_max %d, tim_max %U.\n",
		tck->vol_stt, tck->vol_cur, tck->vol_max, tck->tim_max);
	tb_lv1_upd *upd;
	ns_dls_fe(upd, &tck->upds_tck, upds_tck) {
		debug("  upd %p : tim : %U, tck : %U, vol : %d.\n", upd, upd->tim, upd->tck->tcks.val, upd->vol); 
	}

}

/*
 * Log all updates in @ctx's update list in the
 * [@uid_stt, @uid_end[ range.
 */
static void _upds_log(
	tb_tst_lv1_ctx *ctx,
	u64 uid_stt,
	u64 uid_end
)
{
	debug("Tick of ctx %p in [%U, %U[ : ", ctx, uid_stt, uid_end);
	while (uid_stt < uid_end) {
		debug("  upd %U : tim : %U, tck : %U, vol : %d, prc : %d.\n", uid_stt, ctx->upds[uid_stt].tim, _prc_to_tck(ctx, ctx->upds[uid_stt].prc), ctx->upds[uid_stt].vol, ctx->upds[uid_stt].prc); 
		uid_stt++;
	}
}

/*
 * Log all updates in @ctx's update list in the
 * [@uid_stt, @uid_end[ range at @tck_val.
 */
static void _upds_log_at(
	tb_tst_lv1_ctx *ctx,
	u64 uid_stt,
	u64 uid_end,
	u64 tck_val
)
{
	debug("Tick of ctx %p in [%U, %U[ : ", ctx, uid_stt, uid_end);
	while (uid_stt < uid_end) {
		const f64 prc = ctx->upds[uid_stt].prc;
		const u64 tck = _prc_to_tck(ctx, prc);
		debug("  upd %U : tim : %U, tck : %U, vol : %d, prc : %d.\n", uid_stt, ctx->upds[uid_stt].tim, tck, ctx->upds[uid_stt].vol, prc); 
		uid_stt++;
	}
}

/*
 * Verify @hst internal state.
 */
static inline void _vrf_hst(
	tb_tst_lv1_ctx *ctx,
	tb_lv1_hst *hst,
	u64 uid_cln,
	u64 uid_cur,
	u64 uid_add,
	u64 tim_cln,
	u64 tim_cur,
	u64 tim_add,
	u8 did_cln
)
{

	/* First, check our own data. */
	const u64 upd_nbr = ctx->upd_nbr;
	assert(uid_cln <= upd_nbr);
	assert(uid_cur <= upd_nbr);
	assert(uid_add <= upd_nbr);
	assert((!did_cln) || (uid_cln == upd_nbr) || (ctx->upds[uid_cln].tim == tim_cln));
	assert((uid_cur == upd_nbr) || (ctx->upds[uid_cur].tim == tim_cur));
	assert((uid_add == upd_nbr) || (ctx->upds[uid_add].tim == tim_add));
	assert((!did_cln) || (uid_cln + 1 >= upd_nbr) || (ctx->upds[uid_cln + 1].tim > tim_cln)); 
	assert((uid_cur + 1 >= upd_nbr) || (ctx->upds[uid_cur + 1].tim > tim_cur)); 
	assert((uid_add + 1 >= upd_nbr) || (ctx->upds[uid_add + 1].tim > tim_add)); 

	/*
	 * First, check the full update sequence and verify that :
	 * - every update is registered.
	 * - every update is associated with the correct tick.
	 * - every update is or is not in its tick list as expected.
	 */
	tb_lv1_upd *upd;
	u64 idx = 0;
	u64 max = uid_cln - uid_add;
	ns_slsh_fe(upd, &hst->upds_hst, upds_hst) {
		tb_tst_lv1_upd *src = ctx->upds + uid_cln + idx;

		/* Check the index. */
		assert(idx < max, "update sequence mismatch : too many updates.\n");

		/* Check data. */
		const f64 prc = src->prc;
		const u64 tck = _prc_to_tck(ctx, prc);
		assert(
			(upd->tck->tcks.val == tck) &&
			(upd->tim == src->tim) &&
			(upd->vol == src->vol),
			"update sequence mismatch at index %U/%U (uid %U/%U) : expected (tim : %U, tck : %U (prc : %d), vol : %d), got (tim : %U, tck : %U (prc : %d), vol : %d).\n",
			idx, max, uid_cln + idx, uid_add,
			src->tim, tck, prc, src->vol,
			upd->tim, upd->tck->tcks.val, upd->vol
		);

		/* Check that the update's list state is correct. */
		if ((upd->tim < tim_cur) != (!ns_dls_empty(&upd->upds_tck))) {
			if (upd->tim < tim_cur) {
				assert(!ns_dls_empty(&upd->upds_tck), "update < current time not in tick list.");
			} else {
				assert(ns_dls_empty(&upd->upds_tck), "update >= current time in tick list.");
			}
			assert(0, "should have asserted.\n");
		}

		/* Reiterate. */
		idx++;
	}
	assert(idx == max, "not enough updates in history : expected %U, got %U.\n", idx, max);

	/* Now, clear the debug flag on each tick and count them. */
	tb_lv1_tck *tck;
	u64 tck_cnt = 0;
	ns_map_fe(tck, &hst->tcks, tcks, u64, in) {
		tck->dbg = 0;
		tck_cnt++;
	}
	const u64 tck_nbr = tck_cnt;

	/* Traverse the update list in reverse between add and current. */
	tck_cnt = tck_nbr;
	for (idx = uid_add - uid_cur; idx--;) {
		tb_tst_lv1_upd *src = ctx->upds + uid_cur + idx;
		const f64 prc = src->prc;
		const u64 tck_val = _prc_to_tck(ctx, prc);

		/* Find the tick for this update. If its flag is already set, nothing to do. */
		tck = assert(ns_map_sch(&hst->tcks, tck_val, u64, tb_lv1_tck, tcks));
		if (tck->dbg) continue;

		/* This is the first time we encounter this tick.
		 * Hence, its maximal time and volume should match
		 * @upd's. */ 
		assert(tck->vol_max == upd->vol);
		assert(tck->tim_max == upd->tim);
		tck->dbg = 1;

		/* Count a tick processed, if all have been, stop. */
		SAFE_DECR(tck_cnt);
		if (tck_cnt == 0) break;
		
	}
	
	/* Now, clear the debug flag on each tick.
	 * Every tick we did not encounter should have an equal
	 * current and max volume, and should have a max time < current time. */
	ns_map_fe(tck, &hst->tcks, tcks, u64, in) {
		if (tck->dbg == 0) {
			assert(tck->vol_max == tck->vol_cur);
			assert(tck->tim_max < tim_cur);
		} else {
			tck_cnt++;
		}
		tck->dbg = 0;
	}
	assert(tck_cnt == tck_nbr);

	/* Traverse the update list in reverse between current and clean. */
	tck_cnt = tck_nbr;
	for (idx = uid_add - uid_cur; idx--;) {
		tb_tst_lv1_upd *src = ctx->upds + uid_cur + idx;
		const f64 prc = src->prc;
		const u64 tck_val = _prc_to_tck(ctx, prc);

		/* Find the tick for this update. If its flag is already set, nothing to do. */
		tck = assert(ns_map_sch(&hst->tcks, tck_val, u64, tb_lv1_tck, tcks));
		if (tck->dbg) continue;

		/* This is the first time we encounter this tick.
		 * Hence, its current volume should match @upd's. */ 
		assert(tck->vol_cur == upd->vol);
		tck->dbg = 1;

		/* Count a tick processed, if all have been, stop. */
		SAFE_DECR(tck_cnt);
		if (tck_cnt == 0) break;
		
	}

	/* Now, clear the debug flag on each tick.
	 * Every tick we did not encounter should have an equal
	 * start and current volume. */
	ns_map_fe(tck, &hst->tcks, tcks, u64, in) {
		if (tck->dbg == 0) {
			assert(tck->vol_stt == tck->vol_cur);
		} else {
			tck_cnt++;
		}
		tck->dbg = 0;
	}
	assert(tck_cnt == tck_nbr);

	/* Traverse the update list in reverse before clean.
	 * For each tick, check that the start volume matches
	 * the first update we find. */
	tck_cnt = tck_nbr;
	for (u64 uid = uid_cln; uid--;) {
		tb_tst_lv1_upd *src = ctx->upds + uid;
		const f64 prc = src->prc;
		const u64 tck_val = _prc_to_tck(ctx, prc);

		/* Find the tick for this update. If its flag is already set, nothing to do. */
		tck = assert(ns_map_sch(&hst->tcks, tck_val, u64, tb_lv1_tck, tcks));
		if (tck->dbg) continue;

		/* This is the first time we encounter this tick.
		 * Hence, its current volume should match @upd's. */ 
		assert(tck->vol_stt == upd->vol);
		tck->dbg = 1;

		/* Count a tick processed, if all have been, stop. */
		SAFE_DECR(tck_cnt);
		if (tck_cnt == 0) break;
		
	}

	/* Now, clear the debug flag on each tick.
	 * Every tick we did not encounter should have a
	 * start price equal to the initial resting price. */
	const u64 tck_min = ctx->tck_min; 
	ns_map_fe(tck, &hst->tcks, tcks, u64, in) {
		if (tck->dbg == 0) {
			const u64 tck_val = tck->tcks.val;
			assert(tck_val >= tck_min);
			assert(tck->vol_stt == ctx->hmp_ini[tck_val - tck_min]);
		} else {
			tck_cnt++;
		}
		tck->dbg = 0;
	}
	assert(tck_cnt == tck_nbr);

}

/*
 * Verify the result produced by @hst.
 */
static inline void _vrf_res(
	tb_tst_lv1_ctx *ctx,
	tb_lv1_hst *hst,
	u64 uid_cln,
	u64 uid_cur,
	u64 uid_add,
	u64 tim_cln,
	u64 tim_cur,
	u64 tim_add,
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
static void _lv1_run(
	nh_tst_sys *sys,
	u64 sed,
	tb_tst_lv1_gen *gen
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
	ctx->ref_vol = 10000;

	/* Generate tick data. */
	debug("Generating updates.\n");
	tb_tst_lv1_upds_gen(ctx, gen);
	assert(tck_min == ctx->tck_min);

	/* Delete the generator. */
	(*(gen->dtr))(gen);

	/* Read post generation parameters. */
	const u64 tim_end = ctx->tim_end;

	/* Allocate buffers to contain two entire orderbooks,
	 * which prevents us from overflowing when adding
	 * updates as the current time (not more than TCK_NBR). */
	u64 *tims = nh_all(2 * TCK_NBR * sizeof(u64));
	f64 *prcs = nh_all(2 * TCK_NBR * sizeof(f64));
	f64 *vols = nh_all(2 * TCK_NBR * sizeof(f64));

	/* Export the global heatmap. */
	//_hmp_exp(ctx);

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
			prcs[nb_non_nul] = _tck_to_prc(ctx, ri);
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

	/* Index of next update to be moved to the heatmap. */
	u64 uid_cur = 0;

	/* Index of next update to be inserted in the history. */
	u64 uid_add = 0;

	/* Index of next update to be cleaned. */
	u64 uid_cln = 0;
	
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

	_upds_add_ini(ctx, hst, tims, prcs, vols, tim_add, &uid_add, &bst_bac_aid, &bst_bid, &bst_ask);

	/* Generate and compare the heatmap and bac. */
	u64 itr_idx = 0;
	debug("Adding updates.\n");
	while (tim_add < tim_end) {

		debug("Iteration %U\n", itr_idx);
		
		/* Generate updates to add. */
		u64 gen_nbr = _upds_add(
			ctx, hst,
			itr_idx,
			tims, prcs, vols,
			&tim_add, &uid_add,
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

		/* Add updates at the current tick. */
		gen_nbr = _upds_add_agn(
			ctx, hst,
			tims, prcs, vols,
			tim_add, &uid_add,
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
		_upds_mov(ctx, hst, &uid_cur, tim_cur, chc_tims, chc_sums, chc_vols, &chc_aid);

		/* Cleanup one time out of 10. */
		u8 do_cln = (!itr_idx % 20);
		if (do_cln) {

			/* Determine the clean index. */
			uid_cln = _uid_cln_upd(ctx, uid_cln, tim_cln);

			/* Clean. */
			tb_lv1_cln(hst);

		}

		/* Verify the history internal state. */
		_vrf_hst(
			ctx, hst,
			uid_cln, uid_cur, uid_add,
			tim_cln, tim_cur, tim_add,
			do_cln
		);

		/* Verify the result. */
		_vrf_res(
			ctx, hst,
			uid_cln, uid_cur, uid_add,
			tim_cln, tim_cur, tim_add,
			bst_bid, bst_ask,
			chc_tims, chc_sums, chc_vols);

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
	tb_tst_lv1_upds_del(ctx);

	/* Free settings. */ 
	nh_fre_(ctx);

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

	/* Construct a random orderbook generator. */
	tb_tst_lv1_gen_rdm *rdm = tb_tst_lv1_gen_rdm_ctr(
		43,
		100,
		47,
		8,
		9,
		23,
		27
	);

	_lv1_run(sys, sed, &rdm->gen);

}
