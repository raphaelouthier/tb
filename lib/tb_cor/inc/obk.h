/* Copyright 2025 Raphael Outhier - confidential - proprietary - no copy - no diffusion. */

#ifndef TB_COR_OBK_H
#define TB_COR_OBK_H

/**********************
 * Orderbook snapshot *
 **********************/

/*
 * Return @obs's start (<=) tick.
 */
static inline u64 tb_obs_stt(
	const void *obs
) {return *((u64 *) obs);}

/*
 * Return @obs's (<) end tick.
 */
static inline u64 tb_obs_end(
	const void *obs
) {return tb_obs_stt(obs) + TB_LVL_OBS_NB;}

/*
 * Return @obs's mid tick.
 */
static inline u64 tb_obs_mid(
	const void *obs
) {return tb_obs_stt(obs) + (TB_LVL_OBS_NB >> 1);}

/*
 * Return @obs's volume array.
 */
static inline f64 *tb_obs_arr(
	const void *obs
) {return ns_psum(obs, sizeof(u64));}

/*
 * Set @obs's start tick.
 */
static inline u64 tb_obs_set(
	void *obs,
	u64 stt
) {return *((u64 *) obs) = stt;}

/*
 * From the orderbook snapshot at T0, located at @src,
 * and the updates between T0 and T1, generate the
 * orderbook snapshot for T1 at @dst.
 * Use the giga orderbook snapshot at @gos.
 */
void tb_obs_gen(
	void *dst,
	const void *src,
	f64 *gos,
	u64 upd_nbr,
	const u64 *upd_tcks,
	const f64 *upd_vols
);

/***************************
 * Giga orderbook snapshot *
 ***************************/

/*
 * Allocate and return a giga orderbook snapshot.
 */
static inline f64 *tb_gos_all(
	void
) {return nh_all(sizeof(f64) * TB_LVL_GOS_NB);}

/*
 * Free @gos.
 */
static inline void tb_gos_fre(
	f64 *gos
) {return nh_fre(gos, sizeof(f64) * TB_LVL_GOS_NB);}

/*************
 * Orderbook *
 *************/

/*
 * If @vol is a bid, return 0.
 * If @vol is an ask, return 1.
 * It must not be null.
 */
static inline u8 tb_obk_is_ask(
	f64 vol
)
{
	check(vol != 0);
	return vol > 0;
}

/*
 * Update @dst of @dst_nbr elements starting at @dst_stt
 * with @src of @src_nbr elements starting at @src_stt
 * If @src was fully copied into @dst, return 0.
 * If some elements of @src did not fit in @dst, return 1.
 *
 */
static inline uerr tb_obk_add(
	f64 *dst,
	u64 dst_stt,
	u64 dst_nbr,
	const f64 *src,
	u64 src_stt,
	u64 src_nbr
)
{
	
	/* Determine the intersection range. */
	const u64 dst_end = dst_stt + dst_nbr;
	const u64 src_end = src_stt + src_nbr;
	check(src_stt < src_end);
	check(dst_stt < dst_end);
	const u64 its_stt = (dst_stt < src_stt) ? src_stt : dst_stt;
	const u64 its_end = (dst_end < src_end) ? dst_end : src_end;

	/* Report partial write. */
	const u8 prt = (its_stt != src_stt) || (its_end != src_end); 

	/* If no intersection, nothing to do. */
	if (its_end <= its_stt) goto end;

	/* Determine the copy offsets. */
	check(its_stt >= dst_stt);
	check(its_stt >= src_stt);
	check((its_stt == src_stt) || (its_stt == dst_stt));
	const u64 dst_off = (its_stt - dst_stt); 
	const u64 src_off = (its_stt - src_stt); 
	assert((!dst_off) || (!src_off));
	
	/* Copy. */
	ns_mem_cpy(dst + dst_off, src + src_off, (its_end - its_stt) * sizeof(f64));
	
	/* Complete. */
	end:;
	return prt;

}

/*
 * Update @obk of @nbr elements starting at @stt,
 * with an orderbook snapshot located at @obs 
 * Elements of @obk which have no related elements
 * in @obs are not initialized.
 */
static inline uerr tb_obk_add_obs(
	f64 *obk,
	u64 obk_stt,
	u64 obk_nbr,
	const void *obs
)
{
	return tb_obk_add(
		obk,
		obk_stt,
		obk_nbr,
		tb_obs_arr(obs),
		tb_obs_stt(obs),
		TB_LVL_OBS_NB
	);
}

/*
 * Extract an orderbook snapshot located at @obs
 * from the ordebrook @obk of @nbr elements starting at @stt,
 * Elements of @obs which have no related elements are
 * not initialized.
 */
static inline uerr tb_obk_xtr_obs(
	const f64 *obk,
	u64 obk_stt,
	u64 obk_nbr,
	void *obs
)
{
	return tb_obk_add(
		tb_obs_arr(obs),
		tb_obs_stt(obs),
		TB_LVL_OBS_NB,
		obk,
		obk_stt,
		obk_nbr
	);
}

/*
 * Add the @upd_nbr updates at (@tcks, @vols) in @obk.
 * Store the values of the minimal and maximal updated
 * ticks at @tck_minp and @tck_maxp.
 */
static inline void tb_obk_add_upds(
	f64 *obk,
	u64 obk_stt,
	u64 obk_nbr,
	u64 upd_nbr,
	const u64 *tcks,
	const f64 *vols,
	u64 *tck_minp,
	u64 *tck_maxp
)
{
	assert(upd_nbr);
	const u64 obk_end = obk_stt + obk_nbr;
	check(obk_stt < obk_end);
	u64 min = (u64) -1;
	u64 max = 0;
	for (u64 upd_idx = 0; upd_idx < upd_nbr; upd_idx++) {
		const u64 tck = tcks[upd_idx];
		min = (min < tck) ? min : tck;
		max = (max > tck) ? max : tck;
		if ((obk_stt <= tck) && (tck < obk_end)) {
			obk[tck - obk_stt] = vols[upd_idx];
		}	
	}
	*tck_minp = min;
	*tck_maxp = max;
}

/*
 * Determine the best and worst bid and ask ticks of @obk
 * in the [@stt, @end[ range.
 * If a bid is found after an ask, return 1.
 * Otherwise, return 0.
 */
static inline uerr tb_obk_bst_bat(
	const f64 *obk,
	u64 obk_nbr,
	u64 stt,
	u64 end,
	u64 tck_off,
	u64 *bst_bidp,
	u64 *bst_askp,
	u64 *wst_bidp,
	u64 *wst_askp
)
{
	assert(stt < obk_nbr);
	assert(end <= obk_nbr);
	assert(stt < end);
	check((!bst_bidp) == (!bst_askp));
	check((!wst_bidp) == (!wst_askp));
	uerr inv = 0;

	/* Traverse the [@stt, @end[ range. */ 
	u64 bst_bid = 0;
	u64 bst_ask = (u64) -1;
	u64 wst_bid = 0;
	u64 wst_ask = (u64) -1;
	u8 was_bid = 0;
	u8 was_ask = 0;
	for (u64 obk_idx = stt; obk_idx < end; obk_idx++) {
		const f64 vol = obk[obk_idx];
		if (vol == 0) continue;
		const u64 tck_idx = obk_idx + tck_off;
		check(obk_idx <= tck_idx);
		const u8 is_bid = (vol < 0);

		/* If bid : */
		if (is_bid) {

			/* If no bid before, report as worst bid. */ 
			if (!was_bid) {
				wst_bid = tck_idx;
				was_bid = 1;
			}

			/* Report as best bid. */
			bst_bid = tck_idx;

			/* Detect inversion. */
			if (was_ask) {
				inv = 1;
			}

		}

		/* If ask : */
		else {
			check(!is_bid);

			/* If no ask before, report as best ask. */
			if (!was_ask) {
				bst_ask = tck_idx;
				was_ask = 1;
			}

			/* Report as worst ask. */
			wst_ask = tck_idx;

		}
		
	}

	/* Check ordering. */
	check(wst_bid <= bst_bid);
	check(bst_ask <= wst_ask);

	/* Forward results. */
	if (bst_bidp) {
		*bst_bidp = bst_bid;
		*bst_askp = bst_ask;
	}
	if (wst_bidp) {
		*wst_bidp = wst_bid;
		*wst_askp = wst_ask;
	}

	/* Report any inversion. */
	return inv;

}

/*
 * Return an orderbook anchor price
 * given its best bid (may be 0 if no bid),
 * its best ask (may be -1 if no ask), the
 * orderbook size, and the previous anchor value,
 * to use if no bid or ask exist.
 */
static inline u64 tb_obk_anc(
	u64 bst_bid,
	u64 bst_ask,
	u64 prv,
	u64 obk_siz
)
{
	check(!(obk_siz & 1));
	const u8 no_bid = (bst_bid == 0);
	const u8 no_ask = (bst_ask == (u64) -1);
	const u64 anc_min = obk_siz >> 1;
	check(prv >= anc_min);
	u64 anc;
	if (no_bid && no_ask) {
		anc = prv;
	} else if (no_bid) {
		anc = bst_ask;
	} else if (no_ask) {
		anc = bst_bid;
	} else {
		const u64 sum = bst_bid + bst_ask;
		assert(bst_ask < sum);
		anc = (sum) >> 1;
	}
	return (anc < anc_min) ? anc_min : anc;
}

#endif /* TB_COR_OBK_H */
