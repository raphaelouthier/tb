/* Copyright 2025 Raphael Outhier - confidential - proprietary - no copy - no diffusion. */

#include <tb_tst/tb_tst.all.h>

#include <tb_tst/lv1_utl.h>

/****************************
 * Bid-ask curve management *
 ****************************/

/*
 * Shortcut to incorporate into best bid / ask.
 */
static inline void _bac_add(
	tb_tst_lv1_ctx *ctx,
	u64 tck,
	f64 vol,
	u64 tim,
	u64 *bst_bac_aidp,
	u64 *bst_bidp,
	u64 *bst_askp
)
{
	assert(tck < ctx->tck_nbr);
	const u64 aid_bac = (tim - ctx->tim_stt) / ctx->aid_wid;
	u64 bst_bac_aid = *bst_bac_aidp;
	//debug("bac add tim %U aid_bac %U bst_bac_aid %U.\n", tim, aid_bac, bst_bac_aid);
	//debug("cur bid %U ask %U.\n", *bst_bidp, *bst_askp);
	assert(bst_bac_aid <= aid_bac);
	if (aid_bac != bst_bac_aid) {
		*bst_bac_aidp = bst_bac_aid = aid_bac;
		*bst_bidp = ctx->bid_ini[bst_bac_aid];
		*bst_askp = ctx->ask_ini[bst_bac_aid];
		debug("ini bid %U ask %U.\n", *bst_bidp, *bst_askp);
	}
	const u64 tck_abs = tck + ctx->tck_min;
	if ((vol < 0) && (tck_abs > *bst_bidp)) {
		*bst_bidp = tck_abs;
	} else if ((vol > 0) && (tck_abs < *bst_askp)) {
		*bst_askp = tck_abs;
	}
	//debug("new bid %U ask %U.\n", *bst_bidp, *bst_askp);
	check(*bst_askp >= ctx->tck_min);
}

/*
 * Update best bid / ask state to reflect the add time.
 */
static inline void _bac_prp(
	tb_tst_lv1_ctx *ctx,
	u64 tim,
	u64 *bst_bac_aidp,
	u64 *bst_bidp,
	u64 *bst_askp
)
{
	const u64 aid_bac = (tim - 1 - ctx->tim_stt) / ctx->aid_wid;
	u64 bst_bac_aid = *bst_bac_aidp;
	//debug("bac add tim %U aid_bac %U bst_bac_aid %U.\n", tim, aid_bac, bst_bac_aid);
	//debug("cur bid %U ask %U.\n", *bst_bidp, *bst_askp);
	assert(bst_bac_aid <= aid_bac);
	if (aid_bac != bst_bac_aid) {
		debug("ini %U %U %U.\n", ctx->unt_nbr, bst_bac_aid, aid_bac);
		*bst_bac_aidp = bst_bac_aid = aid_bac;
		*bst_bidp = ctx->bid_ini[bst_bac_aid];
		*bst_askp = ctx->ask_ini[bst_bac_aid];
		debug("ini bid %U ask %U.\n", *bst_bidp, *bst_askp);
	}
}

/***********
 * Updates *
 ***********/

/*
 * Add all updates until @tim_add.
 */
static inline void _upds_add_ini(
	tb_tst_lv1_ctx *ctx,
	tb_lv1_hst *hst,
	u64 *tims,
	f64 *prcs,
	f64 *vols,
	u64 tim_add,
	u64 *uid_addp,
	u64 *bst_bac_aidp,
	u64 *bst_bidp,
	u64 *bst_askp
)
{
	check(*bst_askp >= ctx->tck_min);
	debug("Adding initial orders.\n");
	tb_tst_lv1_upd *upds = ctx->upds;
	u64 uid_add = *uid_addp;
	u64 i = 0;
	u64 tim;
	while (
		(uid_add < ctx->upd_nbr) &&
		((tim = upds[uid_add].tim) < tim_add)
	) {

		/* Add. */
		tims[i] = tim;
		const f64 prc = prcs[i] = upds[uid_add].prc;
		const f64 vol = vols[i] = upds[uid_add].vol;
		const u64 tck = _prc_to_tck(ctx, prc); 

		/* Incorporate in best bid / ask. */
		//debug("add ini %U %U %d.\n", tim, tck, vol);
		_bac_add(ctx, tck, vol, tim, bst_bac_aidp, bst_bidp, bst_askp);

		//debug("%U %U.\n", *bst_bidp, *bst_askp);

		/* Update counters. */
		uid_add++;
		i++;

		/* If buffer full, flush. */
		if (i == 2 * ctx->tck_nbr) {
			tb_lv1_add(hst, i, tims, prcs, vols);
			i = 0;
		}

	}

	/* Add all non-flushed updates. */
	if (i) {
		tb_lv1_add(hst, i, tims, prcs, vols);
	}

	/* Update bid/ask data to reflect the add time. */
	_bac_prp(ctx, tim_add, bst_bac_aidp, bst_bidp, bst_askp);


	*uid_addp = uid_add;

}


#define UPD_GEN_STPS_NB 6
static u64 tim_add_inc_stps[UPD_GEN_STPS_NB] = {
	1, 1, 1, 2, 2, 3,
	//1, 1, 1, 1, 1, 1,
};

/*
 * Update @tim_add.
 */
static inline u64 _tim_add_inc(
	tb_tst_lv1_ctx *ctx,
	const u64 tim_add,
	u64 itr_idx
)
{
	const u64 add_nb = tim_add_inc_stps[itr_idx % UPD_GEN_STPS_NB];
	return tim_add + add_nb * ctx->tim_inc;
}

/*
 * Add updates at @tim_add.
 */
static inline void _upds_add(
	tb_tst_lv1_ctx *ctx,
	tb_lv1_hst *hst,
	u64 *tims,
	f64 *prcs,
	f64 *vols,
	u64 tim_add,
	u64 *uid_addp,
	u64 *bst_bac_aidp,
	u64 *bst_bidp,
	u64 *bst_askp
)
{
	u64 uid_add = *uid_addp; 

	debug("Generating updates to add.\n");

	const u64 gen_max = 2 * ctx->tck_nbr;
	u64 gen_nbr = 0;
	while (uid_add < ctx->upd_nbr) {
		const u64 tim = ctx->upds[uid_add].tim; 
		//debug("%U %U.\n", tim_add, tim);
		if (tim_add <= tim) break;

		/* Add to the arrays. */
		tims[gen_nbr] = tim;
		const f64 prc = prcs[gen_nbr] = ctx->upds[uid_add].prc;
		const f64 vol = vols[gen_nbr] = ctx->upds[uid_add].vol;
		const u64 tck = _prc_to_tck(ctx, prc); 
		assert(_tck_to_prc(ctx, tck) == prc);

		/* Add to the history if needed. */ 
		assert(gen_nbr <= gen_max);
		if (gen_nbr == gen_max) {
			tb_lv1_add(hst, gen_nbr, tims, prcs, vols);
		}

		/* Incorporate in best bid / ask. */
		//debug("add %U %U %d.\n", tim, tck, vol);
		_bac_add(ctx, tck, vol, tim, bst_bac_aidp, bst_bidp, bst_askp);

		uid_add++;
		gen_nbr++;
	}

	/* Add remaining updates. */
	assert(gen_nbr <= gen_max);
	if (gen_nbr) {
		tb_lv1_add(hst, gen_nbr, tims, prcs, vols);
	}

	/* Update bid/ask data to reflect the add time. */
	_bac_prp(ctx, tim_add, bst_bac_aidp, bst_bidp, bst_askp);

	/* Report the new add UID. */
	*uid_addp = uid_add;

}

/*
 * Incorporate all updates until (<=) @tim_cur starting
 * at *@uid_curp.
 * Update @uid_curp.
 */
static inline void _upds_mov(
	tb_tst_lv1_ctx *ctx,
	tb_lv1_hst *hst,
	u64 *uid_curp,
	u64 tim_cur,
	u64 *chc_tims,
	f64 *chc_sums,
	f64 *chc_vols,
	u64 *chc_aidp
)
{
	u64 uid_cur = *uid_curp;

	while (1) {

		/* Check. */
		const u64 tim = ctx->upds[uid_cur].tim; 
		if (tim >= tim_cur) break;

		/* Read. */
		const f64 prc = ctx->upds[uid_cur].prc; 
		const f64 vol = ctx->upds[uid_cur].vol; 
		const u64 tck = _prc_to_tck(ctx, prc); 

		/* Incorporate. */
		const u64 aid = _tim_to_aid(ctx, tim);
		assert(aid >= *chc_aidp);
		if (aid != *chc_aidp) {
			*chc_aidp = aid;
			const u64 aid_tim = aid * ctx->aid_wid + ctx->tim_stt;
			for (u32 rst_idx = 0; rst_idx < ctx->tck_nbr; rst_idx++) {
				chc_tims[rst_idx] = aid_tim;
				chc_sums[rst_idx] = 0;
			}
		}
		assert(tim >= chc_tims[tck]);
		if (tim != chc_tims[tck]) {
			chc_sums[tck] += (f64) (tim - chc_tims[tck]) * vol;
		}
		chc_vols[tck] = vol;
		chc_tims[tck] = tim;

		/* Increment. */
		uid_cur++;
	}

	*uid_curp = uid_cur;
}

/*
 * Update the clean index.
 */
static inline u64 _uid_cln_upd(
	tb_tst_lv1_ctx *ctx,
	u64 uid_cln,
	u64 tim_cln
)
{
	while (uid_cln < ctx->upd_nbr) {
		const u64 tim = ctx->upds[uid_cln].tim; 
		assert(tim_cln <= tim);
		if (tim_cln < tim) break;
		uid_cln++;
	}
	return uid_cln;
}

/*
 * Entrypoint for lv1 tests.
 */
static void _lv1_run(
	nh_tst_sys *sys,
	u64 sed,
	tb_tst_lv1_gen *gen,
	u8 chk_stt,
	u64 tim_stt,
	u64 tim_inc,
	u64 tck_per_unt,
	u64 unt_nbr,
	u64 tck_nbr,
	f64 prc_min,
	u64 hmp_dim_tck,
	u64 hmp_dim_tim,
	u64 bac_siz,
	u64 tim_stp,
	f64 ref_vol
)
{

	const u64 aid_wid = tim_inc * tim_stp;
	const u64 tck_min = (u64) (prc_min * (f64) tck_per_unt);

	/* Generation settings. */ 
	nh_all__(tb_tst_lv1_ctx, ctx);
	ctx->sed = sed;
	ctx->unt_nbr = unt_nbr;
	ctx->tim_stt = tim_stt;
	assert(!(ctx->tim_stt % 100));
	ctx->prc_min = prc_min;
	ctx->tck_nbr = tck_nbr;
	ctx->tim_inc = tim_inc;
	ctx->tim_stp = tim_stp;
	ctx->aid_wid = aid_wid;
	ctx->tck_rat = tck_per_unt;
	ctx->hmp_dim_tck = hmp_dim_tck;
	ctx->hmp_dim_tim = hmp_dim_tim;
	ctx->bac_siz = bac_siz;
	ctx->ref_vol = ref_vol;

	/* Generate tick data. */
	debug("Generating updates.\n");
	tb_tst_lv1_upds_gen(ctx, gen);
	assert(tck_min == ctx->tck_min);

	/* Delete the generator. */
	(*(gen->dtr))(gen);

	/* Read post generation parameters. */
	const u64 tim_end = ctx->tim_end;

	/* Allocate buffers to contain two entire orderbooks,
	 * which prevents us from overflowing when adding
	 * updates as the current time (not more than tck_nbr). */
	u64 *tims = nh_all(2 * tck_nbr * sizeof(u64));
	f64 *prcs = nh_all(2 * tck_nbr * sizeof(f64));
	f64 *vols = nh_all(2 * tck_nbr * sizeof(f64));

	/* Export the global heatmap. */
	//_hmp_exp(ctx);

	/*
	 * Create a reconstructor covering a 
	 * 100 * 100 heatmap window
	 * and a 200 ticks bid-ask prediction.
	 */
	tb_lv1_hst *hst = tb_lv1_ctr(
		aid_wid,
		tck_per_unt,
		hmp_dim_tck,
		hmp_dim_tim,
		bac_siz
	);

	/* Set initial volumes in groups of 19 by step of 19. */
	debug("Adding initial volumes.\n");
	assert(tck_nbr % 19);
	assert(19 % tck_nbr);
	u64 ttl_non_nul = 0;
	for (u32 i = 0; i < tck_nbr;) {
		u64 nb_non_nul = 0;
		for (u32 j = 0; (j < 19) && (i < tck_nbr); i++, j++) {
			u32 ri = (u32) ((19 * i) % tck_nbr);
			const f64 vol = ctx->hmp_ini[ri];
			if (!vol) continue;
			prcs[nb_non_nul] = _tck_to_prc(ctx, ri);
			assert(prcs[nb_non_nul]);
			vols[nb_non_nul] = ctx->hmp_ini[ri];
			nb_non_nul++;
		}
		if (nb_non_nul) tb_lv1_add(hst, nb_non_nul, 0, prcs, vols);
		ttl_non_nul += nb_non_nul;
	}
	assert(ttl_non_nul, "init with no volume.\n");

	/* Determine the initial times. */

	/* The time below (<) which orders belong to the
	 * heatmap. We prepare until tim_cur. */
	u64 tim_cur = tim_stt;

	/* The time below (<) which orders belong to the
	 * bid ask curve. */
	u64 tim_add = tim_stt + bac_siz * aid_wid;

	/* The time below which (<) we remove updates
	 * from the history. */
	assert(tim_stt > hmp_dim_tim * aid_wid);
	u64 tim_cln = tim_stt - hmp_dim_tim * aid_wid;

	/* Index of next update to be moved to the heatmap. */
	u64 uid_cur = 0;

	/* Index of next update to be inserted in the history. */
	u64 uid_add = 0;

	/* Index of next update to be cleaned. */
	u64 uid_cln = 0;
	
	/*
	 * The way we generate orderbook updates is fundamentally
	 * prone to causing incoherence in the bid-ask spread.
	 * Since we generate a new orderbook, then traverse ticks
	 * in a randomized way to create updates, then it is possible
	 * that if we only apply half of those updates, we end up
	 * with alternating bid and asks.
	 * We do this anyway because it adds randomness, and because
	 * since all those updates have the same time, they must
	 * be added altogether before processing, which will
	 * allow the processing stage to see all of them, and hence
	 * to see a coherent bid/ask spread.
	 * One nuance is that best bids and best asks are done in
	 * a step by step manner, which causes the intermediate
	 * states to be incoherent.
	 * For this reason, TODO mute debug messages about
	 * alternating bid and ask levels during testing.
	 */

	/*
	 * The heatmap of @ctx is only relevant for columns
	 * with all updates accounted for, which in practice
	 * comprises all columns except the current one.
	 * To verify @hst's result for the current column,
	 * we need arrays giving the current volume for each
	 * tick, the weighted sum at last update, and the
	 * time of last update.
	 */
	u64 *chc_tims = nh_all(tck_nbr * sizeof(u64));
	f64 *chc_sums = nh_all(tck_nbr * sizeof(f64));
	f64 *chc_vols = nh_all(tck_nbr * sizeof(f64));
	u64 chc_aid = 0;
	for (u32 rst_idx = 0; rst_idx < ctx->tck_nbr; rst_idx++) {
		chc_tims[rst_idx] = tim_stt;
		chc_sums[rst_idx] = 0;
		chc_vols[rst_idx] = ctx->hmp_ini[rst_idx];
	}

	/* Current best bid / asks. */
	u64 bst_bac_aid = 0;
	u64 bst_bid = ctx->bid_ini[0];
	u64 bst_ask = ctx->ask_ini[0];

	debug("Initial prepare + process.\n");

	/* Prepare for insertion until bac end. */
	tb_lv1_prp(hst, tim_cur);

	/* Push all updates until @tim_add. */
	_upds_add_ini(ctx, hst, tims, prcs, vols, tim_add, &uid_add, &bst_bac_aid, &bst_bid, &bst_ask);

	/* Process. */
	tb_lv1_prc(hst);

	/* Verify the history's internal state. */
	if (chk_stt) {
		tb_tst_lv1_vrf_hst_stt(
			ctx, hst,
			uid_cln, uid_cur, uid_add,
			tim_cln, tim_cur, tim_add,
			0
		);
	}

	/* Verify the result. */
	tb_tst_lv1_vrf_hst_res(
		ctx, hst,
		tim_cur,
		bst_bid, bst_ask,
		chc_tims, chc_sums, chc_vols,
		0
	);

	/* Generate and compare the heatmap and bac. */
	u64 itr_idx = 0;
	debug("Main loop.\n");
	while (tim_add < tim_end) {

		debug("Iteration %U\n", itr_idx);
		
		/* Update tim_add. */
		const u64 tim_add_new = _tim_add_inc(ctx, tim_add, itr_idx);
		assert(tim_add_new > tim_add);
		assert(!(tim_add_new % ctx->tim_inc));
		tim_add = tim_add_new;

		/* Update the current time and clean time. */
		assert(tim_cur < tim_add - bac_siz * aid_wid); 
		tim_cur = tim_add - bac_siz * aid_wid;
		
		/* Prepare to add. */
		debug("Preparing to %U.\n", tim_cur);
		tb_lv1_prp(hst, tim_cur);

		/* Determine the new heatmap start time,
		 * and the new bac end time. */
		const u64 hmp_end = aid_wid * ((tim_cur + aid_wid - 1) / aid_wid);
		assert(hmp_end >= hmp_dim_tim * aid_wid);
		tim_cln = hmp_end - hmp_dim_tim * aid_wid;
		assert(hst->tim_cur == tim_cur);
		assert(hst->tim_hmp == hmp_end);
		assert(hst->tim_end == tim_add);

		/* Add updates at the current tick. */
		_upds_add(
			ctx, hst,
			tims, prcs, vols,
			tim_add, &uid_add,
			&bst_bac_aid, &bst_bid, &bst_ask
		);
		
		/* Process. */
		tb_lv1_prc(hst);

		/* Incorporate all orders <= current time in the
		 * heatmap data. */
		_upds_mov(ctx, hst, &uid_cur, tim_cur, chc_tims, chc_sums, chc_vols, &chc_aid);

		/* Cleanup one time out of 10. */
		u8 do_cln = (!itr_idx % 20);
		if (do_cln) {

			/* Verify the history internal state. */
			if (chk_stt) {
				tb_tst_lv1_vrf_hst_stt(
					ctx, hst,
					uid_cln, uid_cur, uid_add,
					tim_cln, tim_cur, tim_add,
					0
				);
			}

			/* Determine the clean index. */
			uid_cln = _uid_cln_upd(ctx, uid_cln, tim_cln);

			/* Clean. */
			tb_lv1_cln(hst);

		}

		/* Verify the history internal state. */
		if (chk_stt) {
			tb_tst_lv1_vrf_hst_stt(
				ctx, hst,
				uid_cln, uid_cur, uid_add,
				tim_cln, tim_cur, tim_add,
				do_cln
			);
		}

		/* Verify the result. */
		tb_tst_lv1_vrf_hst_res(
			ctx, hst,
			tim_cur,
			bst_bid, bst_ask,
			chc_tims, chc_sums, chc_vols,
			0
		);

		itr_idx++;
	}	


	/* Delete the history. */
	tb_lv1_dtr(hst);

	/* Free buffers. */ 
	nh_fre(tims, 2 * tck_nbr * sizeof(u64));
	nh_fre(prcs, 2 * tck_nbr * sizeof(f64));
	nh_fre(vols, 2 * tck_nbr * sizeof(f64));
	nh_fre(chc_tims, tck_nbr * sizeof(u64));
	nh_fre(chc_sums, tck_nbr * sizeof(f64));
	nh_fre(chc_vols, tck_nbr * sizeof(f64));

	/* Delete tick data. */
	tb_tst_lv1_upds_del(ctx);

	/* Free settings. */ 
	nh_fre_(ctx);

}

//#define CHECK_STATE 1
#define CHECK_STATE 0

/*
 * Verification test.
 */
static inline void _vrf_tst(
	nh_tst_sys *sys,
	u64 sed,
	u64 prd,
	u64 phs,
	u8 bid,
	u8 ask
)
{
	assert(bid || ask);
	tb_tst_lv1_gen_spl *spl = tb_tst_lv1_gen_spl_ctr(
		bid, ask,
		1000000, 1000001, 1000002, 1000003,
		prd, phs
	);

	_lv1_run(
		sys, sed, &spl->gen, CHECK_STATE,
		NS_TIM_S(1000), // Start at 10s.
		NS_TIM_1MS, // Order every ms.
		100, // 100 tick per price unit.
		150, // 150 time units.
		7, // 7 tick.
		10000, // prices start at 10000. 
		10, // heatmap has 10 ticks.
		10, // heatmap has 10 units.
		10, // bac has 10 units.
		10, // 10 increments per time unit. 
		1.
	);
}

/*
 * Random test.
 */
static inline void _rdm_tst(
	nh_tst_sys *sys,
	u64 sed
)
{
	tb_tst_lv1_gen_rdm *rdm = tb_tst_lv1_gen_rdm_ctr(
		43,
		100,
		47,
		8,
		9,
		23,
		27
	);

	_lv1_run(
		sys, sed, &rdm->gen, CHECK_STATE,
		NS_TIM_S(1000), // Start at 10s.
		NS_TIM_1MS, // Order every ms.
		100, // 100 tick per price unit.
		1000, // 1000 time units.
		37, // 37 tick.
		10000, // prices start at 10000. 
		100, // heatmap has 100 ticks.
		100, // heatmap has 100 units.
		200, // bac has 200 units.
		10, // 10 increments per time unit. 
		10000
	);
}

/*
 * Entrypoint for lv1 tests.
 */
void tb_tst_lv1(
	nh_tst_sys *sys,
	u64 sed,
	u8 wrk_nb,
	u8 run_prc
)
{
	_vrf_tst(sys, sed, 10, 0, 1, 0);
	assert(0);
	_vrf_tst(sys, sed, 10, 0, 0, 1);
	_vrf_tst(sys, sed, 10, 0, 1, 1);
	_vrf_tst(sys, sed, 10, 5, 1, 0);
	_vrf_tst(sys, sed, 10, 5, 0, 1);
	_vrf_tst(sys, sed, 10, 5, 1, 1);
	_vrf_tst(sys, sed, 1, 0, 1, 0);
	_vrf_tst(sys, sed, 1, 0, 0, 1);
	_vrf_tst(sys, sed, 1, 0, 1, 1);
	_vrf_tst(sys, sed, 30, 0, 1, 0);
	_vrf_tst(sys, sed, 30, 0, 0, 1);
	_vrf_tst(sys, sed, 30, 0, 1, 1);
	_vrf_tst(sys, sed, 50, 0, 1, 1);
	assert(0);
	_rdm_tst(sys, sed);
}
