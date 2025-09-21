/* Copyright 2025 Raphael Outhier - confidential - proprietary - no copy - no diffusion. */

#include <sp_cor/sp_cor.all.h>

/********************
 * Order descriptor *
 ********************/

/*
 * Compare stocks responses.
 */
static uerr _ordd_stk_cmp_req(
	sp_ordd *ordd0,
	sp_ordd *ordd1
)
{

	sp_ordd_stk *ordds0 = &ordd0->stk;
	sp_ordd_stk *ordds1 = &ordd1->stk;

	/* Compare types. */
	u8 typ = ordd0->typ;
	if (typ != ordd1->typ) {
		error("stock order dscs have different types : '%u' and '%u'.\n", typ, ordd1->typ);
		return 1;
	}

	/* Compare symbols. */
	if (ns_str_cmp(ordds0->sym, ordds1->sym)) {
		error("stock order dscs have different symbol names : '%s' and '%s'.\n", ordds0->sym, ordds1->sym);
		return 1;
	}
	/* Compare marketplace names. */
	if (ns_str_cmp(ordds0->mkp, ordds1->mkp)) {
		error("stock order dscs have different marketplace names : '%s' and '%s'.\n", ordds0->mkp, ordds1->mkp);
		return 1;
	}

	/* Compare currencies. */
	if (ordds0->ccy != ordds1->ccy) {
		error("stock order dscs have different currencies : '%@' and '%@'.\n", sp_ccy_dsc(ordds0->ccy), sp_ccy_dsc(ordds1->ccy));
		return 1;
	}

	/* Compare request. */
	if (
		(typ == SP_ORD_STK_LIM_BUY) ||
		(typ == SP_ORD_STK_LIM_SEL)
	) {
		if (!sp_ma_e(ordds0->req_limit_price, ordds1->req_limit_price)) {
			error("limit stock order dscs have different limit prices : '%@' and '%@'.\n",
				sp_ma_dsc(ordds0->req_limit_price), sp_ma_dsc(ordds1->req_limit_price));
			return 1;
		}
	}
	else if (typ == SP_ORD_STK_STP_SEL) {
		if (!sp_ma_e(ordds0->req_stop_price, ordds1->req_stop_price)) {
			error("stop stock order dscs have different stop prices : '%@' and '%@'.\n",
				sp_ma_dsc(ordds0->req_stop_price), sp_ma_dsc(ordds1->req_stop_price));
			return 1;
		}
	}
	else if (
		(typ == SP_ORD_STK_STP_LIM_BUY) ||
		(typ == SP_ORD_STK_STP_LIM_SEL)
	) {
		sp_ma stop0 = ordds0->req_stop_price;
		sp_ma limit0 = ordds0->req_limit_price;
		sp_ma stop1 = ordds1->req_stop_price;
		sp_ma limit1 = ordds1->req_limit_price;
		if (!sp_ma_e(stop0, stop1)) {
			error("stop-limit stock order dscs have different stop prices : '%@' and '%@'.\n",
				sp_ma_dsc(stop0), sp_ma_dsc(stop1));
			return 1;
		}
		if (!sp_ma_e(limit0, limit1)) {
			error("stop-limit stock order dscs have different limit prices : '%@' and '%@'.\n",
				sp_ma_dsc(limit0), sp_ma_dsc(limit1));
			return 1;
		}
	}
	return 0;

}

/*
 * Compare stocks data.
 */
static uerr _ordd_stk_cmp(
	sp_ordd *ordd0,
	sp_ordd *ordd1
)
{

	/* Compare requests. */
	const uerr err = _ordd_stk_cmp_req(ordd0, ordd1);
	if (err) return 1;
	
	/* Fetch descriptors. */
	sp_ordd_stk *ordds0 = &ordd0->stk;
	sp_ordd_stk *ordds1 = &ordd1->stk;

	/* Compare response. */
	if (!sp_ma_e(ordds0->rsp_ttl, ordds1->rsp_ttl)) {
		error("stock order dscs have different response amounts : '%@' and '%@'.\n",
			sp_ma_dsc(ordds0->rsp_ttl), sp_ma_dsc(ordds1->rsp_ttl));
		return 1;
	}
	if (ordds0->rsp_nb != ordds1->rsp_nb) {
		error("stock order dscs have different response number : '%u' and '%u'.\n",
			ordds0->rsp_nb, ordds1->rsp_nb);
		return 1;
	}
	return 0;

}

/*
 * If requests of @ordd0 and @ordd1 are the same, return 0.
 * If they differ, return 1.
 */
uerr sp_ordd_cmp_req(
	sp_ordd *ordd0,
	sp_ordd *ordd1
) {
	if (ordd0->cat != ordd1->cat) {
		error("order descriptors have different categories : '%u' and '%u'.\n", ordd0->cat, ordd1->cat);
		return 1;
	}
	ordd_switch(cmp_req, ordd0, ordd0, ordd1);
	return 1;
}

/*
 * If @ordd0 and ordd1 are the same, return 0.
 * If they differ, return 1.
 */
uerr sp_ordd_cmp(
	sp_ordd *ordd0,
	sp_ordd *ordd1
) {
	if (ordd0->cat != ordd1->cat) {
		error("order descriptors have different categories : '%u' and '%u'.\n", ordd0->cat, ordd1->cat);
		return 1;
	}
	ordd_switch(cmp, ordd0, ordd0, ordd1);
	return 1;
}

static const char *_sts_dscs[4] =
	{"idle", "active", "cancelled", "complete"};
static const char *_typ_dscs[SP_ORD_STK_NB] = {
	"MKT_SEL",
	"STP_SEL",
	"LIM_SEL",
	"STP_LIM_SEL",
	"LIM_BUY",
	"STP_LIM_BUY"
};

static void _ordd_stk_log(
	sp_ordd *ordd,
	ns_stm *stm,
	u8 sts
) {
	assert(sts < 4);
	const u8 typ = ordd->typ;
	assert(typ < SP_ORD_STK_NB);
	const char *typ_dsc = _typ_dscs[typ];
	const char *sts_dsc = _sts_dscs[sts]; 
	assert(sts <= SP_ORD_STS_CPL, "invalid order status '%u'.\n", sts);
	ns_stm_writef(stm, "(stock %s, %s, %s, %s, %@, ", 
		typ_dsc, sts_dsc, ordd->stk.sym, ordd->stk.mkp, sp_ccy_dsc(ordd->stk.ccy));
	if (typ == SP_ORD_STK_MKT_SEL)
		ns_stm_writef(stm, "(%u)", ordd->stk.req_nb);
	else if (
		(typ == SP_ORD_STK_LIM_BUY) ||
		(typ == SP_ORD_STK_LIM_SEL)
	) ns_stm_writef(stm, "(%u, %@)", ordd->stk.req_nb, sp_ma_dsc(ordd->stk.req_limit_price));
	else if (typ == SP_ORD_STK_STP_SEL)
		ns_stm_writef(stm, "(%u, %@)", ordd->stk.req_nb, sp_ma_dsc(ordd->stk.req_stop_price));
	else if (
		(typ == SP_ORD_STK_STP_LIM_BUY) ||
		(typ == SP_ORD_STK_STP_LIM_SEL)
	) ns_stm_writef(stm, "(%u, %@, %@)", ordd->stk.req_nb, sp_ma_dsc(ordd->stk.req_limit_price), sp_ma_dsc(ordd->stk.req_stop_price));
	else {assert(0, "invalid stock order typ '%u'.\n", typ);}
	if (sts == SP_ORD_STS_CPL) ns_stm_writef(stm, "-> (%u, %@)", ordd->stk.rsp_nb, sp_ma_dsc(ordd->stk.rsp_ttl));
	else ns_stm_write(stm, ")", 2);
}

/*
 * Print log data for an order descriptor.
 */
void sp_ordd_log(
	ns_stm *stm,
	va_list *args
) {
	sp_ordd *ordd = va_arg(*args, sp_ordd *);
	const u8 sts = (u8) va_arg(*args, int);
	ordd_switch(log, ordd, ordd, stm, sts);
}

static uerr _ordd_stk_invalid(
	sp_ordd *ordd,
	u8 sts
) {
	sp_ordd_stk *stk = &ordd->stk;
	if (!stk->req_nb) {
		error("stock order invalid : asked for 0 stocks.\n");
		return 1;
	}
	u8 typ = ordd->typ;
	if (
		(typ == SP_ORD_STK_LIM_BUY) ||
		(typ == SP_ORD_STK_LIM_SEL)
	) {
		if (sp_ma_n(stk->req_limit_price)) {
			error("negative limit price (%@).\n", sp_ma_dsc(stk->req_limit_price));
			return 1;
		}
	}
	else if (typ == SP_ORD_STK_STP_SEL) {
		if (sp_ma_n(stk->req_stop_price)) {
			error("negative stop price (%@).\n", sp_ma_dsc(stk->req_stop_price));
			return 1;
		}
	}
	else if (
		(typ == SP_ORD_STK_STP_LIM_BUY) ||
		(typ == SP_ORD_STK_STP_LIM_SEL)
	) {
		sp_ma stop = stk->req_stop_price;
		sp_ma limit = stk->req_limit_price;
		if (sp_ma_n(stop)) {
			error("negative stop-limit stop price (%@).\n", sp_ma_dsc(stop));
			return 1;
		}
		if (sp_ma_n(limit)) {
			error("negative stop-limit limit price (%@).\n", sp_ma_dsc(limit));
			return 1;
		}
	}
	if (sts == SP_ORD_STS_CPL) {
		if (sp_ma_n(stk->rsp_ttl)) {
			error("negative stock order amount '%@'.\n", sp_ma_dsc(stk->rsp_ttl));
			return 1;
		}
	}
	return 0;
}

/*
 * If @ordd is valid, return 0.
 * If its content is invalid, return 1.
 */
uerr sp_ordd_invalid(
	sp_ordd *ordd,
	u8 sts
) {
	if (sts > SP_ORD_STS_CPL) {
		error("invalid order status '%u'.\n", sts);
		return 1;
	}
	if (ordd->cat > SP_ORD_CAT_STK) {
		error("invalid order category '%u'.\n", ordd->cat);
		return 1;
	}
	ordd_switch(invalid, ordd, ordd, sts);
	return 1;
}

static uerr _ordd_stk_ct_imp(
	sp_ordd *ordd,
	ct_val *val
) {
	if (ordd->typ >= SP_ORD_STK_NB) {
		debug("invalid stock order typ %u.\n", ordd->typ);
		return 1;
	}
	sp_ordd_stk *ordd_stk = &ordd->stk;
	ct_imp(sp_ordd_stk, ordd_stk, val,
		(sp_str, sym),
		(sp_str, mkp),
		(sp_ccy, ccy),
		(u32, req_nb),
		(sp_ma, req_stop_price),
		(sp_ma, req_limit_price),
		(u32, rsp_nb),
		(sp_ma, rsp_ttl)
	);
	return 0;
}

static inline uerr _ordd_impl_ct_imp(
	sp_ordd *ordd,
	ct_val *val
)
{
	if (ordd->cat > SP_ORD_CAT_STK) {
		error("invalid order category '%u'.\n", ordd->cat);
		return 1;
	}
}

/*
 * Import @ordd's content from @val and return 0.
 * If an error occurs, return 1.
 */
uerr sp_ordd_ct_imp(
	sp_ordd *ordd,
	ct_val *val
)
{
	ct_imp(sp_ordd, ordd, val,
		(u8, cat),
		(u8, typ)
	);
	ordd_switch(ct_imp, ordd, ordd, val);
	return 1;
}

static ct_val *_ordd_stk_ct_exp(
	sp_ordd *ordd
) {
	sp_ordd_stk *stk = &ordd->stk;
	ct_obj *obj = (ct_obj *) ct_exp(stk,
		(sp_str, mkp),
		(sp_str, sym),
		(sp_ccy, ccy),
		(u32, req_nb),
		(sp_ma, req_stop_price),
		(sp_ma, req_limit_price),
		(u32, rsp_nb),
		(sp_ma, rsp_ttl)
	);
	ct_obj_put(obj, "cat", ct_u8_ctor(ordd->cat));
	ct_obj_put(obj, "typ", ct_u8_ctor(ordd->typ));
	return (ct_val *) obj;
}

/*
 * Generate and return a descriptor of @ordd.
 */
ct_val *sp_ordd_ct_exp(
	sp_ordd *ordd
)
{
	ordd_switch(ct_exp, ordd, ordd);
	return 0;
}

static void _ordd_stk_set_trivial_resp(
	sp_ordd *ordd
) {
	u32 nb = ordd->stk.rsp_nb = ordd->stk.req_nb;
	u8 typ = ordd->typ;
	u8 is_limit = (typ == SP_ORD_STK_LIM_BUY) || (typ == SP_ORD_STK_LIM_SEL);
	sp_ma price = (is_limit) ? 
		ordd->stk.req_limit_price :
		ordd->stk.req_limit_price;
	ordd->stk.rsp_ttl = sp_ma_mul(price, nb);
}

/*
 * Set @ordd's response to what's specified in the request.
 */
void sp_ordd_set_trivial_resp(
	sp_ordd *ordd
) {ordd_switch(set_trivial_resp, ordd, ordd);}


static void _ordd_stk_copy_resp(
	sp_ordd *ordd,
	sp_ordd *src
) {
	ordd->stk.rsp_nb = src->stk.rsp_nb;
	ordd->stk.rsp_ttl = src->stk.rsp_ttl;
}

/*
 * Transition @ordd from active to complete, update @ordd from @src.
 */
void sp_ordd_copy_resp(
	sp_ordd *ordd,
	sp_ordd *src
) {ordd_switch(copy_resp, ordd, ordd, src);}

/*********
 * Order *
 *********/

/*
 * Construct @ord from its fields.
 */
void sp_ord_ctor(
	sp_ord *ord,
	u64 id,
	u8 sts,
	sp_ordd *ordd
)
{
	ord->id = id;
	ord->sts = sts;
	ord->ordd = *ordd;
}

/*
 * If @ord0 and ord1 are the same, return 0.
 * If they differ, return 1.
 */
uerr sp_ord_cmp(
	sp_ord *ord0,
	sp_ord *ord1
)
{

	/* Compare identifiers. */
	if (ord0->id != ord1->id) {
		error("stock order dscs have different ids : '%U' and '%U'.\n", ord0->id, ord1->id);
		return 1;
	}

	/* Compare status. */
	if (ord0->sts != ord1->sts) {
		error("stock order dscs have different status : '%u' and '%u'.\n",
			ord0->sts, ord1->sts);
		return 1;
	}

	/* Compare order descriptors. */
	return sp_ordd_cmp(&ord0->ordd, &ord1->ordd);

}

/*
 * Print log data for an order descriptor.
 */
void sp_ord_log(
	ns_stm *stm,
	va_list *args
)
{
	sp_ord *ord = va_arg(*args, sp_ord *);
	ns_stm_writef(stm, "ord (%U, %u, %@)",
		ord->id, ord->sts, sp_ordd_dsc(&ord->ordd, ord->sts));
}

/*
 * If @ord is valid, return 0.
 * If its content is invalid, return 1.
 */
uerr sp_ord_invalid(
	sp_ord *ord
)
{
	if (!ord->id) {
		error("order invalid : null order id.\n");
		return 1;
	}
	const u8 sts = ord->sts;
	if (sts > SP_ORD_STS_CPL) {
		error("order invalid : unknown status '%u'.\n", sts);
		return 1;
	}
	return sp_ordd_invalid(&ord->ordd, sts);
}

#define ORD_DSC \

/*
 * Import @ord's content from @dsc and return 0.
 * If an error occurs, return 1.
 */
uerr sp_ord_ct_imp(
	sp_ord *ord,
	ct_val *val
)
{
	ct_imp(sp_ord, ord, val,
		(u64, id),
		(bu8, sts, 0, SP_ORD_STS_CPL),
		(sp_ordd, ordd)
	);
	return 0;
}

/*
 * Generate and return a descriptor of @ord.
 */
ct_val *sp_ord_ct_exp(
	sp_ord *ord
)
{
	return ct_exp(ord,
		(u64, id),
		(bu8, sts, 0, SP_ORD_STS_CPL),
		(sp_ordd, ordd)
	);
}

/*
 * Transition @ord from idle to active.
 */
void sp_ord_idle_to_act(
	sp_ord *ord
)
{
	assert(ord->sts == SP_ORD_STS_IDL, "expected idle status, got '%u.\n", ord->sts);
	ord->sts = SP_ORD_STS_ACT;
}

/*
 * Transition @ord from idle to cancelled.
 */
void sp_ord_idle_to_ccl(
	sp_ord *ord
)
{
	assert(ord->sts == SP_ORD_STS_IDL, "expected idle status, got '%u.\n", ord->sts);
	ord->sts = SP_ORD_STS_CCL;
}

/*
 * Transition @ord from idle to cancelled.
 */
void sp_ord_idle_to_cpl(
	sp_ord *ord
)
{
	assert(ord->sts == SP_ORD_STS_IDL, "expected idle status, got '%u.\n", ord->sts);
	ord->sts = SP_ORD_STS_CPL;
}

/*
 * Transition @ord from active to cancelled.
 */
void sp_ord_act_to_ccl(
	sp_ord *ord
)
{
	assert(ord->sts == SP_ORD_STS_ACT, "expected active status, got '%u.\n", ord->sts);
	ord->sts = SP_ORD_STS_CCL;
}

/*
 * Transition @ord from active to complete, update @ord from @src.
 */
void sp_ord_act_to_cpl(
	sp_ord *ord
)
{
	assert(ord->sts == SP_ORD_STS_ACT, "expected active status, got '%u.\n", ord->sts);
	ord->sts = SP_ORD_STS_CPL;
}

