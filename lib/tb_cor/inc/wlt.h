/* Copyright 2025 Raphael Outhier - confidential - proprietary - no copy - no diffusion. */

#ifndef TB_WLT_H
#define TB_WLT_H

/*
 * The wallet tracks assets.
 *
 * Assets are quantified intances of instruments.
 *
 * Assets instance of the same instrument are
 * non-differentiable, and hence, are measured by amount.
 */

/*********
 * Types *
 *********/

types(
	tb_ast,
	tb_wlt
);

/**************
 * Structures *
 **************/

/*
 * An asset is a quantified instance of an instrument.
 */ 
struct tb_ast {

	/* Stocks of the same wallet sorted by intrument. */
	ns_mapn_uad asts;

	/* Amount. */
	f64 amt;

};

/*
 * Wallet.
 */
struct tb_wlt {

	/* Assets indexed by instrument. */
	ns_map_uad asts;

};

/*************
 * Lifecycle *
 *************/

/*
 * Construct an empty wallet.
 */
tb_wlt *tb_wlt_ctor(
	void
);

/*
 * Destruct @wlt.
 */
void tb_wlt_dtor(
	tb_wlt *wlt
);

/*
 * Create and return a clone of @src.
 * It must have no parent.
 */
tb_wlt *tb_wlt_cln(
	tb_wlt *src
);

/*
 * If @wlt0 and @wlt1 have exactly the same amounts of
 * the same instruments, return 0.
 */
uerr tb_wlt_cmp(
	tb_wlt *wlt0,
	tb_wlt *wlt1
);

/*
 * Return the amount of instruments @ist in @wlt if any
 * and 0 otherwise.
 */
f64 tb_wlt_get(
	tb_wlt *wlt,
	tb_ist *ist
);

/********************
 * Order processing *
 ********************/

/*
 * Take resources from @wlt to pass an order
 * described by @ord and return 0.
 * If an error occurs, return 1.
 */
uerr tb_wlt_ord_tak(
	tb_wlt *wlt,
	const tb_ord *ord
);

/*
 * Release resources from @wlt after the order
 * described by @ord was cancelled.
 */
uerr tb_wlt_ord_rel(
	tb_wlt *wlt,
	const tb_ord *ord
);

/*
 * Add new resources to @wlt after the order
 * described by @ord completed.
 */
uerr tb_wlt_ord_col(
	tb_wlt *wlt,
	const tb_ord *ord
);

/******************
 * Simulation API *
 ******************/

/*
 * API available only when simulation is enabled.
 */

/*
 * Increase the anount of instances of @ist by @amt.
 */
void tb_wlt_sim_add(
	tb_wlt *wlt,
	tb_ist *ist,
	f64 amt
);

#endif /* TB_WLT_H */
