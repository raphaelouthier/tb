/* Copyright 2025 Raphael Outhier - confidential - proprietary - no copy - no diffusion. */

#include <tb_cor/tb_cor.all.h>

/**************
 * Marketplaces *
 **************/

/*
 * Define marketplaces.
 */
#define TB_MKP(nam, sym, ccy) tb_mkp tb_mkp_##sym = {{}, #sym, #nam, NS_PRP_CDS_EMP(ccy) (0) (#ccy), 0};
#include <tb_cor/mkps.h>
#undef TB_MKP

/***********
 * Globals *
 ***********/

/* Marketplaces system. */
static tb_mkp_sys _mkp_sys = {};

/* Marketplaces init status. */
static u8 _mkp_ini = 0;

/* Define the marketplaces array. */
static tb_mkp *_mkp_arr[] = {
#define TB_MKP(nam, sym, ccy) &tb_mkp_##sym,
#include <tb_cor/mkps.h>
#undef TB_MKP
};

/* Compute the marketplaces array size. */
static const u32 _mkps_nb = sizeof(_mkp_arr) / sizeof(_mkp_arr[0]);

/********
 * Init *
 ********/

/*
 * Init the marketplaces map.
 */
static inline void _sys_ini(
	tb_mkp_sys *sys
)
{
	check(nh_spn_lkd(&sys->lck));
	if (_mkp_ini) return;
	ns_map_str_init(&sys->mkps);
	for (u32 mkp_id = 0; mkp_id < _mkps_nb; mkp_id++) {
		ns_map_str_put(&sys->mkps, &_mkp_arr[mkp_id]->mkps, _mkp_arr[mkp_id]->sym);
	}
}

/************
 * Currency *
 ************/

/*
 * Print log data for a currency.
 */
void tb_mkp_log(
	ns_stm *stm,
	va_list *args
)
{
	tb_mkp *mkp = va_arg(*args, tb_mkp *);
	ns_stm_writes(stm, mkp->sym);
}

/*
 * Currency lookup by symbol.
 */
tb_mkp *tb_mkp_sch(
	const char *sym
)
{
	tb_mkp_sys *sys = &_mkp_sys;
	nh_spn_lck(&sys->lck); 
	_sys_ini(sys);
	tb_mkp *mkp = ns_map_search(&sys->mkps, sym, str, tb_mkp, mkps);
	nh_spn_ulk(&sys->lck); 
	return mkp;
}

