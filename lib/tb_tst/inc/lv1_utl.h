/* Copyright 2025 Raphael Outhier - confidential - proprietary - no copy - no diffusion. */

/*********
 * Utils *
 *********/

/*
 * Time -> AID.
 */
static inline u64 _tim_to_aid(
	tb_tst_lv1_ctx *ctx,
	u64 tim
)
{
	return (tim - ctx->tim_stt) / ctx->aid_wid;
}

/*
 * Price -> tick.
 */
static inline u64 _prc_to_tck(
	tb_tst_lv1_ctx *ctx,
	f64 prc
)
{
	assert(prc >= ctx->prc_min);
	return (u64) (f64) ((prc - ctx->prc_min) * ((f64) ctx->tck_rat + 0.1));
}

/*
 * Index -> tick.
 */
static inline u64 _idx_to_tck(
	tb_tst_lv1_ctx *ctx,
	u64 idx
)
{
	assert(idx < ctx->tck_nbr);
	return idx + ctx->tck_min;
}

/*
 * Tick -> price.
 */
static inline f64 _tck_to_prc(
	tb_tst_lv1_ctx *ctx,
	u64 tck
) {return ctx->prc_min + ((f64) tck / (f64) ctx->tck_rat);}
