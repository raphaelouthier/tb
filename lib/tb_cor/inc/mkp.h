/* Copyright 2025 Raphael Outhier - confidential - proprietary - no copy - no diffusion. */

#ifndef TB_MKP_H
#define TB_MKP_H

/*********
 * Types *
 *********/

types(
	tb_mkp,
	tb_mkp_sys
);

/**************
 * Structures *
 **************/

/*
 * Marketplace identifier.
 * Uniquely identifies a currency.
 * Can be used for indexing,
 */
struct tb_mkp {

	/* Marketplaces. */
	ns_mapn_str mkps;

	/* Symbol. */
	const char *sym;

	/* Name. */
	const char *nam;

	/* Primary currency symbol if any. */
	const char *ccy_sym;

	/* Primary currency if any. */
	tb_ccy *ccy;
	

};

struct tb_mkp_sys {

	/* Lock. */
	nh_spn lck;

	/* Marketplaces map. */
	ns_map_str mkps;

};

/****************
 * Declarations *
 ****************/

/*
 * Declare all currencies.
 */
#define TB_MKP(nam, sym, ccy) extern tb_mkp tb_mkp_##sym;
#include <tb_cor/mkps.h>
#undef TB_MKP

/*******
 * API *
 *******/

/*
 * Return @mkp's symbol.
 */
static inline const char *tb_mkp_sym(
	tb_mkp *mkp
) {return mkp->sym;}

/*
 * Return @mkp's name.
 */
static inline const char *tb_mkp_nam(
	tb_mkp *mkp
) {return mkp->nam;}

/*
 * Return @mkp's primary currency if any.
 */
static inline tb_ccy *tb_mkp_ccy(
	tb_mkp *mkp
) {return mkp->ccy;}

/*
 * Print log data for a marketplace.
 */
void tb_mkp_log(
	ns_stm *stm,
	va_list *args
);
#define tb_mkp_dsc(val) &tb_mkp_log, val

/*
 * Lookup a marketplace by symbol.
 */
tb_mkp *tb_mkp_sch(
	const char *sym
);

#endif /* TB_MKP_H */
