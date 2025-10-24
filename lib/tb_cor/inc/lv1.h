/* Copyright 2025 Raphael Outhier - confidential - proprietary - no copy - no diffusion. */

/*
 * The level 1 library provides ways to 
 * represent and operate on level 1 data.
 *
 * It is designed around the following use case : 
 * - fast incorporation and storage of burst of level 1
 *   events read from storage.
 * - determination of a current time.
 * - efficient generation and update of a time x price-level
 *   indexed heatmap of events prior to the current time.
 * - efficient generation and update of two time-indexed
 *   bid-ask price curves following the current time.
 *
 * The heatmap has the following characteristics :
 * - 0 : fixed size : the image generated has a fixed
 *   height and width, and as a consequence, only represents
 *   a fraction of time x price of the orderbook.
 * - 1 : fixed time grid : each pixel column occupies a time
 *   range in the form of [start + K * delta, start + (K + 1) * delta[,
 *   K being an integer, and start and delta being two construction-time
 *   constants.
 * - 2 : non-price-gathering : each pixel row represents a
 *   single price level. No aggregation is made accross price
 *   levels.
 * - 3 : synchronous re-anchoring : the heatmap must
 *   represent a relevant orderbook fraction. Consequence
 *   of 1 is that we only re-anchor horizontally once every
 *   delta.
 *   We have two choices for the vertical re-anchoring :
 *   - re-anchor at every event processing.
 *   - re-anchor only when we re-anchor horizontally.
 *   The first would be exact but would prevent us from
 *   reusing the previous image generations which greatly
 *   increases the processing time.
 *   The current choice is to re-anchor the time and
 *   price-levels differences synchronously to maximize
 *   the reuse of previously generated data.
 *   It relies on the assumption that the reference price
 *   does not vary significantly in a single delta.
 *
 * The bid-ask price curves have the following characteristics :
 * - time-indexed, with same delta as heatmap.
 * - fixed size : the curves are mono-dimensional
 *   arrays with a fixed number of elements.
 *
 * The following optimizations are made :
 * - monotonic read : events can only be read in the
 *   increasing time direction, so that past indexed data
 *   is never updated.
 */

#ifndef TB_COR_LV1_H
#define TB_COR_LV1_H

/*********
 * Types *
 *********/

types(
	tb_lv1_upd,
	tb_lv1_prc,
	tb_lv1_hst
);

/***********
 * History *
 ***********/

/*
 * Volume update in the historical data structure.
 */
struct tb_lv1_upd {

	/* Updates of the same sequence indexed by time. */
	ns_dls upds;

	/* Volume. */
	f64 vol;

	/* Time. */
	u64 tim;

};

/*
 * A price tick in the level 1 heatmap.
 */
struct tb_lv1_prc {

	/* Non-empty price ticks of the same history. */
	ns_mapn_f64 prcs;

	/* Volume updates prior to (<=) the current time, sorted by time. */
	ns_dls upds_pre;

	/* Volume updates following (>) the current time, sorted by time. */ 
	ns_dls upds_pst;

	/* The volume before any volume update. */
	f64 vol_stt;

	/* The volume at the current time. */
	f64 vol_cur;

	/* Time of most recent update. */
	u64 tim_end;

};

/*
 * Level 1 history.
 */
struct tb_lv1_hst {

	/* Active price levels. */
	ns_map_u64 prcs;

	/*
	 * Dimensions.
	 */

	/* Time anchor. */
	u64 tim_anc;

	/* Heatmap number of columns. */
	u64 hmp_tim_nb;

	/* Heatmap number of columns. */
	u64 hmp_prc_nb;

	/* Heatmap time (x) cell width is the time anchor. */

	/* Heatmap price (Y) cell width is 1. */

	/* Number of elements of the bid / ask curves.
	 * 0 : not supported. */
	u64 bac_nb;

	/* Heatmap time span (= (tim_nb + 1) * tim_wid). */ 
	u64 hmp_tim_spn;

	/* Bid / ask curves time span. */
	u64 bac_tim_spn;

	/*
	 * Times.
	 */

	/* Current time. */
	u64 tim_cur;

	/* Time below which an order belongs to the heatmap.
	 * = tim_cur aligned up to time anchor. */
	u64 tim_hmp;

	/* Time of (==) the most recent order. */
	u64 tim_max;

	/* Time until which (<) orders can be added. */
	u64 tim_end;

	/*
	 * Re-anchoring.
	 */

	/* Number of new columns in the heatmap at next gen. */
	u64 rnc_nb_tim;

	/*
	 * Arrays.
	 */

	/* Heatmap. [hmp_tim_nb][hmp_prc_nb]. */
	f64 *hmp;

	/* Bid curve if supported. [bac_nb] */
	f64 *bid;

	/* Ask curve if supported. [bac_nb]*/
	f64 *ask;

};

/*******
 * API *
 *******/

/*
 * Construct and return an empty history with a
 * current time of 0.
 * If @bac is set, generate the bid-ask curve.
 */
tb_lv1_hst *tb_lv1_ctr(
	u64 hmp_tim_nb,
	u64 hmp_prc_nb,
	u64 hmp_tim_wid,
	u64 bac_nb
);

/*
 * Delete @hst.
 */
void tb_lv1_dtr(
	tb_lv1_hst *hst
);

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
);

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
);

/*
 * Process newly added updates, update all price levels,
 * update the heatmap.
 */ 
void tb_lv1_prc(
	tb_lv1_hst *hst,
	u64 tim_cur
);

/*
 * Delete all price updates that are too old to appear
 * in the heatmap anymore.
 */
void tb_lv1_cln(
	tb_lv1_hst *hst
);

/*
 * Return @hst's heatmap.
 */
static inline tb_lv1_hmp(
	tb_lv1_hst *hst
) {return hst->hmp;}

/*
 * If @hst supports it, return its bid curve.
 * If not return 0.
 */
static inline tb_lv1_bid(
	tb_lv1_hst *hst
) {return hst->bid;}

/*
 * If @hst supports it, return its ask curve.
 * If not return 0.
 */
static inline tb_lv1_ask(
	tb_lv1_hst *hst
) {return hst->ask;}

#endif /* TB_COR_LV1_H */
