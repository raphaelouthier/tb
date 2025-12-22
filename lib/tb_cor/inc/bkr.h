/* Copyright 2025 Raphael Outhier - confidential - proprietary - no copy - no diffusion. */

#ifndef TB_BKR_H
#define TB_BKR_H

/*
 * Remote Brokers often provide different APIs
 * to pass orders.
 *
 * Those APIs are unlikely to be implemented in pure C,
 * and may vary from one bot to the other.
 *
 * To keep the system simple, bots communicate with
 * remote brokers via an intermediary program, which
 * just translates between low level bot protocol
 * and high level remote broker protocol.
 *
 * Actual brokers will be likely implemented in python
 * for convenience.
 *
 * Bots and brokers communicate via a simple
 * message-based protocol running on TCP sockets,
 * where messages are json-based, allowing the actual
 * communication with the remote brokers to be handled
 * by any language.
 */

/*********
 * Types *
 *********/

types(
	spb_bkrt,
	spb_bkr
);

/********
 * Logs *
 ********/

tb_cmp_log_dec(bkr);

/**************
 * Structures *
 **************/

/*
 * Broker type.
 */
struct spb_bkrt {

	/*
	 * Delete @bkr.
	 */
	void (*dtor)(
		spb_bkr *bkr
	);

	/*
	 * Reset @bkr, initialize it at time @tim.
	 * Sim op.
	 */
	void (*tim_rst)(
		spb_bkr *bkr,
		u64 tim
	);

	/*
	 * Set the current tim of @bkr.
	 * Sim op.
	 */
	void (*tim_set)(
		spb_bkr *bkr,
		u64 tim
	);

	/*
	 * Report shutdown.
	 */
	void (*sht)(
		spb_bkr *bkr
	);

	/*
	 * Pass an order described by @dsc, return
	 * its descriptor.
	 */
	tb_ord (*ord_pas)(
		spb_bkr *bkr,
		tb_ord *ord
	);

	/*
	 * Send a cancel request for the order at index @idx.
	 * If bad index, abort.
	 */
	void (*ord_ccl)(
		spb_bkr *bkr,
		u64 idx
	);

	/*
	 * Save the descriptor of the order at index @idx
	 * at @dst.
	 * If bad index, abort.
	 */
	void (*ord_sts)(
		spb_bkr *bkr,
		u64 idx,
		tb_ord *dst
	);

};

/*
 * Broker interface.
 */
struct spb_bkr {

	/* Broker type. */
	spb_bkrt *bkrt;

	/* Set <=> simulation broker. */
	u8 sim;

};

/*******
 * API *
 *******/

/*
 * Report if we have a bks or a bkr.
 */
static inline u8 spb_bkr_is_sim(
	spb_bkr *bkr
) {return bkr->sim;}

/*
 * Delete @bkr.
 */
static inline void spb_bkr_dtor(
	spb_bkr *bkr
) {return (*(bkr->bkrt->dtor))(bkr);}

/*
 * Reset @bkr, initialize it at time @tim.
 * Sim op.
 */
static inline void spb_bkr_tim_rst(
	spb_bkr *bkr,
	u64 tim
) {return (*(bkr->bkrt->tim_rst))(bkr, tim);}

/*
 * Set the current time of @bkr.
 * Sim op.
 */
static inline void spb_bkr_tim_set(
	spb_bkr *bkr,
	u64 tim
) {return (*(bkr->bkrt->tim_set))(bkr, tim);}

/*
 * Report shutdown.
 */
static inline void spb_bkr_sht(
	spb_bkr *bkr
) {return (*(bkr->bkrt->sht))(bkr);}

/*
 * Pass an order described by @dsc, return
 * its descriptor.
 */
static inline tb_ord tb_bkr_ord_pas(
	spb_bkr *bkr,
	tb_ord *ord
) {return (*(bkr->bkrt->ord_pas))(bkr, ord);}

/*
 * Send a cancel request for the order at index @idx.
 * If bad index, abort.
 */
static inline void tb_bkr_ord_ccl(
	spb_bkr *bkr,
	u64 idx
) {return (*(bkr->bkrt->ord_ccl))(bkr, idx);}

/*
 * Save the descriptor of the order at index @idx
 * at @dst.
 * If bad index, abort.
 */
static inline void tb_bkr_ord_sts(
	spb_bkr *bkr,
	u64 idx,
	tb_ord *dst
) {return (*(bkr->bkrt->ord_sts))(bkr, idx, dst);}

#endif /* TB_BKR_H */
