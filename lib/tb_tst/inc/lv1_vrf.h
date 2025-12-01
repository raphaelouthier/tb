/* Copyright 2025 Raphael Outhier - confidential - proprietary - no copy - no diffusion. */

#ifndef TB_TST_LV1_VRF_H
#define TB_TST_LV1_VRF_H

/*
 * Verify @hst internal state.
 */
void tb_tst_lv1_vrf_hst_stt(
	tb_tst_lv1_ctx *ctx,
	tb_lv1_hst *hst,
	u64 uid_cln,
	u64 uid_cur,
	u64 uid_add,
	u64 tim_cln,
	u64 tim_cur,
	u64 tim_add,
	u8 did_cln
);

/*
 * Verify the result produced by @hst.
 */
void tb_tst_lv1_vrf_hst_res(
	tb_tst_lv1_ctx *ctx,
	tb_lv1_hst *hst,
	u64 tim_cur,
	u64 bst_bid,
	u64 bst_ask,
	u64 *chc_tims,
	f64 *chc_sums,
	f64 *chc_vols,
	u8 ini
);

#endif /* TB_TST_LV1_VRF_H */
