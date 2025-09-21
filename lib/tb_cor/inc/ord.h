/* Copyright 2025 Raphael Outhier - confidential - proprietary - no copy - no diffusion. */

#ifndef SP_COR_ORD_H
#define SP_COR_ORD_H

/*********
 * Types *
 *********/

types(
	sp_ordd_stk,
	sp_ordd,
	sp_ord
)

/*********************
 * Order descriptors *
 *********************/

/*
 * Order categories.
 */

/* Stock buy-sell order. */
#define SP_ORD_CAT_STK 0

/* Number of order categories. */
#define SP_ORD_CAT_NB 1

/*
 * Stock order types.
 */

/* Stock market sell order. */
#define SP_ORD_STK_MKT_SEL 0

/* Stock stop sell order. */
#define SP_ORD_STK_STP_SEL 1

/* Stock limit sell order. */
#define SP_ORD_STK_LIM_SEL 2

/* Stock stop-limit sell order. */
#define SP_ORD_STK_STP_LIM_SEL 3

/* Stock limit buy order. */
#define SP_ORD_STK_LIM_BUY 4

/* Stock stop-limit buy order. */
#define SP_ORD_STK_STP_LIM_BUY 5

/* Start of stock sell order types. */
#define SP_ORD_STK_SEL SP_ORD_STK_LIM_SEL

/* Start of stock buy order types. */
#define SP_ORD_STK_BUY SP_ORD_STK_LIM_BUY

/* Number of stock order types. */
#define SP_ORD_STK_NB 6

/*
 * Stock order descriptors.
 */
struct sp_ordd_stk {

	/* Marketplace name buffer. */
	sp_str mkp;

	/* Stock symbol name buffer. */
	sp_str sym;

	/* Stock currency. */
	sp_ccy *ccy;

	/*
	 * Request data.
	 * Set by the client.
	 */

	/* Number of stocks to buy / sell. */
	u32 req_nb;

	/* Stop price. */
	sp_ma req_stop_price;

	/* Limit price. */
	sp_ma req_limit_price;

	/*
	 * Response data.
	 * Set by the broker after completion.
	 */

	/* Number of stocks sold / bought. */
	u32 rsp_nb;

	/* Total amount of money involved in selling / buying those stocks. */
	sp_ma rsp_ttl;

};


/*********
 * Order *
 *********/

/*
 * An order descriptor describes the implementation details of an order.
 */ 
struct sp_ordd {

	/* Order category. */
	u8 cat;

	/* Order type. */
	u8 typ;

	/* Order data. */
	union {

		/* Stock data. */
		sp_ordd_stk stk;

	};
	
};

/*
 * An order contains all information required to manage an order.
 */
struct sp_ord {

	/* Order id. */
	u64 id;

	/* Status. */
	u8 sts;

	/* Descriptor. */
	sp_ordd ordd;

};

/* 
 * Order status.
 */

/* Order created by the portfolio but not acknowledged by the broker. */
#define SP_ORD_STS_IDL 0

/* Order acknowledged by the broker, not cancelled or complete. */
#define SP_ORD_STS_ACT 1

/* Order cancelled. */
#define SP_ORD_STS_CCL 2

/* Order complete. */
#define SP_ORD_STS_CPL 3

/* Number of order statuses. */
#define SP_ORD_STS_NB 4

/********************
 * Order descriptor *
 ********************/

/*
 * Call a function depending on the order descriptor type.
 */
#define ordd_switch(name, ordd, ...) \
{ \
	u8 __cat = ordd->cat; \
	assert(__cat < SP_ORD_CAT_NB, "invalid order category (%u).\n", __cat); \
	if (__cat == SP_ORD_CAT_STK) \
		return _ordd_stk_##name(__VA_ARGS__); \
	check(0); \
}
#define ordd_switch_noret(name, ordd, ...) \
{ \
	u8 __cat = ordd->cat; \
	assert(__cat < SP_ORD_CAT_NB, "invalid order category (%u).\n", __cat); \
	if (__cat == SP_ORD_CAT_STK) \
		_ordd_stk_##name(__VA_ARGS__); \
}

/*
 * Construct an initialized ordd.
 */
void sp_ordd_ctor(
	sp_ordd *ordd
);

/*
 * Destruct @ordd.
 */
void sp_ordd_dtor(
	sp_ordd *ordd
);

/*
 * If requests of @ordd0 and @ordd1 are the same, return 0.
 * If they differ, return 1.
 */
uerr sp_ordd_cmp_req(
	sp_ordd *ordd0,
	sp_ordd *ordd1
);

/*
 * If @ordd0 and @ordd1 are the same, return 0.
 * If they differ, return 1.
 */
uerr sp_ordd_cmp(
	sp_ordd *ordd0,
	sp_ordd *ordd1
);

/*
 * Print log data for an order descriptor.
 */
void sp_ordd_log(
	ns_stm *stm,
	va_list *args
);
#define sp_ordd_dsc(ordd, sts) &sp_ordd_log, (ordd), (u8) (sts)

/*
 * If @ordd is valid, return 0.
 * If its content is invalid, return 1.
 */
uerr sp_ordd_invalid(
	sp_ordd *ordd,
	u8 sts
);

/*
 * Import @ordd's content from @dsc and return 0.
 * If an error occurs, return 1.
 */
uerr sp_ordd_ct_imp(
	sp_ordd *ordd,
	ct_val *dsc
);

/*
 * Generate and return a descriptor of @ordd.
 */
ct_val *sp_ordd_ct_exp(
	sp_ordd *ordd
);

/*
 * Set @ordd's response to what's specified in the request.
 */
void sp_ordd_set_trivial_resp(
	sp_ordd *ordd
);

/*
 * Copy @src's response into @dst's.
 */
void sp_ordd_copy_resp(
	sp_ordd *dst,
	sp_ordd *src
);

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
);

/*
 * If @ord0 and ord1 are the same, return 0.
 * If they differ, return 1.
 */
uerr sp_ord_cmp(
	sp_ord *ord0,
	sp_ord *ord1
);

/*
 * Print log data for an order descriptor.
 */
void sp_ord_log(
	ns_stm *stm,
	va_list *args
);
#define sp_ord_dsc(val) &sp_ord_log, val

/*
 * Construct @ord, an idle order.
 */
static inline void sp_ord_ctor_idl(
	sp_ord *ord,
	u64 id,
	sp_ordd *ordd
) {sp_ord_ctor(ord, id, SP_ORD_STS_IDL, ordd);}

/*
 * If @ord is valid, return 0.
 * If its content is invalid, put the current component in error
 * and return 1.
 */
uerr sp_ord_invalid(
	sp_ord *ord
);

/*
 * Import @ord's content from @dsc and return 0.
 * If an error occurs, return 1.
 */
uerr sp_ord_ct_imp(
	sp_ord *ord,
	ct_val *val
);

/*
 * Generate and return a descriptor of @ord.
 */
ct_val *sp_ord_ct_exp(
	sp_ord *ord
);

/*
 * Transition @ord from idle to active, update @ord from @src.
 */
void sp_ord_idl_to_act(
	sp_ord *ord
);

/*
 * Transition @ord from idle to active, update @ord from @src.
 */
void sp_ord_idl_to_ccl(
	sp_ord *ord
);

/*
 * Transition @ord from idle to active, update @ord from @src.
 */
void sp_ord_idl_to_cpl(
	sp_ord *ord
);

/*
 * Transition @ord from active to cancelled, update @ord from @src.
 */
void sp_ord_act_to_ccl(
	sp_ord *ord
);

/*
 * Transition @ord from active to complete, update @ord from @src.
 */
void sp_ord_act_to_cpl(
	sp_ord *ord
);

/*
 * Set @ord's response to what's specified in the request.
 */
static inline void sp_ord_set_trivial_resp(
	sp_ord *ord
) {sp_ordd_set_trivial_resp(&ord->ordd);}

/*
 * Copy @src's response into @dst's.
 */
static inline void sp_ord_copy_resp(
	sp_ord *dst,
	sp_ord *src
) {sp_ordd_copy_resp(&dst->ordd, &src->ordd);}

#endif /* SP_COR_ORD_H */
