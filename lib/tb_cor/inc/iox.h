/* Copyright 2025 Raphael Outhier - confidential - proprietary - no copy - no diffusion. */

/*
 * The IO library sits on top of the storage library
 * and provides toplevel (inter-block) data read and
 * write utilities.
 * On the read side, it handles data padding, and
 * intra-block orderbook reconstruction.
 * On the write side, it handles block validation.
 */

#ifndef TB_COR_IOX_H
#define TB_COR_IOX_H

/*********
 * Types *
 *********/

types(
	tb_dr1
);

/**************
 * Structures *
 **************/

/*
 * Level 1 data reconstructor.
 */
struct tb_dr1 {

	/* Marketplace. */
	tb_str mkp;

	/* Instrument. */
	tb_str ist;

	/* Storage. */
	tb_stg_sys *stg;

	/* Index. */
	tb_stg_idx *idx;

	/* Current block. Never null. */
	tb_stg_blk *blk;

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

	/* Last read time. */
	u64 tim_lst;

	/* Level 1 reconstructor. */
	tb_lv1_hst *hst;


};	

/************
 * Read API *
 ************/

/*
 * Construct a level 1 data reconstructor for (@mkp, @ist)
 * read through @sys, initialized with data up to @tim_cur.
 * The reconstructor has the entire heatmap data
 * populated until @tim_cur. 
 */
tb_dr1 *tb_dr1_ctr(
	tb_stg_sys *sys,
	const char *mkp,
	const char *ist,
	u64 tim_res,
	u64 hmp_dim_tck,
	u64 hmp_dim_tim,
	u64 bac_nb,
	u64 tim_cur
);

/*
 * Delete @dr1.
 */
void tb_dr1_dtr(
	tb_dr1 *dr1
);

/*
 * Add data in @dr1 until @tim_cur, process it
 * and update the heatmap and bid-ask curves, if any.
 * If @end is set, tolerate reaching the end of the index.
 * If @end is clear, abort if the end of the index is
 * reached.
 */
void tb_dr1_add(
	tb_dr1 *dr1,
	u64 tim_cur,
	u8 end
);

/*
 * Delete all updates that are too old to appear
 * in the heatmap anymore.
 */
static inline void tb_dr1_cln(
	tb_dr1 *dr1
) {return tb_lv1_cln(dr1->hst);}

/*
 * Return @dr1's heatmap.
 */
static inline f64 *tb_dr1_hmp(
	tb_dr1 *dr1
) {return tb_lv1_hmp(dr1->hst);}

/*
 * If @dr1 supports it, return its bid curve.
 * If not return 0.
 */
static inline u64 *tb_dr1_bid(
	tb_dr1 *dr1
) {return tb_lv1_bid(dr1->hst);}

/*
 * If @dr1 supports it, return its ask curve.
 * If not return 0.
 */
static inline u64 *tb_dr1_ask(
	tb_dr1 *dr1
) {return tb_lv1_bid(dr1->hst);}

/*************
 * Write API *
 *************/

/*
 * Level 0 data write.
 */
#if 0
/* Not implemented for now. */
void tb_io0_wrt(
	tb_stg_idx *idx,
	u64 nb,
	const u64 *tim,
	const f64 *bid,
	const f64 *ask,
	const f64 *avg,
	const f64 *vol
);
#endif

/*
 * Level 1 data write.
 * Receives a giga orderbook snapshot to compute the
 * new block's orderbook snapshot.
 */
void tb_io1_wrt(
	tb_stg_idx *idx,
	u64 nb,
	const u64 *tim,
	const f64 *prc,
	const f64 *vol,
	f64 *gos
);
	
/*
 * Level 2 data write.
 */
#if 0
/* Not implemented for now. */
void tb_io2_wrt(
	tb_stg_idx *idx,
	u64 nb,
	const u64 *tim,
	const u64 *ord,
	const u64 *trd,
	const u8 *typ,
	const f64 *prc,
	const f64 *vol,
);
#endif

#endif /* TB_COR_IOX_H */
