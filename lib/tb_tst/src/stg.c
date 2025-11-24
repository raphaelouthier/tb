/* Copyright 2025 Raphael Outhier - confidential - proprietary - no copy - no diffusion. */

#include <tb_tst/tb_tst.all.h>

static inline tb_tst_stg_dsc *_stg_dsc_gen(
	u64 sed,
	void *dat,
	u64 *tims,
	const char *stg_pth,
	u8 wrk_nb,
	const char *mkp,
	const char *ist,
	u8 wrt
)
{
	/* Allocate the descriptor. */
	nh_all__(tb_tst_stg_dsc, dsc);
	dsc->sed = sed;
	dsc->imp_siz = ns_hsh_u32_rng(sed, 128, 1025, 1);
	dsc->dat = dat;
	dsc->tims = tims;
	dsc->pth = stg_pth;
	dsc->wrk_nb = wrk_nb;
	dsc->syn = 0;
	dsc->mkp = mkp;
	dsc->ist = ist;
	dsc->lvl = 0;
	dsc->wrt = wrt;
	dsc->key = 0;
	assert(dsc->imp_siz <= 1024);
	assert(1 <= dsc->imp_siz);

	/* Complete. */
	return dsc;
}

/*
 * If @stg is set, close it.
 * Then reload it and update @stg.
 */

static inline void _stg_lod(
	tb_stg_sys **sysp,
	tb_stg_idx **idxp,
	tb_tst_stg_dsc *dsc
)
{

	assert((!*idxp) == (!*sysp));

	if (*idxp) {
		tb_stg_cls(*idxp, dsc->key);
	} 
	if (*sysp) {
		tb_stg_dtr(*sysp);
	}

	tb_stg_ini(dsc->pth);

	/* Open the system. */
	*sysp = tb_stg_ctr(dsc->pth, 1);
	dsc->syn = tb_stg_tst_syn(*sysp);
	assert(dsc->syn);
	dsc->syn->gat.wrk_nb = dsc->wrk_nb;

	/* Open the index. */
	*idxp = tb_stg_opn(*sysp, dsc->mkp, dsc->ist, dsc->lvl, dsc->wrt, &dsc->key);
	assert(*idxp);
	assert((!dsc->wrt) == (!dsc->key));

}

/*
 * Get source data arrays for index @idx.
 */
static inline void _get_dat(
	const void **dst,
	u64 *tims,
	void *dat,
	u8 arr_nb,
	u64 idx,
	const u8 *elm_sizs
)
{
	assert(arr_nb);
	assert(elm_sizs[0] == 8);
	dst[0] = tims + idx;
	
	for (u8 i = 1; i < arr_nb; i++) {
		dst[i] = ns_psum(dat, idx * (u64) elm_sizs[i]);
	}
}

/*
 * Per-thread level-specific entrypoint.
 */
static void _stg_exc_lvl(
	tb_tst_stg_dsc *dsc,
	u8 lvl
)
{

	assert(lvl < 3);
	dsc->lvl = lvl;

	/* Gate debug. */
	u64 gat_ctr = 0;
	u64 itr_ctr = 0;

	/* Load. */
	tb_stg_sys *sys = 0;
	tb_stg_idx *idx = 0;
	_stg_lod(&sys, &idx, dsc);
	assert(sys);
	assert(idx);
	
	/* Reset the gate counter. */
	dsc->syn->gat.ctr = 0;

	assert(lvl < 3);
	const u8 lvl_to_arr_nb[3] = {
		5,
		3,
		6
	};
	const u8 *(lvl_to_elm_sizs[3]) = {
		(u8 []) {8, 8, 8, 8, 8},
		(u8 []) {8, 8, 8},
		(u8 []) {8, 8, 8, 1, 8, 8},
	};
	const void *srcs[6];
	const void *dsts[6];
	const u8 arr_nb = lvl_to_arr_nb[lvl];
	const u8 *_elm_sizs = lvl_to_elm_sizs[lvl];
	assert(arr_nb <= 6);

	/* Write the whole index table in 100 steps */
	const u64 blk_siz = 3;
	const u64 itb_siz = 2000;
	const u64 elm_nb = blk_siz * itb_siz;
	const u64 stp_nb = 3 * 50;
	const u64 stp_elm_nb = elm_nb / stp_nb;

	/* Cache the time array. */
	u64 *tims = dsc->tims;

	GAT_PAS(dsc);

	/* Ensure that we're not always filling blocks
	 * and that we fill the whole block area. */
	assert(!(elm_nb % stp_nb));
	assert(stp_elm_nb % blk_siz);

	/* Chose a pseudo-random start time. */
	u64 tim_stt = tims[0];
	assert(tim_stt);
	u64 tim_end = tim_stt;

	/* Execute the required number of steps. */ 
	u64 wrt_id = 0;
	for (u64 stp_id = 0; stp_id < stp_nb; stp_id++) {
		debug("step %U.\n", stp_id);

		GAT_PAS(dsc);

		/* Writer fills data, others just adjust the time
		 * range. */
		_get_dat(srcs, tims, dsc->dat, arr_nb, wrt_id, _elm_sizs);
		tim_end = ((uint64_t *) srcs[0])[stp_elm_nb - 1];
		if (dsc->wrt) {
			tb_stg_wrt(idx, stp_elm_nb, srcs, arr_nb);
		}
		wrt_id += stp_elm_nb;

		GAT_PAS(dsc);

		/* Everyone unloads every 7 steps. */ 
		const u8 unl = !(stp_id % 7);
		if (unl) {
			_stg_lod(&sys, &idx, dsc);
			assert(sys);
			assert(idx);
		}
		GAT_PAS(dsc);

		/* Everyone verifies the index table. */
		const u64 itb_nb = tb_sgm_elm_nb(idx->sgm);
		/* previous line is syncing so all updates to itb should now be observed. */
		const u64 itb_max = tb_sgm_elm_max(idx->sgm);
		assert(itb_nb);
		assert(itb_max == itb_siz); 
		assert(itb_nb <= itb_max);
		assert(itb_nb == (wrt_id + 2) / 3);  
		volatile u64 (*itb)[2] = idx->tbl;
		assert(itb[0][0] == tim_stt);
		assert(itb[itb_nb - 1][1] == tim_end);
		for (u64 blk_id = 0; blk_id < itb_nb; blk_id++) {
			assert(itb[blk_id][0] <= itb[blk_id][1]);
			assert((!blk_id) || (itb[blk_id - 1][1] <= itb[blk_id][0]));
			assert(itb[blk_id][0] == tims[3 * blk_id]);
			if (blk_id + 1 == itb_nb) {
				assert(
					(itb[blk_id][1] == tims[3 * blk_id]) ||
					(itb[blk_id][1] == tims[3 * blk_id + 1]) ||
					(itb[blk_id][1] == tims[3 * blk_id + 2])
				);
			} else {
				assert(itb[blk_id][0] < itb[blk_id][1]);
				assert(itb[blk_id][1] == tims[3 * blk_id + 2]);
			}
		}

		/* Generate the expected values. */
		_get_dat(srcs, tims, dsc->dat, arr_nb, 0, _elm_sizs);

		/* Everyone checks that a time query on the
		 * index table returns the correct result. */
		u64 tim0 = ns_hsh_u32_rng(dsc->sed, 1, (u32) -1, 1);
		u64 nbr;
		assert(tb_stg_sch(idx, tim0 - 1, &nbr));
		for (u64 tst_id = 0; tst_id < wrt_id; tst_id++) {
			const u64 tim = (tim0 + (tst_id >> 1) * 20);
			assert(tims[tst_id] == tim);
			assert(!tb_stg_sch(idx, tim, &nbr));
			assert(nbr == ((tst_id & ~(u64) 1) / 3)); 
		}
		assert(tb_stg_sch(idx, (tim0 + (wrt_id >> 1) * 20) + 1, &nbr));

		/* Everyone checks that they can access each block,
		 * and that the block's data is the expected one. */

		assert(!tb_stg_lod_tim(idx, tim_stt - 1));
		assert(!tb_stg_lod_tim(idx, tim_end + 1));

		/* Check that start and end blocks can be accessed. */
		tb_stg_blk *blk = 0;
	    blk = tb_stg_lod_tim(idx, tim_stt);
		assert(blk->uctr == 1);
		assert(blk);
		tb_stg_unl(blk);
		assert(blk->uctr == 0);
		blk = tb_stg_lod_tim(idx, tim_end);
		assert(blk);
		assert(blk->uctr == 1);
		tb_stg_unl(blk);
		assert(blk->uctr == 0);

		/* All loaded blocks must be unused. */
		ns_map_fe(blk, &idx->blks, blks, u64, in) {
			assert(!blk->uctr);
		}


		/* Iterate over all blocks. */
		u64 red_id = 0;
		tb_stg_red(idx, tim_stt, tim_end, dsts, arr_nb) {
			assert(__blk->uctr == 1);
			for (u8 arr_id = 0; arr_id < arr_nb; arr_id++) {
				const u8 elm_siz = _elm_sizs[arr_id];
				if (elm_siz == 1) {
					assert(((u8 *) dsts[arr_id])[blk_id] == ((u8 *) srcs[arr_id])[red_id]);
				} else {
					assert(elm_siz == 8);
					assert(((u64 *) dsts[arr_id])[blk_id] == ((u64 *) srcs[arr_id])[red_id]);
				}
			}
			red_id++;
		}	
		assert(red_id == wrt_id); 

		/* All blocks must be loaded and unused. */
		u64 blk_nbr = 0;
		ns_map_fe(blk, &idx->blks, blks, u64, in) {
			assert(!blk->uctr);
			assert(blk->blks.val == blk_nbr);
			blk_nbr++;
		}
		assert(blk_nbr == itb_nb);

	}

	/* Unload. */
	tb_stg_cls(idx, dsc->key);
	tb_stg_dtr(sys);

}

/*
 * Per-thread entrypoint.
 */
static u32 _stg_exc(
	tb_tst_stg_dsc *dsc
)
{
	_stg_exc_lvl(dsc, 0);
	_stg_exc_lvl(dsc, 1);
	_stg_exc_lvl(dsc, 2);
	nh_fre_(dsc);
	return 0;
}

/*
 * Storage testing.
 */
void tb_tst_stg(
	nh_tst_sys *sys,
	u64 sed,
	u8 wrk_nb,
	u8 prc
)
{


	/* Clean if needed. */
	system("rm -rf "STG_PTH);

	/* 2000 blocks * 3 u64 elements */
	const uad dat_siz = uad_alu((uad) 2000 * 3 * sizeof(u64), 6);
	void *dat = _dat_all(dat_siz, sed);

	/* Allocate times. */
	u64 *tims = _tim_all(2000 * 3, sed);

	const char *mkp = "MKP";
	const char *ist = "IST";

	/* Parallel testing. */
	TST_PRL(prc, _stg_exc, _stg_dsc_gen(sed, dat, tims, STG_PTH, wrk_nb, mkp, ist, tst_prl_mst));	

	/* Clean if needed. */
	system("rm -rf "STG_PTH);

	/* Free the data block. */
	nh_fre(dat, dat_siz);
	nh_xfre(tims);

}
