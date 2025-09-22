/* Copyright 2025 Raphael Outhier - confidential - proprietary - no copy - no diffusion. */

#include <tb_cor/tb_cor.all.h>

/***********
 * Globals *
 ***********/

/* System. */
static tb_ist_sys _sys = {};

/*******
 * API *
 *******/

/*
 * Deinitialize the instrument tracking system.
 * Deletes all active instruments.
 * Only here for valgrind to get off our backs.
 */
void tb_ist_cln(
	void
)
{
	tb_ist_sys *sys = &_sys;
	nh_spn_lck(&sys->lck);
	tb_ist *ist;
	ns_map_fe(ist, &sys->shrs, shr.shrs, str, in) {
		nh_fre_(ist);
	}
	ns_map_fe(ist, &sys->ccys, ccy.ccys, str, in) {
		nh_fre_(ist);
	}
	ns_map_str_init(&sys->shrs);
	ns_map_str_init(&sys->ccys);
	nh_spn_ulk(&sys->lck);
}

/*
 * Return a share instrument identifier.
 */
tb_ist *tb_ist_ctr_shr(
	tb_mkp *mkp,
	const char *sym
)
{

	/* Generate the name. */
	const char *mkp_val = tb_mkp_nam(mkp);
	TB_STR_GEN2(idt, mkp_val, sym);

	/* Lock. */
	tb_ist_sys *sys = &_sys;
	nh_spn_lck(&sys->lck);
	tb_ist *ist = ns_map_search(&sys->shrs, idt, str, tb_ist, shr.shrs);
	if (!ist) {
		nh_all_(ist);
		tb_str_cpy(ist->nam, idt);
		ist->cls = TB_IST_CLS_SHR;
		assert(!ns_map_str_put(&sys->shrs, &ist->shr.shrs, ist->nam));
		ist->shr.mkp = mkp;
		tb_str_cpy(ist->shr.sym, sym); 
	}
	nh_spn_ulk(&sys->lck);
	return ist;

}

/*
 * Return a currency instrument identifier.
 */
tb_ist *tb_ist_ctr_ccy(
	tb_mkp *mkp,
	tb_ccy *ccy
)
{

	/* Generate the name. */
	const char *mkp_val = tb_mkp_nam(mkp);
	const char *ccy_val = tb_ccy_nam(ccy);
	TB_STR_GEN2(idt, mkp_val, ccy_val);

	/* Lock. */
	tb_ist_sys *sys = &_sys;
	nh_spn_lck(&sys->lck);
	tb_ist *ist = ns_map_search(&sys->ccys, idt, str, tb_ist, ccy.ccys);
	if (!ist) {
		nh_all_(ist);
		tb_str_cpy(ist->nam, idt);
		ist->cls = TB_IST_CLS_CCY;
		assert(!ns_map_str_put(&sys->ccys, &ist->ccy.ccys, ist->nam));
		ist->ccy.mkp = mkp;
		ist->ccy.ccy = ccy;
	}
	nh_spn_ulk(&sys->lck);
	return ist;

}

/*
 * Print log data for an instrument.
 */
void tb_ist_log(
	ns_stm *stm,
	va_list *args
)
{
	tb_ist *ist = va_arg(*args, tb_ist *);
	const u8 cls = ist->cls;
	assert(cls < TB_IST_CLS_NB);
	if (cls == TB_IST_CLS_SHR) {
		ns_stm_writef(stm, "(SHR %s:%s)", tb_mkp_nam(ist->shr.mkp), ist->shr.sym);
	} else {
		ns_stm_writef(stm, "(CCY %s:%s)", tb_mkp_nam(ist->shr.mkp), tb_ccy_nam(ist->ccy.ccy));
	}	
}

