/* Copyright 2025 Raphael Outhier - confidential - proprietary - no copy - no diffusion. */

#include <tb_tst/tb_tst.all.h>

/*
 * Unit test for constants validation.
 */
static inline void _lvl_unt_cst(
	u64 sed,
	u64 *nt_err_cnt
)
{

	/* Three levels. */
	nt_chk(TB_LVL_NB == 3);
	nt_chk(TB_ANB_LV0 == 5);
	nt_chk(TB_ANB_LV1 == 3);
	nt_chk(TB_ANB_LV2 == 6);
	nt_chk(TB_ANB_MAX == 6);

	/* First array is always time. */
	nt_chk(tb_lvl_tim_idx(0) == 0);
	nt_chk(tb_lvl_tim_idx(1) == 0);
	nt_chk(tb_lvl_tim_idx(2) == 0);

	/* 22000 entries index for prod.
	 * 2000 entries for test. */
	nt_chk(tb_lvl_idx_siz(0, 0) == 22000);
	nt_chk(tb_lvl_idx_siz(0, 1) == 22000);
	nt_chk(tb_lvl_idx_siz(0, 2) == 22000);
	nt_chk(tb_lvl_idx_siz(1, 0) == 2000);
	nt_chk(tb_lvl_idx_siz(1, 1) == 2000);
	nt_chk(tb_lvl_idx_siz(1, 2) == 2000);

	/* Three elements per block in test mode.
	 * 2 ^ 19 elements per block in prod lv0.
	 * 2 ^ 26 elements per block in prod lv1 and lv2. */
	nt_chk(tb_lvl_blk_len(0, 0) == 1 << 19);
	nt_chk(tb_lvl_blk_len(0, 1) == 1 << 26);
	nt_chk(tb_lvl_blk_len(0, 2) == 1 << 26);
	nt_chk(tb_lvl_blk_len(1, 0) == 3);
	nt_chk(tb_lvl_blk_len(1, 1) == 3);
	nt_chk(tb_lvl_blk_len(1, 2) == 3);

	/*
	 * Syn data region for level 0.
	 * Syn and snap data for level 1 and 2.
	 */
	nt_chk(tb_lvl_rgn_nbr(0) == 1);
	nt_chk(tb_lvl_rgn_nbr(1) == 2);
	nt_chk(tb_lvl_rgn_nbr(2) == 2);

	/*
	 * Region 0 is always 64K.
	 * Region 1 when it exists is an orderbook snapshot.
	 */
	nt_chk(tb_lvl_rgn_sizs(0)[0] == 65536);
	nt_chk(tb_lvl_rgn_sizs(1)[0] == 65536);
	nt_chk(tb_lvl_rgn_sizs(2)[0] == 65536);
	nt_chk(tb_lvl_rgn_sizs(1)[1] == 1025 * 8);
	nt_chk(tb_lvl_rgn_sizs(2)[1] == 1025 * 8);

	/*
	 * Array numbers as previously checked.
	 */
	nt_chk(tb_lvl_arr_nbr(0) == 5);
	nt_chk(tb_lvl_arr_nbr(1) == 3);
	nt_chk(tb_lvl_arr_nbr(2) == 6);

	/*
	 * Array element sizes.
	 */
	nt_chk(tb_lvl_arr_elm_sizs(0)[0] == 8);
	nt_chk(tb_lvl_arr_elm_sizs(0)[1] == 8);
	nt_chk(tb_lvl_arr_elm_sizs(0)[2] == 8);
	nt_chk(tb_lvl_arr_elm_sizs(0)[3] == 8);
	nt_chk(tb_lvl_arr_elm_sizs(0)[4] == 8);
	nt_chk(tb_lvl_arr_elm_sizs(1)[0] == 8);
	nt_chk(tb_lvl_arr_elm_sizs(1)[1] == 8);
	nt_chk(tb_lvl_arr_elm_sizs(1)[2] == 8);
	nt_chk(tb_lvl_arr_elm_sizs(2)[0] == 8);
	nt_chk(tb_lvl_arr_elm_sizs(2)[1] == 8);
	nt_chk(tb_lvl_arr_elm_sizs(2)[2] == 8);
	nt_chk(tb_lvl_arr_elm_sizs(2)[3] == 1);
	nt_chk(tb_lvl_arr_elm_sizs(2)[4] == 8);
	nt_chk(tb_lvl_arr_elm_sizs(2)[5] == 8);

}

/*
 * Test sequence.
 */
static inline void _lvl_tsq(
	nh_tst_exc *exc,
	void *_
)
{
	NH_TST_UNT(exc, _lvl_unt_cst);
}

/*
 * Level data testing.
 */
void tb_tst_lvl(
	nh_tst_sys *sys,
	u64 sed,
	u8 wrk_nb,
	u8 prc
)
{
	void *arg = 0;
	nh_tst_psh__(sys, sed, _lvl_tsq, arg);
}

