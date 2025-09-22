/* Copyright 2025 Raphael Outhier - confidential - proprietary - no copy - no diffusion. */

#ifndef TB_CCY_H
#define TB_CCY_H

/*********
 * Types *
 *********/

types(
	tb_ccy,
	tb_ccy_sys
);

/**************
 * Structures *
 **************/

/*
 * Currency identifier.
 * Uniquely identifies a currency.
 * Can be used for indexing,
 */
struct tb_ccy {

	/* Currencies. */
	ns_mapn_str ccys;

	/* Symbol. */
	const char *sym;

	/* Name. */
	const char *nam;

};

struct tb_ccy_sys {

	/* Lock. */
	nh_spn lck;

	/* Currencies map. */
	ns_map_str ccys;

};

/****************
 * Declarations *
 ****************/

/*
 * Declare all currencies.
 */
#define TB_CCY(nam, sym) extern tb_ccy tb_ccy_##sym;
#include <tb_cor/ccys.h>
#undef TB_CCY

/*******
 * API *
 *******/

/*
 * Return @ccy's symbol.
 */
static inline const char *tb_ccy_sym(
	tb_ccy *ccy
) {return ccy->sym;}

/*
 * Return @ccy's name.
 */
static inline const char *tb_ccy_nam(
	tb_ccy *ccy
) {return ccy->nam;}

/*
 * Print log data for a currency.
 */
void tb_ccy_log(
	ns_stm *stm,
	va_list *args
);
#define tb_ccy_dsc(val) &tb_ccy_log, val

/*
 * Lookup a currency by symbol.
 */
tb_ccy *tb_ccy_sch(
	const char *sym
);

#endif /* TB_CCY_H */
