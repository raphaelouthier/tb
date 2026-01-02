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
	nt_chk(tb_obk_is_ask((f64) (u64) 1 << 41));
	nt_chk(tb_obk_is_ask(1.3));
	nt_chk(!tb_obk_is_ask(-1));
	nt_chk(!tb_obk_is_ask(-10));
	nt_chk(!tb_obk_is_ask(-(f64) (u64) 1 << 41));
	nt_chk(!tb_obk_is_ask(-1.3));

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
	const u64 baw_wid = ns_hsh_rng_u64(rnd, 1, 10);
	rnd = ns_hsh_mas_gen(rnd);
	nt_chk(1 <= baw_wid);
	nt_chk(baw_wid < 10);
	const u64 baw_mid = ns_hsh_rng_(24, 1000);
	nt_chk(baw_wid < baw_mid);
	const u64 baw_stt = baw_mid - (baw_wid >> 1);
	const u64 baw_end = baw_stt + baw_wid;
	nt_chk(baw_end - baw_stt == baw_wid);

	/* Write data. */
	for (u64 i = 0; i < 1024; i++) {
		const f64 vol_abs = ns_hsh_f64_rng(rnd, 1, 1000, 0.001);
		tb_obs_arr[i] = (i < baw_stt) ?
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
	const u64 baw_wid = ns_hsh_rng_u64(rnd, 1, 10);
	rnd = ns_hsh_mas_gen(rnd);
	nt_chk(1 <= baw_wid);
	nt_chk(baw_wid < 10);
	const u64 baw_mid = ns_hsh_rng_(24, 1000);
	nt_chk(baw_wid < baw_mid);
	const u64 baw_stt = baw_mid - (baw_wid >> 1);
	const u64 baw_end = baw_stt + baw_wid;
	nt_chk(baw_end - baw_stt == baw_wid);

	/* Read data. */
	for (u64 i = 0; i < 1024; i++) {
		const f64 vol_abs = ns_hsh_f64_rng(rnd, 1, 1000, 0.001);
		const f64 vol_red = tb_obs_arr[i];
		const f64 vol_exp = (i < baw_stt) ?
			-vol_abs :
			(i > baw_end) ?
				vol_abs :
				0;	
		nt_chk(vol_red == vol_exp);
		rnd = ns_hsh_mas_gen(rnd);
	}
}

/*
 * Generate a full gos state.
 * Store values in @gos, and updates in arrays.
 */
static inline void _upds_gen(


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
	void *src = ns_all(1025 * 8);
	void *dst = ns_all(1025 * 8);

	/* Allocate a giga orderbook snapshot. */
	void *gos = tb_gos_all();

	/* Allocate order arrays. */ 
	u64 *const tcks = nh_all(((u64) 1 << 21) * 8);
	f64 *const vols = nh_all(((f64) 1 << 21) * 8);

	/* Select the start price in the 1Mi range after 1Gi. */
	const u64 src_stt = ns_hsh_rng_u64(rnd, (u64) 1 << 30, (u64) 1 << 30 + (u64) 1 << 20, 1);
	const u64 src_mid = src_stt + 512;
	rnd = ns_hsh_mas_gen(rnd);

	/* Set @src's start, verify derived operations. */  
	tb_obs_set(src, src_stt);
	nt_chk(tb_obs_stt(src) == src_stt);
	nt_chk(tb_obs_end(src) == src_stt + 1024);
	nt_chk(tb_obs_mid(src) == src_stt + 512);
	nt_chk(tb_obs_arr(src) == ns_psum(src, 8));

	/* Randomly generate @src, starting after tick 2 ^ 30. */
	_obs_rnd(sed, src);

	/* Verify that read values match the written values. */
	_obs_chk(sed, src);

	/* Position @src in the middle of @gos. */
	const u64 gos_stt = src_mid - ((u64) 1 << 19);

	/* Iterate over mid positions, by steps of 512. */
	for (u64 dst_mid = 0; dst_mid < 1024 * 1024; dst_mid += 512) {

		/* Generate an orderbook state with @mid as mid price,
		 * with a window of 16.
		 * Reuse values of @src when in their range,
		 * just adjust their sign. */
		_upd_gen(
			rnd,
			gos, src,
			gos_stt, src_stt,
			tcks, vols
		);

		/* Generate the orderbook snapshot in @dst. */
		tb_obs_gen(dst, src, gos, 1 << 20, tcks, vols);




	}

	/* Cleanup. */
	tb_gos_fre(gos);
	ns_fre(src, 1025 * 8);
	ns_fre(dst, 1025 * 8);
	nh_all(tcks, ((u64) 1 << 20) * 8);
	nh_all(vols, ((f64) 1 << 20) * 8);

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

