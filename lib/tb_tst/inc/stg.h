/* Copyright 2025 Raphael Outhier - confidential - proprietary - no copy - no diffusion. */

#ifndef TB_TST_STG_H
#define TB_TST_STG_H

/*********
 * Types *
 *********/

types(
	tb_tst_stg_syn,
	tb_tst_stg_dsc
);

/**************
 * Structures *
 **************/

/*
 * Shared synchronized data.
 * Occupies the first region of the test segment.
 */
struct tb_tst_stg_syn {

	/* Gate. */
	ns_gat gat;

};

/*
 * Segment test per-thread descriptor.
 */
struct tb_tst_stg_dsc {

	/* Seed. */
	u64 sed;

	/* Segment implementation size. */
	u64 imp_siz;

	/* Random data. */
	u64 *dat;

	/* Times. */
	u64 *tims;

	/* Storage path. */
	const char *pth;

	/* Number of workers. */
	u8 wrk_nb;

	/* Syn data. */
	tb_tst_stg_syn *syn;

	/* Marketplace. */
	const char *mkp;

	/* Marketplace. */
	const char *ist;

	/* Level. */
	u8 lvl;

	/* Are we the ones with write privs ? */
	u8 wrt;

	/* Write key if @wrt is set. */
	u64 key;

};

/*******
 * API *
 *******/

/*
 * Storage testing.
 */
void tb_tst_stg(
	nh_tst_sys *sys,
	u64 sed,
	u8 wrk_nb,
	u8 prc
);

#endif /* TB_TST_STG_H */
