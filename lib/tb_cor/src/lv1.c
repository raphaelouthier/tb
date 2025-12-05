/* Copyright 2025 Raphael Outhier - confidential - proprietary - no copy - no diffusion. */

#include <tb_cor/tb_cor.all.h>

/*******
 * Log *
 *******/

tb_lib_log_def(cor, lv1);

/*******************
 * Bid / ask utils *
 *******************/

/*
 * If @vol is a bid, return 0.
 * If it is an ask, return 1.
 * It should not be null.
 */
static inline u8 _is_ask(
	f64 vol
)
{
	assert(vol != (f64) 0);
	return vol > 0;
}	

/*
 * Return the value for a best bid tick, null or not.
 */
static inline u64 _bst_bid_val(
	tb_lv1_tck *tck
) {return (tck) ? tck->tcks.val : 0;}

/*
 * Return the value for a best ask tick, null or not.
 */
static inline u64 _bst_ask_val(
	tb_lv1_tck *tck
) {return (tck) ? tck->tcks.val : (u64) -1;}

/******************
 * Price <=> tick *
 ******************/

/*
 * Price -> tick.
 */
static inline u64 _prc_to_tck(
	tb_lv1_hst *hst,
	f64 prc
)
{
	check(prc > 0);
	return (u64) (f64) ((f64) prc * (f64) hst->tck_rat + 0.1); 
}

/*
 * Tick -> price.
 */
static inline f64 _tck_to_prc(
	tb_lv1_hst *hst,
	u64 tck
)
{
	check(tck != 0);
	check(tck != (u64) -1);
	return (f64) tck * hst->prc_rat;
}

/*******************
 * Time conversion *
 *******************/

/*
 * Get the heatmap end time corresponding to
 * @tim_cur.
 */
static inline u64 _to_hmp_end_tim(
	tb_lv1_hst *hst,
	u64 tim_cur
)
{
	/* Round up to the resolution. */
	const u64 res = hst->tim_res;
	tim_cur += (res - 1);
	return tim_cur - (tim_cur % res);
}

/***************
 * Propagation *
 ***************/

/*
 * Propagate bid curve values from @bac_aid + 1
 * to @prp_aid.
 */
static inline void _bid_prp(
	tb_lv1_hst *hst,
	u64 bac_aid,
	u64 bid_aid,
	u64 prp_aid,
	tb_lv1_tck *bst_bid
)
{
	if (bac_aid <= prp_aid) {
		const u64 prp_val = _bst_bid_val(bst_bid);
		const u64 stt_aid = (bac_aid <= bid_aid) ? bid_aid + 1 : bac_aid;
		check(bac_aid <= stt_aid);
		for (u64 aid = stt_aid; aid <= prp_aid; aid++) {
			check(aid >= bac_aid);
			check(hst->bid_crv[aid - bac_aid] == (u64) -1);
			hst->bid_crv[aid - bac_aid] = prp_val;
		}	
	}
}

/*
 * Propagate ask curve values from @bac_aid + 1
 * to @prp_aid.
 */
static inline void _ask_prp(
	tb_lv1_hst *hst,
	u64 bac_aid,
	u64 ask_aid,
	u64 prp_aid,
	tb_lv1_tck *bst_ask
)
{
	if (bac_aid <= prp_aid) {
		const u64 prp_val = _bst_ask_val(bst_ask);
		const u64 stt_aid = (bac_aid <= ask_aid) ? ask_aid + 1 : bac_aid;
		check(bac_aid <= stt_aid);
		if (stt_aid <= prp_aid) {
			tb_lv1_log("prp : bst_ask : val %U, aid [%U, %U], tim [%U, %U[.\n",
				prp_val,
				stt_aid - bac_aid, prp_aid - bac_aid,
				stt_aid * hst->tim_res, (prp_aid + 1) * hst->tim_res
			);
		}
		for (u64 aid = stt_aid; aid <= prp_aid; aid++) {
			check(aid >= bac_aid);
			check(hst->ask_crv[aid - bac_aid] == (u64) -1);
			hst->ask_crv[aid - bac_aid] = prp_val;
		}	
	}
}

/********************
 * Bid / ask spread *
 ********************/

/*
 * Invalidate either a current or max best bid/ask.
 */
#define BAS_INV(tck, vol_nam, bst_bid_nam, bst_ask_nam) \
\
	/* No consecutive invalidations. */ \
	check(!inv); \
\
	/* Determine if @tck is the best bid or ask
	 * at the specified time.
	 * If best bid, search for the first lower price 
	 * level with a non-null volume. */ \
	if (tck == prv_bst_bid) { \
		tb_lv1_tck *oth = tck; \
		/* debug("bid inv.\n"); */ \
		bid_upd = 1; \
		while (1) { \
			ns_mapn_u64 *prv = ns_map_u64_fn_inr(&oth->tcks); \
			if (!prv) { \
				hst->bst_bid_nam = 0; \
				break; \
			} \
			oth = ns_cnt_of(prv, tb_lv1_tck, tcks); \
			if (oth->vol_nam > 0) { \
				debug("warning : found an ask price below the previous best bid price.\n"); \
			} \
			if (oth->vol_nam < 0) { \
				hst->bst_bid_nam = oth; \
				break; \
			} \
		} \
	} \
\
	/* If best ask, search for the first upper price 
	 * level with a non-null volume. */  \
	else if (tck == prv_bst_ask) { \
		tb_lv1_tck *oth = tck; \
		ask_upd = 1; \
		/* debug("ask inv.\n"); */ \
		while (1) { \
			ns_mapn_u64 *nxt = ns_map_u64_fn_in(&oth->tcks); \
			if (!nxt) { \
				hst->bst_ask_nam = 0; \
				break; \
			} \
			oth = ns_cnt_of(nxt, tb_lv1_tck, tcks); \
			if (oth->vol_nam < 0) { \
				debug("warning : found a bid price above the previous best ask price.\n"); \
			} \
			if (oth->vol_nam > 0) { \
				hst->bst_ask_nam = oth; \
				break; \
			} \
		} \
	} \
	inv = 1;

/*
 * Generate the code to update the best bid/ask spread
 * at a specified time.
 */
#define BAS_UPD(tck, vol_nam, bst_bid_nam, bst_ask_nam) \
\
	/* Read the volume at the specified time, read
	 * best bis/ask. @tck which was just modified could
	 * have been a previous best so we cannot yet
	 * make checks on their state. */ \
	const f64 vol = tck->vol_nam; \
	tb_lv1_tck *const prv_bst_bid = hst->bst_bid_nam; \
	tb_lv1_tck *const prv_bst_ask = hst->bst_ask_nam; \
	check((prv_bst_bid != prv_bst_ask) || (prv_bst_bid == 0)); \
	_unused_ u8 bid_upd = 0; \
	_unused_ u8 ask_upd = 0; \
\
	/* Invalidation flags. */ \
	u8 inv = 0; \
\
	/* If @tck's volume is null, just invalidate it. */ \
	if (vol == 0) { \
		BAS_INV(tck, vol_nam, bst_bid_nam, bst_ask_nam); \
		check(1 && ((hst->bst_bid_nam == 0) || (hst->bst_bid_nam->vol_nam < 0))); \
		check(2 && ((hst->bst_ask_nam == 0) || (hst->bst_ask_nam->vol_nam > 0))); \
		check((hst->bst_bid_nam != hst->bst_ask_nam) || (hst->bst_bid_nam == 0)); \
\
	} \
\
	/* If not a null volume, just select as new best bid \
	 * or ask if relevant. */ \
	else { \
\
		/* Determine if bid or ask, read the tick. */ \
		const u8 is_ask = _is_ask(vol); \
		/* debug("typ %d %s.\n", vol, is_ask ? "ask" : "bid"); */ \
		const u64 tck_val = tck->tcks.val; \
\
		/* If is bid but was best ask, or
		 * if is ask but was best bid, invalidate. */ \
		if ( \
			(is_ask && (tck == hst->bst_bid_nam)) || \
			((!is_ask) && (tck == hst->bst_ask_nam)) \
		) { \
			/* debug("inv oth\n"); */ \
			BAS_INV(tck, vol_nam, bst_bid_nam, bst_ask_nam); \
		} \
\
		/* If bid, if price is superior to best bid, select as best bid. */ \
		if (!is_ask) { \
			/* debug("cdt %p "#bst_bid_nam" %p %U.\n", prv_bst_bid, (prv_bst_bid) ? prv_bst_bid->tcks.val : 0); */ \
			if ((!prv_bst_bid) || (prv_bst_bid->tcks.val < tck_val)) { \
				/* debug("bst %p "#bst_bid_nam" %U %d.\n", tck, tck->tcks.val, tck->vol_nam); */ \
				bid_upd = 1; \
				hst->bst_bid_nam = tck; \
			} \
		} \
\
		/* If ask, if price is inferior to best 
		 * ask, select as best ask. */ \
		else { \
			if ((!prv_bst_ask) || (prv_bst_ask->tcks.val > tck_val)) { \
				/* debug("bst %p "#bst_ask_nam" %U %d.\n", tck, tck->tcks.val, tck->vol_nam); */ \
				ask_upd = 1; \
				hst->bst_ask_nam = tck; \
			} \
		} \
\
		/* We should not find both bid and ask null (hence not equal). */ \
		check(3 && ((hst->bst_bid_nam == 0) || (hst->bst_bid_nam->vol_nam < 0))); \
		check(4 && ((hst->bst_ask_nam == 0) || (hst->bst_ask_nam->vol_nam > 0))); \
		check(hst->bst_bid_nam != hst->bst_ask_nam); \
\
	}

/*
 * Update the bid-ask spread after the update of
 * @tck at the current time.
 */ 
static inline void _hst_bas_cur_upd(
	tb_lv1_hst *hst,
	tb_lv1_tck *tck
)
{
	BAS_UPD(tck, vol_cur, bst_cur_bid, bst_cur_ask);
}

/*
 * Update the bid-ask spread after the update of
 * @tck at the maximal time.
 */ 
static inline void _hst_bas_max_upd(
	tb_lv1_hst *hst,
	tb_lv1_tck *tck
)
{

	/* First, update the max best bid and ask prices. */
	assert(hst->bac_nb);
	BAS_UPD(tck, vol_max, bst_max_bid, bst_max_ask);

	/* Verify that the update logic makes sense. */
	check(bid_upd == (prv_bst_bid != hst->bst_max_bid));
	check(ask_upd == (prv_bst_ask != hst->bst_max_ask));

	/*
	 * When we add initial orders, there is a window
	 * where we add orders before the bid-ask curves.
	 * In this case, we have nothing to update in the
	 * bid-ask curves.
	 * Just ensure that during init, max and current
	 * bests are the same.
	 */ 
	const u64 tck_tim = tck->tim_max; 
	if (!tck_tim) {
		assert(hst->bst_max_bid == hst->bst_cur_bid);
		assert(hst->bst_max_ask == hst->bst_cur_ask);
		return;
	}
	const u64 tim_res = hst->tim_res;
	const u64 bac_aid = hst->bac_aid;
	const u64 bid_aid = hst->bid_aid;
	const u64 ask_aid = hst->ask_aid;
	const u64 prp_aid = (tck_tim - 1) / tim_res;
	const u64 new_aid = tck_tim / tim_res; 
	check(prp_aid <= new_aid);
	check(bid_aid <= new_aid);
	check(ask_aid <= new_aid);
	check((new_aid < bac_aid) || (new_aid - bac_aid) < hst->bac_nb);
	check((prp_aid < bac_aid) || (prp_aid - bac_aid) < hst->bac_nb);
	check(bid_aid < bid_aid + 1);
	check(ask_aid < ask_aid + 1);
	if (new_aid < bac_aid) {
		return;
	}

	/*
	 * If any change in best bid or ask, we need to update
	 * the bid / ask curves.
	 */

	/* If no bid or ask update, complete. */
	if (!(bid_upd || ask_upd)) return;
	check(bac_aid <= new_aid);
	u64 *const bid_crv = hst->bid_crv;
	u64 *const ask_crv = hst->ask_crv;

	/*
	 * If best bid update :
	 */
	if (bid_upd) { 

		/* Propagate previous best value until max time - 1. */
		_bid_prp(hst, bac_aid, bid_aid, prp_aid, prv_bst_bid);

		/* Select the best bid for the current aid.
		 * If the previous value affected the current
		 * cell, then set the best of current cell best
		 * and new value.
		 * If we're writing to a new cell, only use
		 * the new value. */
		const u64 cel_bst = bid_crv[new_aid - bac_aid];
		const u64 crt_bst = _bst_bid_val(hst->bst_max_bid);
		if (
			(prp_aid != new_aid) ||
			(cel_bst < crt_bst)
		) {
			bid_crv[new_aid - bac_aid] = crt_bst;
		}

		/* Save the current aid. */
		hst->bid_aid = new_aid;

	}

	/*
	 * If best ask update :
	 */
	if (ask_upd) {
		debug("ask upd %U %U %U %U.\n", bac_aid, ask_aid - bac_aid, prp_aid - bac_aid, _bst_ask_val(prv_bst_ask));

		/* Propagate ask values. */
		_ask_prp(hst, bac_aid, ask_aid, prp_aid, prv_bst_ask);

		/* Select the best ask for the current aid.
		 * If the previous value affected the current
		 * cell, then set the best of current cell best
		 * and new value.
		 * If we're writing to a new cell, only use
		 * the new value. */
		const u64 cel_bst = ask_crv[new_aid - bac_aid];
		const u64 crt_bst = _bst_ask_val(hst->bst_max_ask);
		check(((cel_bst == (u64) -1) || (cel_bst >= crt_bst)) == (cel_bst >= crt_bst));
		if (
			(prp_aid != new_aid) ||
			(cel_bst > crt_bst)
		) {
			ask_crv[new_aid - bac_aid] = crt_bst;
		}

		/* Save the current aid. */
		hst->ask_aid = new_aid;

	}

}

/********************
 * Bid / ask curves *
 ********************/

/*
 * Move the bid-ask curves.
 * Caused by an adjustment of the current time large
 * enough to cause re-anchoring during preparation.
 */
static inline void _hst_bac_mov(
	tb_lv1_hst *hst,
	u64 shf
)
{
	assert(shf);
	

	/* If no bid / ask curve, nothing to do. */
	const u64 len = hst->bac_nb; 
	assert(len);

	tb_lv1_log("bac_mov : %U -> %U.\n", hst->bac_aid, hst->bac_aid + shf);

	/* Determine move attributes. */	
	const u8 ovf = (shf >= len);
	const u64 shf_bnd = ovf ? len : shf;
	if (ovf) {
		tb_lv1_log("bac_mov : ovf : %U >= %U.\n", shf, len);
	}

	/* Move old bac elements. */
	u64 *const bid_crv = hst->bid_crv;
	u64 *const ask_crv = hst->ask_crv;
	check(shf_bnd <= len);
	if (shf_bnd < len) {
		const u64 cpy_nb = len - shf_bnd;
		tb_lv1_log("bac_mov : cpy : [%U, %U[ -> [0, %U[.\n", shf_bnd, shf_bnd + cpy_nb, cpy_nb);
		ns_mem_cpy(bid_crv, bid_crv + shf_bnd, cpy_nb * sizeof(u64));
		ns_mem_cpy(ask_crv, ask_crv + shf_bnd, cpy_nb * sizeof(u64));
	}

	/* Set new values to -1 in debug mode. */
	#ifdef DEBUG
	tb_lv1_log("bac_mov : rst : [%U, %U[.\n", len - shf_bnd, len);
	ns_mem_set(bid_crv + len - shf_bnd, 0xff, shf_bnd * sizeof(u64)); 
	ns_mem_set(ask_crv + len - shf_bnd, 0xff, shf_bnd * sizeof(u64)); 
	check(((u64 *) bid_crv)[len - shf_bnd] == (u64) -1);
	check(((u64 *) ask_crv)[len - shf_bnd] == (u64) -1);
	#endif

	/* Report the new start of the bid ask curve. */
	SAFE_ADD(hst->bac_aid, shf);

}

/***********
 * Heatmap *
 ***********/

/*
 * Print the heatmap.
 */
static inline void _hmp_log(
	tb_lv1_hst *hst
)
{
	for (u64 tck_idx = hst->hmp_dim_tck; tck_idx--;) {
		debug("%U (%U) : ", tck_idx, hst->hmp_tck_min + tck_idx);
		for (u64 tim_idx = 0; tim_idx < hst->hmp_dim_tim; tim_idx++) {
			debug("%.3d ", hst->hmp[tim_idx * hst->hmp_dim_tck + tck_idx]);
		}
		debug("\n");
	}
	debug("\n");
}

/*
 * Move the heatmap.
 * Caused by a re-anchor during generation.
 * @shf_tck > 0 -> move data down.
 */
static inline void _hst_hmp_mov(
	tb_lv1_hst *hst,
	u64 shf_tim,
	s64 shf_tck
)
{
	assert(shf_tim);

	/* If shift of more than the dimensions,
	 * no data should be reused. */ 
	const u8 mov_dwn = (shf_tck >= 0);
	const u64 dim_tim = hst->hmp_dim_tim;
	const u64 dim_tck = hst->hmp_dim_tck;
	const u64 shf_abs = mov_dwn ? (u64) shf_tck : (u64) -shf_tck;
	if (
		(shf_tim >= dim_tim) ||
		(shf_abs >= dim_tck)
	) {
		tb_lv1_log("rnc hmp rst.\n");
		/* In debug mode, fill with NANs.
		 * Otherwise, just keep as is. */
		#ifdef DEBUG
		ns_mem_set(hst->hmp, 0xff, dim_tim * dim_tck * sizeof(f64));
		#endif
		return;
	}

	debug("pre :\n");
	_hmp_log(hst);

	debug("shf %U %I.\n", shf_tim, shf_tck);
	#if 0
	/* Bruteforce. Should only be used for debug. */
	check((s64) shf_tim * (s64) dim_tck >= (s64) -shf_tck);
	const u64 cpy_off = shf_tim * dim_tck + (u64) shf_tck;
	debug("off %U %U.\n", cpy_off, dim_tim * dim_tck - cpy_off);
	ns_mem_cpy(hst->hmp, hst->hmp + cpy_off, (dim_tim * dim_tck - cpy_off) * sizeof(f64));

	#else
	/* More fine-grained copy : iterate over
	 * each column and copy just what is needed in this column. */
	f64 *dst = hst->hmp + (mov_dwn ? 0 : -shf_tck);
	f64 *src = hst->hmp + shf_tim * dim_tck + (mov_dwn ? shf_tck : 0);
	const u64 len = sizeof(f64) * (dim_tck - shf_abs);
	for (u64 col_id = 0; col_id < dim_tim - shf_tim; col_id++) {
		ns_mem_cpy(dst, src, len);
		dst += dim_tck;
		src += dim_tck;
	}
	#endif

	debug("post :\n");
	_hmp_log(hst);

}

/*
 * Write the @wrt_nb first cells of the heatmap row
 * at index @row_id of @hst.
 * If @tck is non-null, it contains the volume updates
 * that should be used to compute cell values.
 * Otherwise, we have no volume data, and cells must
 * be filled with 0s.
 */
static inline void _hmp_wrt_row(
	tb_lv1_hst *hst,
	u64 row_id,
	u64 wrt_nb,
	tb_lv1_tck *tck
)
{

	debug("wrt_dat %U %U.\n", row_id, wrt_nb);
	/*
	 * Utils.
	 */

	/* Get heatmap location. */
	#define HMP_LOC(col, row) hmp[(col) * dim_tck + (row)]

	/* Get the next update. */
	#define _upd_nxt(upd) ({ \
		ns_dls *__nxt = upd->upds_tck.next; \
		(__nxt == &tck->upds_tck) ? 0 : ns_cnt_of(__nxt, tb_lv1_upd, upds_tck); \
	})

	/* Get the previous update. */
	#define _upd_prv(upd) ({ \
		ns_dls *__prv = upd->upds_tck.prev; \
		(__prv == &tck->upds_tck) ? 0 : ns_cnt_of(__prv, tb_lv1_upd, upds_tck); \
	})

	/* Cache heatmap. */
	f64 *hmp = hst->hmp;
	const u64 dim_tck = hst->hmp_dim_tck;

	/* Cache current time. */
	const u64 tim_cur = hst->tim_cur;

	/* Verify the current tick. */

	/* Read the current volume.
	 * If data, check that the last update matches
	 * the current volume. */
	f64 vol_cur = 0;
	f64 vol_stt = 0;
	tb_lv1_upd *upd = 0;
	if (tck) {
		vol_stt = tck->vol_stt;
		vol_cur = tck->vol_cur;
		if (ns_dls_emptype(&tck->upds_tck, upd, tb_lv1_upd, upds_tck)) {
			upd = 0;	
			assert(vol_cur == vol_stt);
		} else {
			assert(vol_cur == upd->vol);
		}
	}

	/* If no data, just write the current volume. */
	if (!upd) {
		u64 wrt_cnt = 0;
		for (u64 col_id = hst->hmp_dim_tim; (col_id--) && wrt_nb--;) {
			wrt_cnt++;
			debug("  rs %U %U %d.\n", row_id, col_id, vol_cur); 
			HMP_LOC(col_id, row_id) = vol_cur;	
		}
		debug("wrt_cnt %U.\n", wrt_cnt);
		return;
	}

	/* Write all cells. */
	const u64 tim_res = hst->tim_res;
	check(!(hst->tim_hmp % tim_res)); 
	check(!(hst->hmp_tim_spn % tim_res)); 
	const u64 aid_hmp = (hst->tim_hmp - hst->hmp_tim_spn) / tim_res;  
	u64 wrt_cnt = 0;
	for (u64 col_id = hst->hmp_dim_tim; (col_id--) && wrt_nb--;) {
		check((upd) || (vol_stt == vol_cur));
		debug("col %U, [%U, %U[, cur %U.\n", col_id, tim_res * (aid_hmp + col_id), tim_res * (aid_hmp + col_id + 1), tim_cur);

		/* If no update anymore, just write the start volume. */
		if (!upd) {
			debug("  end %U %U %d.\n", row_id, col_id, vol_stt); 
			HMP_LOC(col_id, row_id) = vol_stt;	
			wrt_cnt++;
			continue;
		}

		debug("  UPD0 %U %d.\n", upd->tim, upd->vol);

		/* The previous update should be in or before
		 * this cell.
		 * The next update should be after this cell. */
		const u64 aid_col = aid_hmp + col_id;
		const u64 aid_upd = upd->tim / tim_res;
		check(aid_upd <= aid_col);
		#ifdef DEBUG
		tb_lv1_upd *nxt = _upd_nxt(upd);
		if (nxt) debug("  NXT0 %U %d, %U %U.\n", nxt->tim, nxt->vol, nxt->tim / tim_res, aid_col);
		check((!nxt) || ((nxt->tim / tim_res) > aid_col));
		#endif

		/* If the previous update is before this cell,
		 * just write the current volume. */
		if (aid_upd < aid_col) {
			debug("  bef %U %U %d.\n", row_id, col_id, vol_stt); 
			HMP_LOC(col_id, row_id) = vol_cur;	
			wrt_cnt++;
			continue;
		}

		/* Iterate over all updates in this cell and
		 * determine a compound volume. */
		const u64 cel_stt = tim_res * aid_col;
		const u64 cel_end = tim_res * (aid_col + 1); 
		u64 upd_nxt = (tim_cur < cel_end) ? tim_cur : cel_end;
		check(upd_nxt > cel_stt);
		const u64 cel_tim_ttl = upd_nxt - cel_stt;
		f64 wgt_sum = 0;
		u64 tim_ttl = 0;
		while (1) {
			check((upd) || (vol_stt == vol_cur));

			if (upd) debug("  UPDI %U %d.\n", upd->tim, upd->vol);
			else debug("  UPDI null.\n");

			/* Compute the current calculation attrs.
			 * If no more updates, use the current volume
			 * for the rest of the time.
			 * If an update exists, use its volume until
			 * its start or the start of the cell. */
			u64 upd_tim = 0;
			f64 upd_vol = vol_cur;
			if (upd) {
				upd_tim = upd->tim; 
				upd_vol = upd->vol;
			}
			check(upd_nxt <= cel_end);
			check(upd_tim <= upd_nxt);
			if (upd_tim < cel_stt) upd_tim = cel_stt; 

			/* Incorporate into the average. */
			u64 upd_dur = upd_nxt - upd_tim;
			wgt_sum += (upd_vol * (f64) upd_dur);
			SAFE_ADD(tim_ttl, upd_dur); 

			/* Report new next update time. */
			upd_nxt = upd_tim;

			/* If no more update, or if update before this cell, stop. */
			if ((!upd) || (upd->tim < cel_stt)) break;

			/* Fetch the previous update. */
			upd = _upd_prv(upd);

			/* Update the current volume. */
			vol_cur = (upd) ? upd->vol : vol_stt;

		}

		/* Verify that the total time matches the cell's total active time. */
		check(tim_ttl == cel_tim_ttl);

		/* Compute the average. */
		const f64 vol_avg = wgt_sum / (f64) tim_ttl;
		debug("  avg %U %U %d (%d / %U).\n", row_id, col_id, vol_avg, wgt_sum, tim_ttl); 
		HMP_LOC(col_id, row_id) = vol_avg;	

		wrt_cnt++;

	}

	debug("wrt_cnt %U.\n", wrt_cnt);

}

/******************
 * Tick internals *
 ******************/

/*
 * Delete @tck.
 */
static inline void _tck_dtr(
	tb_lv1_hst *hst,
	tb_lv1_tck *tck
)
{
	check(ns_dls_empty(&tck->upds_tck));
	check(tck->vol_stt == tck->vol_cur);
	check(tck->vol_stt == tck->vol_max);
	ns_map_u64_rem(&hst->tcks, &tck->tcks);
	nh_fre_(tck);
}

/*
 * If a tick at @val exists in @hst, return it.
 * If @ini is set, it must not exist.
 * If it does not exist, create one with a volume of 0,
 * and return it.
 */
static inline tb_lv1_tck *_tck_get(
	tb_lv1_hst *hst,
	u64 val,
	u8 ini
)
{

	/* Search. */
	tb_lv1_tck *tck = ns_map_sch(&hst->tcks, val, u64, tb_lv1_tck, tcks); 

	/* If found, forward. */
	if (tck) {
		assert(!ini);
		return tck;
	}

	/* Otherwise, allocate. */
	nh_all_(tck);
	assert(!ns_map_u64_put(&hst->tcks, &tck->tcks, val));
	ns_dls_init(&tck->upds_tck);
	tck->vol_stt = 0;
	tck->vol_cur = 0;
	tck->vol_max = 0;
	tck->tim_max = 0;
	return tck;

}

/*
 * Return the tick reference at the current time.
 * If both best bid and ask exist, use the average.
 * If only a best bid or ask exists, use it.
 * If the orderbook is currently empty, use the
 * previous reference.
 */
static inline u64 _tck_ref_cpt(
	tb_lv1_hst *hst
)
{

	/* Read the current up-to-date bid and ask. */
	tb_lv1_tck *bid = hst->bst_cur_bid;
	tb_lv1_tck *ask = hst->bst_cur_ask;

	/* Choose the current reference price based on their
	 * existence. */
	u64 tck_ref = 0;
	if (bid && ask) {
		tck_ref = (bid->tcks.val + ask->tcks.val) / 2; 
		//assert(0, "tck_ref bid %U ask %U val %U.\n", bid->tcks.val, ask->tcks.val, tck_ref);
	} else if (!(bid || ask)) {
		tck_ref = hst->tck_ref;
		debug("orderbook empty.\n");
	} else if (bid) {
		tck_ref = bid->tcks.val;
		debug("no asks in orderbook.\n");
	} else {
		check(ask);
		tck_ref = ask->tcks.val;
		debug("no bids in orderbook.\n");
	}

	/* Check integrity and complete. */
	const u64 tck_min = hst->hmp_dim_tck >> 1;
	if (tck_ref < tck_min) {
		tck_ref = tck_min;
		debug("price fell too low, offseting the heatmap.\n");
	}
	return tck_ref;
}

/********************
 * Update internals *
 ********************/

/*
 * Construct and return an update in @tck at volume @vol
 * and time @tim. 
 * Update @tck accordingly.
 */
static inline tb_lv1_upd *_upd_ctr(
	tb_lv1_hst *hst,
	tb_lv1_tck *tck,
	f64 vol,
	u64 tim
)
{
	nh_all__(tb_lv1_upd, upd);
	ns_dls_init(&upd->upds_tck);
	ns_slsh_push(&hst->upds_hst, &upd->upds_hst);
	upd->tck = tck;
	upd->vol = vol;
	upd->tim = tim;
	tck->tim_max = tim;
	tck->vol_max = vol;
	return upd;
}

/*
 * Delete @upd from @hst.
 * It must be @hst's oldest update.
 * If its tick has no more updates and a current volume
 * of 0, delete it as well.
 * Callable from cleanup only.
 */
static inline void _upd_dtr(
	tb_lv1_hst *hst,
	tb_lv1_upd *upd,
	u64 hmp_stt
)
{
	check(hst->upds_hst.oldest == &upd->upds_hst);

	/* Remove from price list. */
	tb_lv1_tck *tck = upd->tck;
	check(!ns_dls_empty(&tck->upds_tck));
	check(upd->upds_tck.prev == &tck->upds_tck);
	check(!ns_dls_empty(&upd->upds_tck));
	ns_dls_rmu(&upd->upds_tck);
	assert(ns_slsh_pull(&hst->upds_hst) == &upd->upds_hst);
	tck->vol_stt = upd->vol;
	nh_fre_(upd);

	/* If the tick is past its max time, has no more updates,
	 * and a resting volume of 0, delete it. */
	if (
		(tck->tim_max <= hmp_stt) &&
		(ns_dls_empty(&tck->upds_tck)) &&
		(tck->vol_stt == 0)
	) {
		_tck_dtr(hst, tck);
	}

}

/*******
 * API *
 *******/

/*
 * Construct and return an empty history with a
 * current time of 0.
 * If @bac is set, generate the bid-ask curve.
 */
tb_lv1_hst *tb_lv1_ctr(
	u64 tim_res,
	u64 tck_rat,
	u64 hmp_dim_tim,
	u64 hmp_dim_tck,
	u64 bac_nb
)
{
	assert(tck_rat >= 1);
	assert(tim_res);
	assert(hmp_dim_tim);
	assert(hmp_dim_tck);
	assert(!(hmp_dim_tck & 1));

	/* Allocate. */
	nh_all__(tb_lv1_hst, hst);

	/* Reset structs. */
	ns_map_u64_ini(&hst->tcks);
	ns_slsh_init(&hst->upds_hst);
	hst->upd_prc = 0;

	/* Save dimenstions. */
	hst->tim_res = tim_res;
	hst->hmp_dim_tim = hmp_dim_tim;
	hst->hmp_dim_tck = hmp_dim_tck;
	hst->bac_nb = bac_nb;
	hst->hmp_tim_spn = (hmp_dim_tim) * tim_res;
	hst->bac_tim_spn = bac_nb * tim_res;
	hst->tck_rat = (f64) tck_rat;
	hst->prc_rat = (f64) 1 / (f64) tck_rat;

	/* Reset times. */
	hst->tim_cur = 0;
	hst->tim_rnc = 0;
	hst->tim_hmp = 0;
	hst->tim_max = 0;
	hst->tim_end = 0;
	hst->tim_prc = 0;

	/* Reset ticks. */
	hst->bst_cur_bid = 0;
	hst->bst_cur_ask = 0;

	/* Start with a heatmap lower-anchored at 0. */
	hst->tck_ref = hmp_dim_tck >> 1;

	/* Trigger a full heatmap generation next time. */
	hst->hmp_tck_min = 0;
	hst->hmp_tck_max = 0;

	/* Reset bad metadata. */
	hst->bst_max_bid = 0;
	hst->bst_max_ask = 0;
	hst->bac_aid = 0;
	hst->bid_aid = 0;
	hst->ask_aid = 0;

	/* No re-anchoring. Will be set at first prp call. */
	hst->hmp_shf_tim = 0;
	
	/* Allocate arrays. */
	hst->hmp = nh_all(sizeof(f64) * hmp_dim_tim * hmp_dim_tck);
	hst->bid_crv = bac_nb ? nh_all(sizeof(u64) * bac_nb) : 0;
	hst->ask_crv = bac_nb ? nh_all(sizeof(u64) * bac_nb) : 0;

	/* Complete. */
	return hst;

}

/*
 * Delete @hst.
 */
void tb_lv1_dtr(
	tb_lv1_hst *hst
)
{

	/* Prepare at time where all orders are out of heatmap. */
	tb_lv1_prp(hst, hst->tim_end + hst->hmp_tim_spn);
	
	/* Process. Will move all orders in tick lists. */
	tb_lv1_prc(hst);

	/* Cleanup. Will purge all updates and only leave resting
	 * ticks. */
	tb_lv1_cln(hst);

	/* Delete all resting ticks. */
	tb_lv1_tck *tck;
	const u64 hmp_stt = hst->tim_hmp - hst->hmp_tim_spn;
	ns_map_fe(tck, &hst->tcks, tcks, u64, in) {
		assert(tck->tim_max <= hmp_stt);
		assert(tck->vol_max != 0);
		_tck_dtr(hst, tck);
	}

	/* Verify everything is deleted. */
	assert(hst->upds_hst.oldest == 0);
	assert(ns_map_u64_emp(&hst->tcks));

	/* Free. */
	nh_fre(hst->hmp, sizeof(f64) * hst->hmp_dim_tim * hst->hmp_dim_tck);
	if (hst->bid_crv) nh_fre(hst->bid_crv, sizeof(u64) * hst->bac_nb);
	if (hst->ask_crv) nh_fre(hst->ask_crv, sizeof(u64) * hst->bac_nb);
	nh_fre_(hst);
}

/*
 * Update the current time to @tim_cur,
 * update the bid-ask curves to reflect it.
 * Do not update price levels nor the bitmap.
 * Allows adding orders up to (<)
 * @tim_cur + @hst->bad_tim_spn.
 */
void tb_lv1_prp(
	tb_lv1_hst *hst,
	u64 tim_cur
)
{
	assert(tim_cur);

	assert(tim_cur >= hst->tim_cur);

	tb_lv1_log("prp : cur : %U -> %U : +%U.\n", hst->tim_cur, tim_cur, tim_cur - hst->tim_cur);
	tb_lv1_log("prp : end : %U -> %U : +%U.\n", hst->tim_end, tim_cur + hst->bac_tim_spn, tim_cur + hst->bac_tim_spn - hst->tim_end);

	/* If no time adjustment, nothing to do. */
	if (tim_cur == hst->tim_cur) return;

	/* Update the current time. */
	hst->tim_cur = tim_cur;

	/* Determine the new heatmap end time. */ 
	const u64 tim_res = hst->tim_res;
	const u64 tim_hmp_new = _to_hmp_end_tim(hst, tim_cur);
	const u64 tim_hmp_prv = hst->tim_hmp;
	assert(!(tim_hmp_new % tim_res));
	assert(!(tim_hmp_prv % tim_res));
	assert(tim_hmp_new >= tim_hmp_prv);

	tb_lv1_log("prp : hmp : %U -> %U.\n", tim_hmp_prv, tim_hmp_new);

	/*
	 * Report re-anchoring.
	 * Do not re-anchor for now, as we need all updates
	 * until the current time to determine the
	 * re-anchoring height.
	 */
	check(!(tim_hmp_prv % tim_res));
	check(!(tim_hmp_new % tim_res));
	const u64 hmp_shf_tim = (tim_hmp_new - tim_hmp_prv) / tim_res;
	if (hmp_shf_tim) {
	
		tb_lv1_log("prp : rnc : %U : %U -> %U.\n", hmp_shf_tim, hst->hmp_shf_tim, hst->hmp_shf_tim + hmp_shf_tim);

		/* Move the bid-ask curves. */
		if (hst->bac_nb) { 
			_hst_bac_mov(hst, hmp_shf_tim);
		}

		/* Count re-anchoring. */
		SAFE_ADD(hst->hmp_shf_tim, hmp_shf_tim);
		
		/* Update the heatmap end time. */
		hst->tim_hmp = tim_hmp_new;

	}

	/* Update the bid/ask end time. */
	hst->tim_end = hst->tim_cur + hst->bac_tim_spn;

}

/*
 * Add @upd_nb volume updates to @hst.
 * If @tims is null, volumes are considered to be
 * start volumes only and are not registered as updates,
 * and in this case, all price levels must be new. 
 * If any, all created price updates are added in the
 * pre list if they are below (<=) the current time and
 * in the post list otherwise.
 * If bid/ask curves are supported, all times must be
 * in their (shared) range.
 * Otherwise, all times must be below (<=) the current
 * time.
 */
void tb_lv1_add(
	tb_lv1_hst *hst,
	u64 upd_nb,
	u64 *tims,
	f64 *prcs,
	f64 *vols
)
{
	assert(upd_nb);

	tb_lv1_log("add : %U%s.\n", upd_nb, (tims) ? "" : " : ini");

	/* Require a prepared history. */
	assert((!hst->tim_cur) == (!tims));

	/* Add all updates. */
	const u8 ini = (!tims);
	const u8 has_bac = !!hst->bac_nb;
	u64 tim_max = hst->tim_max;
	assert((!ini) || (!tim_max));
	const u64 tim_end = hst->tim_end;
	for (u64 upd_id = 0; upd_id < upd_nb; upd_id++) {

		/* Read arrays. */
		const u64 tim = (ini) ? 0 : tims[upd_id];
		const f64 prc = prcs[upd_id];
		const f64 vol = vols[upd_id];
		const u64 tck_val = _prc_to_tck(hst, prc);
		check(tck_val == _prc_to_tck(hst, _tck_to_prc(hst, tck_val)));

		tb_lv1_log("add : ord : %U %d(%U) %d.\n", tim, prc, tck_val, vol);
	
		/* Ensure time is monotonic and in prepared range. */
		assert(tim_max <= tim);
		assert((tim < tim_end) || ((!tim) && (!tim_end)));
		tim_max = tim;

		/* Get or create the corresponding tick level.
		 * If initial population, ensure that price level
		 * is created. */
		tb_lv1_tck *tck = _tck_get(hst, tck_val, ini);

		/* If initial population, just report the start volume. */
		if (ini) {

			/* Report. */
			assert(vol);
			check(ns_dls_empty(&tck->upds_tck));
			check(tck->vol_stt == 0);
			check(tck->tim_max == 0);
			tck->vol_stt = vol;
			tck->vol_cur = vol;
			tck->vol_max = vol;

			/* Update the current bid-ask spread. */
			_hst_bas_cur_upd(hst, tck);

		}

		/* Otherwise, add a volume update. */
		else {
			assert(tim);

			/* Ensure monotonicity. */
			assert(tim >= tck->tim_max);

			/* Create an update.
			 * It will be inserted in the active updates
			 * during processing. */
			_upd_ctr(hst, tck, vol, tim);


		}

		/* If the update is in the bid/ask curves range, update them. */
		if (has_bac) {

			/* Now that an update was created at the max time,
			 * update the bid-ask spread at max time. */
			_hst_bas_max_upd(hst, tck);

		}

	}

	/* Update the maximal time. */
	hst->tim_max = tim_max;
	assert(ini == (!tim_max));

}

/*
 * Process newly added updates, update all price levels,
 * update the heatmap.
 */ 
void tb_lv1_prc(
	tb_lv1_hst *hst
)
{

	tb_lv1_log("prc %U.\n", hst->hmp_shf_tim);

	/* Require a prepared history. */
	assert(hst->tim_cur);
	assert(hst->tim_end);

	/* Propagate best bid / ask until last cells of
	 * bid / ask curves. */ 
	if (hst->bac_nb) {
		const u64 bac_aid = hst->bac_aid;
		const u64 bid_aid = hst->bid_aid;
		const u64 ask_aid = hst->ask_aid;
		const u64 prp_aid = (hst->tim_end - 1) / hst->tim_res;
		assert(bid_aid <= prp_aid);
		assert(ask_aid <= prp_aid);
		_bid_prp(hst, bac_aid, bid_aid, prp_aid, hst->bst_max_bid);
		_ask_prp(hst, bac_aid, ask_aid, prp_aid, hst->bst_max_ask);
		hst->bid_aid = prp_aid;
		hst->ask_aid = prp_aid;
	}

	/* Determine the re-anchor time. */
	u64 tim_rnc = (u64) -1;
	if (hst->hmp_shf_tim) {
		tim_rnc = hst->tim_hmp - hst->tim_res;
		assert(hst->tim_rnc < tim_rnc);
		assert(tim_rnc < hst->tim_cur);
	}

	/* Determine the first node to update. */
	tb_lv1_upd *upd = hst->upd_prc;
	if (!upd) {
		ns_sls *sls = hst->upds_hst.oldest; 
		upd = hst->upd_prc = sls ? ns_cnt_of(sls, tb_lv1_upd, upds_hst) : 0;
	}

	/* Process all updates until the first >= tim_cur. 
	 * If no updates, stop here. */
	const u64 tim_cur = hst->tim_cur;
	u64 tck_ref_new = 0;
	while (upd && (upd->tim < tim_cur)) {
		const u64 upd_tim = upd->tim;
		check(upd_tim >= hst->tim_prc);
		hst->tim_prc = upd_tim;

		/* If we found an update after the reanchor time,
		 * we should reanchor now, compute the new tick ref. */
		if (upd_tim >= tim_rnc) { 
			tck_ref_new = _tck_ref_cpt(hst);
			tim_rnc = (u64) -1;
		}

		/* Insert the update at the end of its price list. */
		tb_lv1_tck *tck = upd->tck;
		ns_dls_ib(&tck->upds_tck, &upd->upds_tck);
		tck->vol_cur = upd->vol;

		/* Update the current bid-ask spread. */
		_hst_bas_cur_upd(hst, tck);

		/* Fetch the successor. */
		ns_sls *sls = upd->upds_hst.next; 
		upd = hst->upd_prc = (sls ? ns_cnt_of(sls, tb_lv1_upd, upds_hst) : 0);

	}
	
	/* If reanchor was needed but no update was after it,
	 * reanchor now. */
	if (tim_rnc != (u64) -1) {
		tck_ref_new = _tck_ref_cpt(hst);
	}

	/* Cache the previous heatmap price range. */
	const u64 hmp_tck_prv_min = hst->hmp_tck_min; 
	const u64 hmp_tck_prv_max = hst->hmp_tck_max; 
	const u64 hmp_tck_hln = hst->hmp_dim_tck >> 1;
	u64 hmp_tck_cur_min = hmp_tck_prv_min; 
	u64 hmp_tck_cur_max = hmp_tck_prv_max; 

	/* Minimal number of columns to write. */
	const u64 hmp_shf_tim = hst->hmp_shf_tim;
	const u64 wrt_min = hmp_shf_tim + 1;

	/* Re-anchor if needed. */
	if (hmp_shf_tim) {

		/* New tick ref should have been computed. */
		assert(tck_ref_new != (u64) -1);

		/* Determine the new reference price. */
		const u64 tck_ref_cur = hst->tck_ref;
		check(tck_ref_cur >= (hst->hmp_dim_tck >> 1));
		check(tck_ref_new >= (hst->hmp_dim_tck >> 1));

		/* Update references and current heatmap range. */
		hst->tck_ref = tck_ref_new; 
		hst->hmp_shf_tim = 0;
		hst->hmp_tck_min = hmp_tck_cur_min = tck_ref_new - hmp_tck_hln;
		hst->hmp_tck_max = hmp_tck_cur_max = tck_ref_new + hmp_tck_hln;

		tb_lv1_log("rnc : %U -> %U,%U,%U.\n", tck_ref_cur, tck_ref_new, hst->hmp_tck_min, hst->hmp_tck_max);

		/* Move heatmap data. */
		const s64 hmp_shf_tck = (s64) tck_ref_new - (s64) tck_ref_cur;
		_hst_hmp_mov(hst, hmp_shf_tim, hmp_shf_tck);

	}
	check(hst->hmp_tck_max == hst->hmp_tck_min + hst->hmp_dim_tck);

	/* Iterate over all ticks (decreasing order). */
	tb_lv1_tck *tck = ns_map_sch_gs(&hst->tcks, hmp_tck_cur_max, u64, tb_lv1_tck, tcks); 
	check(hmp_tck_cur_max - hmp_tck_cur_min == hst->hmp_dim_tck);
	const u64 hmp_dim_tim = hst->hmp_dim_tim;
	for (u64 row_id = hst->hmp_dim_tck; row_id--;) {
		const u64 tck_val = hmp_tck_cur_min + row_id;
		check(tck_val < hmp_tck_cur_max);
		check((!tck) || tck->tcks.val <= tck_val); 

		/* Determine the number of heatmap cells to fill in this row. */
		const u8 wrt_ful = !((hmp_tck_prv_min <= tck_val) && (tck_val < hmp_tck_prv_max));  
		const u64 wrt_nbr = wrt_ful ? hmp_dim_tim : wrt_min;

		/* Determine if we have tick data for this level. */
		const u8 has_dat = tck && (tck->tcks.val == tck_val);

		/* Fill heatmap cells. */
		_hmp_wrt_row(hst, row_id, wrt_nbr, has_dat ? tck : 0); 

		/* If current price used, fetch the previous one. */
		if (has_dat) {
			check(tck);
			ns_mapn_u64 *prv = ns_map_u64_fn_inr(&tck->tcks);
			tck = (prv) ? ns_cnt_of(prv, tb_lv1_tck, tcks) : 0;
		}

	}

	debug("Res : \n");
	_hmp_log(hst);

}

/*
 * Delete all price updates that are too old to appear
 * in the heatmap anymore.
 */
void tb_lv1_cln(
	tb_lv1_hst *hst
)
{

	/* Require a prepared history. */
	assert(hst->tim_cur);

	/* Purge all updates before @hmp_stt. */
	assert(hst->tim_hmp > hst->hmp_tim_spn);
	const u64 hmp_stt = hst->tim_hmp - hst->hmp_tim_spn;
	tb_lv1_upd *upd;
	ns_slsh_fes(upd, &hst->upds_hst, upds_hst) {

		/* If update is after heatmap start, stop. */
		if (upd->tim > hmp_stt) break;

		/* Delete. */
		_upd_dtr(hst, upd, hmp_stt);

	}

	/* Update the list. */
	if (upd) {
		hst->upds_hst.oldest = &upd->upds_hst;
	} else {
		hst->upds_hst.oldest = hst->upds_hst.youngest = 0;
	}

}
