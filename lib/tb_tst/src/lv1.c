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
	assert(tck < ctx->tck_nbr);
	const u64 aid_bac = (tim - ctx->tim_stt) / ctx->aid_wid;
	u64 bst_bac_aid = *bst_bac_aidp;
	//debug("bac add tim %U aid_bac %U bst_bac_aid %U.\n", tim, aid_bac, bst_bac_aid);
	//debug("cur bid %U ask %U.\n", *bst_bidp, *bst_askp);
	assert(bst_bac_aid <= aid_bac);
	if (aid_bac != bst_bac_aid) {
		*bst_bac_aidp = bst_bac_aid = aid_bac;
		*bst_bidp = ctx->bid_ini[bst_bac_aid];
		*bst_askp = ctx->ask_ini[bst_bac_aid];
		debug("ini bid %U ask %U.\n", *bst_bidp, *bst_askp);
	}
	const u64 tck_abs = tck + ctx->tck_min;
	if ((vol < 0) && (tck_abs > *bst_bidp)) {
		*bst_bidp = tck_abs;
	} else if ((vol > 0) && (tck_abs < *bst_askp)) {
		*bst_askp = tck_abs;
	}
	//debug("new bid %U ask %U.\n", *bst_bidp, *bst_askp);
	check(*bst_askp >= ctx->tck_min);
}

/*
 * Update best bid / ask state to reflect the add time.
 */
static inline void _bac_prp(
	tb_tst_lv1_ctx *ctx,
	u64 tim,
	u64 *bst_bac_aidp,
	u64 *bst_bidp,
	u64 *bst_askp
)
{
	const u64 aid_bac = (tim - 1 - ctx->tim_stt) / ctx->aid_wid;
	u64 bst_bac_aid = *bst_bac_aidp;
	//debug("bac add tim %U aid_bac %U bst_bac_aid %U.\n", tim, aid_bac, bst_bac_aid);
	//debug("cur bid %U ask %U.\n", *bst_bidp, *bst_askp);
	assert(bst_bac_aid <= aid_bac);
	if (aid_bac != bst_bac_aid) {
		debug("ini %U %U %U.\n", ctx->unt_nbr, bst_bac_aid, aid_bac);
		*bst_bac_aidp = bst_bac_aid = aid_bac;
		*bst_bidp = ctx->bid_ini[bst_bac_aid];
		*bst_askp = ctx->ask_ini[bst_bac_aid];
		debug("ini bid %U ask %U.\n", *bst_bidp, *bst_askp);
	}
}

/*
 * Push all updates until @tim_add.
 */
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
	check(*bst_askp >= ctx->tck_min);
	debug("Adding initial orders.\n");
	tb_tst_lv1_upd *upds = ctx->upds;
	u64 uid_add = *uid_addp;
	u64 i = 0;
	u64 tim;
	while (
		(uid_add < ctx->upd_nbr) &&
		((tim = upds[uid_add].tim) < tim_add)
	) {

		/* Add. */
		tims[i] = tim;
		const f64 prc = prcs[i] = upds[uid_add].prc;
		const f64 vol = vols[i] = upds[uid_add].vol;
		const u64 tck = _prc_to_tck(ctx, prc); 

		/* Incorporate in best bid / ask. */
		//debug("add ini %U %U %d.\n", tim, tck, vol);
		_bac_add(ctx, tck, vol, tim, bst_bac_aidp, bst_bidp, bst_askp);

		//debug("%U %U.\n", *bst_bidp, *bst_askp);

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

	/* Update bid/ask data to reflect the add time. */
	_bac_prp(ctx, tim_add, bst_bac_aidp, bst_bidp, bst_askp);


	*uid_addp = uid_add;

}


#define UPD_GEN_STPS_NB 6
static u64 tim_add_inc_stps[UPD_GEN_STPS_NB] = {
	//1, 1, 1, 2, 2, 3,
	1, 1, 1, 1, 1, 1,
};

/*
 * Update @tim_add.
 */
static inline u64 _tim_add_inc(
	tb_tst_lv1_ctx *ctx,
	const u64 tim_add,
	u64 itr_idx
)
{
	const u64 add_nb = tim_add_inc_stps[itr_idx % UPD_GEN_STPS_NB];
	return tim_add + add_nb * ctx->tim_inc;
}

/*
 * Add updates at @tim_add.
 */
static inline void _upds_add(
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

	debug("Generating updates to add.\n");

	const u64 gen_max = 2 * ctx->tck_nbr;
	u64 gen_nbr = 0;
	while (uid_add < ctx->upd_nbr) {
		const u64 tim = ctx->upds[uid_add].tim; 
		//debug("%U %U.\n", tim_add, tim);
		if (tim_add <= tim) break;

		/* Add to the arrays. */
		tims[gen_nbr] = tim;
		const f64 prc = prcs[gen_nbr] = ctx->upds[uid_add].prc;
		const f64 vol = vols[gen_nbr] = ctx->upds[uid_add].vol;
		const u64 tck = _prc_to_tck(ctx, prc); 
		assert(_tck_to_prc(ctx, tck) == prc);

		/* Add to the history if needed. */ 
		assert(gen_nbr <= gen_max);
		if (gen_nbr == gen_max) {
			tb_lv1_add(hst, gen_nbr, tims, prcs, vols);
		}

		/* Incorporate in best bid / ask. */
		//debug("add %U %U %d.\n", tim, tck, vol);
		_bac_add(ctx, tck, vol, tim, bst_bac_aidp, bst_bidp, bst_askp);

		uid_add++;
		gen_nbr++;
	}

	/* Add remaining updates. */
	assert(gen_nbr <= gen_max);
	if (gen_nbr) {
		tb_lv1_add(hst, gen_nbr, tims, prcs, vols);
	}

	/* Update bid/ask data to reflect the add time. */
	_bac_prp(ctx, tim_add, bst_bac_aidp, bst_bidp, bst_askp);

	/* Report the new add UID. */
	*uid_addp = uid_add;

}

/*
 * Incorporate all updates until (<=) @tim_cur starting
 * at *@uid_curp.
 * Update @uid_curp.
 */
static inline void _upds_mov(
	tb_tst_lv1_ctx *ctx,
	tb_lv1_hst *hst,
	u64 *uid_curp,
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
			const u64 aid_tim = aid * ctx->aid_wid + ctx->tim_stt;
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
		if (tim_cln < tim) break;
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
	assert(tim_cur);

	/* First, check our own data. */
	const u64 upd_nbr = ctx->upd_nbr;
	assert(uid_cln <= upd_nbr);
	assert(uid_cur <= upd_nbr);
	assert(uid_add <= upd_nbr);
	assert((!did_cln) || (!uid_cln) || (ctx->upds[uid_cln - 1].tim <= tim_cln));
	assert((!did_cln) || (uid_cln == upd_nbr) || (ctx->upds[uid_cln].tim > tim_cln));
	assert((!uid_cur) || (ctx->upds[uid_cur - 1].tim < tim_cur));
	assert((!uid_add) || (ctx->upds[uid_add - 1].tim < tim_add));
	assert((uid_cur <= upd_nbr) || (ctx->upds[uid_cur].tim >= tim_cur));
	assert((uid_add == upd_nbr) || (ctx->upds[uid_add].tim >= tim_add));
	assert((!did_cln) || (uid_cln + 1 >= upd_nbr) || (ctx->upds[uid_cln + 1].tim > tim_cln)); 

	/*
	 * First, check the full update sequence and verify that :
	 * - every update is registered.
	 * - every update is associated with the correct tick.
	 * - every update is or is not in its tick list as expected.
	 */
	u64 idx = 0;
	u64 max = uid_add - uid_cln;
	const u64 tck_min = ctx->tck_min; 
	{
		tb_lv1_upd *upd;
		ns_slsh_fe(upd, &hst->upds_hst, upds_hst) {
			tb_tst_lv1_upd *src = ctx->upds + uid_cln + idx;

			/* Check the index. */
			assert(idx < max, "update sequence mismatch : too many updates.\n");

			/* Check data. */
			const f64 prc = src->prc;
			const u64 tck = _prc_to_tck(ctx, prc);
			assert(
				(upd->tck->tcks.val == tck_min + tck) &&
				(upd->tim == src->tim) &&
				(upd->vol == src->vol),
				"update sequence mismatch at index %U/%U (uid %U/%U) :\n"
				"\texp (tim : %U, vol : %d, tck : %U (prc : %d)),\n"
				"\tgot (tim : %U, vol : %d, tck : %U).\n",
				idx, max, uid_cln + idx, uid_add,
				src->tim, src->vol, tck_min + tck, prc,
				upd->tim, upd->vol, upd->tck->tcks.val
			);

			/* Check that the update's list state is correct. */
			if ((upd->tim < tim_cur) != (!ns_dls_empty(&upd->upds_tck))) {
				if (upd->tim < tim_cur) {
					assert(!ns_dls_empty(&upd->upds_tck), "update < current time not in tick list.");
				} else {
					assert(ns_dls_empty(&upd->upds_tck), "update >+ current time in tick list.");
				}
				assert(0, "should have asserted.\n");
			}

			/* Reiterate. */
			idx++;
		}
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
		const u64 tck_val = tck_min + _prc_to_tck(ctx, prc);

		/* Find the tick for this update. If its flag is already set, nothing to do. */
		tck = assert(ns_map_sch(&hst->tcks, tck_val, u64, tb_lv1_tck, tcks));
		if (tck->dbg) continue;

		/* This is the first time we encounter this tick.
		 * Hence, its maximal time and volume should match
		 * @src's. */ 
		assert(tck->vol_max == src->vol);
		assert(tck->tim_max == src->tim);
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
			assert((!tim_cur) || tck->tim_max < tim_cur);
		} else {
			tck_cnt++;
		}
		tck->dbg = 0;
	}
	assert(tck_cnt == tck_nbr);

	/* Traverse the update list in reverse between current and clean. */
	tck_cnt = tck_nbr;
	for (idx = uid_cur - uid_cln; idx--;) {
		tb_tst_lv1_upd *src = ctx->upds + uid_cln + idx;
		const f64 prc = src->prc;
		const u64 tck_val = tck_min + _prc_to_tck(ctx, prc);

		/* Find the tick for this update. If its flag is already set, nothing to do. */
		tck = assert(ns_map_sch(&hst->tcks, tck_val, u64, tb_lv1_tck, tcks));
		if (tck->dbg) continue;

		/* This is the first time we encounter this tick.
		 * Hence, its current volume should match @src's. */ 
		assert(tck->vol_cur == src->vol);
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
		const u64 tck_val = tck_min + _prc_to_tck(ctx, prc);

		/* Find the tick for this update. If its flag is already set, nothing to do. */
		tck = assert(ns_map_sch(&hst->tcks, tck_val, u64, tb_lv1_tck, tcks));
		if (tck->dbg) continue;

		/* This is the first time we encounter this tick.
		 * Hence, its current volume should match @src's. */ 
		assert(tck->vol_stt == src->vol);
		tck->dbg = 1;

		/* Count a tick processed, if all have been, stop. */
		SAFE_DECR(tck_cnt);
		if (tck_cnt == 0) break;
		
	}

	/* Now, clear the debug flag on each tick.
	 * Every tick we did not encounter should have a
	 * start price equal to the initial resting price.
	 * At the same time, determine the best bid / ask. */
	u64 bst_cur_bid = 0;
	u64 bst_max_bid = 0;
	u64 bst_cur_ask = (u64) -1;
	u64 bst_max_ask = (u64) -1;
	ns_map_fe(tck, &hst->tcks, tcks, u64, in) {
		debug("tck %U/%U %p %U %d.\n", tck_cnt, tck_nbr, tck, tck->tcks.val, tck->vol_max);

		/* Check tick state. */
		if (tck->dbg == 0) {
			const u64 tck_val = tck->tcks.val;
			assert(tck_val >= tck_min);
			assert(tck->vol_stt == ctx->hmp_ini[tck_val - tck_min]);
		} else {
			tck_cnt++;
		}
		tck->dbg = 0;

		/* Determine best bids / asks. */
		const u64 tck_val = tck->tcks.val;
		const f64 vol_max = tck->vol_max;
		const f64 vol_cur = tck->vol_cur;
		assert(tck_val != 0);
		assert(tck_val != (u64) -1);
		if (vol_max != 0) {
			if (vol_max < 0) {
				assert(bst_max_ask == (u64) -1, "found a bid after an ask at max time.\n");
				bst_max_bid = tck_val;
			} else {
				if (bst_max_ask == (u64) -1) {
					bst_max_ask = tck_val;
				}
			}
		}
		if (vol_cur != 0) {
			if (vol_cur < 0) {
				assert(bst_cur_ask == (u64) -1, "found a bid after an ask at cur time.\n");
				bst_cur_bid = tck_val;
			} else {
				if (bst_cur_ask == (u64) -1) {
					bst_cur_ask = tck_val;
				}
			}
		}
		assert(bst_cur_bid < bst_cur_ask);
		assert(bst_max_bid < bst_max_ask);
	}
	assert(tck_cnt == tck_nbr);

	/* Verify best bids/asks. */
	assert((bst_cur_bid == 0) == (hst->bst_cur_bid == 0));
	assert((bst_max_bid == 0) == (hst->bst_max_bid == 0));
	assert((bst_cur_ask == (u64) -1) == (hst->bst_cur_ask == 0));
	assert((bst_max_ask == (u64) -1) == (hst->bst_max_ask == 0));
	assert((bst_cur_bid == 0) || (hst->bst_cur_bid->tcks.val == bst_cur_bid));
	assert((bst_max_bid == 0) || (hst->bst_max_bid->tcks.val == bst_max_bid));
	assert((bst_cur_ask == (u64) -1) || (hst->bst_cur_ask->tcks.val == bst_cur_ask));
	assert((bst_max_ask == (u64) -1) || (hst->bst_max_ask->tcks.val == bst_max_ask));

	
	/*
	 * Verify the bid / ask curve.
	 */

	debug("Verifying the bid/ask curve.\n");

	/* Now that the history's state is verified, build an
	 * orderbook at the current time using @hst. */
	f64 *obk = ctx->obk;
	ns_mem_rst(obk, ctx->tck_nbr * sizeof(f64));
	ns_map_fe(tck, &hst->tcks, tcks, u64, in) {
		u64 tck_val = tck->tcks.val;
		check(tck_val >= ctx->tck_min);
		check(tck_val < ctx->tck_min + ctx->tck_nbr);
		if (tck->vol_cur) obk[tck_val - ctx->tck_min] = tck->vol_cur; 
	}

	/* Incorporate all updates before the bac time. */
	const u64 tim_bac = hst->tim_hmp;
	debug("bac : %U -> %U.\n", tim_bac, hst->bac_nb * hst->tim_res);
	debug("end : %U.\n", hst->tim_end);
	const u64 aid_bac = (tim_bac - ctx->tim_stt) / ctx->aid_wid;
	u64 uid = uid_cur;
	u64 tim_fst;
	for (; uid <= uid_add; uid++) {
		tb_tst_lv1_upd *src = (uid == uid_add) ? 0 : ctx->upds + uid;
		tim_fst = (src) ? src->tim : tim_add;
		assert(tim_cur <= tim_fst);
		assert(tim_fst <= tim_add);
		if (tim_fst > tim_bac) break;

		/* Read the tick. */
		const u64 tck_val = tck_min + _prc_to_tck(ctx, src->prc);
		check(tck_val >= ctx->tck_min);
		check(tck_val < ctx->tck_min + ctx->tck_nbr);
		obk[tck_val - ctx->tck_min] = src->vol;
	}

	/* Best bids and best asks at current update. */
	u64 bst_bid;
	u64 bst_ask;
	#define _get_bsts() ({\
		bst_bid = 0; bst_ask = (u64) -1; \
		for (u64 i = 0; i < ctx->tck_nbr; i++) { \
			const f64 vol = obk[i]; \
			debug("%U : %d.\n", i, vol); \
			if (vol == 0) continue; \
			const u64 abs_val = ctx->tck_min + i; \
			const u8 is_bid = vol < 0; \
			if (is_bid) bst_bid = abs_val; \
			else if (bst_ask == (u64) -1) bst_ask = abs_val; \
		} \
		assert(bst_bid < bst_ask); \
		debug("%I : %I\n", bst_bid, bst_ask); \
	})
	_get_bsts();

	/* Maintain a local best bid / ask for the current cell. */
	u64 tim_prv = tim_fst;
	u64 aid_bst = (tim_bac - ctx->tim_stt) / ctx->aid_wid;
	u64 aid_bst_bid = bst_bid;
	u64 aid_bst_ask = bst_ask;
	u64 lst_bid = bst_bid;
	u64 lst_ask = bst_ask;
	
	/* Now, incorporate all bac updates until @uid_add[,
	 * and whenever we change time, verify that the bid-ask
	 * curve was right. */
	/* We iterate INCLUDING uid_add but never access it.
	 * Rather, we detect it and force the cleanup path when
	 * we do. This allows us to use the same check path. */
	u64 nxt_chk = aid_bac;
	for (; uid <= uid_add; uid++) {

		/* Read the time.
		 * If no more updates, take the verif path once. */
		tb_tst_lv1_upd *src = (uid == uid_add) ? 0 : ctx->upds + uid;
		u64 tim_upd = (src) ? src->tim : tim_add;
		assert(tim_bac < tim_upd);
		assert(tim_upd <= tim_add);

		/* If we completed an update stride : */
		u8 has_chk = 0;
		if ((tim_upd != tim_prv)) {

			/* Determine aids. */
			const u64 aid_prv = (tim_prv - ctx->tim_stt) / ctx->aid_wid;
			debug("complete %U %U.\n", tim_prv, aid_prv);
			debug("lst : %I %I.\n", lst_bid, lst_ask);

			/* Determine the best bid/ask. */
			_get_bsts();

			/* The best bid and best ask must reflect @hst's
			 * bid ask curve. */
			if (aid_prv >= aid_bac) {
				assert(bst_bid <= hst->bid_crv[aid_prv - aid_bac],
					"%I > %I, [%U, %U]",
					bst_bid, hst->bid_crv[aid_prv - aid_bac],
					ctx->tim_stt + aid_prv * ctx->aid_wid, ctx->tim_stt + (aid_prv + 1) * ctx->aid_wid
				);
				assert(bst_ask >= hst->ask_crv[aid_prv - aid_bac],
					"%I != %I, [%U, %U]",
					bst_ask, hst->ask_crv[aid_prv - aid_bac],
					ctx->tim_stt + aid_prv * ctx->aid_wid, ctx->tim_stt + (aid_prv + 1) * ctx->aid_wid
				);
			}

			/* If we changed cell : */
			if (aid_prv != aid_bst) {

				debug("cel change %U -> %U.\n", aid_bst, aid_prv);

				/* The aggregate best bid and ask must be
				 * exactly @hst's. */
				if (aid_bst >= aid_bac) {
					assert(aid_bst == nxt_chk, "%U != %U.\n", aid_bst, nxt_chk);
					assert(aid_bst_bid == hst->bid_crv[aid_bst - aid_bac],
						"%I != %I, [%U, %U]",
						aid_bst_bid, hst->bid_crv[aid_bst - aid_bac],
						ctx->tim_stt + aid_bst * ctx->aid_wid, ctx->tim_stt + (aid_bst + 1) * ctx->aid_wid
					);
					assert(aid_bst_ask == hst->ask_crv[aid_bst - aid_bac],
						"%I != %I, [%U, %U]",
						aid_bst_ask, hst->ask_crv[aid_bst - aid_bac],
						ctx->tim_stt + aid_bst * ctx->aid_wid, ctx->tim_stt + (aid_bst + 1) * ctx->aid_wid
					);
					nxt_chk++;
				}

				/* Every cell in ]aid_bst, aid_prv[ was
				 * using the same orderbook, and the best
				 * bid / ask must match the oldest observed
				 * at aid_bst.
				 * aid_bst == -1 gracefully handled here. */
				for (idx = aid_bst + 1; idx < aid_prv; idx++) {
					if (idx >= aid_bac) {
						assert(idx == nxt_chk, "%U != %U.\n", idx, nxt_chk);
						assert(lst_bid == hst->bid_crv[idx - aid_bac],
							"%I != %I, [%U, %U]",
							lst_bid == hst->bid_crv[idx - aid_bac],
							ctx->tim_stt + idx * ctx->aid_wid, ctx->tim_stt + (idx + 1) * ctx->aid_wid
						);
						assert(lst_ask == hst->ask_crv[idx - aid_bac],
							"%I != %I, [%U, %U]",
							lst_ask == hst->ask_crv[idx - aid_bac],
							ctx->tim_stt + idx * ctx->aid_wid, ctx->tim_stt + (idx + 1) * ctx->aid_wid
						);
						nxt_chk++;
					}
				}

				/* Report that we changed cell.
				 * Propagate the last best only if 
				 * the current update is not on the
				 * cell's start time. */
				aid_bst = aid_prv;
				if (tim_prv == ((aid_prv * ctx->aid_wid) + ctx->tim_stt)) {
					aid_bst_bid = 0;
					aid_bst_ask = (u64) -1;
				} else {
					aid_bst_bid = lst_bid;
					aid_bst_ask = lst_ask;
				}

			}
			check(aid_bst != (u64) -1);
			check(aid_bst == aid_prv);

			debug("pre :\n");
			debug("  bst : %U, %I - %I.\n", aid_bst, aid_bst_bid, aid_bst_ask);
			debug("  lst : %I %I.\n", lst_bid, lst_ask);

			/* Incorporate in the best bid / ask. */
			if (bst_bid > aid_bst_bid) aid_bst_bid = bst_bid;
			if (bst_ask < aid_bst_ask) aid_bst_ask = bst_ask;

			/* Keep track of the last incorporated bests,
			 * to check for propagation. */
			lst_bid = bst_bid;
			lst_ask = bst_ask;

			debug("pst : \n");
			debug("  bst : %U, %I - %I.\n", aid_bst, aid_bst_bid, aid_bst_ask);
			debug("  lst : %I %I.\n", lst_bid, lst_ask);
			
			/* Update the previous time. */
			tim_prv = tim_upd;
		
		}

		/* If no more updates, exit. */
		if (!src) {
			break;
		}

		debug("UPD %U %U %d.\n", tim_upd, _prc_to_tck(ctx, src->prc), src->vol);

		/* Read the tick. */
		const u64 tck_val = tck_min + _prc_to_tck(ctx, src->prc);
		check(tck_val >= ctx->tck_min);
		check(tck_val < ctx->tck_min + ctx->tck_nbr);

		/* Incorporate the update. */
		obk[tck_val - ctx->tck_min] = src->vol;
	}
	assert(tim_prv == tim_add);
	assert(aid_bst < aid_bac + ctx->bac_siz);

	/*
	 * Check the final cells after the last updates.
	 */
	debug("final checks.\n");
	const u64 aid_max = aid_bac + ctx->bac_siz;

	/* The aggregate best bid and ask must be
	 * exactly @hst's. */
	if (aid_bst >= aid_bac) {
		assert(aid_bst == nxt_chk, "%U != %U.\n", aid_bst, nxt_chk);
		assert(aid_bst_bid == hst->bid_crv[aid_bst - aid_bac],
			"%I != %I, [%U, %U]",
			aid_bst_bid, hst->bid_crv[aid_bst - aid_bac],
			ctx->tim_stt + aid_bst * ctx->aid_wid, ctx->tim_stt + (aid_bst + 1) * ctx->aid_wid
		);
		assert(aid_bst_ask == hst->ask_crv[aid_bst - aid_bac],
			"%I != %I, [%U, %U]",
			aid_bst_ask, hst->ask_crv[aid_bst - aid_bac],
			ctx->tim_stt + aid_bst * ctx->aid_wid, ctx->tim_stt + (aid_bst + 1) * ctx->aid_wid
		);
		nxt_chk++;
	}

	/* Every cell in ]aid_bst, aid_max[ was
	 * using the same orderbook, and the best
	 * bid / ask must match the oldest observed
	 * at aid_bst.
	 * aid_bst == -1 gracefully handled here. */
	for (idx = aid_bst + 1; idx < aid_max; idx++) {
		if (idx >= aid_bac) {
			assert(idx == nxt_chk, "%U != %U.\n", idx, nxt_chk);
			assert(lst_bid == hst->bid_crv[idx - aid_bac],
				"%I != %I, [%U, %U]",
				lst_bid == hst->bid_crv[idx - aid_bac],
				ctx->tim_stt + idx * ctx->aid_wid, ctx->tim_stt + (idx + 1) * ctx->aid_wid
			);
			assert(lst_ask == hst->ask_crv[idx - aid_bac],
				"%I != %I, [%U, %U]",
				lst_ask == hst->ask_crv[idx - aid_bac],
				ctx->tim_stt + idx * ctx->aid_wid, ctx->tim_stt + (idx + 1) * ctx->aid_wid
			);
			nxt_chk++;
		}
	}

	/* Verify that we verified the entire bid-ask curve. */
	assert(nxt_chk == aid_bac + ctx->bac_siz);

}

/*
 * Verify the result produced by @hst.
 */
static inline void _vrf_res(
	tb_tst_lv1_ctx *ctx,
	tb_lv1_hst *hst,
	u64 tim_cur,
	u64 bst_bid,
	u64 bst_ask,
	u64 *chc_tims,
	f64 *chc_sums,
	f64 *chc_vols,
	u8 ini
)
{

//#define SKP_CHK

#ifndef SKP_CHK

	/* Verify the heatmap reference. */
	const u64 ref = hst->tck_ref;
	const u64 aid_cur = (tim_cur <= ctx->tim_stt) ? 0 : (tim_cur - 1 - ctx->tim_stt) / ctx->aid_wid;
	assert(ref == ctx->ref_arr[aid_cur], "reference mismatch at time %U cell %U : expected %U, got %U.\n",
		tim_cur, aid_cur, ctx->ref_arr[aid_cur], ref); 
	
	/*
	 * The heatmap's anchor point may be inside or
	 * outside @ctx's tick range.
	 * We will iterate over all ticks of the heatmap,
	 * and dynamically compte the corresponding
	 * index in @ctx's datastructures.
	 */
	const u64 hmp_min = hst->hmp_tck_min;
	const u64 hmp_max = hst->hmp_tck_max;
	const u64 hmp_nbr = ctx->hmp_dim_tck;
	assert(hmp_min + hmp_nbr == hmp_max);
	assert(hmp_min < hmp_max);

	/* Verify all heatmap columns except the current
	 * against the reference. */
	assert(!(ctx->tim_stt % ctx->aid_wid));
	assert(tim_cur >= ctx->tim_stt);
	const u64 aid_bac = (tim_cur + ctx->aid_wid - 1 - ctx->tim_stt) / ctx->aid_wid;
	const s64 aid_hmp = (s64) aid_bac - (s64) ctx->hmp_dim_tim;  
	const f64 *hmp = tb_lv1_hmp(hst);
	for (u64 cnt = 0; cnt < ctx->hmp_dim_tim - 1; cnt++) {
		const s64 col_idx = aid_hmp + (s64) cnt; 

		/* If column is before start point, it must be equal
		 * to the history's reset prices.
		 * If not, it must be equal to the actual history prices. */
		const u64 src_idx = (col_idx < 0) ? 0 : (u64) col_idx;
		const f64 *src = (col_idx < 0) ? ctx->hmp_ini : ctx->hmp;
		const f64 *src_cmp = src + src_idx * ctx->tck_nbr;
		const f64 *hmp_cmp = hmp + cnt * ctx->hmp_dim_tck;

		/* Compare each value in the heatmap. */
		for (u64 tck_idx = hmp_min; tck_idx < hmp_nbr; tck_idx++) {

			/* Read the value from the heatmap. */
			const f64 hmp_val = hmp_cmp[tck_idx - hmp_min];

			/* If the tick is in the generation range, read from source.
			 * Otherwise, the value is 0. */
			const f64 src_val = ((ctx->tck_min <= tck_idx) && (tck_idx < ctx->tck_max)) ?
				src_cmp[tck_idx - ctx->tck_min] :
				0;

			assert(hmp_val == src_val,
				"heatmap mismatch at column %U/%U row %U/%U (tick %U/%U, context : %I/%U) : expected %d got %d.\n",
				col_idx, ctx->hmp_dim_tim - 1,
				tck_idx - hmp_min, ctx->hmp_dim_tck,
				tck_idx, hmp_nbr,
				(s64) tck_idx - (s64) ctx->tck_min, ctx->tck_max,
				src_val, hmp_val
			);
		}

	}

	/* Verify the heatmap current column against the
	 * currently updated column. */
	const f64 *hmp_cmp = hmp + (ctx->hmp_dim_tim - 1) * ctx->hmp_dim_tck;
	const u64 div = (tim_cur - ctx->tim_stt) % ctx->aid_wid;
	/* Compare each value in the heatmap. */
	for (u64 tck_idx = hmp_min; tck_idx < hmp_nbr; tck_idx++) {

		/* If the tick is in the generation range, read from source.
		 * Otherwise, the value is 0. */
		f64 avg = 0;
		if ((ctx->tck_min <= tck_idx) && (tck_idx < ctx->tck_max)) {
			const u64 off = tck_idx - ctx->tck_min;
			assert(tim_cur >= chc_tims[off]);
			avg = (div) ? 
				(chc_sums[off] + (f64) (tim_cur - chc_tims[off]) * chc_vols[off]) / (f64) div :
				chc_vols[off];
		}

		assert(hmp_cmp[tck_idx - hmp_min] == avg,
			"heatmap mismatch at current column row %U/%U (tick %U/%U, context : %I/%U) : expected %d got %d.\n",
			tck_idx - hmp_min, ctx->hmp_dim_tck,
			tck_idx, hmp_nbr,
			(s64) tck_idx - (s64) ctx->tck_min, ctx->tck_max,
			avg, hmp_cmp[tck_idx - hmp_min]
		);
	}

	/*
	 * If init, we cannot verify the bid-ask curve,
	 * as it requires all information until the add
	 * time.
	 */
	if (!ini) {

		/* Verify all values of the bid-ask curves except the current one. */
		const u64 aid_max = (ini) ? 0 : ctx->bac_siz - 1;
		const u64 *bid = tb_lv1_bid(hst);
		const u64 *ask = tb_lv1_ask(hst);
		for (u64 chk_idx = 0; chk_idx < aid_max; chk_idx++) {
			assert(bid[chk_idx] == ctx->bid_arr[aid_bac + chk_idx],
				"bid curve mismatch at index %U/%U : expected %U got %U.\n",
				chk_idx, aid_max, ctx->bid_arr[aid_bac + chk_idx], bid[chk_idx]
			);
			assert(ask[chk_idx] == ctx->ask_arr[aid_bac + chk_idx],
				"ask curve mismatch at index %U/%U : expected %U got %U.\n",
				chk_idx, aid_max, ctx->ask_arr[aid_bac + chk_idx], ask[chk_idx]
			);
		}

		/* Verify the current best bid and ask. */
		assert(bid[aid_max] == bst_bid,
			"bid curve mismatch at index %U/%U : expected %U got %U.\n",
			aid_max, aid_max, bst_bid, bid[aid_max]
		);
		assert(ask[aid_max] == bst_ask,
			"ask curve mismatch at index %U/%U (aid [%U, %U[ tim [%U, %U[) : expected %U got %U.\n",
			aid_max, aid_max,
			(hst->tim_hmp - ctx->tim_stt) / ctx->aid_wid + aid_max, (hst->tim_hmp - ctx->tim_stt) / ctx->aid_wid + (aid_max + 1), 
			hst->tim_hmp + aid_max * ctx->aid_wid, hst->tim_hmp + (aid_max + 1) * ctx->aid_wid, 
			bst_ask, ask[aid_max]
		);

	}
#endif

}


/*
 * Entrypoint for lv1 tests.
 */
static void _lv1_run(
	nh_tst_sys *sys,
	u64 sed,
	tb_tst_lv1_gen *gen,
	u8 chk_stt,
	u64 tim_stt,
	u64 tim_inc,
	u64 tck_per_unt,
	u64 unt_nbr,
	u64 tck_nbr,
	f64 prc_min,
	u64 hmp_dim_tck,
	u64 hmp_dim_tim,
	u64 bac_siz,
	u64 tim_stp
)
{


	const u64 aid_wid = tim_inc * tim_stp;
	const u64 tck_min = (u64) (prc_min * (f64) hmp_dim_tck);

	/* Generation settings. */ 
	nh_all__(tb_tst_lv1_ctx, ctx);
	ctx->sed = sed;
	ctx->unt_nbr = unt_nbr;
	ctx->tim_stt = tim_stt;
	assert(!(ctx->tim_stt % 100));
	ctx->prc_min = prc_min;
	ctx->tck_nbr = tck_nbr;
	ctx->tim_inc = tim_inc;
	ctx->tim_stp = tim_stp;
	ctx->aid_wid = aid_wid;
	ctx->tck_rat = tck_per_unt;
	ctx->hmp_dim_tck = hmp_dim_tck;
	ctx->hmp_dim_tim = hmp_dim_tim;
	ctx->bac_siz = bac_siz;
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
	 * updates as the current time (not more than tck_nbr). */
	u64 *tims = nh_all(2 * tck_nbr * sizeof(u64));
	f64 *prcs = nh_all(2 * tck_nbr * sizeof(f64));
	f64 *vols = nh_all(2 * tck_nbr * sizeof(f64));

	/* Export the global heatmap. */
	//_hmp_exp(ctx);

	/*
	 * Create a reconstructor covering a 
	 * 100 * 100 heatmap window
	 * and a 200 ticks bid-ask prediction.
	 */
	tb_lv1_hst *hst = tb_lv1_ctr(
		aid_wid,
		tck_per_unt,
		hmp_dim_tck,
		hmp_dim_tim,
		bac_siz
	);

	/* Set initial volumes in groups of 19 by step of 19. */
	debug("Adding initial volumes.\n");
	assert(tck_nbr % 19);
	assert(19 % tck_nbr);
	u64 ttl_non_nul = 0;
	for (u32 i = 0; i < tck_nbr;) {
		u64 nb_non_nul = 0;
		for (u32 j = 0; (j < 19) && (i < tck_nbr); i++, j++) {
			u32 ri = (u32) ((19 * i) % tck_nbr);
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

	/* The time below (<) which orders belong to the
	 * heatmap. We prepare until tim_cur. */
	u64 tim_cur = tim_stt;

	/* The time below (<) which orders belong to the
	 * bid ask curve. */
	u64 tim_add = tim_stt + bac_siz * aid_wid;

	/* The time below which (<) we remove updates
	 * from the history. */
	assert(tim_stt > hmp_dim_tim * aid_wid);
	u64 tim_cln = tim_stt - hmp_dim_tim * aid_wid;

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
	u64 *chc_tims = nh_all(tck_nbr * sizeof(u64));
	f64 *chc_sums = nh_all(tck_nbr * sizeof(f64));
	f64 *chc_vols = nh_all(tck_nbr * sizeof(f64));
	u64 chc_aid = 0;
	for (u32 rst_idx = 0; rst_idx < ctx->tck_nbr; rst_idx++) {
		chc_tims[rst_idx] = tim_stt;
		chc_sums[rst_idx] = 0;
		chc_vols[rst_idx] = ctx->hmp_ini[rst_idx];
	}

	/* Current best bid / asks. */
	u64 bst_bac_aid = 0;
	u64 bst_bid = ctx->bid_ini[0];
	u64 bst_ask = ctx->ask_ini[0];

	debug("Initial prepare + process.\n");

	/* Prepare for insertion until bac end. */
	tb_lv1_prp(hst, tim_cur);

	/* Push all updates until @tim_add. */
	_upds_add_ini(ctx, hst, tims, prcs, vols, tim_add, &uid_add, &bst_bac_aid, &bst_bid, &bst_ask);

	/* Process. */
	tb_lv1_prc(hst);

	/* Verify the history's internal state. */
	if (chk_stt) {
		_vrf_hst(
			ctx, hst,
			uid_cln, uid_cur, uid_add,
			tim_cln, tim_cur, tim_add,
			0
		);
	}

	/* Verify the result. */
	_vrf_res(
		ctx, hst,
		tim_cur,
		bst_bid, bst_ask,
		chc_tims, chc_sums, chc_vols,
		0
	);

	/* Generate and compare the heatmap and bac. */
	u64 itr_idx = 0;
	debug("Main loop.\n");
	while (tim_add < tim_end) {

		debug("Iteration %U\n", itr_idx);
		
		/* Update tim_add. */
		const u64 tim_add_new = _tim_add_inc(ctx, tim_add, itr_idx);
		assert(tim_add_new > tim_add);
		assert(!(tim_add_new % ctx->tim_inc));
		tim_add = tim_add_new;

		/* Update the current time and clean time. */
		assert(tim_cur < tim_add - bac_siz * aid_wid); 
		tim_cur = tim_add - bac_siz * aid_wid;
		
		/* Prepare to add. */
		debug("Preparing to %U.\n", tim_cur);
		tb_lv1_prp(hst, tim_cur);

		/* Determine the new heatmap start time,
		 * and the new bac end time. */
		const u64 hmp_end = aid_wid * ((tim_cur + aid_wid - 1) / aid_wid);
		assert(hmp_end >= hmp_dim_tim * aid_wid);
		tim_cln = hmp_end - hmp_dim_tim * aid_wid;
		assert(hst->tim_cur == tim_cur);
		assert(hst->tim_hmp == hmp_end);
		assert(hst->tim_end == tim_add);

		/* Add updates at the current tick. */
		_upds_add(
			ctx, hst,
			tims, prcs, vols,
			tim_add, &uid_add,
			&bst_bac_aid, &bst_bid, &bst_ask
		);
		
		/* Process. */
		tb_lv1_prc(hst);

		/* Incorporate all orders <= current time in the
		 * heatmap data. */
		_upds_mov(ctx, hst, &uid_cur, tim_cur, chc_tims, chc_sums, chc_vols, &chc_aid);

		/* Cleanup one time out of 10. */
		u8 do_cln = (!itr_idx % 20);
		if (do_cln) {

			/* Verify the history internal state. */
			if (chk_stt) {
				_vrf_hst(
					ctx, hst,
					uid_cln, uid_cur, uid_add,
					tim_cln, tim_cur, tim_add,
					0
				);
			}

			/* Determine the clean index. */
			uid_cln = _uid_cln_upd(ctx, uid_cln, tim_cln);

			/* Clean. */
			tb_lv1_cln(hst);

		}

		/* Verify the history internal state. */
		if (chk_stt) {
			_vrf_hst(
				ctx, hst,
				uid_cln, uid_cur, uid_add,
				tim_cln, tim_cur, tim_add,
				do_cln
			);
		}

		/* Verify the result. */
		_vrf_res(
			ctx, hst,
			tim_cur,
			bst_bid, bst_ask,
			chc_tims, chc_sums, chc_vols,
			0
		);

		itr_idx++;
	}	


	/* Delete the history. */
	tb_lv1_dtr(hst);

	/* Free buffers. */ 
	nh_fre(tims, 2 * tck_nbr * sizeof(u64));
	nh_fre(prcs, 2 * tck_nbr * sizeof(f64));
	nh_fre(vols, 2 * tck_nbr * sizeof(f64));
	nh_fre(chc_tims, tck_nbr * sizeof(u64));
	nh_fre(chc_sums, tck_nbr * sizeof(f64));
	nh_fre(chc_vols, tck_nbr * sizeof(f64));

	/* Delete tick data. */
	tb_tst_lv1_upds_del(ctx);

	/* Free settings. */ 
	nh_fre_(ctx);

}

#define CHECK_STATE 0

/*
 * Verification test.
 */
static inline void _vrf_tst(
	nh_tst_sys *sys,
	u64 sed,
	u64 prd,
	u64 phs,
	u8 bid,
	u8 ask
)
{
	assert(bid || ask);
	tb_tst_lv1_gen_spl *spl = tb_tst_lv1_gen_spl_ctr(
		bid, ask,
		1000000, 1000001, 1000002, 1000003,
		prd, phs
	);

	_lv1_run(
		sys, sed, &spl->gen, CHECK_STATE,
		NS_TIM_S(1000), // Start at 10s.
		NS_TIM_1MS, // Order every ms.
		100, // 100 tick per price unit.
		150, // 150 time units.
		7, // 7 tick.
		10000, // prices start at 10000. 
		100, // heatmap has 100 ticks.
		100, // heatmap has 100 units.
		10, // bac has 10 units.
		10 // 10 increments per time unit. 
	);
}

/*
 * Random test.
 */
static inline void _rdm_tst(
	nh_tst_sys *sys,
	u64 sed
)
{
	tb_tst_lv1_gen_rdm *rdm = tb_tst_lv1_gen_rdm_ctr(
		43,
		100,
		47,
		8,
		9,
		23,
		27
	);

	_lv1_run(
		sys, sed, &rdm->gen, CHECK_STATE,
		NS_TIM_S(1000), // Start at 10s.
		NS_TIM_1MS, // Order every ms.
		100, // 100 tick per price unit.
		1000, // 1000 time units.
		37, // 37 tick.
		10000, // prices start at 10000. 
		100, // heatmap has 100 ticks.
		100, // heatmap has 100 units.
		200, // bac has 200 units.
		10 // 10 increments per time unit. 
	);
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
	_rdm_tst(sys, sed);

	assert(0);
	_vrf_tst(sys, sed, 10, 0, 1, 0);
	_vrf_tst(sys, sed, 10, 0, 0, 1);
	_vrf_tst(sys, sed, 10, 0, 1, 1);
	_vrf_tst(sys, sed, 10, 5, 1, 0);
	_vrf_tst(sys, sed, 10, 5, 0, 1);
	_vrf_tst(sys, sed, 10, 5, 1, 1);
	_vrf_tst(sys, sed, 1, 0, 1, 0);
	_vrf_tst(sys, sed, 1, 0, 0, 1);
	_vrf_tst(sys, sed, 1, 0, 1, 1);
	_vrf_tst(sys, sed, 30, 0, 1, 0);
	_vrf_tst(sys, sed, 30, 0, 0, 1);
	_vrf_tst(sys, sed, 30, 0, 1, 1);
	_vrf_tst(sys, sed, 50, 0, 1, 1);
	assert(0);
	_rdm_tst(sys, sed);
}
