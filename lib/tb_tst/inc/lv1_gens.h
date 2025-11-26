/* Copyright 2025 Raphael Outhier - confidential - proprietary - no copy - no diffusion. */

/*
 * Level 1 updates generators.
 */

#ifndef TB_TST_LV1_GEN_H
#define TB_TST_LV1_GEN_H

/*********
 * Types *
 *********/

types(
	tb_tst_lv1_gen_spl,
	tb_tst_lv1_gen_rdm
);


/********************
 * Simple generator *
 ********************/

struct tb_tst_lv1_gen_spl {

	/* Generator base. */
	tb_tst_lv1_gen gen;

	/*
	 * Parameters.
	 */

	/* Bid tictick*/
	u64 bid0;

	/* Bid tick 1. */
	u64 bid1;

	/* Ask tick 0. */
	u64 ask0;

	/* Ask tick 1. */
	u64 ask1;

	/* Alternation period. */
	u64 prd;

	/*
	 * Context.
	 */
	
	/* Min tick. */
	u64 tck_min;

	/* Max tick. */
	u64 tck_max;

	/* Reference volume. */
	f64 ref_vol;

	/* Count until shift. */
	u64 cnt;

	/* Price 0 or price 1 ? */
	u8 is0;

};

/*
 * Construct.
 */
tb_tst_lv1_gen_spl *tb_tst_lv1_gen_spl_ctr(
	u64 bid0,
	u64 bid1,
	u64 ask0,
	u64 ask1,
	u64 prd
);

/**************************
 * Fully random generator *
 **************************/

/*
 * Realistic orderbook generator.
 */
struct tb_tst_lv1_gen_rdm {

	/* Generator base. */
	tb_tst_lv1_gen gen;

	/*
	 * Generation parameters.
	 */

	/* Generation base volume. */
	f64 gen_vol;

	/* Skip period. */
	u64 skp_prd;

	/* Orderbook total reset period. */
	u64 obk_rst_prd;

	/* Orderbook volume reset period. */
	u64 obk_vol_rst_prd;

	/* Orderbook normal bid period. 1. */

	/* Orderbook exceptional bid period. */
	u64 obk_bid_exc_prd;

	/* Orderbook bid reset period. */
	u64 obk_bid_rst_prd;

	/* Orderbook normal ask period. 1. */

	/* Orderbook exceptional ask period. */
	u64 obk_ask_exc_prd;

	/* Orderbook ask reset period. */
	u64 obk_ask_rst_prd;

	/*
	 * Generation context.
	 */

	/* Min tick. */
	u64 tck_min;

	/* Max tick. */
	u64 tck_max;

	/* Generation seed. */
	u64 gen_sed;

	/* Generation index. */
	u64 gen_idx;

	/* Current tick. */
	u64 tck_cur;

	/* Previous tick. */
	u64 tck_prv;

	/* Reference volume. */
	f64 ref_vol;
	
	/* Was the orderbook reset ? */
	u64 obk_rst;

	/* How long ago ? */
	u64 obk_rst_dly;

	/* Not sure about what that means anymore. */
	u64 obk_rst_skp;

};

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
);

#endif /* TB_TST_LV1_GEN_H */
