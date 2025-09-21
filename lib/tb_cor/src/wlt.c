/* Copyright 2025 Raphael Outhier - confidential - proprietary - no copy - no diffusion. */

#include <tb_cor/tb_cor.all.h>

/*******
 * Log *
 *******/

tb_lib_log_def(cor, wlt);
#define _wlt_log(...) tb_log_lib(cor, wlt, __VA_ARGS__)

/*********
 * Asset *
 *********/

/*
 * Return @ast's investment.
 */
static inline tb_ist *_ast_ist(
	tb_ast *ast
) {return (tb_ist *) ast->asts.value;}

/*
 * Pring log data for @ast.
 */
void _ast_log(
	ns_stm *stm,
	va_list *args
)
{
	tb_ast *ast = va_arg(*args, tb_ast *);
	ns_stm_writef(stm, "asset (%@, %d)", tb_ist_dsc(_ast_ist(ast)), ast->amt));
}
#define _ast_dsc(ast) &_ast_log, ast

/*
 * Check @ast.
 */
static void _ast_chk(
	tb_wlt *wlt,
	tb_ast *ast
) {check(ast->amt > 0);}

/*
 * If @wlt has instances of @ist, return the asset.
 * Otherwise, return 0.
 */
static inline tb_ast *_ast_sch(
	tb_wlt *wlt,
	tb_ist *ist
)
{
	tb_ast *ast = ns_map_search(&wlt->asts, (uad) ist, uad, tb_ast, asts);
	if (ast) _ast_chk(wlt, ast);
	return ast;
}	

/*
 * Destruct @ast from @wlt.
 */
static inline void _ast_dtr(
	tb_wlt *wlt,
	tb_ast *ast
)
{
	_wlt_log("[%p] ast_del %@ : %d.\n",
		wlt, tb_ist_dsc(_ast_ist(ast)), ast->amt);
	_ast_chk(wlt, ast);
	ns_map_uad_remove(&wlt->asts, &ast->asts);
	nh_fre_(ast);
}

/*
 * Create and return a depot for currency @ccy with amount @amt in @wlt.
 */
static inline tb_ast *_ast_ctr(
	tb_wlt *wlt,
	tb_ist *ist,
	f64 amt
)
{

	assert(amt > 0);

	/* Create. */
	nh_all__(tb_ast, ast);
	uerr err = ns_map_uad_put(&wlt->asts, &ast->asts, (uad) ist);
	assert(!err, "ast_insertion error.\n");
	ast->amt = amt;
	_wlt_log("[%p] ast %@ amt %@\n", wlt, tb_ist_dsc(_ast_ist(ast)), ast->amt);

	/* Complete. */
	_ast_chk(wlt, ast);
	return ast;

}

/*
 * Increase @ast's amount by @amt.
 */
static inline void _ast_add_(
	tb_wlt *wlt,
	tb_ast *ast,
	f64 amt
)
{
	check(amt >= 0);
	_ast_chk(wlt, ast);
	const f64 sum = ast->amt + amt;
	_wlt_log("[%p] ast_add %@ : %d -> %d.\n", wlt, tb_ist_dsc(_ast_ist(ast)), ast->amt, sum);
	ast->amt = sum;
	_wlt_log("[%p] ast_add %@ : %d -> %d.\n", wlt, tb_ist_dsc(_ast_ist(ast)), ast->amt, sum);
	_wlt_log("[%p] ast %@ amt %@\n", wlt, tb_ist_dsc(_ast_ist(ast)), ast->amt);
	_ast_chk(wlt, ast);
}

/*
 * Decrease @ast's amount by @amt and return 0.
 * If an error occurs, return 1.
 */
static inline uerr _ast_rem_(
	tb_wlt *wlt,
	tb_ast *ast,
	f64 amt
)
{
	_ast_chk(wlt, ast);
	const f64 dif = tb_amt_dif(ast->amt, amt);
	_wlt_log("[%p] ast_add %@ : %d -> %d : %s.\n", wlt, tb_ist_dsc(_ast_ist(ast)), ast->amt, dif, (dif < 0) ? "error (< 0)" : "OK");
	if (dif < 0) return 1;
	ast->amt = dif;
	_wlt_log("[%p] ast %@ amt %@\n", wlt, tb_ist_dsc(_ast_ist(ast)), ast->amt);
	if (amt == 0) _ast_dtr(wlt, ast);
	else {_ast_chk(wlt, ast);}
	return 0;
}

/*
 * If @wlt has instances of @ist, increase their amount
 * by @amt.
 * Otherwise, create an asset for @ist. 
 */
static inline void _ast_add(
	tb_wlt *wlt,
	tb_ist *ist,
	tb_amt amt
)
{
	check(amt >= 0);
	if (amt > 0) {
		tb_ast *ast = _ast_sch(wlt, ist);
		if (ast) _ast_add_(wlt, ast, amt);
		else _ast_ctr(wlt, ist, amt);
	}
}

/*
 * If @wlt has instances of @ist, try to decrease their
 * amount by @amt and return 0 if possible.
 * If no instances or not enough, return 1.
 */
static inline uerr _ast_rem(
	tb_wlt *wlt,
	tb_ist *ist,
	tb_amt amt
)
{
	check(amt >= 0);
	tb_ast *ast = _ast_sch(wlt, ist);
	if (!ast) return (amt == 0);
	else return _ast_rem_(wlt, ist, amt);
}

/*************
 * Lifecycle *
 *************/

/*
 * Construct an empty wallet.
 */
tb_wlt *tb_wlt_ctr(
	void
)
{
	nh_all__(tb_wlt, wlt);
	ns_map_uad_init(&wlt->asts);
	_wlt_log("[%p] ctr.\n", wlt);
	return wlt;
}

/*
 * Destruct @wlt.
 */
void tb_wlt_dtr(
	tb_wlt *wlt
)
{
	tb_ast *ast;
	ns_map_fe(ast, &wlt->asts, asts, uad, in) {
		_ast_chk(wlt, ast);
		_ast_dtr(wlt, ast);
	}
	_wlt_log("[%p] : dtr.\n", wlt);
	nh_fre_(wlt);
}

/*
 * Create and return a clone of @src.
 * It must have no parent.
 */
tb_wlt *tb_wlt_cln(
	tb_wlt *src
)
{
	nh_all__(tb_wlt, dst);
	_wlt_log("[%p] : cln : %p.\n", src, dst);
	tb_ast *ast;
	ns_map_fe(ast, &src->asts, asts, uad, post) {
		_ast_ctor(dst, ast->asts.value, _ast_ist(ast), ast->amt);
	}
	return dst;

}

/*
 * If @ast0 and ast1 are the same, return 0.
 * If they differ, return 1.
 */
static uerr _ast_cmp(
	tb_ast *ast0,
	tb_ast *ast1
)
{

	/* Compare currencies. */
	tb_ist *ist0 = _ast_ist(ast0);
	tb_ist *ist1 = _ast_ist(ast1);
	if (ist0 != ist1) {
		error("assets have different currencies : '%@' and '%@'.\n", tb_ist_dsc(ist0), tb_ist_dsc(ist1));
		return 1;
	}

	/* Compare amounts. */
	if (ast0->amt != ast1->amt) {
		error("assets have different amounts : '%d' and '%d'.\n", ast0->amt, ast1->amt);
		return 1;
	}

	/* Complete. */
	return 0;

}

/*
 * If @wlt0 and wlt1 are the same, return 0.
 * If they differ, return 1.
 */
uerr tb_wlt_cmp(
	tb_wlt *wlt0,
	tb_wlt *wlt1
)
{
	/* Compare assets. */
	tb_ast *ast0;
	tb_ast *ast1 = _map_entry(ns_map_uad_fi_post(&wlt1->asts), tb_ast, asts);
	ns_map_fe(ast0, &wlt0->asts, asts, uas, post) {
		_ast_chk(wlt0, ast0);
		_ast_chk(wlt1, ast1);
		check(ast0);
		if (!ast1) {
			error("wlts have different number of assets.\n");
			return 1;
		}
		if (_ast_cmp(ast0, ast1)) {
			error("wlts differ.\n");
			return 1;	
		}
		ast1 = _map_entry(ns_map_uas_fn_post(&ast1->asts), tb_ast, asts);
	}

	return 0;

}

/**************
 * Amount API *
 **************/

/*
 * Return the amount of money in depot of currency @ccy of @wlt.
 * If no such depot exists, return 0.
 */
tb_amt tb_wlt_get(
	tb_wlt *wlt,
	tb_ist *ist
)
{
	tb_ast *ast = _ast_sch(wlt, ist);
	f64 amt = (ast) ? ast->amt : 0;
	_wlt_log("[%p] ast_get %@ : %d.\n", wlt, tb_ist_dsc(ist), amt);
	return amt;
}

/******************************
 * Order processing internals *
 ******************************/

/*
 * Take resources from @wlt to pass an order
 * described by @ord and return 0.
 * If an error occurs, return 1.
 */
uerr tb_wlt_ord_res_tak(
	tb_wlt *wlt,
	const tb_ord *ord
)
{

	/* Read order data. */
	const u8 typ = ord->typ;
	const u8 buy = TB_ORD_TYP_IS_BUY(typ)
	const f64 req_vol_prm = ord->req_vol_prm;
	check(req_vol_prm);

	/* Log. */
	_wlt_log("[%p] res_tak : prm %@ sec %@\n", wlt, sp_ist_dsc(ord->prm), sp_ist_dsc(ord->sec));
	_wlt_log("[%p] res_tak : typ %u, buy %u, req_vol_prm %d.\n", wlt, typ, buy, req_vol_prm);

	/*
	 * If buy, compute the maximal volume of secondary instrument
	 * and remove it.
	 */
	if (buy) {
		assert(
			(typ == TB_ORD_STK_LIM_BUY) ||
			(typ == TB_ORD_STK_STP_LIM_BUY),
			"invalid stock buy typ : %u\n", typ
		);

		/* Compute the maximal volume. */
		const f64 req_prc_lim = ord->req_prc_lim;
		_wlt_log("[%p] res_tak : req_prc_lim %d\n", wlt, req_prc_lim);
		const f64 req_vol_sec = req_prc_lim * req_vol_prm;
		_wlt_log("[%p] res_tak : req_vol_sec %d\n", wlt, req_vol_sec);

		/* Take that volume from the wallet. */
		return _ast_rem(wlt, ord->sec, req_vol_sec);
		
	}

	/*
	 * If sell, remove the sell volume from the
	 * primary instrument.
	 */
	else {
		return _ast_rem(wlt, ord->prm, req_vol_prm);
	}

}

/*
 * Release resources from @wlt after the order
 * described by @ord was cancelled.
 */
uerr tb_wlt_ord_res_rel(
	tb_wlt *wlt,
	const tb_ord *ord
)
{

	/* Read order data. */
	const u8 typ = ord->typ;
	const u8 buy = TB_ORD_TYP_IS_BUY(typ)
	const f64 req_vol_prm = ord->req_vol_prm;
	check(req_vol_prm);

	/* Log. */
	_wlt_log("[%p] res_rel : prm %@ sec %@\n", wlt, sp_ist_dsc(ord->prm), sp_ist_dsc(ord->sec));
	_wlt_log("[%p] res_rel : typ %u, buy %u, req_vol_prm %d.\n", wlt, typ, buy, req_vol_prm);

	/* If buy, release money in the pop's wallet. */
	if (buy) {
		assert(
			(typ == TB_ORD_STK_LIM_BUY) ||
			(typ == TB_ORD_STK_STP_LIM_BUY),
			"invalid stock buy typ : %u\n", typ
		);

		/* Compute the maximal volume. */
		const f64 req_prc_lim = ord->req_prc_lim;
		_wlt_log("[%p] res_rel : req_prc_lim %d\n", wlt, req_prc_lim);
		const f64 req_vol_sec = req_prc_lim * req_vol_prm;
		_wlt_log("[%p] res_rel : req_vol_sec %d\n", wlt, req_vol_sec);

		/* Put this volume back in the wallet. */
		_ast_add(wlt, ord->sec, req_vol_sec);

	}

	/* If sell, add primary back. */
	else {
		_stk_add(wlt, ord->sec, req_vol_prm);
	}

	/* No failure. */
	return 0;

}

/*
 * Add new resources to @wlt after the order
 * described by @ord completed.
 */
uerr tb_wlt_ord_res_col(
	tb_wlt *wlt,
	const tb_ord *ord
)
{
	/* Read order data. */
	const u8 typ = ord->typ;
	const u8 buy = (typ >= TB_ORD_STK_BUY);
	const u8 buy = TB_ORD_TYP_IS_BUY(typ)
	const f64 req_vol_prm = ord->req_vol_prm;
	check(req_vol_prm);
	const f64 rsp_vol_prm = ord->rsp_vol_prm;
	const f64 rsp_vol_sec = ord->rsp_vol_sec;

	/* Log. */
	_wlt_log("[%p] res_col : prm %@ sec %@\n", wlt, sp_ist_dsc(ord->prm), sp_ist_dsc(ord->sec));
	_wlt_log("[%p] res_col : typ %u, buy %u, req_vol_prm %d, rsp_vol_prm %d, rsp_vol_sec %d.\n", wlt, typ, buy, req_vol_prm, rsp_vol_prm, rsp_vol_sec);

	tb_ccy *const ccy = ordd->stk.ccy;

	/* If buy, add primary and update secondary. */
	if (buy) {

		/* Add the primary volume. */
		_ast_add(wlt, ord->prm, rsp_vol_prm);

		/* Compute the diff amount. */
		const f64 req_prc_lim = ord->req_prc_lim;
		_wlt_log("[%p] res_col : req_prc_lim %d\n", wlt, req_prc_lim);
		const f64 req_vol_sec_lim = req_prc_lim * req_vol_prm;
		_wlt_log("[%p] res_col : req_vol_sec_lim %d\n", wlt, req_vol_sec_lim));
		_wlt_log("[%p] res_col : rsp_vol_sec %d\n", wlt, rsp_vol_sec);
		f64 vol_sec_dif = req_vol_sec_lim - rsp_vol_sec;
		_wlt_log("[%p] res_col : vol_sec_dif %d\n", wlt, vol_sec_dif);

		/* If less secondary volume used than originally planned, add it. */
		assert(dif >= 0, "order %@ cost more than expected.\n", tb_ord_dsc(ord, 0));
		if (dif > 0) _ast_add(wlt, ord->sec, vol_sec_dif);

		return 0;
		
	}

	/* If sell, add secondary and update primary. */
	else {

		/* Add non-sold primary. */
		assert(rsp_vol_prm <= req_vol_prm, "order %@ sold more (%d) than required (%d).\n", tb_ord_dsc(ord, 0), rsp_vol_sec, req_vol_prm);
		const f64 vol_prm_dif = req_vol_prm - rsp_vol_prm;
		_ast_add(wlt, ord->prm, vol_prm_dif);

		/* Add money. */
		_wlt_log("[%p] res_col : rsp_vol_sec %d\n", wlt, rsp_vol_sec);
		_ast_add(wlt, ord->sec, rsp_vol_sec);

		return 0;
	}

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
void tb_wlt_sim_add(
	tb_wlt *wlt,
	tb_ist *ist,
	f64 amt
) {return ast_add(wlt, ist, amt);}

