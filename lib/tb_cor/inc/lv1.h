/* Copyright 2025 Raphael Outhier - confidential - proprietary - no copy - no diffusion. */

/*
 * The level 1 library provides ways to 
 * represent and operate on level 1 data.
 *
 * It is designed around the following use case : 
 * - fast incorporation and storage of burst of level 1
 *   events read from storage.
 * - determination of a current time.
 * - efficient generation and update of a time x tick-level
 *   indexed heatmap of events prior to the current time.
 * - efficient generation and update of two time-indexed
 *   bid-ask tick curves following the current time.
 *
 * The heatmap has the following characteristics :
 * - 0 : fixed size : the image generated has a fixed
 *   height and width, and as a consequence, only represents
 *   a fraction of time x tick of the orderbook.
 * - 1 : fixed time grid : each pixel column occupies a time
 *   range in the form of [start + K * delta, start + (K + 1) * delta[,
 *   K being an integer, and start and delta being two construction-time
 *   constants.
 * - 2 : non-tick-gathering : each pixel row represents a
 *   single tick level. No aggregation is made accross tick
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
 *   tick-levels differences synchronously to maximize
 *   the reuse of previously generated data.
 *   It relies on the assumption that the reference tick
 *   does not vary significantly in a single delta.
 *
 * The bid-ask tick curves have the following characteristics :
 * - time-indexed, with same delta as heatmap.
 * - fixed size : the curves are mono-dimensional
 *   arrays with a fixed number of elements.
 *
 * The following optimizations are made :
 * - monotonic read : events can only be read in the
 *   increasing time direction, so that past indexed data
 *   is never updated.
 *
 * The heatmap and bid-ask curves are time-indexed.
 * Though, since they :
 * - group times in a cell of the width of
 *   1x the time resolution; and
 * - are re-anchored every time the current time increases
 *   enough
 * it is convenient to index their cells on a metric
 * derived from the time, the absolute index (aid), 
 * computed from the time and time resolution by :
 * aid = (u64) ((u64) time / (u64) resolution).
 * A reanchoring always does a shift of an integer > 0
 * number of aids.
 */

#ifndef TB_COR_LV1_H
#define TB_COR_LV1_H

/*********
 * Types *
 *********/

types(
	tb_lv1_upd,
	tb_lv1_tck,
	tb_lv1_hst
);

/***********
 * History *
 ***********/

/*
 * Volume update in the history.
 */
struct tb_lv1_upd {

	/* Updates past the current time of @tck indexed by time. */
	ns_dls upds_tck;

	/* Updates of the same history indexed by increasing time. */
	ns_sls upds_hst;

	/* Tick. */
	tb_lv1_tck *tck;

	/* Volume. */
	f64 vol;

	/* Time. */
	u64 tim;

};

/*
 * A price tick in the level 1 heatmap.
 */
struct tb_lv1_tck {

	/* Non-empty ticks of the same history ordered by level. */
	ns_mapn_u64 tcks;

	/* Volume updates prior to (<=) the current time, sorted by time. */
	ns_dls upds_tck;

	/* The volume before any volume update. */
	f64 vol_stt;

	/* The volume at the current time. */
	f64 vol_cur;

	/* The volume at the maximal time. */
	f64 vol_max;

	/* Time of most recent volume update. */
	u64 tim_max;

	#ifdef DEBUG
	/* Debug flag. */
	u8 dbg;
	#endif

};

/*
 * Level 1 history.
 */
struct tb_lv1_hst {
	
	/* Active price ticks. */
	ns_map_u64 tcks;

	/* Updates sorted by time. */
	ns_slsh upds_hst;

	/* Next update to process. */
	tb_lv1_upd *upd_prc;

	/*
	 * Dimensions.
	 */

	/* Time resolution. Used as time cell width and anchor. */
	u64 tim_res;

	/* Heatmap number of columns. */
	u64 hmp_dim_tim;

	/* Heatmap number of rows. Must be even. */
	u64 hmp_dim_tck;

	/* Heatmap time (x) cell width is the time anchor. */

	/* Heatmap tick (Y) cell width is 1. */

	/* Number of elements of the bid / ask curves.
	 * 0 : not supported. */
	u64 bac_nb;

	/* Heatmap time span (= (tim_nb + 1) * tim_wid). */ 
	u64 hmp_tim_spn;

	/* Bid / ask curves time span. */
	u64 bac_tim_spn;

	/* Number of ticks per price unit. */
	f64 tck_rat;

	/* Inverse. */
	f64 prc_rat;

	/*
	 * Times.
	 */

	/* Current time. */
	u64 tim_cur;

	/* Reanchoring time. */
	u64 tim_rnc;

	/* Time below (<) which an order belongs to the heatmap.
	 * = tim_cur aligned up to time anchor. */
	u64 tim_hmp;

	/* Time of (==) the most recent order. */
	u64 tim_max;

	/* Time until which (<) orders can be added. */
	u64 tim_end;

	/* Time of last processed update. */
	u64 tim_prc;

	/*
	 * Ticks.
	 */

	/* Best (>) bid at current time. */
	tb_lv1_tck *bst_cur_bid;

	/* Best (<) ask at current time. */
	tb_lv1_tck *bst_cur_ask;

	/* Current heatmap tick reference. */
	u64 tck_ref;

	/* Current heatmap tick range [min, max[. */
	u64 hmp_tck_min;
	u64 hmp_tck_max;

	/*
	 * Bid-ask curve metadata.
	 */

	/* Best (>) bid at max time. */
	tb_lv1_tck *bst_max_bid;

	/* Best (<) ask at max time. */
	tb_lv1_tck *bst_max_ask;

	/* Bid-ask curve start AID. */
	u64 bac_aid;

	/* AID of last best bid. */
	u64 bid_aid;

	/* AID of last best ask. */
	u64 ask_aid;

	/*
	 * Re-anchoring.
	 */

	/* Number of new columns in the heatmap at next gen. */
	u64 hmp_shf_tim;

	/*
	 * Arrays.
	 */

	/* Heatmap. [hmp_dim_tck][hmp_dim_tim]. */
	f64 *hmp;

	/* Bid curve if supported. [bac_nb] */
	u64 *bid_crv;

	/* Ask curve if supported. [bac_nb]*/
	u64 *ask_crv;

};

/*******
 * Log *
 *******/

tb_lib_log_dec(cor, lv1);
#define tb_lv1_log(...) tb_log_lib(cor, lv1, __VA_ARGS__)

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
	u64 prc_res,
	u64 hmp_dim_tck,
	u64 hmp_dim_tim,
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
 * Do not update tick levels nor the bitmap.
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
 * and in this case, all tick levels must be new. 
 * If any, all created updates are added in the
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
 * Process newly added updates, update all tick levels,
 * update the heatmap.
 */ 
void tb_lv1_prc(
	tb_lv1_hst *hst
);

/*
 * Delete all updates that are too old to appear
 * in the heatmap anymore.
 */
void tb_lv1_cln(
	tb_lv1_hst *hst
);

/*
 * Return @hst's heatmap.
 */
static inline f64 *tb_lv1_hmp(
	tb_lv1_hst *hst
) {return hst->hmp;}

/*
 * If @hst supports it, return its bid curve.
 * If not return 0.
 */
static inline u64 *tb_lv1_bid(
	tb_lv1_hst *hst
) {return hst->bid_crv;}

/*
 * If @hst supports it, return its ask curve.
 * If not return 0.
 */
static inline u64 *tb_lv1_ask(
	tb_lv1_hst *hst
) {return hst->ask_crv;}

#endif /* TB_COR_LV1_H */
