/* Copyright 2025 Raphael Outhier - confidential - proprietary - no copy - no diffusion. */

#include <tb_tst/tb_tst.all.h>

#include <tb_tst/lv1_utl.h>

/*****************
 * Updates check *
 *****************/

/*
 * Check the full update sequence and verify that :
 * - every update is registered.
 * - every update is associated with the correct tick.
 * - every update is or is not in its tick list as expected.
 */
static inline void _vrf_hst_upds(
	tb_tst_lv1_ctx *ctx,
	tb_lv1_hst *hst,
	u64 uid_cln,
	u64 uid_add,
	u64 tim_cur
)
{
	const u64 max = uid_add - uid_cln;
	const u64 tck_min = ctx->tck_min; 
	u64 idx = 0;
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
	assert(idx == max, "not enough updates in history : expected %U, got %U.\n", idx, max);
}

/*********************
 * Tick verification *
 *********************/

/*
 * Recompute the best bid and ask at current and max
 * times and verify that they match @hst's version.
 */
static inline u64 _vrf_hst_tcks(
	tb_tst_lv1_ctx *ctx,
	tb_lv1_hst *hst,
	u64 uid_cln,
	u64 uid_cur,
	u64 uid_add,
	u64 tim_cln,
	u64 tim_cur,
	u64 tim_add
)
{

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
	const u64 tck_min = ctx->tck_min; 
	u64 upd_cnt = 0;
	for (u64 idx = uid_add - uid_cur; idx--;) {
		upd_cnt++;
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
	upd_cnt = 0;
	for (u64 idx = uid_cur - uid_cln; idx--;) {
		upd_cnt++;
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
		//if (tck_cnt == 0) break;
		
	}
	assert(upd_cnt == uid_cur - uid_cln);

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

	/* Return the tick number. */
	return tck_nbr;

}

/*****************************
 * Best bid/ask verification *
 *****************************/

/*
 * Log.
 */
#ifdef BAC_LOG
#define _bac_dbg debug
#else
#define _bac_dbg(...)
#endif

/*
 * Recompute the best bid and ask at current and max
 * times and verify that they match @hst's version.
 */
static inline void _vrf_hst_bba(
	tb_tst_lv1_ctx *ctx,
	tb_lv1_hst *hst,
	u64 uid_cln,
	u64 uid_cur,
	u64 uid_add,
	u64 tim_cln,
	u64 tim_cur,
	u64 tim_add,
	u64 tck_nbr
)
{
	/* Now, clear the debug flag on each tick.
	 * Every tick we did not encounter should have a
	 * start price equal to the initial resting price.
	 * At the same time, determine the best bid / ask. */
	u64 bst_cur_bid = 0;
	u64 bst_max_bid = 0;
	u64 bst_cur_ask = (u64) -1;
	u64 bst_max_ask = (u64) -1;
	tb_lv1_tck *tck;
	u64 tck_cnt = 0;
	const u64 tck_min = ctx->tck_min;
	ns_map_fe(tck, &hst->tcks, tcks, u64, in) {
		_bac_dbg("tck %U/%U %p %U %d.\n", tck_cnt, tck_nbr, tck, tck->tcks.val, tck->vol_max);
		tck_cnt++;

		/* Check tick state. */
		const u64 tck_val = tck->tcks.val;
		assert(tck_val >= tck_min);
		assert(tck->vol_stt == ctx->hmp_ini[tck_val - tck_min]);
		tck->dbg = 0;

		/* Determine best bids / asks. */
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

}

/********************************
 * Bid - ask curve verification *
 ********************************/

/*
 * Reconstruct the bid-ask curve of @hst and verify
 * that it matches @hst's version.
 */
static inline void _vrf_hst_bac(
	tb_tst_lv1_ctx *ctx,
	tb_lv1_hst *hst,
	u64 uid_cln,
	u64 uid_cur,
	u64 uid_add,
	u64 tim_cln,
	u64 tim_cur,
	u64 tim_add
)
{	
	/*
	 * Verify the bid / ask curve.
	 */

	_bac_dbg("Verifying the bid/ask curve.\n");
	const u64 tck_min = ctx->tck_min; 

	/* Now that the history's state is verified, build an
	 * orderbook at the current time using @hst. */
	f64 *obk = ctx->obk;
	ns_mem_rst(obk, ctx->tck_nbr * sizeof(f64));
	tb_lv1_tck *tck;
	ns_map_fe(tck, &hst->tcks, tcks, u64, in) {
		u64 tck_val = tck->tcks.val;
		check(tck_val >= tck_min);
		check(tck_val < tck_min + ctx->tck_nbr);
		if (tck->vol_cur) obk[tck_val - tck_min] = tck->vol_cur; 
	}

	/* Incorporate all updates before the bac time. */
	const u64 tim_bac = hst->tim_hmp;
	_bac_dbg("bac : %U -> %U.\n", tim_bac, hst->bac_nb * hst->tim_res);
	_bac_dbg("end : %U.\n", hst->tim_end);
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
		check(tck_val >= tck_min);
		check(tck_val < tck_min + ctx->tck_nbr);
		obk[tck_val - tck_min] = src->vol;
	}

	/* Best bids and best asks at current update. */
	u64 bst_bid;
	u64 bst_ask;
	#define _get_bsts() ({\
		bst_bid = 0; bst_ask = (u64) -1; \
		for (u64 i = 0; i < ctx->tck_nbr; i++) { \
			const f64 vol = obk[i]; \
			_bac_dbg("%U : %d.\n", i, vol); \
			if (vol == 0) continue; \
			const u64 abs_val = tck_min + i; \
			const u8 is_bid = vol < 0; \
			if (is_bid) bst_bid = abs_val; \
			else if (bst_ask == (u64) -1) bst_ask = abs_val; \
		} \
		assert(bst_bid < bst_ask); \
		_bac_dbg("%I : %I\n", bst_bid, bst_ask); \
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
		if ((tim_upd != tim_prv)) {

			/* Determine aids. */
			const u64 aid_prv = (tim_prv - ctx->tim_stt) / ctx->aid_wid;
			_bac_dbg("complete %U %U.\n", tim_prv, aid_prv);
			_bac_dbg("lst : %I %I.\n", lst_bid, lst_ask);

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

				_bac_dbg("cel change %U -> %U.\n", aid_bst, aid_prv);

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
				for (u64 idx = aid_bst + 1; idx < aid_prv; idx++) {
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

			_bac_dbg("pre :\n");
			_bac_dbg("  bst : %U, %I - %I.\n", aid_bst, aid_bst_bid, aid_bst_ask);
			_bac_dbg("  lst : %I %I.\n", lst_bid, lst_ask);

			/* Incorporate in the best bid / ask. */
			if (bst_bid > aid_bst_bid) aid_bst_bid = bst_bid;
			if (bst_ask < aid_bst_ask) aid_bst_ask = bst_ask;

			/* Keep track of the last incorporated bests,
			 * to check for propagation. */
			lst_bid = bst_bid;
			lst_ask = bst_ask;

			_bac_dbg("pst : \n");
			_bac_dbg("  bst : %U, %I - %I.\n", aid_bst, aid_bst_bid, aid_bst_ask);
			_bac_dbg("  lst : %I %I.\n", lst_bid, lst_ask);
			
			/* Update the previous time. */
			tim_prv = tim_upd;
		
		}

		/* If no more updates, exit. */
		if (!src) {
			break;
		}

		_bac_dbg("UPD %U %U %d.\n", tim_upd, _prc_to_tck(ctx, src->prc), src->vol);

		/* Read the tick. */
		const u64 tck_val = tck_min + _prc_to_tck(ctx, src->prc);
		check(tck_val >= tck_min);
		check(tck_val < tck_min + ctx->tck_nbr);

		/* Incorporate the update. */
		obk[tck_val - tck_min] = src->vol;
	}
	assert(tim_prv == tim_add);
	assert(aid_bst < aid_bac + ctx->bac_siz);

	/*
	 * Check the final cells after the last updates.
	 */
	_bac_dbg("final checks.\n");
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
	for (u64 idx = aid_bst + 1; idx < aid_max; idx++) {
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


/************************
 * Heatmap verification *
 ************************/

/*
 * Reconstruct the heatmap of @hst and verify
 * that it matches @hst's version.
 */
static inline void _vrf_hst_hmp(
	tb_tst_lv1_ctx *ctx,
	tb_lv1_hst *hst,
	u64 uid_cln,
	u64 uid_cur,
	u64 uid_add,
	u64 tim_cln,
	u64 tim_cur,
	u64 tim_add
)
{

	/* Constants. */
	const u64 dim_tck = hst->hmp_dim_tck;
	const u64 dim_tim = hst->hmp_dim_tim;
	const u64 tim_res = hst->tim_res;
	assert(hst->tim_cur <= hst->tim_hmp);
	const u64 hmp_tim_end = hst->tim_hmp;
	const u64 hmp_tim_stt = hst->tim_hmp - hst->hmp_tim_spn;
	assert(hmp_tim_end > hmp_tim_stt);
	assert((hmp_tim_end - hmp_tim_stt) == ((hst->hmp_dim_tim) * tim_res));

	/*
	 * Traverse each tick of the heatmap.
	 */
	u64 tck_cnt = 0;
	for (u64 row_idx = 0; row_idx < dim_tck; row_idx++) {
		tck_cnt++;

		/* Determine the effective tick ID. */
		u64 tck_val = row_idx + hst->hmp_tck_min;
		
		/* Find the tick. */
		tb_lv1_tck *tck = ns_map_sch(&hst->tcks, tck_val, u64, tb_lv1_tck, tcks);
		
		/* If tick not present, the entire heatmap row must be null. */
		if (!tck) {
			u64 tim_cnt = 0;
			for (u64 col_idx = 0; col_idx < dim_tim; col_idx++) {
				tim_cnt++;
				assert(hst->hmp[col_idx * dim_tck + row_idx] == 0,
					"incorrect heatmap value at row %U/%U (tck %U) col %U/%U.\n"
					"Expected 0 (no tick data), got %d.",
					row_idx, dim_tck,
					tck_val, 
					col_idx, dim_tim,
					hst->hmp[col_idx * dim_tck + row_idx]
				);
			}	
			assert(tim_cnt == dim_tim);
			continue;
		}

		/* Otherwise, recompute the tick data. */

		/* Cell computation state. */
		assert(hst->tim_cur <= hmp_tim_end);
		assert(hmp_tim_end - tim_res < hst->tim_cur);
		u64 cel_tim_stt = hmp_tim_end - tim_res; 
		s64 col_nxt = (s64) dim_tim - 1;
		u64 upd_lst_tim = hst->tim_cur; 
		u64 cel_ttl_dur = upd_lst_tim - cel_tim_stt;
		u64 cel_dur = 0;
		f64 cel_sum = 0;


		/* If tick is present, recompute the value of each cell for this tick. */
		tb_lv1_upd *upd;
		u64 upd_cnt = 0;
		u8 fst = 1;
		ns_dls_fer(upd, &tck->upds_tck, upds_tck) {
			upd_cnt++;

			/* Check context. */
			assert(col_nxt >= 0);
			assert(upd_lst_tim >= cel_tim_stt);
			assert(!(cel_tim_stt % tim_res));
			assert((cel_tim_stt - hmp_tim_stt) / tim_res == (u64) col_nxt);

			/* Determine the update's cell. */
			const u64 upd_tim = upd->tim;
			assert(upd_tim <= upd_lst_tim);
			const f64 upd_vol = upd->vol;
			const s64 upd_cel = ((s64) upd_tim - (s64) hmp_tim_stt) / (s64) tim_res;
			assert((upd_cel != col_nxt) == (upd_tim < cel_tim_stt));

			/* If first update, must match the current volume. */ 
			assert((!fst) || (upd_vol == tck->vol_cur));
			fst = 0;

			/* Incorporate the update in the current cell's state. */
			const u64 inc_stt = (upd_tim <= cel_tim_stt) ? cel_tim_stt : upd_tim;
			assert(inc_stt <= upd_lst_tim);
			const u64 inc_dur = upd_lst_tim - inc_stt;
			if (inc_dur) {
				cel_dur += inc_dur; 
				cel_sum += (f64) upd_vol * (f64) inc_dur;
				assert(cel_dur <= tim_res);
			}

			/* Update the time of last update. */
			upd_lst_tim = upd_tim;

			/* If we changed cell : */
			assert(upd_cel <= col_nxt);
			if (upd_cel != col_nxt) {

				/* Compute and compare the cell's expected value. */
				assert(cel_dur == cel_ttl_dur);
				assert(cel_dur <= tim_res);
				const f64 cel_val = (f64) cel_sum / (f64) cel_dur;
				assert(cel_val == hst->hmp[((u64) col_nxt) * dim_tck + row_idx],
					"incorrect heatmap value at row %U/%U col %U/%U.\n"
					"Expected %d, got %d.",
					row_idx, dim_tck,
					col_nxt, dim_tim,
					cel_val, hst->hmp[((u64) col_nxt) * dim_tck + row_idx]
				);
				col_nxt--;

				/* All cells between ] max(upd_cel, 0), col_nxt [ should have @upd_vol as value. */ 
				s64 prp_min = (upd_cel < 0) ? 0 : upd_cel + 1;
				assert(prp_min >= 0);
				for (s64 prp_idx = col_nxt; prp_idx >= prp_min; prp_idx--) {
					assert(col_nxt >= 0);
					assert(prp_idx == col_nxt);
					assert(upd_vol == hst->hmp[((u64) col_nxt) * dim_tck + row_idx],
						"incorrect heatmap value at row %U/%U col %U/%U.\n"
						"Expected %d, got %d.",
						row_idx, dim_tck,
						col_nxt, dim_tim,
						upd_vol, hst->hmp[((u64) col_nxt) * dim_tck + row_idx]
					);
					col_nxt--;
				}

				/* If we're past the heatmap start, stop now. */
				if (upd_tim < hmp_tim_stt) break;

				/* Reset the cell state. */
				cel_tim_stt = upd_tim - (upd_tim % tim_res);
				assert(cel_tim_stt <= upd_tim); 
				assert(upd_tim < cel_tim_stt + tim_res);
				cel_ttl_dur = tim_res;
				cel_dur = cel_tim_stt + tim_res - upd_tim; 
				assert(cel_dur);
				assert(cel_dur <= cel_ttl_dur);
				assert((cel_dur == cel_ttl_dur) == (!(upd_tim % tim_res)));
				cel_sum = (f64) upd_vol * (f64) cel_dur;
			}
			
		}
		assert(fst == (upd_cnt == 0));
		assert((!fst) || (tck->vol_stt == tck->vol_cur));

		/* If we did not verify all cells, use the resting time to check remaining ones. */
		assert((col_nxt != -1) == (upd_lst_tim > hmp_tim_stt));
		if (col_nxt != -1) {

			/* Get the resting volume. */
			const f64 vol_stt = tck->vol_stt;

			/* Incorporate the resting volume in the current cell's state. */
			cel_tim_stt = hmp_tim_stt + tim_res * col_nxt;
			const u64 inc_stt = cel_tim_stt;
			assert(inc_stt <= upd_lst_tim);
			const u64 inc_dur = upd_lst_tim - inc_stt;
			assert(inc_dur <= tim_res);
			if (inc_dur) {
				cel_dur += inc_dur; 
				cel_sum += (f64) vol_stt * (f64) inc_dur;
			}
			assert(cel_dur <= tim_res);

			/* Update the time of last update. */
			upd_lst_tim = hmp_tim_stt;

			/* Compute and compare the cell's expected value. */
			assert(cel_dur == cel_ttl_dur);
			const f64 cel_val = (f64) cel_sum / (f64) cel_dur;
			assert(cel_val == hst->hmp[((u64) col_nxt) * dim_tck + row_idx],
				"incorrect heatmap value at row %U/%U col %U/%U.\n"
				"Expected %d, got %d.",
				row_idx, dim_tck,
				col_nxt, dim_tim,
				cel_val, hst->hmp[((u64) col_nxt) * dim_tck + row_idx]
			);
			col_nxt--;

			/* All cells between [0, col_nxt [ should have @vol_stt as value. */ 
			for (s64 prp_idx = col_nxt + 1; prp_idx--;) {
				assert(col_nxt >= 0);
				assert(prp_idx == col_nxt);
				assert(vol_stt == hst->hmp[((u64) col_nxt) * dim_tck + row_idx],
					"incorrect heatmap value at row %U/%U col %U/%U.\n"
					"Expected %d, got %d.",
					row_idx, dim_tck,
					col_nxt, dim_tim,
					vol_stt, hst->hmp[((u64) col_nxt) * dim_tck + row_idx]
				);
				col_nxt--;
			}

		}

		/* Check that we verified all cells. */
		assert(col_nxt == -1);
		assert(upd_lst_tim <= hmp_tim_stt);
		
	}
	assert(tck_cnt == dim_tck);

}

/***************************
 * Verification entrypoint *
 ***************************/

/*
 * Verify @hst internal state.
 */
void tb_tst_lv1_vrf_hst_stt(
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

	/* Verify the updates list. */
	_vrf_hst_upds(
		ctx, hst,
		uid_cln, uid_add,
		tim_cur
	);

	/* Verify tick data. */
	u64 tck_nbr = _vrf_hst_tcks(
		ctx, hst,
		uid_cln, uid_cur, uid_add,
		tim_cln, tim_cur, tim_add
	);

	/* Verify best bid and ask at current and max times. */
	_vrf_hst_bba(
		ctx, hst,
		uid_cln, uid_cur, uid_add,
		tim_cln, tim_cur, tim_add,
		tck_nbr
	);

	/* Verify the bid-ask curve. */
	_vrf_hst_bac(
		ctx, hst,
		uid_cln, uid_cur, uid_add,
		tim_cln, tim_cur, tim_add
	);

	/* Verify the heatmap. */
	_vrf_hst_hmp(
		ctx, hst,
		uid_cln, uid_cur, uid_add,
		tim_cln, tim_cur, tim_add
	);

}

/************************
 * Results verification * 
 ************************/

/*
 * Verify the result produced by @hst.
 */
void tb_tst_lv1_vrf_hst_res(
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
	u64 itr_nbr = 0;
	for (u64 cnt = 0; cnt < ctx->hmp_dim_tim; cnt++) {
		itr_nbr++;
		const s64 col_idx = aid_hmp + (s64) cnt; 

		/* If column is before start point, it must be equal
		 * to the history's reset prices.
		 * If not, it must be equal to the actual history prices. */
		const u64 src_idx = (col_idx < 0) ? 0 : (u64) col_idx;
		const f64 *src = (col_idx < 0) ? ctx->hmp_ini : ctx->hmp;
		const f64 *src_cmp = src + src_idx * ctx->tck_nbr;
		const f64 *hmp_cmp = hmp + cnt * ctx->hmp_dim_tck;

		/* Compare each value in the heatmap. */
		u64 chk_nbr = 0;
		for (u64 tck_idx = hmp_min; tck_idx < hmp_min + hmp_nbr; tck_idx++) {
			chk_nbr++;

			/* Read the value from the heatmap. */
			const f64 hmp_val = hmp_cmp[tck_idx - hmp_min];

			/* If the tick is in the generation range, read from source.
			 * Otherwise, the value is 0. */
			const f64 src_val = ((ctx->tck_min <= tck_idx) && (tck_idx < ctx->tck_max)) ?
				src_cmp[tck_idx - ctx->tck_min] :
				0;

			assert(hmp_val == src_val,
				"heatmap mismatch at column %U/%U (aid %I), row %U/%U (tick %U/%U, context : %I/%U) : expected %d got %d.\n",
				cnt, ctx->hmp_dim_tim - 1,
				col_idx,
				tck_idx - hmp_min, ctx->hmp_dim_tck,
				tck_idx, hmp_nbr + hmp_min,
				(s64) tck_idx - (s64) ctx->tck_min, ctx->tck_max - ctx->tck_min,
				src_val, hmp_val
			);
		}
		assert(chk_nbr == hst->hmp_dim_tck);

	}
	assert(itr_nbr == hst->hmp_dim_tim);

	/* Verify the heatmap current column against the
	 * currently updated column. */
	const f64 *hmp_cmp = hmp + (ctx->hmp_dim_tim - 1) * ctx->hmp_dim_tck;
	const u64 div = (tim_cur - ctx->tim_stt) % ctx->aid_wid;
	/* Compare each value in the heatmap. */
	debug("%U %U.\n", hmp_min, hmp_nbr);
	u64 chk_nbr = 0;
	for (u64 tck_idx = hmp_min; tck_idx < hmp_min + hmp_nbr; tck_idx++) {
		chk_nbr++;

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
	assert(chk_nbr == hst->hmp_dim_tck);

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
		chk_nbr = 0;
		for (u64 chk_idx = 0; chk_idx < aid_max; chk_idx++) {
			chk_nbr++;
			assert(bid[chk_idx] == ctx->bid_arr[aid_bac + chk_idx],
				"bid curve mismatch at index %U/%U : expected %U got %U.\n",
				chk_idx, aid_max, ctx->bid_arr[aid_bac + chk_idx], bid[chk_idx]
			);
			assert(ask[chk_idx] == ctx->ask_arr[aid_bac + chk_idx],
				"ask curve mismatch at index %U/%U : expected %U got %U.\n",
				chk_idx, aid_max, ctx->ask_arr[aid_bac + chk_idx], ask[chk_idx]
			);
		}
		assert(chk_nbr == hst->bac_nb - 1);

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

}

