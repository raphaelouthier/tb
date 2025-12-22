/* Copyright 2025 Raphael Outhier - confidential - proprietary - no copy - no diffusion. */

/*
 * The provider is the abstract interface between
 * the agent and the storage.
 * It allows the agent to operate the same way in
 * both simulation and real time mode.
 */

#ifndef TB_PRV_H
#define TB_PRV_H

/*********
 * Types *
 *********/

types(
	tb_prv_stm,
	tb_prv
);

/**********************
 * Provider interface *
 **********************/

/*
 * Data stream.
 */
struct tb_prv_stm {

	/* Provider. */
	tb_prv *prv;

	/* Marketplace. */
	tb_str mkp;

	/* Instrument. */
	tb_str ist;

	/* Data level. */
	u8 lvl;

	/* Index. */
	tb_stg_idx *idx;

	/* Current block. Never null. */
	void *blk;

	/* Index in @blk where @dats point to. */
	u64 elm_idx;

	/* Current number of elements of @blk. */
	u64 elm_nbr;

	/* Maximal number of elements of @blk. */
	u64 elm_max;

	/* Current block end. */
	u64 blk_end;

	/* Number of used arrays for this level. */
	u8 dat_nbr;

	/* Data arrays. */
	const void *dats[TB_ANB_MAX];

	/* Array sizes. */
	const u8 *sizs;


};

/*
 * Unified interface for array / storage provider.
 */
struct tb_prv {

	/* Storage. */
	tb_stg_sys *stg;

	/*
	 * Max time at which read is allowed.
	 * Real-time : -1.
	 * Simulation : not -1.
	 */
	u64 tim_max;

};

tb_cmp_log_dec(prv);
#define _prv_log(...) tb_log_cmp(prv, __VA_ARGS__)

/****************
 * Provider API *
 ****************/

/*
 * Construct and return a provider.
 */
tb_prv *tb_prv_ctr(
	const char *pth,
	u8 sim
);

/*
 * Delete @prv.
 */
void tb_prv_dtr(
	tb_prv *prv
);

/*
 * Update @prv's max read time.
 * It must be in simulation mode.
 */
static inline void tb_prv_sim_tim_set(
	tb_prv *prv,
	u64 tim
)
{
	assert(prv->tim_max != (u64) -1);
	assert(prv->tim_max <= tim);
	prv->tim_max = tim;
}

/**************
 * Stream API *
 **************/

/*
 * Create a stream.
 */
tb_prv_stm *tb_prv_opn(
	tb_prv *prv,
	const char *mkp,
	const char *ist,
	u8 lvl,
	u64 tim_stt
);

/*
 * Delete a stream.
 */
void tb_prv_cls(
	tb_prv_stm *stm
);

/*
 * Return @stm's current block's second tier data.
 * If @stm has no current block, return 0.
 */
static inline void *tb_prv_std(
	tb_prv_stm *stm
) {return tb_stg_std(stm->blk);} 

/*
 * If data is available, store its locations at *dstp,
 * and return the number of available data elements.
 * Otherwise, return 0.
 */
u64 tb_prv_red(
	tb_prv_stm *stm,
	const void **dstp,
	u8 dst_nb
);

#endif /* TB_PRV_H */
