/* Copyright 2025 Raphael Outhier - confidential - proprietary - no copy - no diffusion. */

#ifndef SP_WLT_H
#define SP_WLT_H

/*
 * The wallet references assets that are immediately
 * available to place orders.
 *
 * It does not track any asset involved in an order.
 *
 * Assets are : 
 * - stocks.
 * - depots in a given currency.
 *
 * Assets of the same type are undifferentiated (every dollar/euro/company
 * stock is the same, and as such, are tracked by available amount.
 *
 * The wallet is updated by the creation, execution or
 * cancellation of orders.
 */

/*********
 * Types *
 *********/

types(
	sp_wsk,
	sp_wdp,
	sp_wlt
);

/**************
 * Structures *
 **************/

/*
 * An account stock is a tradeable stock owned by an wallet.
 */ 
struct sp_wsk {

	/* Stocks of the same wallet sorted by "marketplace:symbol". */
	ns_mapn_str wsks;

	/* Available amount. */
	u64 nb;

	/* Currency. */
	sp_ccy *ccy;

};

/*
 * A wallet depot is a single currency money container.
 */
struct sp_wdp {

	/* Depots of the same wallet sorted bycurrency id. */
	ns_mapn_uad wdps_ccy;

	/* Depots of the same wallet sorted by currency name. */
	ns_mapn_str wdps_nam;

	/* Amount of money. Positive. */
	sp_ma nb;

};

/*
 * Wallet.
 */
struct sp_wlt {

	/* Depots sorted by currency id. */
	ns_map_uad wdps_ccy;

	/* Depots sorted by currency name. */
	ns_map_str wdps_nam;

	/* Stocks sorted by symbol name. */
	ns_map_str wsks;

};

/*************
 * Lifecycle *
 *************/

/*
 * Construct an empty wallet.
 */
sp_wlt *sp_wlt_ctor(
	void
);

/*
 * Destruct @wlt.
 */
void sp_wlt_dtor(
	sp_wlt *wlt
);

/*
 * Create and return a clone of @src.
 * It must have no parent.
 */
sp_wlt *sp_wlt_clone(
	sp_wlt *src
);

/*
 * If @wlt has stocks, return 1.
 * Otherwise, return 0.
 */
static inline u8 sp_wlt_has_wsk(
	sp_wlt *src
) {return !ns_map_str_empty(&src->wsks);}

/*
 * If @wlt0 and wlt1 are the same, return 0.
 * If they differ, return 1.
 */
uerr sp_wlt_cmp(
	sp_wlt *wlt0,
	sp_wlt *wlt1
);

/*
 * Generate and return a descriptor of @wlt.
 */
ct_val *sp_wlt_ct_exp(
	sp_wlt *wlt
);

/*
 * Construct and return a wallet from @dsc.
 * If an error occurs, return 0.
 */
sp_wlt *sp_wlt_ct_imp(
	ct_val *dsc
);

/*************
 * Money API *
 *************/

/*
 * Return the amount of money in depot of currency @ccy of @wlt.
 * If no such depot exists, return 0.
 */
sp_ma sp_wlt_mny_get(
	sp_wlt *wlt,
	sp_ccy *ccy
);

/*
 * Return the amount of money in depot of currency named @name of @wlt.
 * If no such depot exists, return 0.
 */
sp_ma sp_wlt_mny_get_name(
	sp_wlt *wlt,
	const char *name
);

/*************
 * Stock API *
 *************/

/*
 * Return the amount of stocks named @nam available
 * in @wlyt.
 */
u64 sp_wlt_stk_get(
	sp_wlt *wlt,
	const char *mkp,
	const char *sym
);

/******************
 * Order resource *
 ******************/

/*
 * Take resources from @wlt to pass an order described by @ordd and return 0.
 * If an error occurs, return 1.
 */
uerr sp_wlt_ord_res_tak(
	sp_wlt *wlt,
	const sp_ordd *ordd
);

/*
 * Release resources from @wlt after the order described by @ordd
 * was cancelled.
 */
uerr sp_wlt_ord_res_rel(
	sp_wlt *wlt,
	const sp_ordd *ordd
);

/*
 * Add new resources to @wlt after the order described by @ordd completed.
 */
uerr sp_wlt_ord_res_col(
	sp_wlt *wlt,
	const sp_ordd *ordd
);

/******************
 * Simulation API *
 ******************/

/*
 * API available only when simulation is enabled.
 */

/*
 * Add @nb money of @ccy to @wlt.
 */
void sp_wlt_mny_add(
	sp_wlt *wlt,
	sp_ma nb,
	sp_ccy *ccy
);

#endif /* SP_WLT_H */
