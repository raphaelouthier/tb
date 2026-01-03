/* Copyright 2025 Raphael Outhier - confidential - proprietary - no copy - no diffusion. */

#include <tb_tst/tb_tst.all.h>

/*******
 * Ask *
 *******/

/*
 * Unit test for orderbook snapshot computation
 * validation.
 */
static inline void _obk_unt_ask(
	u64 sed,
	u64 *nt_err_cnt
)
{

	/* Basic is_ask checks. */
	nt_chk(tb_obk_is_ask(1));
	nt_chk(tb_obk_is_ask(10));
	nt_chk(tb_obk_is_ask((f64) ((u64) 1 << 41)));
	nt_chk(tb_obk_is_ask(1.3));
	nt_chk(!tb_obk_is_ask(-1));
	nt_chk(!tb_obk_is_ask(-10));
	nt_chk(!tb_obk_is_ask(-(f64) ((u64) 1 << 41)));
	nt_chk(!tb_obk_is_ask(-1.3));

}

/*******
 * Add *
 *******/

/*
 * Unit test for orderbook add.
 */
static inline void _obk_unt_add(
	u64 sed,
	u64 *nt_err_cnt
)
{

	/* Allocate two arrays and exercise all overlap configs */
	f64 *src = nh_all(2 * sizeof(f64));
	f64 *dst = nh_all(4 * sizeof(f64));

	#define TST(shf_src, shf_dst, res0, res1, res2) \
		for (u8 i = 0; i < 2; i++) src[i] = 1; \
		for (u8 i = 0; i < 4; i++) dst[i] = 2; \
		tb_obk_add( \
			dst, BAS + shf_dst, 3, \
			src, BAS + shf_src, 2 \
		); \
		nt_chk(dst[0] == res0); \
		nt_chk(dst[1] == res1); \
		nt_chk(dst[2] == res2); \

	/* Shift @dst of 3 relatively to @src
	 * to test all cases. */
	#define SEQ() \
		TST(0, 3, 2, 2, 2); \
		TST(1, 3, 2, 2, 2); \
		TST(2, 3, 1, 2, 2); \
		TST(3, 3, 1, 1, 2); \
		TST(4, 3, 2, 1, 1); \
		TST(5, 3, 2, 2, 1); \
		TST(6, 3, 2, 2, 2); \
		TST(7, 3, 2, 2, 2);

	#define BAS 0
	SEQ();
	#undef BAS
	#define BAS 65337
	SEQ();
	#undef BAS
	#define BAS ((u64) 1 << 32)
	SEQ();
	#undef BAS
	#define BAS ((u64) 1 << 56)
	SEQ();
	#undef BAS
	#define BAS (((u64) -1) - 9)
	SEQ();

}

/*******
 * OBS *
 *******/

/*
 * Generate random values in @obs.
 */
static inline void _obs_gen(
	u64 rnd,
	void *obs
)
{
	/* Decide of a random bid-ask window. */
	const u64 baw_wid = 8;
	rnd = ns_hsh_mas_gen(rnd);
	check(1 <= baw_wid);
	check(baw_wid < 10);
	const u64 baw_mid = 512;
	check(baw_wid < baw_mid);
	const u64 baw_stt = baw_mid - (baw_wid >> 1);
	const u64 baw_end = baw_stt + baw_wid;
	check(baw_end - baw_stt == baw_wid);

	/* Write data. */
	for (u64 i = 0; i < TB_LVL_OBS_NB; i++) {
		const f64 vol_abs = ns_hsh_f64_rng(rnd, 1, 1000, 0.001);
		tb_obs_arr(obs)[i] = (i < baw_stt) ?
			-vol_abs :
			(i > baw_end) ?
				vol_abs :
				0;	
		rnd = ns_hsh_mas_gen(rnd);
	}
}

/*
 * Verify values in @obs.
 */
static inline void _obs_chk(
	u64 rnd,
	void *obs
)
{
	/* Decide of a random bid-ask window. */
	const u64 baw_wid = 8;
	rnd = ns_hsh_mas_gen(rnd);
	check(1 <= baw_wid);
	check(baw_wid < 10);
	const u64 baw_mid = 512;
	check(baw_wid < baw_mid);
	const u64 baw_stt = baw_mid - (baw_wid >> 1);
	const u64 baw_end = baw_stt + baw_wid;
	check(baw_end - baw_stt == baw_wid);

	/* Read data. */
	for (u64 i = 0; i < TB_LVL_OBS_NB; i++) {
		const f64 vol_abs = ns_hsh_f64_rng(rnd, 1, 1000, 0.001);
		const f64 vol_red = tb_obs_arr(obs)[i];
		const f64 vol_exp = (i < baw_stt) ?
			-vol_abs :
			(i > baw_end) ?
				vol_abs :
				0;	
		check(vol_red == vol_exp);
		rnd = ns_hsh_mas_gen(rnd);
	}
}

/*
 * Generate an orderbook state with @mid as mid price,
 * with a window of 16.
 * Reuse values of @src when in their range,
 * and they have the correct sign.
 * Ensure that the first update is at @obs's mid.
 */
static inline u64 _upd_gen(
	u64 rnd,
	u64 mid,
	f64 *gos,
	void *obs,
	u64 gos_stt,
	u64 src_stt,
	u64 *tcks,
	f64 *vols
)
{
	check(0 <= mid);
	check(mid <= TB_LVL_GOS_NB);

	/* Iterate over all elements of @gos in pseudo random
	 * order starting at element mid. */
	for (u64 idx = 0; idx < TB_LVL_GOS_NB; idx++) {
		gos[idx] = 0;
	}
	u64 upd_idx = 0;
	check(TB_LVL_GOS_NB % 661);
	check(661 && TB_LVL_GOS_NB);
	for (u64 idx = 0; idx < TB_LVL_GOS_NB; idx++) {

		/* Randomize starting at mid. */
		const u64 gos_idx = ((TB_LVL_GOS_NB >> 1) + idx * 661) % (TB_LVL_GOS_NB); 
		const u64 abs_idx = gos_stt + gos_idx;
		assert(gos_stt <= abs_idx);
	
		/* Determine the gos volume. */	
		const f64 gos_vol_abs = ns_hsh_f64_rng(rnd, 1, 1000, 0.001);
		check(gos_vol_abs > 0);
		const f64 gos_vol = (gos_idx < mid) ? -gos_vol_abs : gos_vol_abs;
		check(gos[gos_idx] == 0);
		gos[gos_idx] = gos_vol;
		
		/* Determine the source volume. */
		u8 has_src = (src_stt <= abs_idx) && (abs_idx < src_stt + TB_LVL_OBS_NB);
		f64 src_vol = 0;
		if (has_src)	{
			src_vol = tb_obs_arr(obs)[abs_idx - src_stt];
		}

		/* If we're doing the first update,
		 * or if we have no source value,
		 * or if the source value is non-null and has
		 * the opposite sign of the gos volume,
		 * generate an update. */
		check((idx == 0) == (gos_idx == (TB_LVL_GOS_NB >> 1)));
		check(gos_vol != 0);
		if (
			(gos_idx == (TB_LVL_GOS_NB >> 1)) ||
			(!has_src) ||
			((src_vol != 0) && ((src_vol < 0) != (gos_vol <= 0)))
		) {
			tcks[upd_idx] = abs_idx;
			vols[upd_idx] = gos_vol;
			upd_idx++;
		}
	}
	for (u64 idx = 0; idx < TB_LVL_GOS_NB; idx++) {
		const f64 vol = gos[idx];
		check(vol != 0);
		check((vol < 0) == (idx < mid));
	}
	check(upd_idx);
	return upd_idx;
}

/*
 * Verify @dst.
 */
static inline void _dst_chk(
	f64 *gos,
	u64 gos_stt,
	void *dst,
	void *src,
	u8 no_src,
	u8 *cas0p,
	u8 *cas1p,
	u8 *cas2p
)
{

	/* Determine obs boundaries. */
	check(src);
	check(dst);
	const u64 src_stt = tb_obs_stt(src);
	const u64 dst_stt = tb_obs_stt(dst);

	/* Fetch obs arrays. */
	f64 *const dst_arr = tb_obs_arr(dst);

	/* Traverse @dst. */
	u64 cnt0 = 0;
	u64 cnt1 = 0;
	u64 cnt2 = 0;
	for (u64 dst_idx = 0; dst_idx < TB_LVL_OBS_NB; dst_idx++) {

		/* Read. */
		const f64 dst_vol = dst_arr[dst_idx];

		/* Get the absolute index. */
		const u64 abs_idx = dst_idx + dst_stt;
		assert(dst_stt <= abs_idx);

		/* Determine if in @src. */
		const f64 gos_vol = ((gos_stt <= abs_idx) && (abs_idx < gos_stt + TB_LVL_GOS_NB)) ? 
			gos[abs_idx - gos_stt] : 0;

		/* Determine if we had an update for this tick. */
		u8 has_src = (src_stt <= abs_idx) && (abs_idx < src_stt + TB_LVL_OBS_NB);
		f64 src_vol = 0;
		if (has_src)	{
			src_vol = tb_obs_arr(src)[abs_idx - src_stt];
		}
		const u8 has_upd = (
			(abs_idx == (gos_stt + (TB_LVL_GOS_NB >> 1))) ||
			(!has_src) ||
			((src_vol != 0) && ((src_vol < 0) != (gos_vol <= 0)))
		);

		/* If no and we're using @src, check against the
		 * volume in @src instead. */
		if ((!has_upd) && (!no_src)) {
			check(src_vol == dst_vol); 
			cnt0++;
		}

		/* If no update and we're not using @src, should
		 * be null. */
		else if ((!has_upd) && (no_src)) {
			check(dst_vol == 0);
			cnt1++;
		}

		/* If no update in @src, check against @gos's volume. */
		else {
			assert(dst_vol != -1);
			assert(gos_vol != -1);
			check(gos_vol == dst_vol);
			cnt2++;
		}

	}
	if (cnt0) *cas0p = 1;
	if (cnt1) *cas1p = 1;
	if (cnt2) *cas2p = 1;
	assert((cnt0 + cnt1 + cnt2) == TB_LVL_OBS_NB);

}	


/*
 * Unit test for orderbook snapshot computation
 * validation.
 */
static inline void _obk_unt_obs(
	u64 sed,
	u64 *nt_err_cnt
)
{

	/* Randomization setup. */
	u64 rnd = sed;

	/* Allocate two orderbook snapshots. */
	void *src = nh_all(1025 * 8);
	void *dst = nh_all(1025 * 8);

	/* Allocate a giga orderbook snapshot. */
	void *gos = tb_gos_all();
	void *gos1 = tb_gos_all();

	/* Allocate order arrays. */ 
	u64 *const tcks = nh_all(((u64) 1 << 21) * 8);
	f64 *const vols = nh_all(((u64) 1 << 21) * 8);

	/* Select the start price in the 1Mi range after 1Gi. */
	const u64 src_stt = ((u64) 1 << 30) - (TB_LVL_OBS_NB >> 1);
	const u64 src_mid = src_stt + (TB_LVL_OBS_NB >> 1);
	rnd = ns_hsh_mas_gen(rnd);

	/* Set @src's start, verify derived operations. */  
	tb_obs_set(src, src_stt);
	check(tb_obs_stt(src) == src_stt);
	check(tb_obs_end(src) == src_stt + TB_LVL_OBS_NB);
	check(tb_obs_mid(src) == src_stt + (TB_LVL_OBS_NB >> 1));
	check(tb_obs_arr(src) == ns_psum(src, 8));

	/* Randomly generate @src, starting after tick 2 ^ 30. */
	_obs_gen(sed, src);

	/* Verify that read values match the written values. */
	_obs_chk(sed, src);

	/* Position @src in the middle of @gos. */
	const u64 gos_stt = src_mid - ((u64) 1 << 19);

	u8 cas0 = 0;
	u8 cas1 = 0;
	u8 cas2 = 0;

	/*
	 * In order to save test time, only interesting
	 * mid positions are tested.
	 * A more exhaustive mode can be enabled by
	 * uncommenting the following line.
	 */
	//#define TB_TST_OBK_ALL_MIDS

	#ifdef TB_TST_OBK_ALL_MIDS
	/* Iterate over mid positions, by steps of 512. */
	for (u64 dst_mid = 0; dst_mid <= TB_LVL_GOS_NB; dst_mid += 512) {
		debug("\rdst_mid : %U/%U.\n", dst_mid, TB_LVL_GOS_NB);
	#else
	/* Iterate over only an interesting set of mid positions. */
	#define NB_MIDS 8
	u64 dst_mids[NB_MIDS] = {
		0, TB_LVL_GOS_NB,
		src_stt - gos_stt, src_mid - gos_stt, src_stt + TB_LVL_OBS_NB - gos_stt,
		1024, 2048, 65536,
	};
	for (u64 dst_mid_idx = 0; dst_mid_idx < NB_MIDS; dst_mid_idx++) {
		u64 dst_mid = dst_mids[dst_mid_idx];
		debug("dst_mid : %U : %H.\n", dst_mid_idx, dst_mid);
		check(dst_mid <= TB_LVL_GOS_NB);
	#endif

		/* Generate an orderbook state with @mid as mid price,
		 * with a window of 16.
		 * Reuse values of @src when in their range,
		 * just adjust their sign.
		 * Ensure that the first update is at @obs's mid. */
		const u64 upd_nbr = _upd_gen(
			rnd,
			dst_mid,
			gos, src,
			gos_stt, src_stt,
			tcks, vols
		);

		/* Generate the orderbook snapshot in @dst using @src. */
		tb_obs_gen(dst, src, gos1, upd_nbr, tcks, vols);

		/* Verify @dst. */
		_dst_chk(gos, gos_stt, dst, src, 0, &cas0, &cas1, &cas2);

		/* Generate the orderbook snapshot in @dst without @src. */
		tb_obs_gen(dst, 0, gos1, upd_nbr, tcks, vols);

		/* Verify @dst. */
		_dst_chk(gos, gos_stt, dst, src, 1, &cas0, &cas1, &cas2);

	}

	assert(cas0, "case 0 not validated.\n");
	assert(cas1, "case 1 not validated.\n");
	assert(cas2, "case 2 not validated.\n");

	/* Verify that @src has not been altered. */
	_obs_chk(sed, src);

	/* Cleanup. */
	tb_gos_fre(gos);
	tb_gos_fre(gos1);
	nh_fre(src, 1025 * 8);
	nh_fre(dst, 1025 * 8);
	nh_fre(tcks, ((u64) 1 << 20) * 8);
	nh_fre(vols, ((u64) 1 << 20) * 8);

}


/*
 * Test sequence.
 */
static inline void _obk_tsq(
	nh_tst_exc *exc,
	void *_
)
{
	NH_TST_UNT(exc, _obk_unt_ask);
	NH_TST_UNT(exc, _obk_unt_add);
	NH_TST_UNT(exc, _obk_unt_obs);
}

/*
 * Level data testing.
 */
void tb_tst_obk(
	nh_tst_sys *sys,
	u64 sed,
	u8 wrk_nb,
	u8 prc
)
{
	void *arg = 0;
	nh_tst_psh__(sys, sed, _obk_tsq, arg);
}

