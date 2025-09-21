/* Copyright 2025 Raphael Outhier - confidential - proprietary - no copy - no diffusion. */

#include <sp_cor/sp_cor.all.h>

/*
 * Return a descriptor of @ma using @gen.
 */
ct_val *sp_ma_ct_exp(
	sp_ma *ma
)
{
	return ct_exp(ma,
		(s64, itg),
		(u16, dec)
	);
}

/*
 * Import a money amount at @ma from @val and return 0.
 * If the import fails, log and return 1.
 */ 
uerr sp_ma_ct_imp(
	sp_ma *ma,
	ct_val *val
)
{
	ct_args(val, return 1,
		(s64, itg),
		(u16, dec)
	);
	if (dec >= SP_MA_DEC_RES) {
		error("invalid decimal part '%u' : must be lower than 1000.\n", dec);
		return 1;
	}
	*ma = (sp_ma) {itg, dec};
	return 0;
}

//#define MA_LOG_FULL

/*
 * Pring log data for @ma.
 */
void sp_ma_log(
	ns_stm *stm,
	va_list *args
)
{
	sp_ma *ma = va_arg(*args, sp_ma *);
	check(ma->dec < SP_MA_DEC_RES);
#ifdef MA_LOG_FULL
	ns_stm_writef(stm, "(%d = %I:%u)", sp_ma_to_flt(*ma), ma->itg, ma->dec);
#else
	ns_stm_writef(stm, "%d", sp_ma_to_flt(*ma));
#endif
}


