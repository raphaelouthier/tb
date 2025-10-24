/* Copyright 2025 Raphael Outhier - confidential - proprietary - no copy - no diffusion. */

#include <tb_cor/tb_cor.all.h>

/*
 * Construct and return an empty history with a
 * current time of 0.
 * If @bac is set, generate the bid-ask curve.
 */
tb_lv1_hst *tb_lv1_ctr(
	u64 tim_anc,
	u64 hmp_tim_nb,
	u64 hmp_prc_nb,
	u64 bac_nb
)
{
	assert(tim_anc);
	assert(hmp_tim_nb);
	assert(hmp_prc_nb);

	/* Construct. */
	ns_all__(tb_lv1_hst, hst);
	ns_map_u64_ini(&hst->prcs);
	hst->tim_anc = tim_anc;
	hst->hmp_tim_nb = hmp_tim_nb;
	hst->hmp_prc_nb = hmp_prc_nb;
	hst->bac_nb = bac_nb;
	hst->hmp_tim_spn = (hmp_tim_nb + 1) * tim_anc;
	hst->bac_tim_spn = bac_nb * tim_anc;

	/* Reset times. */
	hst->tim_cur = 0;
	hst->tim_hmp = 0;
	hst->tim_max = 0;
	hst->tim_end = 0;

	/* No re-anchoring. Will be set at first prp call. */
	hst->rnc_nb_tim = 0;
	
	/* Allocate. */
	hst->hmp = ns_all(sizeof(f64) * hmp_tim_nb * hmp_prc_nb);
	hst->bid = bac_nb ? ns_all(sizeof(f64) * bac_nb) : 0;
	hst->ask = bac_nb ? ns_all(sizeof(f64) * bac_nb) : 0;

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

	/* Delete all data. */
	tb_lv1_prc *prc;
	map_fe(prc, &hst->prcs, prcs, u64, in) {
		_prc_dtr(hst, prc);
	}
	assert(map_u64_emp(&hst->prcs));

	/* Free. */
	ns_fre(hst->hmp, sizeof(f64) * hmp_tim_nb * hmp_prc_nb);
	if (hst->bid) ns_fre(hst->bid, sizeof(f64) * hst->bac_nb);
	if (hst->ask) ns_fre(hst->ask, sizeof(f64) * hst->bac_nb);
	ns_fre_(hst):
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
	const u64 tim_anc = hst->tim_anc;
	const u64 tim_hmp_new = _to_hmp_tim(tim_cur);/* TODO = (align up to anchor) */
	const u64 tim_hmp_prv = hst->tim_hmp;
	assert(!(tim_hmp_new % tim_anc));
	assert(!(tim_hmp_prv % tim_anc));
	assert(tim_hmp_new <= tim_hmp_prv);

	/*
	 * Report re-anchoring.
	 * Do not re-anchor for now, as we all updates
	 * until the current time to determine the
	 * re-anchoring height.
	 */
	const u64 rnc_nb_tim = (tim_hmp_prv - tim_hmp_new) / tim_anc;
	if (rnc_nb_tim) {

#error UPDATE THE BACs, add NANs to the new elements.

		/* Count re-anchoring. */
		SAFE_ADD(hst->rnc_nb_tim, rnc_nb_tim);
		
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
		const f64 tck = prcs[upd_id];
		const f64 vol = vols[upd_id];
	
		/* Ensure monotonic time. */
		assert(tim_max <= tim);
		tim_max = tim;

		/* Ensure time in prepared range. */
		assert(tim < tim_end);

		/* Get or create the corresponding price level.
		 * If initial population, ensure that price level
		 * is created. */
		tb_lv1_prc *prc = _prc_get(tck, ini);

		/* If initial population, just report the start volume. */
		if (ini) {
			assert(vol);
			check(ns_dls_emp(&prc->upds_pre));
			check(ns_dls_emp(&prc->upds_pst));
			check(prc->vol_stt == 0);
			check(prc->tim_end == 0);
			prc->vol_stt = vol;
			prc->vol_cur = vol;
		}

		/* Otherwise, add a volume update. */
		else {
			assert(tim);

			/* Ensure monotonicity. */
			assert(tim >= prc->tim_end);

			/* Create an update, and insert it in the
			 * post list for now, as updates will be
			 * processed later on. */
			ns_all__(tb_lv1_upd, upd);
			ns_dls_ib(&prc->upds_pst, &upd->upds);
			upd->vol = vol;
			upd->tim = tim;
			
			/* If the order is in the bid/ask curves range, update them. */
			if (has_bac && (bac_stt <= tim)) {
#error TODO _bac_upd MUST UPDATE EVERY NAN CASE FOUND BEFORE @TIM
#error   THE FACT THAT THEY ARE NAN MEANS THAT THERE WAS NO BID/ASK UPDATE IN THEIR TIME SPAN -> WE CAN JUST TAKE THE OLDEST ONE AND PROPAGATE IT.
#error   PROCEDURE : FIND THE FIRST CASE BEFORE WITH A NON-NAN VALUE AND PROPAGATE IT ON THE RIGHT.
#error   IF NONE, THERE WAS NO BID/ASK UPDATE SINCE BEFORE BAC START SO WE CAN TAKE THE OLDER ONE.
				_bac_upd(hst, prc, upd);
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

	/* Process all price levels in increasing order,
	 * determine the max bid and min ask prices. */
	const u64 tim_cur = hst->tim_cur;
	tb_lv1_prc *prc;
	u8 saw_bid = 0;
	u8 saw_ask = 0;
	f64 bst_bid = 0;
	f64 bst_ask = 0;
	map_fe(prc, &hst->prcs, prcs, u64, in) {

		/* Move all post updates < @tim_cur in the pre list. */
		tb_lv1_upd *upd;
		ns_dls_fes(upd, &prc->upds_pst, upds) {
			assert(upd->tim <= prc->tim_end);

			/* If update is after heatmap end, stop. */
			if (upd->tim >= tim_cur) break;

			/* If update is before heatmap end, transfer
			 * it in the pre list and update the current volume. */
			ns_dls_rmu(&upd->upds);
			ns_dls_ib(&prc->upds_pre, &upd->upds);
			prc->vol_cur = upd->vol;

		}

		/* If volume at that level, use it to compute bid / ask prices. */
		const f64 vol_cur = prc->vol_cur;
		if (vol_cur == 0) continue;

		/* Determine if bid or ask. */
		const u8 is_bid = vol_cur < 0;

		/* If bid price and ask seen at lesser price, report it
		 * as it may be an orderbook corruption symptom.
		 * (or a careless marketplace symptom). */
		if (is_bid && saw_ask) {
			debug("WARNING : Saw a bid price greater than an ask price.\n");
		}

		/* If bid price, replace as best bid price.
		 * If ask price, set as best ask price only if
		 * first ask price seen. */
		if (is_bid) {
			bst_bid = prc->prcs.val;
		} else {
			if (!saw_ask) {
				bst_ask = prc->prcs.val;
			}
		}

		/* Report ask observation. */
		saw_ask |= !is_bid;
		saw_bid |= is_bid;

	}

	/* Re-anchor if needed. */
	if (hst->rnc_nb_tim) {

		/* Determine the re-anchoring price. */
		#error TODO.

		#error TODO re-anchor.

	}

	#error TODO INCORPORATE PROCESSED DATA INTO THE HEATMAP
	#error TODO WE NEED TO PROCESS hst->rnc_nb_tim + 1 columns.	

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

	/* Clean all price levels. */
	assert(hst->tim_hmp > hst->hmp_tim_spn);
	const u64 hmp_stt = hst->tim_hmp - hst->hmp_tim_spn;
	tb_lv1_prc *prc;
	map_fe(prc, &hst->prcs, prcs, u64, in) {

		/* Purge all orders before @hmp_stt. */
		tb_lv1_upd *upd;
		ns_dls_fes(upd, &prc->upds_pre, upds) {
			assert(upd->tim <= prc->tim_end);

			/* If update is after heatmap start, stop. */
			if (upd->tim > hmp_stt) break;

			/* If update is before or at heatmap start,
			 * delete it and save its volume as start volume. */
			prc->vol_stt = upd->vol;
			ns_dls_rmu(&upd->upds);
			_upd_fre(upd);
		}

		/* If the price has no more updates and a volume
		 * of 0, delete it. */
		if (ns_dls_emp(&prc->upds_pre)) {
			assert(prc->vol_stt == prc->vol_cur);
			if ((prc->vol_stt == 0) && (ns_dls_emp(&prc->upds_pst))) {
				_prc_dtr(hst, prc);
			}
		}

	}

}
