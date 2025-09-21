/* Copyright 2025 Raphael Outhier - confidential - proprietary - no copy - no diffusion. */

#ifndef TB_ORD_H
#define TB_ORD_H

/*********
 * Types *
 *********/

types(
	tb_ord
)

/*********************
 * Order descriptors *
 *********************/

/*
 * Order types.
 */

/* Market sell order. */
#define TB_ORD_TYP_MKT_SEL 0

/* Stop sell order. */
#define TB_ORD_TYP_STP_SEL 1

/* Limit sell order. */
#define TB_ORD_TYP_LIM_SEL 2

/* Stop-limit sell order. */
#define TB_ORD_TYP_STP_LIM_SEL 3

/* Limit buy order. */
#define TB_ORD_TYP_LIM_BUY 4

/* Stop-limit buy order. */
#define TB_ORD_TYP_STP_LIM_BUY 5

/* Start of sell order types. */
#define TB_ORD_TYP_SEL TB_ORD_TYP_LIM_SEL

/* Start of buy order types. */
#define TB_ORD_TYP_BUY TB_ORD_TYP_LIM_BUY

/* Number of order types. */
#define TB_ORD_TYP_NB 6


/*
 * Determine if an order is a buy order or a sell order.
 */
#define TB_ORD_TYP_IS_BUY(typ) ((typ) >= TB_ORD_STK_BUY)
#define TB_ORD_TYP_IS_SEL(typ) (!TB_ORD_TYP_IS_BUY(typ))

/*
 * Determine if an order has a limit.
 */
#define TB_ORD_TYP_HAS_LIM(typ) ( \
	(typ == TB_ORD_TYP_LIM_BUY) || \
	(typ == TB_ORD_TYP_LIM_SEL) || \
	(typ == TB_ORD_TYP_STP_LIM_BUY) || \
	(typ == TB_ORD_TYP_STP_LIM_SEL)
)

/*
 * Determine if an order has a stop price.
 */
#define TB_ORD_TYP_HAS_STP(typ) ( \
	(typ == TB_ORD_TYP_STP_SEL) || \
	(typ == TB_ORD_TYP_STP_LIM_BUY) || \
	(typ == TB_ORD_TYP_STP_LIM_SEL) \
)

/* 
 * Order status.
 */

/* Order created by the portfolio but not acknowledged by the broker. */
#define TB_ORD_STS_IDL 0

/* Order acknowledged by the broker, not cancelled or complete. */
#define TB_ORD_STS_ACT 1

/* Order cancelled. */
#define TB_ORD_STS_CCL 2

/* Order complete. */
#define TB_ORD_STS_CPL 3

/* Number of order statuses. */
#define TB_ORD_STS_NB 4

/*
 * An order contains all information required to manage an order.
 */
struct tb_ord {

	/*
	 * Request data.
	 * Set by the client.
	 */

	/* Primary instrument. */
	tb_ist *prm;

	/* Secondary. */
	tb_ccy *sec;

	/* Order type. */
	u8 typ;

	/* Volume of @prm to buy or sell. */
	f64 req_vol_prm;

	/* Limit volume of @sec per unit of @prm. */
	f64 req_prc_lim;

	/* Stop volume of @sec per unit of @prm. */
	f64 req_prc_stp;

	/*
	 * Passing data.
	 * Set by the broker.
	 */

	/* Identifier. */
	u64 id;

	/* Status. */
	u8 sts;

	/*
	 * Response data.
	 * Set by the broker after completion.
	 */

	/* Volume of @prm bought or sold. */
	f64 rsp_vol_prm;

	/* Volume of @sec traded in exchange. */
	f64 rsp_vol_sec;
};

/********************
 * Order descriptor *
 ********************/

/*
 * Construct an order.
 */
tb_ord tb_ord_ctor(
	tb_ist *ist,
	tb_ccy *ccy,
	u8 typ,
	f64 req_vol_prm,
	f64 req_prc_lim,
	f64 req_prc_stp,
	u64 id,
	u8 sts
)
{
	ord->ist = ist;
	ord->ccy = ccy;
	ord->typ = typ;
	ord->req_vol_prm = req_vol_prm;
	ord->req_prc_lim = req_prc_lim;
	ord->req_prc_stp = req_prc_stp;
	ord->id = id;
	ord->sts = sts;
	ord->rsp_vol_prm = 0;
	ord->rsp_vol_sec = 0;
}

/*
 * If requests of @ord0 and @ord1 are the same, return 0.
 * If they differ, return 1.
 */
uerr tb_ord_cmp_req(
	tb_ord *ord0,
	tb_ord *ord1
);

/*
 * If @ord0 and @ord1 are the same, return 0.
 * If they differ, return 1.
 */
uerr tb_ord_cmp(
	tb_ord *ord0,
	tb_ord *ord1
);

/*
 * Print log data for an order descriptor.
 */
void tb_ord_log(
	ns_stm *stm,
	va_list *args
);
#define tb_ord_dsc(ord, sts) &tb_ord_log, (ord), (u8) (sts)

/*
 * If @ord is valid, return 0, otherwise return 1.
 * @sts overrides its status.
 */
uerr tb_ord_val_(
	tb_ord *ord,
	u8 sts
);

/*
 * If @ord is valid, return 0, otherwise return 1.
 */
static inline uerr tb_ord_val(
	tb_ord *ord
) {return tb_ord_val(ord, ord->sts);}

/***************
 * Transitions *
 ***************/

/*
 * Transition @ord from idle to active.
 */
static inline void tb_ord_idl_to_act(
	tb_ord *ord
)
{
	assert(ord->sts == TB_ORD_TYP_IDL, "expected idle status, got '%u.\n", ord->sts);
	ord->sts = TB_ORD_TYP_ACT;
}

/*
 * Transition @ord from idle to cancelled.
 */
static inline void tb_ord_idl_to_ccl(
	tb_ord *ord
)
{
	assert(ord->sts == TB_ORD_TYP_IDL, "expected idle status, got '%u.\n", ord->sts);
	ord->sts = TB_ORD_TYP_CCL;
}

/*
 * Transition @ord from idle to cancelled.
 */
static inline void tb_ord_idle_to_cpl(
	tb_ord *ord
)
{
	assert(ord->sts == TB_ORD_TYP_IDL, "expected idle status, got '%u.\n", ord->sts);
	ord->sts = TB_ORD_TYP_CPL;
}

/*
 * Transition @ord from active to cancelled.
 */
static inline void tb_ord_act_to_ccl(
	tb_ord *ord
)
{
	assert(ord->sts == TB_ORD_TYP_ACT, "expected active status, got '%u.\n", ord->sts);
	ord->sts = TB_ORD_TYP_CCL;
}

/*
 * Transition @ord from active to complete, update @ord from @src.
 */
static inline void tb_ord_act_to_cpl(
	tb_ord *ord
)
{
	assert(ord->sts == TB_ORD_TYP_ACT, "expected active status, got '%u.\n", ord->sts);
	ord->sts = TB_ORD_TYP_CPL;
}

/************
 * Response *
 ************/

/*
 * Set @ord's response to what's specified in the request.
 */
static inline void tb_ord_rsp_trv(
	tb_ord *ord
)
{
	ord->rsp_vol_prm = ord->req_vol;
	ord->rsp_vol_sec = ord->req_prc_stp * ord->rsp_vol_prm;
}

/*
 * Copy @src's response into @dst's.
 */
static inline void tb_ord_rsp_cpy(
	tb_ord *dst,
	tb_ord *src
)
{
	dst->rsp_vol_prm = src->rsp_vol_prm;
	dst->rsp_vol_sec = src->rsp_vol_sec;
}

#endif /* TB_ORD_H */
