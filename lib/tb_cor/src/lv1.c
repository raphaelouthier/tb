/* Copyright 2025 Raphael Outhier - confidential - proprietary - no copy - no diffusion. */

#include <tb_cor/tb_cor.all.h>


/********************
 * Bid / ask curves *
 ********************/

#error TODO _bac_upd MUST UPDATE EVERY NAN CASE FOUND BEFORE @TIM
#error   THE FACT THAT THEY ARE NAN MEANS THAT THERE WAS NO BID/ASK UPDATE IN THEIR TIME SPAN -> WE CAN JUST TAKE THE OLDEST ONE AND PROPAGATE IT.
#error   PROCEDURE : FIND THE FIRST CASE BEFORE WITH A NON-NAN VALUE AND PROPAGATE IT ON THE RIGHT.
#error   IF NONE, THERE WAS NO BID/ASK UPDATE SINCE BEFORE BAC START SO WE CAN TAKE THE OLDER ONE.
static inline void _hst_bac_upd(
	tb_lv1_hst *hst,
	tb_lv1_upd *upd
);

/*************
 * Tick refer

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
	check(ns_dls_emp(&tck->upds_tck));
	check(tck->vol_stt == tck->vol_cur);
	check(tck->vol_stt == tck->vol_max);
	map_u64_rem(&hst->tcks, &tck->tcks);
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
	ns_dls_ini(&tck->upds_tck);
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
	tb_lv1_tck *bid = hst->tck_bid;
	tb_lv1_tck *ask = hst->tck_ask;

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
	tb_lv1_tck *tck,
	f64 vol,
	u64 tim
)
{
	nh_all__(tb_lv1_upd, upd);
	ns_dls_ini(&upd->upds_tck);
	ns_slsh_ia(&hst->upds_hst, &ups->upds_hst);
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
	tb_lv1_upd *upd
)
{
	check(hst->upds.next == &upd->upds);

	/* Remove from price list. */
	tb_lv1_tck *tck = upd->tck;
	check(!ns_dls_emp(&tck->upds_tck));
	check(tck->upds_tck.next == &tck->upds_tck);
	check(!ns_dls_emp(&upd->upds_tck));
	ns_dls_rmu(&upd->upds);
	tck->vol_stt = upd->vol;
	nh_fre_(upd);

	/* If the price has no more updates and a volume
	 * of 0, delete it. */
	if (tck->tim_max <= hmp_stt) {
		check(ns_dls_emp(&tck->upds_tck));
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
	u64 hmp_dim_tim,
	u64 hmp_dim_tck,
	u64 bac_nb
)
{
	assert(tim_res);
	assert(hmp_dim_tim);
	assert(hmp_dim_tck);
	assert(!(hmp_dim_tck & 1));

	/* Allocate. */
	nh_all__(tb_lv1_hst, hst);

	/* Reset structs. */
	ns_map_u64_ini(&hst->tcks);
	ns_slsh_ini(&hst->upds_hst);
	hst->upd_prc = 0;

	/* Save dimenstions. */
	hst->tim_res = tim_res;
	hst->hmp_dim_tim = hmp_dim_tim;
	hst->hmp_dim_tck = hmp_dim_tck;
	hst->bac_nb = bac_nb;
	hst->hmp_tim_spn = (hmp_dim_tim + 1) * tim_res;
	hst->bac_tim_spn = bac_nb * tim_res;

	/* Reset times. */
	hst->tim_cur = 0;
	hst->tim_hmp = 0;
	hst->tim_max = 0;
	hst->tim_end = 0;

	/* Reset ticks. */
	hst->tck_bid = 0;
	hst->tck_ask = 0;
	hst->tck_ref = hmp_dim_tck >> 1;

	/* Trigger a full heatmap generation next time. */
	hst->hmp_tck_min = 0;
	hst->hmp_tck_max = 0;

	/* No re-anchoring. Will be set at first prp call. */
	hst->hmp_shf_tim = 0;
	
	/* Allocate arrays. */
	hst->hmp = nh_all(sizeof(f64) * hmp_dim_tim * hmp_dim_tck);
	hst->bid = bac_nb ? nh_all(sizeof(f64) * bac_nb) : 0;
	hst->ask = bac_nb ? nh_all(sizeof(f64) * bac_nb) : 0;

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
		_upd_dtr(hst, upd);
	}

	/* Delete ticks with a non-null volume. */
	tb_lv1_tck *tck;
	map_fe(tck, &hst->tcks, tcks, u64, in) {
		check(tck->vol != 0);
		_tck_dtr(hst, tck);
	}
	assert(map_u64_emp(&hst->tcks));

	/* Free. */
	nh_fre(hst->hmp, sizeof(f64) * hmp_dim_tim * hmp_dim_tck);
	if (hst->bid) nh_fre(hst->bid, sizeof(f64) * hst->bac_nb);
	if (hst->ask) nh_fre(hst->ask, sizeof(f64) * hst->bac_nb);
	nh_fre_(hst):
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
	const u64 tim_hmp_new = _to_hmp_tim(tim_cur);/* TODO = (align up to anchor) */
	const u64 tim_hmp_prv = hst->tim_hmp;
	assert(!(tim_hmp_new % tim_res));
	assert(!(tim_hmp_prv % tim_res));
	assert(tim_hmp_new <= tim_hmp_prv);

	/*
	 * Report re-anchoring.
	 * Do not re-anchor for now, as we all updates
	 * until the current time to determine the
	 * re-anchoring height.
	 */
	const u64 hmp_shf_tim = (tim_hmp_prv - tim_hmp_new) / tim_res;
	if (hmp_shf_tim) {

#error TODO BAC MOV, add NANs to the new elements IN DEBUG MODE.

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
	const u8 has_bac = hst->bac_nb;
	u64 tim_max = hst->tim_max;
	assert(ini == (!tim_max));
	for (u64 upd_id = 0; upd_id < upd_nb; upd_id++) {

		/* Read arrays. */
		const u64 tim = (ini) ? 0 : tims[upd_id];
		const f64 prc = prcs[upd_id];
		const f64 vol = vols[upd_id];
	
		/* Ensure monotonic time. */
		assert(tim_max <= tim);
		tim_max = tim;

		/* Ensure time in prepared range. */
		assert(tim < tim_end);

		/* Get or create the corresponding tick level.
		 * If initial population, ensure that price level
		 * is created. */
		tb_lv1_tck *tck = _tck_get(hst, _prc_to_tck(prc), ini);

		/* If initial population, just report the start volume. */
		if (ini) {

			/* Report. */
			assert(vol);
			check(ns_dls_emp(&tck->upds_tck));
			check(tck->vol_stt == 0);
			check(tck->tim_max == 0);
			tck->vol_stt = vol;
			tck->vol_cur = vol;
			tck->vol_max = vol;

			/* Update the bid ask spread and bid-ask
			 * curves. */
			_hst_bas_upd(hst, tck);
			_hst_bac_upd(hst, tck);

		}

		/* Otherwise, add a volume update. */
		else {
			assert(tim);

			/* Ensure monotonicity. */
			assert(tim >= tck->tim_max);

			/* Create an update.
			 * It will be inserted in the active updates
			 * during processing. */
			tb_lv1_upd *upd = _upd_ctr(tck, vol, tim);

			/* Bid-ask spread will be updated during processing.
			 * Just update the bid-ask curves. */
			
			/* If the update is in the bid/ask curves range, update them. */
			if (has_bac && (bac_stt <= tim)) {
				_hst_bac_upd(hst, upd);
			}

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
	tb_lv1_hst *hst,
	u64 tim_cur
)
{

	/* Require a prepared history. */
	assert(hst->tim_cur);

	/* Determine the first node to update. */
	tb_lv1_upd *upd = hst->upd_prc;
	if (!upd) {
		ns_sls *sls = hst->upds_hst.next; 
		upd = hst->upd_prc = sls ? ns_cnt_of(sls, tb_lv1_upd, upds_hst) : 0;
	}

	/* Process all updates until the first >= tim_cur. 
	 * If no updates, stop here. */
	const u64 tim_cur = hst->tim_cur;
	while (upd && (upd->tim < tim_cur)) {

		/* Insert the update at the end of its price list. */
		tb_lbv1_tck *tck = upd->tck;
		ns_dls_ib(&tck->upds_tck, &upd->upds_tck);
		tck->vol_cur = upd->vol;

		/* Update the current bid-ask spread. */
		_hst_bas_upd(hst, tck);

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
	const u64 wrt_min = hmp_shf_tim + 1;

	/* Re-anchor if needed. */
	const u64 hmp_shf_tim = hst->hmp_shf_tim;
	check(col_nb);
	if (hst->hmp_shf_tim) {

		/* Determine the new reference price. */
		const u64 tck_cur = hst->tck_ref;
		const u64 tck_new = _tck_ref_cpt(hst);
		check(tck_cur >= (hst->hmp_dim_tck >> 1));
		check(tck_new >= (hst->hmp_dim_tck >> 1));

		/* Move heatmap data. */
#error TODO FILL WITH NANs IF IN DEBUG MODE.
		const s64 hmp_shf_tck = (s64) tck_new - (s64) tck_cur;
		_hmp_mov(hst, hst->hmp_shf_tim, hmp_shf_tck);

		/* Update references and current heatmap range. */
		hst->tck_ref = tck_ref; 
		hst->hmp_shf_tim = 0;
		hst->hmp_tck_min = hmp_tck_cur_min = tck_ref - hmp_tck_hln;
		hst->hmp_tck_max = hmp_tck_cur_max = tck_ref - hmp_tck_hln;

	}

	/* Iterate over all ticks (decreasing order). */
	tb_lv1_tck *tck = ns_map_sch_gs(&hst->tcks, hmp_tck_cur_min, u64, tb_lv1_tck, tcks); 
	check(hmp_tck_cur_max - hmp_tck_cur_min == hst->hmp_dim_tck);
	const u64 hmp_dim_tim = hst->hmp_dim_tim;
	for (u64 tck_id = hst->hmp_dim_tck; tck_id--;) {
		const u64 tck_val = hmp_tck_cur_min + tck_id;
		check(tck_val < hmp_tck_cur_max);
		check((!tck) || tck->tcks.val <= tck_val); 

		/* Determine the number of heatmap cells to fill in this row. */
		const u8 wrt_ful = !((hmp_tck_prv_min <= tck_val) && (tck_val < hmp_tck_prv_max));  
		const u64 wrt_nbr = prt_wrt ? hmp_dim_tim : wrt_min;

		/* Determine if we have tick data for this level. */
		const u8 has_dat = tck && (tck->tcks.val == tck_val);

		/* Fill heatmap cells. */
		_hmp_wrt_row(hst, tck_id, wrt_nbr, has_dat ? tck : 0); 

		/* If current price used, fetch the previous one. */
		if (has_dat) {
			check(tck);
			ns_mapn_u64 *prv = ns_map_fn_inr(&tck->tcks);
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
		_upd_dtr(hst, upd);

	}

	/* Update the list. */
	if (upd) {
		hst->upds_hst.next = &upd->upds_hst;
	} else {
		hst->upds_hst.next = hst->upds_hst.prev = 0;
	}

}
