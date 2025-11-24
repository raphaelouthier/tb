/* Copyright 2025 Raphael Outhier - confidential - proprietary - no copy - no diffusion. */

#ifndef TB_TST_SGM_H
#define TB_TST_SGM_H

/*********
 * Types *
 *********/

types(
	tb_tst_sgm_syn,
	tb_tst_sgm_dsc
);

/***********
 * Structs *
 ***********/

/*
 * Shared synchronized data.
 * Occupies the first region of the test segment.
 */
struct tb_tst_sgm_syn {

	/* Gate. */
	ns_gat gat;

	/* Write perm acquisistion failure counter. */
	volatile aad fal_ctr;

	/* Counter. */
	volatile aad val_ctr;

	/* Error. */
	volatile aad err_ctr;

};

/*
 * Segment test per-thread descriptor.
 */
struct tb_tst_sgm_dsc {

	/* Seed. */
	u64 sed;

	/* Segment implementation size. */
	u64 imp_siz;

	/* Random data. */
	u64 *dat;

	/* Storage path. */
	const char *pth;

	/* Number of workers. */
	u8 wrk_nb;

	/* Syn data. */
	tb_tst_sgm_syn *syn;

};

/*******
 * API *
 *******/

/*
 * Segment testing.
 */
void tb_tst_sgm(
	nh_tst_sys *sys,
	u64 sed,
	u8 wrk_nb,
	u8 prc
);

#endif /* TB_TST_SGM_H */
