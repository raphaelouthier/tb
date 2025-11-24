/* Copyright 2025 Raphael Outhier - confidential - proprietary - no copy - no diffusion. */

#include <tb_tst/tb_tst.all.h>

/********************
 * Simple generator *
 ********************/

/*
 * Delete.
 */
static void _spl_dtr(
	tb_tst_lv1_gen *gen
)
{
	tb_tst_lv1_gen_spl *spl = ns_cnt_of(gen, tb_tst_lv1_gen_spl, gen);
	nh_fre_(spl);
}
/*
 * Init.
 */
static void _spl_ini(
	tb_tst_lv1_gen *gen,
	u64 sed,
	u64 tck_min,
	u64 tck_max,
	f64 ref_vol
)
{
	tb_tst_lv1_gen_spl *spl = ns_cnt_of(gen, tb_tst_lv1_gen_spl, gen);
}

/*
 * Tick update.
 */
static void _spl_tck_upd(
	tb_tst_lv1_gen *gen
)
{
	tb_tst_lv1_gen_spl *spl = ns_cnt_of(gen, tb_tst_lv1_gen_spl, gen);
}

/*
 * Orderbook update.
 */
static void _spl_obk_upd(
	tb_tst_lv1_gen *gen,
	f64 *obk
)
{
	tb_tst_lv1_gen_spl *spl = ns_cnt_of(gen, tb_tst_lv1_gen_spl, gen);
}

/*
 * Choose if we should skip.
 */
static u64 _spl_skp(
	tb_tst_lv1_gen *gen,
	u64 itr_idx
)
{
	return 0;
}

/*
 * Construct.
 */
tb_tst_lv1_gen_spl *tb_tst_lv1_gen_spl_ctr(
	void
)
{
	nh_all__(tb_tst_lv1_gen_spl, spl);
	spl->gen.dtr = &_spl_dtr;
	spl->gen.ini = &_spl_ini;
	spl->gen.tck_upd = &_spl_tck_upd;
	spl->gen.obk_upd = &_spl_obk_upd;
	spl->gen.skp = &_spl_skp;
	return spl; 
}

/********************
 * Random generator *
 ********************/

/*
 * Delete.
 */
static void _rdm_dtr(
	tb_tst_lv1_gen *gen
)
{
	tb_tst_lv1_gen_rdm *rdm = ns_cnt_of(gen, tb_tst_lv1_gen_rdm, gen);
	nh_fre_(rdm);
}

/*
 * Init.
 */
static void _rdm_ini(
	tb_tst_lv1_gen *gen,
	u64 sed,
	u64 tck_min,
	u64 tck_max,
	f64 ref_vol
)
{
	tb_tst_lv1_gen_rdm *rdm = ns_cnt_of(gen, tb_tst_lv1_gen_rdm, gen);
	rdm->tck_min = tck_min; 
	rdm->tck_max = tck_max; 
	rdm->gen_sed = ns_hsh_mas_gen(sed);
	rdm->gen_idx = 0;
	rdm->tck_cur = ns_hsh_u64_rng(rdm->gen_sed, tck_min, tck_max + 1, 1);
	rdm->tck_prv = rdm->tck_cur;
	rdm->ref_vol = ref_vol;
	rdm->obk_rst = 0;
	rdm->obk_rst_dly = 0;
	rdm->obk_rst_skp = 0;
}

/*
 * Tick update.
 */
static void _rdm_tck_upd(
	tb_tst_lv1_gen *gen
)
{
	tb_tst_lv1_gen_rdm *rdm = ns_cnt_of(gen, tb_tst_lv1_gen_rdm, gen);

	u64 tck_cur = rdm->tck_cur; 
	u64 gen_sed = rdm->gen_sed;
	rdm->tck_prv = rdm->tck_cur;

	/* Determine if we should go up or down. */
	u8 up_ok = tck_cur + 1 < rdm->tck_max;
	u8 dwn_ok = rdm->tck_min < tck_cur;
	assert(up_ok || dwn_ok);
	u8 is_dwn;
	if (!up_ok) {
		is_dwn = 1;
	} else if (!dwn_ok) {
		is_dwn = 0;
	} else {
		is_dwn = ((gen_sed >> 32) & 0xff) < 0x80;
	}
	gen_sed = ns_hsh_mas_gen(gen_sed);

	/* Determine the price increment logarithmically. */
	u8 inc_sed = gen_sed & 0xff;
	u8 inc_ord = 0;
	if (inc_sed < (1 << 1)) {
		inc_ord = 7;
	} else if (inc_sed < (1 << 2)) {
		inc_ord = 6;
	} else if (inc_sed < (1 << 3)) {
		inc_ord = 5;
	} else if (inc_sed < (1 << 4)) {
		inc_ord = 4;
	} else if (inc_sed < (1 << 5)) {
		inc_ord = 3;
	} else {
		inc_ord = 2;
	}
	check(inc_ord);
	u64 inc_max = 1 << inc_ord;
	assert(inc_max < rdm->tck_min); 
	assert(inc_max < inc_max + rdm->tck_max);
	gen_sed = rdm->gen_sed = ns_hsh_mas_gen(gen_sed);

	/* Determine the actual increment. */
	const u64 inc_val = ns_hsh_u64_rng(gen_sed, 0, inc_max, 1);

	/* Determine the new target price. */
	if (is_dwn) {
		tck_cur = tck_cur - inc_val;
		if (tck_cur < rdm->tck_min) tck_cur = rdm->tck_min;
	} else {
		tck_cur = tck_cur + inc_val;
		if (tck_cur >= rdm->tck_max) tck_cur = rdm->tck_max - 1;
	}
	rdm->gen_sed = ns_hsh_mas_gen(gen_sed);
	rdm->tck_cur = tck_cur;

}

/*
 * Orderbook update.
 */
static void _rdm_obk_upd(
	tb_tst_lv1_gen *gen,
	f64 *obk
)
{
	tb_tst_lv1_gen_rdm *rdm = ns_cnt_of(gen, tb_tst_lv1_gen_rdm, gen);

	/* Constants. */
	const f64 ref_vol = rdm->ref_vol; 

	/* Read the current and previous prices. */
	const u64 tck_cur = rdm->tck_cur;
	const u64 tck_prv = rdm->tck_prv;
	debug("tck_cur %U.\n", tck_cur);
	debug("tck_prv %U.\n", tck_prv);
	const u64 tck_min = rdm->tck_min;
	const u64 tck_max = rdm->tck_max;
	assert(tck_min > 400);
	#define OBK(idx) (obk[((idx) - tck_min)])
	#define OBK_WRT(idx, val) ({ \
		if ((tck_min <= idx) && (idx < tck_max)) { \
			OBK(idx) = (val); \
		} \
	})

	/* Read parameters. */
	u64 gen_sed = rdm->gen_sed;
	const u64 gen_idx = rdm->gen_idx;

	/* Reset ticks in between. */
	{
		assert(!(OBK(tck_prv)));
		u64 rst_stt = tck_cur; 
		u64 rst_end = tck_prv; 
		if (rst_end < rst_stt) {
			_swap(rst_end, rst_stt);
		}
		for (;rst_stt <= rst_end; rst_stt++) {
			OBK(rst_stt) = 0;
		}
		assert(!(OBK(tck_cur)));
	}

	/* Reset around the current tick. */
	{
		if (gen_sed & (1 << 20)) {
			u8 rst_up = (gen_sed & 0x7);
			u8 rst_dwn = ((gen_sed >> 3) & 0x7);
			for (
				u64 rst_stt = (tck_cur - rst_dwn), rst_end = (tck_cur + rst_up);
				rst_stt <= rst_end;
				rst_stt++
			) {
				if ((tck_min <= rst_stt) && (rst_stt < tck_max)) {
					OBK(rst_stt) = 0;
				}	
			}
		}
	}
	gen_sed = ns_hsh_mas_gen(gen_sed);

	/* Reset the orderbook if required. */ 
	if (gen_idx && (!(gen_idx % rdm->obk_rst_prd))) {
		for (u64 idx = tck_min; idx < tck_max; idx++) {
			OBK(idx) = 0;
		}

		/* Skip all other generation steps. */
		goto end;
	}

	/* Normal and exceptional steps. */
	static u64 nrm_stps[4] = {
		1,
		1,
		5,
		2		
	};

	/* Apply normal bids and ask updates on 10 ticks around the current price. */
	if (1) { //!(gen_idx % rdm->obk_ask_nrm_prd)) {
		f64 ask_max = ns_hsh_f64_rng(gen_sed, ref_vol / 2, ref_vol, ref_vol / 100);
		const u64 stp = nrm_stps[(gen_idx) % 4];
		for (u64 idx = tck_cur, ctr = 0; ((idx += stp) < tck_max) && (ctr++ < 10);) {
			OBK(idx) = ask_max;
			ask_max /= 1.1;
		}	
		gen_sed = ns_hsh_mas_gen(gen_sed);
	}
	if (1) { //!(gen_idx % rdm->obk_bid_nrm_prd)) {
		f64 bid_max = -ns_hsh_f64_rng(gen_sed, ref_vol / 2, ref_vol, ref_vol / 100);
		const u64 stp = nrm_stps[(gen_idx) % 4];
		for (u64 idx = tck_cur, ctr = 0; (tck_min <= (idx -= stp)) && (ctr++ < 10);) {
			OBK(idx) = bid_max;
			bid_max /= 1.1;
		}	
		gen_sed = ns_hsh_mas_gen(gen_sed);
	}

	/* Skip resets if first iteration. */
	if (!gen_idx) goto end;

	static u64 exc_stps[3] = {
		100,
		200,
		400
	};
	/* Apply exceptional bids and asks on the whole orderbook. */
	if (!(gen_idx % rdm->obk_ask_exc_prd)) {
		f64 ask_max = ns_hsh_f64_rng(gen_sed, ref_vol / 2, ref_vol, ref_vol / 100);
		const u64 stp = exc_stps[(gen_idx / rdm->obk_ask_exc_prd) % 3];
		for (u64 idx = tck_cur; (idx += stp) < tck_max;) {
			OBK(idx) = ask_max;
			ask_max /= 1.1;
		}	
		gen_sed = ns_hsh_mas_gen(gen_sed);
	}
	if (!(gen_idx % rdm->obk_bid_exc_prd)) {
		f64 bid_max = -ns_hsh_f64_rng(gen_sed, ref_vol / 2, ref_vol, ref_vol / 100);
		const u64 stp = exc_stps[(gen_idx / rdm->obk_bid_exc_prd) % 3];
		for (u64 idx = tck_cur; tck_min <= (idx -= stp);) {
			OBK(idx) = bid_max;
			bid_max /= 1.1;
		}	
		gen_sed = ns_hsh_mas_gen(gen_sed);
	}

	static u64 rst_stps[3] = {
		12,
		29,
		51
	};
	/* Apply resets on the whole orderbook. */
	if (gen_idx && !(gen_idx % rdm->obk_ask_rst_prd)) {
		const u64 stp = rst_stps[(gen_idx / rdm->obk_ask_rst_prd) % 3];
		for (u64 idx = tck_cur; (idx += stp) < tck_max;) {
			OBK(idx) = 0;
		}	
		gen_sed = ns_hsh_mas_gen(gen_sed);
	}
	if (gen_idx && !(gen_idx % rdm->obk_bid_rst_prd)) {
		const u64 stp = rst_stps[(gen_idx / rdm->obk_bid_rst_prd) % 3];
		for (u64 idx = tck_cur; tck_min <= (idx -= stp);) {
			OBK(idx) = 0;
		}	
		gen_sed = ns_hsh_mas_gen(gen_sed);
	}

	/* Save post generation params. */	
	end:;
	rdm->gen_sed = gen_sed;
	rdm->gen_idx = gen_idx + 1;

}

/*
 * Choose if we should skip.
 */
static u64 _rdm_skp(
	tb_tst_lv1_gen *gen,
	u64 itr_idx
)
{
	tb_tst_lv1_gen_rdm *rdm = ns_cnt_of(gen, tb_tst_lv1_gen_rdm, gen);

	/* Periodically let some time pass, up to 10 columns. */
	u64 skp_nb = 0;
	if (!(itr_idx % rdm->skp_prd)) {

		/* Determine how much time should pass. */
		skp_nb = ns_hsh_u8_rng(rdm->gen_sed, 1, 30, 1);
		if (rdm->obk_rst) {
			SAFE_INCR(rdm->obk_rst_dly);
			if (skp_nb > 20) {
				SAFE_INCR(rdm->obk_rst_skp);
			}
		}
	}

	/* Return the select skip amount. */
	return skp_nb;
}

/*
 * Construct.
 */
tb_tst_lv1_gen_rdm *tb_tst_lv1_gen_rdm_ctr(
	u64 skp_prd,
	u64 obk_rst_prd,
	u64 obk_vol_rst_prd,
	u64 obk_bid_exc_prd,
	u64 obk_ask_exc_prd,
	u64 obk_bid_rst_prd,
	u64 obk_ask_rst_prd
)
{
	nh_all__(tb_tst_lv1_gen_rdm, rdm);
	rdm->gen.dtr = &_rdm_dtr;
	rdm->gen.ini = &_rdm_ini;
	rdm->gen.tck_upd = &_rdm_tck_upd;
	rdm->gen.obk_upd = &_rdm_obk_upd;
	rdm->gen.skp = &_rdm_skp;
	rdm->skp_prd = skp_prd;
	rdm->obk_rst_prd = obk_rst_prd;
	rdm->obk_vol_rst_prd = obk_vol_rst_prd;
	rdm->obk_bid_exc_prd = obk_bid_exc_prd;
	rdm->obk_bid_rst_prd = obk_bid_rst_prd;
	rdm->obk_ask_exc_prd = obk_ask_exc_prd;
	rdm->obk_ask_rst_prd = obk_ask_rst_prd;
	return rdm; 
}

