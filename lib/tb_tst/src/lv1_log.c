/* Copyright 2025 Raphael Outhier - confidential - proprietary - no copy - no diffusion. */

#include <tb_tst/tb_tst.all.h>

#include <tb_tst/lv1_utl.h>

/**********
 * Export *
 **********/

/*
 * Export tick data.
 */
static inline void _hmp_exp(
	tb_tst_lv1_ctx *ctx
)
{
	const u64 siz_x = ctx->unt_nbr;
	const u64 siz_y = ctx->tck_nbr;
	const u64 elm_nb = siz_x * siz_y;
	debug("exporting heatmap of size %Ux%U (volume ref %d) at /tmp/hmp.\n", siz_x, siz_y, ctx->ref_vol);
	nh_res res;
	ns_stg *stg = nh_stg_opn(NH_FIL_ATT_RWC, &res, "/tmp/hmp");	
	ns_stg_rsz(stg, elm_nb * sizeof(f64)); 
	f64 *dst = ns_stg_map(stg, 0, 0, elm_nb * sizeof(f64), NS_STG_ATT_RWS); 
	ns_mem_cpy(dst, ctx->hmp, elm_nb * sizeof(f64));
	ns_stg_syn(stg, dst, elm_nb * sizeof(f64));
	ns_stg_ump(stg, dst, elm_nb * sizeof(f64));
	nh_stg_cls(&res);
}

/*
 * Log all updates in @hst's update queue.
 */
static void _log_hst_upds(
	tb_lv1_hst *hst
)
{
	debug("Updates of hst %p.\n", hst);
	tb_lv1_upd *upd;
	ns_slsh_fe(upd, &hst->upds_hst, upds_hst) {
		debug("  upd %p : tim : %U, tck : %U, vol : %d.\n", upd, upd->tim, upd->tck->tcks.val, upd->vol); 
	}
}

/*
 * Log all updates in @hst's update queue at the tick
 * at value @tck_val.
 */
static void _log_hst_upds_at(
	tb_lv1_hst *hst,
	u64 tck_val
)
{
	debug("Updates of hst %p at %U.\n", hst, tck_val);
	tb_lv1_tck *tck = 0;
	tb_lv1_upd *upd;
	ns_slsh_fe(upd, &hst->upds_hst, upds_hst) {
		if (upd->tck->tcks.val == tck_val) {
			if (!tck) tck = upd->tck;
			assert(upd->tck == tck, "multiple ticks at the same value.\n");
			debug("  upd %p : tim : %U, tck : %U, vol : %d.\n", upd, upd->tim, upd->tck->tcks.val, upd->vol); 
		}
	}
}

/*
 * Log @hst's tick at @tck_val.
 */
static void _log_hst_tck_lst(
	tb_lv1_hst *hst,
	u64 tck_val
)
{
	debug("Tick of hst %p at %U : ", hst, tck_val);
	tb_lv1_tck *tck = ns_map_sch(&hst->tcks, tck_val, u64, tb_lv1_tck, tcks);
	if (!tck) {
		debug("not found.\n");
		return;
	}
	debug("vol_stt : %d, vol_cur : %d, vol_max %d, tim_max %U.\n",
		tck->vol_stt, tck->vol_cur, tck->vol_max, tck->tim_max);
	tb_lv1_upd *upd;
	ns_dls_fe(upd, &tck->upds_tck, upds_tck) {
		debug("  upd %p : tim : %U, tck : %U, vol : %d.\n", upd, upd->tim, upd->tck->tcks.val, upd->vol); 
	}

}

/*
 * Log all updates in @ctx's update list in the
 * [@uid_stt, @uid_end[ range.
 */
static void _upds_log(
	tb_tst_lv1_ctx *ctx,
	u64 uid_stt,
	u64 uid_end
)
{
	debug("Tick of ctx %p in [%U, %U[ : ", ctx, uid_stt, uid_end);
	while (uid_stt < uid_end) {
		debug("  upd %U : tim : %U, tck : %U, vol : %d.\n", uid_stt, ctx->upds[uid_stt].tim, ctx->upds[uid_stt].tck, ctx->upds[uid_stt].vol); 
		uid_stt++;
	}
}

/*
 * Log all updates in @ctx's update list in the
 * [@uid_stt, @uid_end[ range at @tck_val.
 */
static void _upds_log_at(
	tb_tst_lv1_ctx *ctx,
	u64 uid_stt,
	u64 uid_end,
	u64 tck_val
)
{
	debug("Tick of ctx %p in [%U, %U[ : ", ctx, uid_stt, uid_end);
	while (uid_stt < uid_end) {
		const u64 tck = ctx->upds[uid_stt].tck;
		debug("  upd %U : tim : %U, tck : %U, vol : %d.\n", uid_stt, ctx->upds[uid_stt].tim, tck, ctx->upds[uid_stt].vol); 
		uid_stt++;
	}
}

