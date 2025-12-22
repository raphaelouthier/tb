/* Copyright 2025 Raphael Outhier - confidential - proprietary - no copy - no diffusion. */

#include <tb_cor/tb_cor.all.h>

/**********************
 * Orderbook snapshot *
 **********************/

/*
 * From the orderbook snapshot at T0, located at @src,
 * and the updates between T0 and T1, generate the
 * orderbook snapshot for T1 at @dst.
 * Use the giga orderbook snapshot at @gos.
 * Return 1 if there was a data loss during
 * the transfer from @gos to @dst.
 * Return 0 if all data did fit in @dst.
 */
uerr tb_obk_gen(
	void *dst,
	void *src,
	f64 *gos,
	u64 upd_nbr,
	u64 *upd_tcks,
	f64 *upd_vols
)
{
	assert(upd_nbr);

	/* Reset @gos. */
	ns_mem_rst(gos, sizeof(f64) * TB_LVL_GOS_NB); 

	/* Determine the mid price of @src. */
	const u64 src_mid = tb_obs_mid(src);
	assert(src_mid >= (TB_LVL_OBS_NB >> 1));

	/* Determine the range of @src. */
	const u64 src_stt = tb_obs_stt(src);
	const u64 src_end = tb_obs_end(src);
	check(src_end == src_stt + TB_LVL_OBS_NB);
	check(src_mid == src_stt + (TB_LVL_OBS_NB >> 1));

	/* Determine the start price of @gos. */
	const u64 gos_stt = src_mid >= (TB_LVL_GOS_NB >> 1) ?
		src_mid - (TB_LVL_GOS_NB >> 1):
		0;

	/* Copy into @gos. */
	assert(!tb_obk_add(
		gos,
		gos_stt,
		TB_LVL_GOS_NB,
		tb_obs_arr(src),
		src_stt,
		TB_LVL_OBS_NB
	));

	/* Add all udpates. */
	u64 upd_min = (u64) -1;
	u64 upd_max = 0;
	tb_obk_add_upds(
		gos,
		gos_stt,
		TB_LVL_GOS_NB,
		upd_nbr,
		upd_tcks,
		upd_vols,
		&upd_min,
		&upd_max
	);
	assert(upd_min <= upd_max);

	/* Determine the best range to query best bids and asks. */
	const u64 bac_stt = (upd_min < src_stt) ? upd_min : src_stt;
	const u64 bac_end = (src_end < upd_max) ? upd_max : src_end;

	/* Compute the best bid and ask. */ 
	u64 bst_bid = 0;
	u64 bst_ask = (u64) -1;
	u64 wst_bid = 0;
	u64 wst_ask = (u64) -1;
	tb_obk_bst_bat(
		gos,
		gos_stt,
		TB_LVL_GOS_NB,
		bac_stt,
		bac_end,
		&bst_bid,
		&bst_ask,
		&wst_bid,
		&wst_ask
	);

	/* Determine the ideal anchor price. */
	const u64 anc = tb_obk_anc(bst_bid, bst_ask, TB_LVL_OBS_NB);

	/* Derive @dst's start price. */
	assert(anc >= (TB_LVL_OBS_NB >> 1));
	const u64 dst_stt = anc - (TB_LVL_OBS_NB >> 1);
	const u64 dst_end = dst_stt + TB_LVL_OBS_NB;

	/* Extract @dst's snapshot. */
	assert(tb_obk_add(
		tb_obs_arr(dst),
		tb_obs_stt(dst),
		TB_LVL_OBS_NB,
		gos,
		gos_stt,
		TB_LVL_GOS_NB
	));

	/* Report loss if the bid ask range does not fit
	 * in @dst. */
	const u64 bar_stt = (wst_bid < bst_ask) ? wst_bid : bst_ask;
	const u64 bar_end = (bst_bid < wst_ask) ? wst_ask : bst_bid;
	check(bar_stt <= bar_end);
	check(dst_stt < dst_end);
	return (bar_stt < dst_stt) || (dst_end < bar_end);

}

