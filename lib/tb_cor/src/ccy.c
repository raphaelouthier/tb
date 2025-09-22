/* Copyright 2025 Raphael Outhier - confidential - proprietary - no copy - no diffusion. */

#include <tb_cor/tb_cor.all.h>

/**************
 * Currencies *
 **************/

/*
 * Define currencies.
 */
#define TB_CCY(nam, sym) tb_ccy tb_ccy_##sym = {{}, #sym, #nam};
#include <tb_cor/ccys.h>
#undef TB_CCY

/***********
 * Globals *
 ***********/

/* Currencies system. */
static tb_ccy_sys _ccy_sys = {};

/* Currencies init status. */
static u8 _ccy_ini = 0;

/* Define the currencies array. */
static tb_ccy *_ccy_arr[] = {
#define TB_CCY(nam, sym) &tb_ccy_##sym,
#include <tb_cor/ccys.h>
#undef TB_CCY
};

/* Compute the currencies array size. */
static const u32 _ccys_nb = sizeof(_ccy_arr) / sizeof(_ccy_arr[0]);

/********
 * Init *
 ********/

/*
 * Init the currencies map.
 */
static inline void _sys_ini(
	tb_ccy_sys *sys
)
{
	check(nh_spn_lkd(&sys->lck));
	if (_ccy_ini) return;
	ns_map_str_init(&sys->ccys);
	for (u32 ccy_id = 0; ccy_id < _ccys_nb; ccy_id++) {
		ns_map_str_put(&sys->ccys, &_ccy_arr[ccy_id]->ccys, _ccy_arr[ccy_id]->sym);
	}
}

/************
 * Currency *
 ************/

/*
 * Print log data for a currency.
 */
void tb_ccy_log(
	ns_stm *stm,
	va_list *args
)
{
	tb_ccy *ccy = va_arg(*args, tb_ccy *);
	ns_stm_writes(stm, ccy->sym);
}

/*
 * Currency lookup by symbol.
 */
tb_ccy *tb_ccy_sch(
	const char *sym
)
{
	tb_ccy_sys *sys = &_ccy_sys;
	nh_spn_lck(&sys->lck); 
	_sys_ini(sys);
	tb_ccy *ccy = ns_map_search(&sys->ccys, sym, str, tb_ccy, ccys);
	nh_spn_ulk(&sys->lck); 
	return ccy;
}

