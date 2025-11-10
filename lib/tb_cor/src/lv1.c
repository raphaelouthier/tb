/* Copyright 2025 Raphael Outhier - confidential - proprietary - no copy - no diffusion. */

#include <tb_cor/tb_cor.all.h>

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

/*****************
 * Price to tick *
 *****************/

static inline u64 _prc_to_tck(
	tb_lv1_hst *hst,
	f64 prc
)
{
	assert(prc > 0);
	return (u64) (f64) ((f64) prc * (f64) hst->prc_cff); 
}

/*******************
 * Time conversion *
 *******************/

/*
 * Get the hashmap end time corresponding to
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

/********************
 * Bid / ask spread *
 ********************/

/*
 * Generate the code to update the best bid/ask spread
 * at a specified time.
 */
#define BAS_UPD(vol_nam, bst_bid_nam, bst_ask_nam) \
	/* Read the volume at the specified time. */ \
	const f64 vol = tck->vol_nam; \
	tb_lv1_tck *const prv_bst_bid = hst->bst_bid_nam; \
	tb_lv1_tck *const prv_bst_ask = hst->bst_ask_nam; \
	_unused_ u8 bid_upd = 0; \
	_unused_ u8 ask_upd = 0; \
\
	/* If @tck's volume is null, complex case. */ \
	if (vol == 0) { \
\
		/* Determine if @tck is the best bid or ask \
		 * at the specified time. */ \
		check((prv_bst_bid != prv_bst_ask) || (prv_bst_bid == 0)); \
\
		/* If best bid, search for the first lower price 
		 * level with a non-null volume. */  \
		if (tck == prv_bst_bid) { \
			bid_upd = 1; \
			while (1) { \
				ns_mapn_u64 *prv = ns_map_u64_fn_inr(&tck->tcks); \
				if (!prv) { \
					hst->bst_bid_nam = 0; \
					break; \
				} \
				tck = ns_cnt_of(prv, tb_lv1_tck, tcks); \
				if (tck->vol_nam > 0) { \
					debug("warning : found an ask price below the previous best bid price.\n"); \
				} \
				if (tck->vol_nam < 0) { \
					hst->bst_bid_nam = tck; \
					break; \
				} \
			} \
		} \
\
		/* If best ask, search for the first upper price 
		 * level with a non-null volume. */  \
		else if (tck == prv_bst_ask) { \
			ask_upd = 1; \
			while (1) { \
				ns_mapn_u64 *nxt = ns_map_u64_fn_in(&tck->tcks); \
				if (!nxt) { \
					hst->bst_ask_nam = 0; \
					break; \
				} \
				tck = ns_cnt_of(nxt, tb_lv1_tck, tcks); \
				if (tck->vol_nam < 0) { \
					debug("warning : found a bid price above the previous best ask price.\n"); \
				} \
				if (tck->vol_nam > 0) { \
					hst->bst_ask_nam = tck; \
					break; \
				} \
			} \
		} \
\
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
		const u64 tck_val = tck->tcks.val; \
\
		/* If bid, if price is superior to best 
		 * bid, select as best bid. */ \
		if (!is_ask) { \
			if ((!prv_bst_bid) || (prv_bst_bid->tcks.val < tck_val)) { \
				bid_upd = 1; \
				hst->bst_bid_nam = tck; \
			} \
		} \
\
		/* If ask, if price is inferior to best 
		 * ask, select as best ask. */ \
		else { \
			if ((!prv_bst_ask) || (prv_bst_ask->tcks.val > tck_val)) { \
				ask_upd = 1; \
				hst->bst_ask_nam = tck; \
			} \
		} \
\
		/* We should not find both bid and ask null (hence not equal). */ \
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
	BAS_UPD(vol_cur, bst_cur_bid, bst_cur_ask);
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
	BAS_UPD(vol_max, bst_max_bid, bst_max_ask);

	/*
	 * If any change in best bid or ask, we need to update
	 * the bid / ask curves.
	 */
	check(!(bid_upd && ask_upd));
	check(bid_upd == (prv_bst_bid != hst->bst_max_bid));
	check(ask_upd == (prv_bst_ask != hst->bst_max_ask));

	/* If no bid or ask update, complete. */
	if (!(bid_upd || ask_upd)) return;
	const u64 tim_res = hst->tim_res;
	const u64 bac_aid = hst->bac_aid;
	const u64 bid_aid = hst->bid_aid;
	const u64 ask_aid = hst->ask_aid;
	const u64 prp_aid = (tck->tim_max - 1) / tim_res; 
	const u64 new_aid = tck->tim_max / tim_res; 
	check(prp_aid <= new_aid);
	check(bid_aid <= prp_aid);
	check(ask_aid <= prp_aid);
	u64 *const bid_crv = hst->bid_crv;
	u64 *const ask_crv = hst->ask_crv;

	/*
	 * If best bid update :
	 */
	if (bid_upd) {

		/* Propagate previous best value until max time - 1. */
		if (bac_aid <= prp_aid) {
			const u64 prp_val = _bst_bid_val(prv_bst_bid);
			const u64 stt_aid = (bac_aid < bid_aid) ? bid_aid : bac_aid;
			for (u64 aid = stt_aid; aid <= prp_aid; aid++) {
				bid_crv[aid - bac_aid] = prp_val;
			}	
		}

		/* Select the best bid for the current aid.
		 * If the previous value affected the current
		 * cell, then set the best of current cell best
		 * and new value.
		 * If we're writing to a new cell, only use
		 * the new value. */
		const u64 cel_bst = bid_crv[new_aid - bac_aid];
		const u64 crt_bst = _bst_bid_val(hst->bst_max_bid);
		check((cel_bst == (u64) -1) == (prp_aid != new_aid));
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

		/* Propagate previous best value until max time - 1. */
		if (bac_aid <= prp_aid) {
			const u64 prp_val = _bst_ask_val(prv_bst_ask);
			const u64 stt_aid = (bac_aid < ask_aid) ? ask_aid : bac_aid;
			for (u64 aid = stt_aid; aid <= prp_aid; aid++) {
				ask_crv[aid - bac_aid] = prp_val;
			}	
		}

		/* Select the best ask for the current aid.
		 * If the previous value affected the current
		 * cell, then set the best of current cell best
		 * and new value.
		 * If we're writing to a new cell, only use
		 * the new value. */
		const u64 cel_bst = ask_crv[new_aid - bac_aid];
		const u64 crt_bst = _bst_ask_val(hst->bst_max_ask);
		check((cel_bst == (u64) -1) == (prp_aid != new_aid));
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
	assert(hst->bac_nb);

	/* If no bid / ask curve, nothing to do. */
	const u64 len = hst->bac_nb; 
	if (!len) return;

	/* Determine move attributes. */	
	const u8 ovf = (shf >= len);
	const u64 rem_nb = ovf ? 0 : len - shf;

	/* Move old bac elements. */
	u64 *const bid_crv = hst->bid_crv;
	u64 *const ask_crv = hst->ask_crv;
	if (rem_nb) {
		ns_mem_cpy(bid_crv, bid_crv + shf, rem_nb * sizeof(u64));
		ns_mem_cpy(ask_crv, ask_crv + shf, rem_nb * sizeof(u64));
	}

	/* Set new values to -1 in debug mode. */
	#ifdef DEBUG
	ns_mem_set(bid_crv + len - shf, 0xff, shf * sizeof(u64)); 
	ns_mem_set(ask_crv + len - shf, 0xff, shf * sizeof(u64)); 
	check(((u64 *) bid_crv)[len - shf] == (u64) -1);
	check(((u64 *) ask_crv)[len - shf] == (u64) -1);
	#endif

}

/***********
 * Heatmap *
 ***********/

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
		/* In debug mode, fill with NANs.
		 * Otherwise, just keep as is. */
		#ifdef DEBUG
		ns_mem_set(hst->hmp, 0xff, dim_tim * dim_tck * sizeof(f64));
		#endif
		return;
	}

	#if 0
	/* Bruteforce. Should only be used for debug. */
	const u64 cpy_off = shf_tim * dim_tck + shf_tck;
	ns_mem_cpy(hst->hmp + cpy_off, hst->hmp, dim_tim * dim_tck * sizeof(f64) - cpy_off);

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

}

/*
 * Write the @wrt_nb first cells of the hashmap row
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

	/* Cache hashmap. */
	f64 *hmp = hst->hmp;
	const u64 dim_tck = hst->hmp_dim_tck;

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
		for (u64 col_id = hst->hmp_dim_tim; (col_id--), wrt_nb--;) {
			HMP_LOC(col_id, row_id) = vol_cur;	
		}
		return;
	}

	/* Write all cell. */
	const u64 tim_res = hst->tim_res;
	check(!(hst->tim_hmp % tim_res)); 
	check(!(hst->hmp_tim_spn % tim_res)); 
	const u64 aid_hmp = (hst->tim_hmp - hst->hmp_tim_spn) / tim_res;  
	u64 aid_upd = upd->tim / tim_res;
	for (u64 col_id = hst->hmp_dim_tim; (col_id--), wrt_nb--;) {
		check((upd) || (vol_stt == vol_cur));

		/* If no update anymore, just write the start volume. */
		if (!upd) {
			HMP_LOC(col_id, row_id) = vol_stt;	
			continue;
		}

		/* The previous update should be in or before
		 * this cell.
		 * The next update should be after this cell. */
		const u64 aid_col = aid_hmp + col_id;
		check(aid_upd <= aid_col);
		#ifdef DEBUG
		tb_lv1_upd *nxt = _upd_nxt(upd);
		check((!nxt) || ((upd->tim / tim_res) > aid_col));
		#endif

		/* If the previous update is before this cell,
		 * just write the current volume. */
		if (aid_upd < aid_col) {
			HMP_LOC(col_id, row_id) = vol_cur;	
			continue;
		}

		/* Iterate over all updates in this cell and
		 * determine a compound volume. */
		u64 cel_tim = tim_res * aid_col;
		u64 upd_end = tim_res * (aid_col + 1); 
		f64 wgt_sum = 0;
		u64 tim_ttl = 0;
		while (1) {
			check((upd) || (vol_stt == vol_cur));

			/* Compute the current calculation attrs.
			 * If no more updates, use the current volume
			 * for the rest of the time.
			 * If an update exists, use its volume until
			 * its start or the start of the cell. */
			u64 upd_stt = 0;
			f64 upd_vol = vol_cur;
			if (upd) {
				upd_stt = upd->tim; 
				upd_vol = upd->vol;
			}
			check(upd_stt <= upd_end);
			if (upd_stt < cel_tim) upd_stt = cel_tim; 

			/* Incorporate into the average. */
			u64 upd_tim = upd_end - upd_stt;
			wgt_sum += (upd_vol * (f64) upd_tim);
			SAFE_ADD(tim_ttl, upd_tim); 

			/* If no more update, stop. */
			if (!upd) break;

			/* Fetch the previous update. */
			upd = _upd_prv(upd);

			/* Update the current volume. */
			vol_cur = (upd) ? upd->vol : vol_stt;

		}
		check(tim_ttl == tim_res);

		/* Compute the average. */
		const f64 vol_avg = wgt_sum / (f64) tim_ttl;
		HMP_LOC(col_id, row_id) = vol_avg;	

	}

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
 * If the orderbook is currently empty, use the
 * previous bid price.
 * If both best bid and ask exist, use the average.
 * If only a best bid or ask exists, use it.
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
	upd->upds_tck.next = 0;
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
	check(tck->upds_tck.next == &tck->upds_tck);
	check(!ns_dls_empty(&upd->upds_tck));
	ns_dls_rmu(&upd->upds_tck);
	tck->vol_stt = upd->vol;
	nh_fre_(upd);

	/* If the price has no more updates and a volume
	 * of 0, delete it. */
	if (tck->tim_max <= hmp_stt) {
		check(ns_dls_empty(&tck->upds_tck));
		check(tck->vol_stt == tck->vol_cur);
		check(tck->vol_stt == tck->vol_max);
		if (tck->vol_stt == 0) {
			_tck_dtr(hst, tck);
		}
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
	f64 prc_res,
	u64 hmp_dim_tim,
	u64 hmp_dim_tck,
	u64 bac_nb
)
{
	assert(prc_res >= 0.001);
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
	hst->hmp_tim_spn = (hmp_dim_tim + 1) * tim_res;
	hst->bac_tim_spn = bac_nb * tim_res;
	hst->prc_cff = (f64) 1 / prc_res;

	/* Reset times. */
	hst->tim_cur = 0;
	hst->tim_hmp = 0;
	hst->tim_max = 0;
	hst->tim_end = 0;

	/* Reset ticks. */
	hst->bst_cur_bid = 0;
	hst->bst_cur_ask = 0;
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

	/* Delete all orders. */
	tb_lv1_upd *upd;
	ns_slsh_fe(upd, &hst->upds_hst, upds_hst) {
		_upd_dtr(hst, upd, hst->tim_max);
	}

	/* Delete ticks with a non-null volume. */
	tb_lv1_tck *tck;
	ns_map_fe(tck, &hst->tcks, tcks, u64, in) {
		check(tck->vol_max != 0);
		_tck_dtr(hst, tck);
	}
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
 */
void tb_lv1_prp(
	tb_lv1_hst *hst,
	u64 tim_cur
)
{
	assert(tim_cur);

	assert(tim_cur >= hst->tim_cur);

	/* Update the current time. */
	hst->tim_cur = tim_cur;

	/* Determine the new hashmap end time. */ 
	const u64 tim_res = hst->tim_res;
	const u64 tim_hmp_new = _to_hmp_end_tim(hst, tim_cur);
	const u64 tim_hmp_prv = hst->tim_hmp;
	assert(!(tim_hmp_new % tim_res));
	assert(!(tim_hmp_prv % tim_res));
	assert(tim_hmp_new <= tim_hmp_prv);

	/*
	 * Report re-anchoring.
	 * Do not re-anchor for now, as we need all updates
	 * until the current time to determine the
	 * re-anchoring height.
	 */
	check(!(tim_hmp_prv % tim_res));
	check(!(tim_hmp_new % tim_res));
	const u64 hmp_shf_tim = (tim_hmp_prv - tim_hmp_new) / tim_res;
	if (hmp_shf_tim) {

		/* Move the bid-ask curves. */
		if (hst->bac_nb) { 
			_hst_bac_mov(hst, hmp_shf_tim);
		}

		/* Count re-anchoring. */
		SAFE_ADD(hst->hmp_shf_tim, hmp_shf_tim);
		
		/* Update the hashmap end time. */
		hst->tim_hmp = tim_hmp_new;

		/* Update the bid/ask end time. */
		hst->tim_end = hst->tim_cur + hst->bac_tim_spn;

	}

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

	/* Require a prepared history. */
	assert(hst->tim_cur);

	/* Add all updates. */
	const u8 ini = (!tims);
	const u8 has_bac = !!hst->bac_nb;
	u64 tim_max = hst->tim_max;
	assert(ini == (!tim_max));
	const u64 tim_end = hst->tim_end;
	for (u64 upd_id = 0; upd_id < upd_nb; upd_id++) {

		/* Read arrays. */
		const u64 tim = (ini) ? 0 : tims[upd_id];
		const f64 prc = prcs[upd_id];
		const f64 vol = vols[upd_id];
	
		/* Ensure time is monotonic and in prepared range. */
		assert(tim_max <= tim);
		assert(tim < tim_end);
		tim_max = tim;

		/* Get or create the corresponding tick level.
		 * If initial population, ensure that price level
		 * is created. */
		tb_lv1_tck *tck = _tck_get(hst, _prc_to_tck(hst, prc), ini);

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

	/* Require a prepared history. */
	assert(hst->tim_cur);

	/* Determine the first node to update. */
	tb_lv1_upd *upd = hst->upd_prc;
	if (!upd) {
		ns_sls *sls = hst->upds_hst.oldest; 
		upd = hst->upd_prc = sls ? ns_cnt_of(sls, tb_lv1_upd, upds_hst) : 0;
	}

	/* Process all updates until the first >= tim_cur. 
	 * If no updates, stop here. */
	const u64 tim_cur = hst->tim_cur;
	while (upd && (upd->tim < tim_cur)) {

		/* Insert the update at the end of its price list. */
		tb_lv1_tck *tck = upd->tck;
		ns_dls_ib(&tck->upds_tck, &upd->upds_tck);
		tck->vol_cur = upd->vol;

		/* Update the current bid-ask spread. */
		_hst_bas_cur_upd(hst, tck);

		/* Fetch the successor. */
		ns_sls *sls = upd->upds_hst.next; 
		upd = sls ? ns_cnt_of(sls, tb_lv1_upd, upds_hst) : 0;

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
	if (hst->hmp_shf_tim) {

		/* Determine the new reference price. */
		const u64 tck_ref_cur = hst->tck_ref;
		const u64 tck_ref_new = _tck_ref_cpt(hst);
		check(tck_ref_cur >= (hst->hmp_dim_tck >> 1));
		check(tck_ref_new >= (hst->hmp_dim_tck >> 1));

		/* Move heatmap data. */
		const s64 hmp_shf_tck = (s64) tck_ref_new - (s64) tck_ref_cur;
		_hst_hmp_mov(hst, hst->hmp_shf_tim, hmp_shf_tck);

		/* Update references and current heatmap range. */
		hst->tck_ref = tck_ref_new; 
		hst->hmp_shf_tim = 0;
		hst->hmp_tck_min = hmp_tck_cur_min = tck_ref_new - hmp_tck_hln;
		hst->hmp_tck_max = hmp_tck_cur_max = tck_ref_new - hmp_tck_hln;

	}
	check(hst->hmp_tck_max == hst->hmp_tck_min + hst->hmp_dim_tck);

	/* Iterate over all ticks (decreasing order). */
	tb_lv1_tck *tck = ns_map_sch_gs(&hst->tcks, hmp_tck_cur_min, u64, tb_lv1_tck, tcks); 
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
	ns_slsh_fe(upd, &hst->upds_hst, upds_hst) {

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
