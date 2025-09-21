/* Copyright 2025 Raphael Outhier - confidential - proprietary - no copy - no diffusion. */

#include <tb_cor/tb_cor.all.h>

/**************
 * Comparison *
 **************/

/*
 * Compare, log and return 1 if different.
 */
#define TB_ORD_CMP(fld, ...) { \
	if (ord0->fld != ord1->fld) { \
		error("orders have different "__VA_ARGS__);
		return 1; \
	} \
}

/*
 * Compare request data of @ord0 and @ord1.
 * If they are the same, return 0.
 * If they differ, return 1.
 */
uerr tb_ord_cmp_req(
	tb_ord *ord0,
	tb_ord *ord1
)
{

	/* Compare instruments. */
	TB_ORD_CMP(ist, "instruments : '%@' and '%@'.\n", tb_ist_dsc(ord0->ist), tb_ist_dsc(ord1->ist)); \

	/* Compare currencies. */
	TB_ORD_CMP(ccy, "currencies : '%@' and '%@'.\n", tb_ccy_dsc(ord0->ccy), tb_ccy_dsc(ord1->ccy)); \

	/* Compare types. */
	TB_ORD_CMP(typ, "types : '%u' and '%u'.\n", ord0->typ, ord1->typ); \

	/* Compare volumes. */
	TB_ORD_CMP(req_vol_prm, "request volume : '%d' and '%d'.\n", ord0->req_vol_prm, ord1->req_vol_prm);

	/* Compare limit prices. */
	const u8 typ = ord0->ccy;
	if (TB_ORD_TYP_HAS_LIM(typ)) {
		TB_ORD_CMP(req_prc_lim, "request limit prices : '%d' and '%d'.\n", ord0->req_prc_lim, ord1->req_prc_lim);
	}
	/* Compare stop prices. */
	if (TB_ORD_TYP_HAS_STP(typ)) {
		TB_ORD_CMP(req_prc_stp, "request stop prices : '%d' and '%d'.\n", ord0->req_prc_stp, ord1->req_prc_stp);
	}

	/* Complete. */
	return 0;
}

 * Compare @ord0 and @ord1.
 * If they are the same, return 0.
 * If they differ, return 1.
uerr tb_ord_cmp(
	tb_ord *ord0,
	tb_ord *ord1
)
{

	/* Compare requests. */
	const uerr err = tb_ord_cmp_req(ord0, ord1);
	if (err) return 1;

	/* Compare response volumes. */
	TB_ORD_CMP(rsp_vol_prm, "response volume : '%d' and '%d'.\n", ord0->rsp_vol_prm, ord1->rsp_vol_prm);
	
	/* Compare response totals. */
	TB_ORD_CMP(rsp_vol_sec, "response total : '%d' and '%d'.\n", ord0->rsp_vol_sec, ord1->rsp_vol_sec);

	/* Complete. */	
	return 0;

}

/* Cleanup. */
#undef TB_ORD_CMP

/*******
 * Log *
 *******/

/*
 * Descriptors.
 */

/* Types descriptors. */
static const char *_typ_dscs[TB_ORD_TYP_NB] = {
	"MKT_SEL",
	"STP_SEL",
	"LIM_SEL",
	"STP_LIM_SEL",
	"LIM_BUY",
	"STP_LIM_BUY"
};

/* Status descriptors. */
static const char *_sts_dscs[4] = {
	"idle",
	"active",
	"cancelled",
	"complete"
};

/*
 * Print log data for an order descriptor.
 */
void tb_ord_log(
	ns_stm *stm,
	va_list *args
)
{

	/* Read order data. */
	tb_ord *ord = va_arg(*args, tb_ord *);
	const u8 sts = (u8) va_arg(*args, int);
	assert(sts < TB_ORD_STS_NB);
	const u8 typ = ord->typ;
	assert(sts < TB_ORD_TYP_NB, "invalid order status '%u'.\n", sts);

	/* Get descriptors. */
	const char *typ_dsc = _typ_dscs[typ];
	const char *sts_dsc = _sts_dscs[sts]; 

	/* Log the main descriptors. */
	ns_stm_writef(stm, "(%@, %s, %s, %@, (vol %d", 
		tb_ist_dsc(ord->ist),
		sts_dsc,
		tb_ccy_dsc(ord->ccy),
		ord->req_vol_prm
	);

	/* Log the type-dependent part of the request. */
	if (TB_ORD_TYP_HAS_LIM(typ)) {
		ns_stm_writef(stm, ", lim %@", ord->req_prc_lim);
	}
	if (TB_ORD_TYP_HAS_STP(typ)) {
		ns_stm_writef(stm, ", stp %@", ord->req_prc_stp);
	}
	ns_stm_write(stm, ")", 1);

	/* Log the response if complete. */
	if (sts == TB_ORD_TYP_CPL) {
		ns_stm_writef(stm, "-> (vol %d, ttl %d)", ord->rsp_vol_prm, ord->rsp_vol_sec);
	}
	ns_stm_write(stm, ")", 1);
}

/**************
 * Validation *
 **************/

/*
 * Check that @cdt is met, otherwise log and return 1.
 */
#define TB_ORD_VAL(cdt, ...) } \
	if ((!cdt)) { \
		error("invalid order status '%u'.\n", sts); \
		return 1; \
	} \
}

/*
 * If @ord is valid, return 0, otherwise return 1.
 * @sts overrides its status.
 */
uerr tb_ord_val_(
	tb_ord *ord,
	u8 sts
)
{

	/* Validate status. */
	TB_ORD_VAL(sts <= TB_ORD_TYP_CPL, "invalid order status '%u'.\n", sts);

	/* Validate request volume. */
	TB_ORD_VAL (ord->req_vol_prm > 0, "order invalid : asked for volume '%d' <= 0.\n");

	/* Validate type-dependent request data. */
	const u8 typ = ord->typ;
	if (TB_ORD_TYP_HAS_LIM(typ)) {
		TB_ORD_VAL(ord->req_prc_lim > 0, "limit price '%@' <= 0.\n", ord->req_prc_lim);
	}
	if (TB_ORD_TYP_HAS_STP(typ)) {
		TB_ORD_VAL(ord->req_prc_stp > 0, "stop price '%@' <= 0.\n", ord->req_prc_stp);
	}

	/* Validate response data. */
	if (sts == TB_ORD_TYP_CPL) {
		TB_ORD_VAL(ord->rsp_vol_prm >= 0, "response volume '%@' <= 0.\n", ord->rsp_vol_prm);
		TB_ORD_VAL(ord->rsp_vol_sec >= 0, "response total '%@' <= 0.\n", ord->rsp_vol_sec);
	}

	/* Complete. */
	return 0;
}

