/* Copyright 2025 Raphael Outhier - confidential - proprietary - no copy - no diffusion. */

#include <sp_cor/sp_cor.all.h>

sp_lib_log_def(cor, wlt);
#define _wlt_log(...) sp_log_lib(cor, wlt, __VA_ARGS__)

/*
 * Return @wdp's currency,
 */
static inline sp_ccy *_wdp_ccy(
	sp_wdp *wdp
) {return (sp_ccy *) wdp->wdps_ccy.value;}

/*
 * Pring log data for @ma.
 */
void _wdp_log(
	ns_stm *stm,
	va_list *args
)
{
	sp_wdp *wdp = va_arg(*args, sp_wdp *);
	ns_stm_writef(stm, "depot (%s, %@)",
		_wdp_ccy(wdp), sp_ma_dsc(wdp->nb)
	);
}
#define _wdp_dsc(wdp) &_wdp_log, wdp

/*
 * Return @wsk's currency,
 */
static inline sp_ccy *_wsk_ccy(
	sp_wsk *wsk
) {return (sp_ccy *) wsk->ccy;}

/*
 * Pring log data for @ma.
 */
void _wsk_log(
	ns_stm *stm,
	va_list *args
)
{
	sp_wsk *wsk = va_arg(*args, sp_wsk *);
	ns_stm_writef(stm, "stock (%s, %U)",
		_wsk_ccy(wsk), wsk->nb
	);
}
#define _wsk_dsc(wsk) &_wsk_log, wsk

/*
 * Check @wdp.
 */
static void _wdp_check(
	sp_wlt *wlt,
	sp_wdp *wdp
)
{
	check(!sp_ma_n(wdp->nb));
}

/*
 * Check @wsk.
 */
static void _wsk_check(
	sp_wlt *wlt,
	sp_wsk *wsk
) {}

/*
 * If @wlt has a depot in currency @ccy, return it.
 * Otherwise, return 0.
 */
static inline sp_wdp *_wdp_csch(
	sp_wlt *wlt,
	sp_ccy *ccy
) {
	sp_wdp *wdp = ns_map_search(&wlt->wdps_ccy, (uad) ccy, uad, sp_wdp, wdps_ccy);
	if (wdp) _wdp_check(wlt, wdp);
	return wdp;
}	

/*
 * If @wlt has a depot in currency named @name, return it.
 * Otherwise, return 0.
 */
static inline sp_wdp *_wdp_nsch(
	sp_wlt *wlt,
	const char *name
) {
	sp_wdp *wdp = ns_map_search(&wlt->wdps_nam, name, str, sp_wdp, wdps_nam);
	_wdp_check(wlt, wdp);
	return wdp;
}	

/*
 * If @wlt has a depot in currency @name, return it.
 * Otherwise, return 0.
 */
static inline sp_wsk *_wsk_sch(
	sp_wlt *wlt,
	const char *name
) {
	sp_wsk *wsk = ns_map_search(&wlt->wsks, name, str, sp_wsk, wsks);
	if (wsk) _wsk_check(wlt, wsk);
	return wsk;
}	

/*******************
 * Depot internals *
 *******************/

/*
 * Destruct @wdp from @wlt.
 */
static inline void _wdp_dtor(
	sp_wlt *wlt,
	sp_wdp *wdp
)
{
	_wlt_log("[%p] wdp_del %s : %@.\n", wlt, wdp->wdps_nam.value,
		sp_ma_dsc(wdp->nb));
	_wdp_check(wlt, wdp);
	ns_map_uad_remove(&wlt->wdps_ccy, &wdp->wdps_ccy);
	ns_map_str_remove(&wlt->wdps_nam, &wdp->wdps_nam);
	nh_fre_(wdp);
}

/*
 * Create and return a depot for currency @ccy with amount @nb in @wlt.
 */
static inline sp_wdp *_wdp_ctor(
	sp_wlt *wlt,
	sp_ccy *ccy,
	sp_ma nb
)
{
	assert(!sp_ma_z(nb));
	check(!sp_ma_n(nb));

	/* Create. */
	nh_all__(sp_wdp, wdp);
	uerr err = ns_map_uad_put(&wlt->wdps_ccy, &wdp->wdps_ccy, (uad) ccy);
	assert(!err, "wdp_id insertion error.\n");
	err = ns_map_str_put(&wlt->wdps_nam, &wdp->wdps_nam, ccy->nam);
	assert(!err, "wdp_name insertion error.\n");
	wdp->nb = nb;
	_wlt_log("[%p] wdp ccy %s nb %@\n", wlt, wdp->wdps_nam.value, sp_ma_dsc(wdp->nb));

	/* Complete. */
	_wdp_check(wlt, wdp);
	return wdp;

}

/*
 * Add @nb money in @wdp and return 0.
 * If an error occurs, return 1.
 */
static inline void _wdp_add(
	sp_wlt *wlt,
	sp_wdp *wdp,
	sp_ma nb
)
{
	check(!sp_ma_n(nb));
	_wdp_check(wlt, wdp);
	sp_ma sum = sp_ma_sum(nb, wdp->nb);
	_wlt_log("[%p] wdp_add %s : %@ -> %@.\n", wlt, wdp->wdps_nam.value,
		sp_ma_dsc(wdp->nb), sp_ma_dsc(sum));
	wdp->nb = sum;
	_wlt_log("[%p] wdp ccy %s nb %@\n", wlt, wdp->wdps_nam.value, sp_ma_dsc(wdp->nb));
	_wdp_check(wlt, wdp);
}

/*
 * Remove @nb money in @wdp and return 0.
 * If an error occurs, return 1.
 */
static inline uerr _wdp_rem(
	sp_wlt *wlt,
	sp_wdp *wdp,
	sp_ma nb
)
{
	_wdp_check(wlt, wdp);
	sp_ma new_nb = sp_ma_dif(wdp->nb, nb);
	_wlt_log("[%p] wdp_rem %s : %@ -> %@ : %s\n", wlt, wdp->wdps_nam.value,
		sp_ma_dsc(wdp->nb), sp_ma_dsc(new_nb), sp_ma_n(new_nb) ? "error (< 0)" : "OK");
	if (sp_ma_n(new_nb)) {
		return 1;
	}
	wdp->nb = new_nb;
	_wlt_log("[%p] wdp ccy %s nb %@\n", wlt, wdp->wdps_nam.value, sp_ma_dsc(wdp->nb));
	if (sp_ma_z(wdp->nb)) _wdp_dtor(wlt, wdp);
	else {_wdp_check(wlt, wdp);}
	return 0;
}


/*
 * Add @nb of currency @ccy in @wlt and return 0.
 * If an error occurs, return 1.
 */
static inline void _mny_add(
	sp_wlt *wlt,
	sp_ccy *ccy,
	sp_ma nb
)
{
	check(sp_ma_pz(nb));
	sp_wdp *wdp = _wdp_csch(wlt, ccy);
	if (wdp) _wdp_add(wlt, wdp, nb);
	else if (sp_ma_p(nb)) _wdp_ctor(wlt, ccy, nb);
}

/*
 * Remove @nb of currency in @ccy from @wlt and return 0.
 * If @wlt doesn't have enough of @ccy, do nothing and return 1.
 */
static inline uerr _mny_rem(
	sp_wlt *wlt,
	sp_ccy *ccy,
	sp_ma nb
)
{
	check(sp_ma_pz(nb));
	sp_wdp *wdp = _wdp_csch(wlt, ccy);
	if (!wdp) return sp_ma_z(nb);
	return _wdp_rem(wlt, wdp, nb);
}

/********************
 * Stocks internals *
 ********************/

/*
 * Destruct @wsk from @wlt.
 */
static inline void _wsk_dtor(
	sp_wlt *wlt,
	sp_wsk *wsk
)
{
	_wlt_log("[%p] wsk_dtor %s : %U.\n", wlt, wsk->wsks.value, wsk->nb);
	ns_map_str_remove(&wlt->wsks, &wsk->wsks);
	nh_sfre(wsk->wsks.value);
	nh_fre_(wsk);
}

/*
 * Create and return a depot for currency @ccy with amount @nb in @wlt.
 */
static inline sp_wsk *_wsk_ctor(
	sp_wlt *wlt,
	const char *name,
	u64 nb,
	sp_ccy *ccy
)
{

	/* Create a depot with no money. */
	sp_wsk *wsk;
	nh_all_(wsk);
	const uerr err = ns_map_str_put(&wlt->wsks, &wsk->wsks, name);
	assert(!err, "wsk duplicate name.\n");
	wsk->wsks.value = nh_sall(name);
	wsk->nb = nb;
	wsk->ccy = ccy;
	_wlt_log("[%p] wsk sym %s nb %U\n", wlt, wsk->wsks.value, wsk->nb);
	return wsk;

}

/*
 * Add @nb elements in @wsk and return 0.
 * If an error occurs, return 1.
 */
static inline void _wsk_add(
	sp_wlt *wlt,
	sp_wsk *wsk,
	u64 nb
)
{
	_wsk_check(wlt, wsk);
	u64 new_nb = 0;
	assert(!u64_add_ovf(&new_nb, nb, wsk->nb));
	_wlt_log("[%p] wsk_add %s : %U -> %U.\n", wlt, wsk->wsks.value, wsk->nb, new_nb);
	wsk->nb = new_nb; 
	_wlt_log("[%p] wsk sym %s nb %U\n", wlt, wsk->wsks.value, wsk->nb);
}


/*
 * Remove @nb money in @wsk and return 0.
 * If an error occurs, return 1.
 */
static inline uerr _wsk_rem(
	sp_wlt *wlt,
	sp_wsk *wsk,
	u64 nb
)
{
	_wsk_check(wlt, wsk);
	_wlt_log("[%p] wsk_rem %s : %U -> %U : %s.\n",
			wlt, wsk->wsks.value, wsk->nb, (s64) wsk->nb - (s64)nb, (nb > wsk->nb) ? "error (< 0)" : "ok");
	if (nb > wsk->nb) return 1;
	wsk->nb = wsk->nb - nb;
	check(wsk->nb >= 0);
	_wlt_log("[%p] wsk sym %s nb %U\n", wlt, wsk->wsks.value, wsk->nb);
	if (!wsk->nb) _wsk_dtor(wlt, wsk);
	return 0;
}


/*
 * Add @nb stocks in @wlt and return 0.
 * If an error occurs, return 1.
 */
static inline void _stk_add(
	sp_wlt *wlt,
	const char *name,
	u64 nb,
	sp_ccy *ccy
)
{
	sp_wsk *wsk = _wsk_sch(wlt, name);
	if (wsk) _wsk_add(wlt, wsk, nb);
	else _wsk_ctor(wlt, name, nb, ccy);
}

/*
 * Remove @nb stocks in @wlt and return 0.
 * If an error occurs, return 1.
 */
static inline uerr _stk_rem(
	sp_wlt *wlt,
	const char *name,
	u64 nb
)
{
	sp_wsk *wsk = _wsk_sch(wlt, name);
	if (!wsk) return !nb;
	return _wsk_rem(wlt, wsk, nb);
}

/*************
 * Lifecycle *
 *************/

/*
 * Construct an empty wallet.
 */
sp_wlt *sp_wlt_ctor(
	void
)
{
	nh_all__(sp_wlt, wlt);
	ns_map_uad_init(&wlt->wdps_ccy);
	ns_map_str_init(&wlt->wdps_nam);
	ns_map_str_init(&wlt->wsks);
	_wlt_log("[%p] ctor.\n", wlt);
	return wlt;
}

/*
 * Destruct @wlt.
 */
void sp_wlt_dtor(
	sp_wlt *wlt
)
{

	/* Delete all depots. */
	sp_wdp *wdp;
	ns_map_fe(wdp, &wlt->wdps_ccy, wdps_ccy, uad, in) {
		_wdp_check(wlt, wdp);
		_wdp_dtor(wlt, wdp);
	}

	/* Delete stocks. */
	sp_wsk *wsk;
	ns_map_fe(wsk, &wlt->wsks, wsks, str, post) {
		_wsk_dtor(wlt, wsk);
	}

	_wlt_log("[%p] : dtor.\n", wlt);
	nh_fre_(wlt);

}

/*
 * Create and return a clone of @src.
 * It must have no parent.
 */
sp_wlt *sp_wlt_clone(
	sp_wlt *src
)
{
	nh_all__(sp_wlt, dst);
	_wlt_log("[%p] : clone : %p.\n", src, dst);

	/* Clone depots. */
	sp_wdp *wdp;
	ns_map_fe(wdp, &src->wdps_ccy, wdps_ccy, uad, post) {
		_wdp_check(src, wdp);
		check(!sp_ma_nz(wdp->nb));
		sp_wdp *wdp_dst = _wdp_ctor(dst, (sp_ccy *) wdp->wdps_ccy.value, wdp->nb);
		check(wdp_dst);
		wdp_dst->nb = wdp->nb;
	}

	/* Clone stocks. */
	sp_wsk *wsk;
	ns_map_fe(wsk, &src->wsks, wsks, str, post) {
		_wsk_ctor(dst, wsk->wsks.value, wsk->nb, wsk->ccy);
	}

	/* Complete. */
	return dst;

}

/*
 * If @wdp0 and wdp1 are the same, return 0.
 * If they differ, return 1.
 */
static uerr _wdp_cmp(
	sp_wdp *wdp0,
	sp_wdp *wdp1
)
{

	/* Compare currencies. */
	sp_ccy *ccy0 = (sp_ccy *) wdp0->wdps_ccy.value;
	sp_ccy *ccy1 = (sp_ccy *) wdp1->wdps_ccy.value;
	if (ccy0 != ccy1) {
		error("wdps have different currencies : '%@' and '%@'.\n", sp_ccy_dsc(ccy0), sp_ccy_dsc(ccy1));
		return 1;
	}

	/* Compare names. */
	if (ns_str_cmp(wdp0->wdps_nam.value, wdp1->wdps_nam.value)) {
		error("wdps have different names : '%s' and '%s'.\n", wdp0->wdps_nam.value, wdp1->wdps_nam.value);
		return 1;
	}

	/* Compare amounts. */
	if (!sp_ma_e(wdp0->nb, wdp1->nb)) {
		error("wdps have different amounts : '%@' and '%@'.\n", sp_ma_dsc(wdp0->nb), sp_ma_dsc(wdp1->nb));
		return 1;
	}
	return 0;

}

/*
 * If @wsk0 and wsk1 are the same, return 0.
 * If they differ, return 1.
 */
static uerr _wsk_cmp(
	sp_wsk *wsk0,
	sp_wsk *wsk1
)
{

	/* Compare currencies. */
	sp_ccy *ccy0 = wsk0->ccy;
	sp_ccy *ccy1 = wsk1->ccy;
	if (ccy0 != ccy1) {
		error("wsks have different currencies : '%@' and '%@'.\n", sp_ccy_dsc(ccy0), sp_ccy_dsc(ccy1));
		return 1;
	}

	/* Compare names. */
	if (ns_str_cmp(wsk0->wsks.value, wsk1->wsks.value)) {
		error("wsks have different names : '%s' and '%s'.\n", wsk0->wsks.value, wsk1->wsks.value);
		return 1;
	}

	/* Compare amounts. */
	if (wsk0->nb != wsk1->nb) {
		error("wsks have different amounts : '%U' and '%U'.\n", wsk0->nb, wsk1->nb);
		return 1;
	}
	return 0;

}

/*
 * If @wlt0 and wlt1 are the same, return 0.
 * If they differ, return 1.
 */
uerr sp_wlt_cmp(
	sp_wlt *wlt0,
	sp_wlt *wlt1
)
{

	/* Compare depot. */
	sp_wdp *wdp0;
	sp_wdp *wdp1 = _map_entry(ns_map_uad_fi_post(&wlt1->wdps_ccy), sp_wdp, wdps_ccy);
	ns_map_fe(wdp0, &wlt0->wdps_ccy, wdps_ccy, uad, post) {
		_wdp_check(wlt0, wdp0);
		_wdp_check(wlt1, wdp1);
		check(wdp0);
		if (!wdp1) {
			error("wlts have different number of depots.\n");
			return 1;
		}
		if (_wdp_cmp(wdp0, wdp1)) {
			error("wlts differ.\n");
			return 1;	
		}
		wdp1 = _map_entry(ns_map_uad_fn_post(&wdp1->wdps_ccy), sp_wdp, wdps_ccy);
	}

	/* Compare stocks. */
	sp_wsk *wsk0;
	sp_wsk *wsk1 = _map_entry(ns_map_str_fi_post(&wlt1->wsks), sp_wsk, wsks);
	ns_map_fe(wsk0, &wlt0->wsks, wsks, str, post) {
		_wsk_check(wlt0, wsk0);
		_wsk_check(wlt1, wsk1);
		check(wsk0);
		if (!wsk1) {
			error("wlts have different number of stocks.\n");
			return 1;
		}
		if (_wsk_cmp(wsk0, wsk1)) {
			error("wlts differ.\n");
			return 1;	
		}
		wsk1 = _map_entry(ns_map_str_fn_post(&wsk1->wsks), sp_wsk, wsks);
	}

	return 0;

}

/*
 * Import wdp data from @dsc and return 0.
 * If the import fails, log and return 1.
 */ 
static uerr _wdp_import(
	sp_wlt *wlt,
	ct_val *dsc
)
{

	/* Extract arguments. */	
	ct_args(dsc, return 1,
		(str, name),
		(obj, nb)
	);

	/* Find the currency. */
	sp_ccy *ccy = sp_ccy_sch(name);
	if (!ccy) {
		error("currency '%s' not found.\n", name);
		return 1;
	}

	/* Check the amount. */
	sp_ma _nb = {0};
	uerr err = sp_ma_ct_imp(&_nb, (ct_val *) nb);
	if (err) return 1;
	if (sp_ma_n(_nb)) {
		error("negative amount '%@' for currency '%s'.\n", sp_ma_dsc(_nb), name);
		return 1;
	}
	if (sp_ma_z(_nb)) {
		error("null amount '%@' for currency '%s'.\n", sp_ma_dsc(_nb), name);
		return 1;
	}
	if (_wdp_csch(wlt, ccy)) {
		error("duplicate depot for currency '%s'.\n", name);
		return 1;
	}

	/* Construct. */
	assert(!_wdp_ctor(wlt, ccy, _nb)); 

	/* Complete. */
	return 0;

}

/*
 * Import wsk data from @dsc and return 0.
 * If the import fails, log and return 1.
 */ 
static uerr _wsk_import(
	sp_wlt *wlt,
	ct_val *dsc
)
{

	/* Extract arguments. */	
	ct_args(dsc, return 1,
		(str, name),
		(u64, nb),
		(str, ccy)
	);

	/* Find the currency. */
	sp_ccy *_ccy = sp_ccy_sch(ccy);
	if (!_ccy) {
		error("currency '%s' not found.\n", ccy);
		return 1;
	}

	/* Check the amount. */
	if (!nb) {
		error("null amount '%U' for currency '%s'.\n", nb, ccy);
		return 1;
	}
	if (_wsk_sch(wlt, name)) {
		error("duplicate entry for stock '%s'.\n", name);
		return 1;
	}

	/* Construct. */
	assert(!_wsk_ctor(wlt, name, nb, _ccy)); 

	/* Complete. */
	return 0;

}

/*
 * Construct and return a wallet from @dsc.
 * If an error occurs, return 0.
 */
sp_wlt *sp_wlt_ct_imp(
	ct_val *dsc
)
{

	/* Extract arguments. */	
	ct_args(dsc, return 0,
		(arr, wdps),
		(arr, wsks)
	);

	/* Construct a wallet. */
	sp_wlt *wlt = sp_wlt_ctor();
		
	/* Import depots. */
	ct_val *val;
	ct_arr_fe(val, wdps) {
		uerr err = _wdp_import(wlt, val);
		if (err) {
			sp_wlt_dtor(wlt);
			return 0;
		}
	}

	/* Import stocks. */
	ct_arr_fe(val, wsks) {
		uerr err = _wsk_import(wlt, val);
		if (err) {
			sp_wlt_dtor(wlt);
			return 0;
		}
	}

	/* Complete. */
	return wlt;

}

/*
 * Generate and return a wdp descriptor.
 */
static ct_val *_wdp_export(
	sp_wdp *wdp
)
{
	ct_obj *obj = ct_obj_ctor();
	ct_obj_put(obj, "name", ct_str_ctor(wdp->wdps_nam.value));
	ct_obj_put(obj, "nb", sp_ma_ct_exp(&wdp->nb));
	return (ct_val *) obj;
}

/*
 * Generate and return a wsk descriptor.
 */
static ct_val *_wsk_export(
	sp_wsk *wsk
)
{
	ct_obj *obj = ct_obj_ctor();
	ct_obj_put(obj, "name", ct_str_ctor(wsk->wsks.value));
	ct_obj_put(obj, "nb", ct_u64_ctor(wsk->nb));
	ct_obj_put(obj, "ccy", ct_str_ctor(wsk->ccy->nam));
	return (ct_val *) obj;
}

/*
 * Generate and return a descriptor of @wlt.
 */
ct_val *sp_wlt_ct_exp(
	sp_wlt *wlt
)
{

	/* Sub generation. */
	ct_obj *obj = ct_obj_ctor();

	/* Export depots. */
	ct_arr *arr = ct_arr_ctor();
	ct_obj_put(obj, "wdps", (ct_val *) arr);
	sp_wdp *wdp;
	ns_map_fe(wdp, &wlt->wdps_nam, wdps_nam, str, in) {
		_wdp_check(wlt, wdp);
		ct_arr_push(arr, _wdp_export(wdp));
	}

	/* Export stocks. */
	arr = ct_arr_ctor();
	ct_obj_put(obj, "wss", (ct_val *) arr);
	sp_wsk *wsk;
	ns_map_fe(wsk, &wlt->wsks, wsks, str, in) {
		_wsk_check(wlt, wsk);
		ct_arr_push(arr, _wsk_export(wsk));
	}

	/* Complete. */
	return (ct_val *) obj;

}

/**************
 * Amount API *
 **************/

/*
 * Return the amount of money in depot of currency @ccy of @wlt.
 * If no such depot exists, return 0.
 */
sp_ma sp_wlt_mny_get(
	sp_wlt *wlt,
	sp_ccy *ccy
)
{
	sp_wdp *wdp = _wdp_csch(wlt, ccy);
	sp_ma ma = (wdp) ? wdp->nb : sp_ma_0();	
	_wlt_log("[%p] wdp_get %s : %@.\n", wlt, ccy->nam, sp_ma_dsc(ma));
	return ma;
}

/*
 * Return the amount of money in depot of currency named @name of @wlt.
 * If no such depot exists, return 0.
 */
sp_ma sp_wlt_mny_get_name(
	sp_wlt *wlt,
	const char *nam
)
{
	sp_wdp *wdp = _wdp_nsch(wlt, nam);
	sp_ma ma = (wdp) ? wdp->nb : sp_ma_0();	
	_wlt_log("[%p] wdp_get %s : %@.\n", wlt, nam, sp_ma_dsc(ma));
	return ma;
}

/*************
 * Stock API *
 *************/

/*
 * Initialize @wsk_nam with @mkp and @sym of known
 * lengths.
 */
static inline void _gen_nam(
	char *wsk_nam,
	const char *mkp,
	const uad mkp_len,
	const char *sym,
	const uad sym_len
)
{
	ns_mem_cpy(wsk_nam, mkp, mkp_len);
	wsk_nam[mkp_len] = ':';
	ns_mem_cpy(&wsk_nam[mkp_len + 1], sym, sym_len);
	wsk_nam[mkp_len + 1 + sym_len] = 0;
}

#define WSK_NAM_GEN(mkp, sym) \
char wsk_nam[64]; \
{ \
	const uad __mkp_len = ns_str_len(mkp); \
	const uad __sym_len = ns_str_len(sym); \
	assert(__mkp_len < 21); \
	assert(__sym_len < 21); \
	_gen_nam(wsk_nam, mkp, __mkp_len, sym, __sym_len); \
}

/*
 * Return the amount of stocks named @nam available
 * in @wlyt.
 */
u64 sp_wlt_stk_get(
	sp_wlt *wlt,
	const char *mkp,
	const char *sym
)
{
	WSK_NAM_GEN(mkp, sym);
	sp_wsk *wsk = _wsk_sch(wlt, wsk_nam);
	_wlt_log("[%p] : wsk_get : %s:%s : %U.\n", mkp, sym, (wsk) ? wsk->nb : 0);
	return (wsk) ? wsk->nb : 0;
}

/********************************
 * Ord status change internals *
 ********************************/

/*
 * A broker implementation may rely on the internal broker account model to
 * be updated when it updates an order, instead of relying on the whole
 * account reload procedure.
 * Doing this is adequate for debug broker impls with no remote broker,
 * but a broker interacting with a remote broker should preferably rely on
 * the whole update, as it is the remote broker that has the last word on
 * the status.
 * As mentioned in the doc, NEVER used a mixed strategy, ie relying both on
 * account status update and full account reload as it may cause corrupt
 * account status due to the asynchronicity of load update response and
 * order completion update.
 */

/*
 * Take resources from @wlt to pass an order described by @ordd and return 0.
 * If an error occurs, return 1.
 */
static uerr _ord_stk_res_tak(
	sp_wlt *wlt,
	const sp_ordd *ordd
)
{
	assert(ordd->cat == SP_ORD_CAT_STK,
		"invalid order category, expected '%u', got '%u'.\n",
		SP_ORD_CAT_STK, ordd->cat);
	assert(ordd->cat < SP_ORD_STK_NB, "invalid stock order typ (%u).\n", ordd->cat);


	/* Cache dsc data. */
	const char *mkp = ordd->stk.mkp;
	const char *sym = ordd->stk.sym;
	WSK_NAM_GEN(mkp, sym);
	const u8 typ = ordd->typ;
	const u8 buy = (typ >= SP_ORD_STK_BUY);
	const u32 nb = ordd->stk.req_nb;
	sp_ccy *const ccy = ordd->stk.ccy;
	check(nb);

	_wlt_log("[%p] res_tak : %s:%s\n", wlt, mkp, sym);
	_wlt_log("[%p] res_tak : typ %u, buy %u, nb %u.\n", wlt, typ, buy, nb);

	/* If buy, attempt to take the amount from the account. */
	if (buy) {
		assert(
			(typ == SP_ORD_STK_LIM_BUY) ||
			(typ == SP_ORD_STK_STP_LIM_BUY),
			"invalid stock buy typ : %u\n", typ
		);

		/* Compute the maximal amount. */
		const sp_ma prc = ordd->stk.req_limit_price;
		_wlt_log("[%p] res_tak : limit price %@\n", wlt, sp_ma_dsc(prc));
		const sp_ma ttl = sp_ma_mul(prc, nb);
		_wlt_log("[%p] res_tak : total price %@\n", wlt, sp_ma_dsc(ttl));

		/* Take that amount from the account. */
		return _mny_rem(wlt, ccy, ttl);
		
	}

	/* If sell, remove stocks. */
	else {
		return _stk_rem(wlt, wsk_nam, nb);
	}

}

/*
 * Take resources from @wlt to pass an order described by @ordd and return 0.
 * If an error occurs, return 1.
 */
uerr sp_wlt_ord_res_tak(
	sp_wlt *wlt,
	const sp_ordd *ordd
)
{
	assert(ordd->cat < SP_ORD_CAT_NB, "invalid order category (%u).\n", ordd->cat);

	/* Select the appropriate function. */
	if (ordd->cat == SP_ORD_CAT_STK)
		return _ord_stk_res_tak(wlt, ordd);

	/* Unreachable. */      
	check(0);
	return 1;

}

/*
 * Release resources from @wlt after @ord was cancelled.
 */
static uerr _ord_stk_res_rel(
	sp_wlt *wlt,
	const sp_ordd *ordd
)
{
	assert(ordd->cat == SP_ORD_CAT_STK,
		"invalid order category, expected '%u', got '%u'.\n",
		SP_ORD_CAT_STK, ordd->cat);
	assert(ordd->cat < SP_ORD_STK_NB, "invalid stock order typ (%u).\n", ordd->cat);

	/* Cache dsc data. */
	const char *mkp = ordd->stk.mkp;
	const char *sym = ordd->stk.sym;
	WSK_NAM_GEN(mkp, sym);
	const u8 typ = ordd->typ;
	const u8 buy = (typ >= SP_ORD_STK_BUY);
	const u32 nb = ordd->stk.req_nb;
	sp_ccy *const ccy = ordd->stk.ccy;

	_wlt_log("[%p] res_rel : %s:%s\n", wlt, mkp, sym);
	_wlt_log("[%p] res_rel : typ %u, buy %u, nb %u.\n", wlt, typ, buy, nb);

	/* If buy, release money in the pop's wallet. */
	if (buy) {
		assert(
			(typ == SP_ORD_STK_LIM_BUY) ||
			(typ == SP_ORD_STK_STP_LIM_BUY),
			"invalid stock buy typ : %u\n", typ
		);

		/* Compute the maximal amount. */
		const sp_ma prc = ordd->stk.req_limit_price;
		_wlt_log("[%p] res_rel : limit price %@\n", wlt, sp_ma_dsc(prc));
		const sp_ma ttl = sp_ma_mul(prc, nb);
		_wlt_log("[%p] res_rel : total price %@\n", wlt, sp_ma_dsc(ttl));

		/* Put this amount back in the wallet. */
		_mny_add(wlt, ccy, ttl);

	}

	/* If sell, reclaim stocks. */
	else {
		_stk_add(wlt, wsk_nam, nb, ccy);
	}

	/* No failure. */
	return 0;

}

/*
 * Release resources from @wlt after the order described by @ordd
 * was cancelled.
 */
uerr sp_wlt_ord_res_rel(
	sp_wlt *wlt,
	const sp_ordd *ordd
)
{
	assert(ordd->cat < SP_ORD_CAT_NB, "invalid order category (%u).\n", ordd->cat);

	/* Select the appropriate function. */
	if (ordd->cat == SP_ORD_CAT_STK)
		return _ord_stk_res_rel(wlt, ordd);

	/* Unreachable. */      
	check(0);
	return 1;

}

/*
 * Collect resources from @wlt after @ordd is complete.
 */
static uerr _ord_stk_res_col(
	sp_wlt *wlt,
	const sp_ordd *ordd
)
{
	assert(ordd->cat == SP_ORD_CAT_STK,
		"invalid order category, expected '%u', got '%u'.\n",
		SP_ORD_CAT_STK, ordd->cat);
	assert(ordd->cat < SP_ORD_STK_NB, "invalid stock order typ (%u).\n", ordd->cat);


	/* Cache dsc data. */
	const char *mkp = ordd->stk.mkp;
	const char *sym = ordd->stk.sym;
	WSK_NAM_GEN(mkp, sym);
	const u8 typ = ordd->typ;
	const u8 buy = (typ >= SP_ORD_STK_BUY);
	const u32 req_nb = ordd->stk.req_nb;
	const u32 rsp_nb = ordd->stk.rsp_nb;

	_wlt_log("[%p] res_col : %s:%s\n", wlt, mkp, sym);
	_wlt_log("[%p] res_col : typ %u, buy %u, req_nb %u, rsp_nb %u\n", wlt, typ, buy, req_nb, rsp_nb);

	sp_ccy *const ccy = ordd->stk.ccy;

	/* If buy, update @pop's wallet with and add stocks. */
	if (buy) {
		assert(
			(typ == SP_ORD_STK_LIM_BUY) ||
			(typ == SP_ORD_STK_STP_LIM_BUY),
			"invalid stock buy typ : %u\n", typ
		);

		/* Add the number of bought stocks. */
		_stk_add(wlt, wsk_nam, rsp_nb, ccy);

		/* Compute the diff amount. */
		assert((typ == SP_ORD_STK_LIM_BUY) || (typ == SP_ORD_STK_STP_LIM_BUY));
		const sp_ma prc = ordd->stk.req_limit_price;
		_wlt_log("[%p] res_col : limit price %@\n", wlt, sp_ma_dsc(prc));
		const sp_ma req_ttl = sp_ma_mul(prc, req_nb);
		_wlt_log("[%p] res_col : req total price %@\n", wlt, sp_ma_dsc(req_ttl));
		const sp_ma rsp_ttl = ordd->stk.rsp_ttl;
		_wlt_log("[%p] res_col : rsp total price %@\n", wlt, sp_ma_dsc(rsp_ttl));
		sp_ma diff = sp_ma_dif(req_ttl, rsp_ttl);
		_wlt_log("[%p] res_col : diff price %@\n", wlt, sp_ma_dsc(diff));

		/* If less money used than originally planned, add it. */
		if (sp_ma_z(diff)) {}
			else if (sp_ma_p(diff)) _mny_add(wlt, ccy, diff);

		/* If more money used, danger. Try to cover losses. */
		else {
			info("order %@ cost more than expected. Attempting to cover losses.\n", sp_ordd_dsc(ordd, 0));
			assert(0, "FIND WHY THAT COST MORE, THAT IS NOT NORMAL.\n");
			diff = sp_ma_neg(diff);
			const uerr err = _mny_rem(wlt, ccy, diff);
			if (err) {
				info("couldn't cover losses.\n");
				return 1;
			}
			else {
				info("successfully covered losses.\n");
			}
		}
		return 0;
		
	}

	/* If sell, unlock stocks, tagging them kept or sold. */
	else {

		/* Add non-sold stocks. */
		assert(rsp_nb <= req_nb, "order %@ sold more stocks (%u) than required (%u).\n", sp_ordd_dsc(ordd, 0), rsp_nb, req_nb);
		const u32 nb_kept = req_nb - rsp_nb;
		_stk_add(wlt, wsk_nam, nb_kept, ccy);

		/* Add money. */
		const sp_ma rsp_ttl = ordd->stk.rsp_ttl;
		_wlt_log("[%p] res_col : rsp total price %@\n", wlt, sp_ma_dsc(rsp_ttl));
		_mny_add(wlt, ccy, rsp_ttl);

		return 0;
	}

}

/*
 * Add new resources to @wlt after the order described by @ordd completed.
 */
uerr sp_wlt_ord_res_col(
	sp_wlt *wlt,
	const sp_ordd *ordd
)
{
	assert(ordd->cat < SP_ORD_CAT_NB, "invalid order category (%u).\n", ordd->cat);

	/* Select the appropriate function. */
	if (ordd->cat == SP_ORD_CAT_STK)
		return _ord_stk_res_col(wlt, ordd);

	/* Unreachable. */      
	check(0);
	return 1;

}

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
)
{
	sp_wdp *wdp = _wdp_csch(wlt, ccy);
	(wdp) ?
		_wdp_add(wlt, wdp, nb) :
		_wdp_ctor(wlt, ccy, nb);
}

