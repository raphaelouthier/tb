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
	for (u64 idx = uid_add - uid_cur; idx--;) {
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
	for (u64 idx = uid_cur - uid_cln; idx--;) {
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

	/* Return the tick number. */
	return tck_nbr;

}

/*****************************
 * Best bid/ask verification *
 *****************************/

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

	debug("Verifying the bid/ask curve.\n");
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
			debug("%U : %d.\n", i, vol); \
			if (vol == 0) continue; \
			const u64 abs_val = tck_min + i; \
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

	/* Test the history state. */
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
		u64 cnt = 0;
		for (u64 tck_idx = hmp_min; tck_idx < hmp_min + hmp_nbr; tck_idx++) {
			cnt++;

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
		assert(cnt == hst->hmp_dim_tck);

	}

	/* Verify the heatmap current column against the
	 * currently updated column. */
	const f64 *hmp_cmp = hmp + (ctx->hmp_dim_tim - 1) * ctx->hmp_dim_tck;
	const u64 div = (tim_cur - ctx->tim_stt) % ctx->aid_wid;
	/* Compare each value in the heatmap. */
	debug("%U %U.\n", hmp_min, hmp_nbr);
	u64 cnt = 0;
	for (u64 tck_idx = hmp_min; tck_idx < hmp_min + hmp_nbr; tck_idx++) {
		cnt++;

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
	assert(cnt == hst->hmp_dim_tck);

#error TODO FOR EVERY FOR LOOK KEEP TRACK OF A COUNTER AND CHECK THAT WE ITERATED FOR THE REQUIRED NUMBER OF STEPS.
#error TODO FOR EVERY FOR LOOK KEEP TRACK OF A COUNTER AND CHECK THAT WE ITERATED FOR THE REQUIRED NUMBER OF STEPS.
#error TODO FOR EVERY FOR LOOK KEEP TRACK OF A COUNTER AND CHECK THAT WE ITERATED FOR THE REQUIRED NUMBER OF STEPS.
#error TODO FOR EVERY FOR LOOK KEEP TRACK OF A COUNTER AND CHECK THAT WE ITERATED FOR THE REQUIRED NUMBER OF STEPS.
#error TODO FOR EVERY FOR LOOK KEEP TRACK OF A COUNTER AND CHECK THAT WE ITERATED FOR THE REQUIRED NUMBER OF STEPS.
#error TODO FOR EVERY FOR LOOK KEEP TRACK OF A COUNTER AND CHECK THAT WE ITERATED FOR THE REQUIRED NUMBER OF STEPS.
#error TODO FOR EVERY FOR LOOK KEEP TRACK OF A COUNTER AND CHECK THAT WE ITERATED FOR THE REQUIRED NUMBER OF STEPS.
#error TODO FOR EVERY FOR LOOK KEEP TRACK OF A COUNTER AND CHECK THAT WE ITERATED FOR THE REQUIRED NUMBER OF STEPS.
#error TODO FOR EVERY FOR LOOK KEEP TRACK OF A COUNTER AND CHECK THAT WE ITERATED FOR THE REQUIRED NUMBER OF STEPS.
#error TODO FOR EVERY FOR LOOK KEEP TRACK OF A COUNTER AND CHECK THAT WE ITERATED FOR THE REQUIRED NUMBER OF STEPS.
#error TODO FOR EVERY FOR LOOK KEEP TRACK OF A COUNTER AND CHECK THAT WE ITERATED FOR THE REQUIRED NUMBER OF STEPS.
#error TODO FOR EVERY FOR LOOK KEEP TRACK OF A COUNTER AND CHECK THAT WE ITERATED FOR THE REQUIRED NUMBER OF STEPS.
#error TODO FOR EVERY FOR LOOK KEEP TRACK OF A COUNTER AND CHECK THAT WE ITERATED FOR THE REQUIRED NUMBER OF STEPS.

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

}

