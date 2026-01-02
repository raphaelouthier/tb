/* Copyright 2025 Raphael Outhier - confidential - proprietary - no copy - no diffusion. */

/*
 * Level specific constants and utils.
 */

#ifndef TB_LVL_H
#define TB_LVL_H

/********************
 * Number of levels *
 ********************/

/* Three levels. */
#define TB_LVL_NB 3

/********************
 * Number of arrays *
 ********************/

/*
 * Level specific constants.
 */

/* Level 0 number of arrays. */
#define TB_ANB_LV0 5

/* Level 1 number of arrays. */
#define TB_ANB_LV1 3

/* Level 2 number of arrays. */
#define TB_ANB_LV2 6

/* Maximal levels number of arrays. */
#define TB_ANB_MAX 6

/*************************
 * Timestamp array index *
 *************************/

/*
 * Determine the index of the timestamp array
 * in the arrays of level @lvl.
 */
static inline u8 tb_lvl_tim_idx(
	u8 lvl
)
{
	/* Always at index 0. */
	return 0;
}

/********************
 * Index table size *
 ********************/

/*
 * Determine the maximal number of elements
 * that an index table of level @lvl has.
 */
static inline u64 tb_lvl_idx_siz(
	u8 tst,
	u8 lvl
)
{
	/* See design.md. */
	assert(lvl < 3);
	return tst ? 2000 : 22000;
}

/**********************
 * Maximal array size *
 **********************/

/*
 * Determine the number of elements that a block
 * of level @lvl should contain.
 */
static inline inline uad tb_lvl_blk_len(
	u8 tst,
	u8 lvl
)
{
	/* See design.md. */
	assert(lvl < 3);
	return tst ? 3 : (
		(uad) 1 << ((lvl == 0) ? 19 : 26)
	);
}

/*********************
 * Number of regions *
 *********************/

/*
 * Determine the number of regions that a block
 * of level @lvl should contain.
 */
static inline inline u8 tb_lvl_rgn_nbr(
	u8 lvl
)
{
	/*
	 * See design.md.
	 * Level 0 has just test sync data.
	 * Level 1 and 2 also have the orderbook state.
	 */
	assert(lvl < 3);
	return (lvl == 0) ? 1 : 2;
}

/****************
 * Region sizes *
 ****************/

/*
 * Every block's region 0 is reserved for its sync
 * data, which occupies the first page.
 */
#define TB_LVL_RGN_SIZ_SYN TB_SGM_PAG_SIZ

/*
 * Levels 1 and 2 contain a snapshot of the orderbook
 * at the end of the block, so that orderbook
 * reconstruction does not need to go many blocks behind.
 * The snapshot of the orderbook is composed of :
 * - nb, the number of tick levels saved (constant).
 * - prc, the tick of the first volume element.
 * - loss, a flag reporting if the orderbook snapshot
 *   is incomplete (extremal values did not fit in a
 *   1024 entries orderbook).
 * - the volumes of the @nb ticks >= @prc, stored in
 *   incremental tick levels starting at @prc.
 *
 * In order to compute the orderbook snapshot at the end
 * of a block, we start with the snapshot of the
 * previous block, which is of size 1024.
 * Then we insert the values in an actual orderbook of
 * size 1Mi elements, centered at the same value than
 * the block's.
 *
 * Then we update the orderbook with all updates that
 * the block contains.
 *
 * Finally, we determine the best and worst bids and asks,
 * deduce the center value, report if the final
 * orderbook snapshot fits in a 1024 entries orderbook
 * (if not, loss), and finallly write the orderbook
 * snapshot.
 */

/*
 * Number of tick levels saved in orderbook snapshot.
 */ 
#define TB_LVL_OBS_NB 1024 

/*
 * Number of tick levels saved in a giga orderbook
 * snapshot.
 */
#define TB_LVL_GOS_NB (1024 * 1024)

/* Number of bytes of an orderbook snapshot region. */
#define TB_LVL_RGN_SIZ_OBS (sizeof(u64) + (TB_LVL_OBS_NB) * sizeof(f64)) 

/*
 * Determine the region sizes that a block
 * of level @lvl should contain.
 */
extern const u64 *const (tb_lvl_to_rgn_sizs[TB_LVL_NB]);
static inline const u64 *tb_lvl_rgn_sizs(
	u8 lvl
)
{
	/* See design.md. */
	assert(lvl < 3);
	return tb_lvl_to_rgn_sizs[lvl];
}

/*****************
 * Arrays number *
 *****************/

/*
 * Determine the number of arrays that a block
 * of level @lvl should contain.
 */
extern const u8 tb_lvl_to_arr_nbr[TB_LVL_NB];
static inline inline u8 tb_lvl_arr_nbr(
	u8 lvl
)
{
	/* See design.md. */
	assert(lvl < TB_LVL_NB);
	return tb_lvl_to_arr_nbr[lvl];
}

/***********************
 * Array element sizes *
 ***********************/

/*
 * Determine the maximal number of elements that a block
 * of level @lvl should contain.
 */
extern const u8 *const (tb_lvl_to_arr_elm_sizs[TB_LVL_NB]);
static inline const u8 *tb_lvl_arr_elm_sizs(
	u8 lvl
)
{
	/* See design.md. */
	assert(lvl < 3);
	return tb_lvl_to_arr_elm_sizs[lvl];
}

#endif /* TB_LVL_H */
