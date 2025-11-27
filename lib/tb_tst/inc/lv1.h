/* Copyright 2025 Raphael Outhier - confidential - proprietary - no copy - no diffusion. */

#ifndef TB_TST_LV1_H
#define TB_TST_LV1_H

/*********
 * Types *
 *********/

types(
	tb_tst_lv1_gen,
	tb_tst_lv1_upd,
	tb_tst_lv1_ctx
);

/**************
 * Structures *
 **************/

/*
 * A generator generates updates on an
 * orderbook in a implementation-defined
 * manner.
 */
struct tb_tst_lv1_gen {
	
	/*
	 * Destructor.
	 */
	void (*dtr)(
		tb_tst_lv1_gen *gen
	);

	/*
	 * Price init.
	 */
	void (*ini)(
		tb_tst_lv1_gen *gen,
		u64 sed,
		u64 tck_min,
		u64 tck_max,
		f64 ref_vol
	);

	/*
	 * Tick update.
	 */
	void (*tck_upd)(
		tb_tst_lv1_gen *gen
	);

	/*
	 * Orderbook update.
	 */
	void (*obk_upd)(
		tb_tst_lv1_gen *gen,
		f64 *obk
	);

	/*
	 * Choose the skip count for an iteration.
	 */
	u64 (*skp)(
		tb_tst_lv1_gen *gen,
		u64 itr_idx
	);

};


/*
 * Orderbook update.
 */
struct tb_tst_lv1_upd {

	/* Price of update. */
	f64 prc;

	/* Volume of update. */
	f64 vol;

	/* Time of update. */
	u64 tim;

};

/*
 * Orderbook generation context.
 */
struct tb_tst_lv1_ctx {

	/*
	 * Request.
	 */

	/* Seed. */
	u64 sed;

	/* Number of time units. */
	u64 unt_nbr;

	/* Start time. */
	u64 tim_stt;

	/* Time increment. */
	u64 tim_inc;

	/* Aid width = @tim_stt * @tim_inc. */ 
	u64 aid_wid;

	/* Price minimum. */
	f64 prc_min;

	/* Total number of ticks in the orderbook. */
	u64 tck_nbr;

	/* Number of time beats per unit. */
	u64 tim_stp;

	/* Ticks per price unit. */
	u64 tck_rat;

	/* Heatmap tick dimension. */
	u64 hmp_dim_tck;

	/* Heatmap time dimension. */
	u64 hmp_dim_tim;

	/* Bid/ask curves size. */
	u64 bac_siz;

	/* Reference volume. */
	f64 ref_vol;

	/*
	 * Generation stats.
	 */

	/* Current time. */
	u64 tim_cur;

	/* Tick min. */
	u64 tck_min;

	/* Tick max. */
	u64 tck_max;

	/* Did an orderbook reset happen at last orderbook update ? */
	u8 obk_rst;

	/* Number of times the orderbook was reset. */
	u64 obk_rst_ctr;

	/* Number of times the orderbook was reset before a delay. */
	u64 obk_rst_dly;

	/* Number of times the orderbook was reset before a delay spanning at least one complete cell. */
	u64 obk_rst_skp;

	/*
	 * Generation-output.
	 */

	/* Number of updates. */
	u64 upd_nbr;

	/* Maximal number of updates. */
	u64 upd_max;

	/* Updates array. */
	tb_tst_lv1_upd *upds;

	/* Scratch orderbook to play with. */
	f64 *obk;

	/* Bid array[unt_nbr]. */
	u64 *bid_arr;

	/* Ask array[unt_nbr]. */
	u64 *ask_arr;

	/* Bid array for first value of each cell[unt_nbr]. */
	u64 *bid_ini;

	/* Ask array for first value of each cell[unt_nbr]. */
	u64 *ask_ini;

	/* Tick reference array. */
	u64 *ref_arr;

	/* Global heatmap [unt_nbr * tim_stp][tck_nbr]. */
	f64 *hmp;

	/* Init heatmap. Each cell has the resting value for its price/time. */
	f64 *hmp_ini;

	/* Time of last update. */
	u64 tim_end;

};

/************
 * Internal *
 ************/

/*
 * Generate a sequence of orderbook updates as
 * specified by @ctx.
 */
void tb_tst_lv1_upds_gen(
	tb_tst_lv1_ctx *ctx,
	tb_tst_lv1_gen *gen
);

/*
 * Delete generated updates.
 */
void tb_tst_lv1_upds_del(
	tb_tst_lv1_ctx *ctx
);

/*******
 * API *
 *******/

/*
 * Entrypoint for lv1 tests.
 */
void tb_tst_lv1(
	nh_tst_sys *sys,
	u64 sed,
	u8 wrk_nb,
	u8 run_prc
);

#endif /* TB_TST_LV1_H */
