
/* Copyright 2025 Raphael Outhier - confidential - proprietary - no copy - no diffusion. */

#ifndef TB_COR_TYP_H
#define TB_COR_TYP_H

/*********
 * Types *
 *********/

types(
	tb_stg_blk_syn,
	tb_stg_blk,
	tb_stg_idx,
	tb_stg_sys
);

/**************
 * Structures *
 **************/

/*
 * A data block is composed of first tier data fetched
 * directly from a provider, and of second tier data,
 * derived from first tier data of this block, or from
 * first or second tier data of earlier blocks. 
 * Each block has a shard syn page that reports
 * the status of second tier data.
 */
struct tb_stg_blk_syn {

	/* Is someone currently initializing second-tier data ? */
	volatile aad scd_wip;
	
	/* Is the second tier data initialized ? */
	volatile aad scd_ini;

};

/*
 * The storage block contains historical data for a
 * time range.
 */
struct tb_stg_blk {

	/* Blocks of the same index indexed by block number */
	ns_mapn_u64 blks;

	/* Segment. */
	tb_sgm *sgm;

	/* Block sync data. */
	tb_stg_blk_syn *syn;

	/* Usage counter. */
	u32 uctr;
	
};

/*
 * Storage index.
 */
struct tb_stg_idx {

	/* Indexes of the same system indexed by identifier. */
	ns_mapn_str idxs;

	/* Blocks indexed by start time. */
	ns_map_u64 blks;

	/* System. */
	tb_stg_sys *sys;

	/* Level. */
	u8 lvl;

	/* Write key. */
	u64 key;

	/* Index segment.
	 * One array of u64 couples. */
	tb_sgm *sgm;

	/* Block table. */
	volatile u64 (*tbl)[2];

	/* Usage counter. */
	u32 uctr;

	/* Identifier : "mkp:ist:lvl" */
	tb_str idt;

	/* Marketplace. */
	tb_str mkp;

	/* Instrument. */
	tb_str ist;

};

/*
 * Storage system.
 */
struct tb_stg_sys {

	/* Storage directory path. */
	const char *pth;

	/* Indexes. */
	ns_map_str idxs;

	/* Number of active data interfaces. */
	u32 itf_nb;

};

#endif /* TB_COR_TYP_H */
