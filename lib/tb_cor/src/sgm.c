/* Copyright 2025 Raphael Outhier - confidential - proprietary - no copy - no diffusion. */

#include <tb_cor/tb_cor.all.h>

/*******
 * API *
 *******/

/*
 * Determine if the storage should be intialized by the
 * caller.
 */
static inline u8 _syn_ini(
	tb_sgm_syn *syn
)
{

	/* Read the @ini_res, do the initialization if it is 0,
	 * report init reserved in all cases. */ 
	const u8 do_ini = !ns_atm(a64, xch, aar, &syn->ini_res, 1);

	/* If initialization reserved by someone else,
	 * wait for completion, then ensure visibility. */
	if (!do_ini) {
		while (!ns_atm(a64, red, acq, &syn->ini_cpl));
		return 0;
	}

	/* If we must do the initialization, lock, 
	 * initialize @syn, and report that initialization
	 * must be done. */
	ns_atm(a64, wrt, rel, &syn->wrt, 1);
	syn->elm_nb = 0;
	return 1;
	
}

/*
 * Report initialization of @stg done.
 */
static inline void _sgm_ini_cpl(
	tb_sgm *sgm
)
{

	/* Synchronize metadata. */
	ns_stg_syn(sgm->stg, sgm->mtd, TB_SGM_SIZ_MTD); 

	/* Unlock. */
	const u64 prv = ns_atm(a64, xch, aar, &sgm->syn->wrt, 0);
	assert(prv == 1);

	/* Report init done, unblock others. */
	ns_atm(a64, wrt, rel, &sgm->syn->ini_cpl, 1);

}

/*
 * Return @dsc's region size array.
 */
static inline u64 *_dsc_rgn_sizs(
	tb_sgm_dsc *dsc
) {return ns_psum(dsc, sizeof(tb_sgm_dsc));}

/*
 * Return @dsc's element size array.
 */
static inline u8 *_dsc_elm_sizs(
	tb_sgm_dsc *dsc,
	u8 rgn_nb
) {return ns_psum(dsc, sizeof(tb_sgm_dsc) + rgn_nb * sizeof(u64));}

/*
 * Initialize @dsc.
 */
static inline void _dsc_ini(
	tb_sgm_dsc *dsc,
	u64 elm_max,
	u64 dat_siz,
	u8 rgn_nb,
	const u64 *rgn_sizs,
	u8 arr_nb,
	const u8 *elm_sizs
)
{
	dsc->elm_max = elm_max;
	dsc->dat_siz = dat_siz;
	dsc->rgn_nb = rgn_nb;
	dsc->arr_nb = arr_nb;
	u64 *dsc_rgn_sizs = _dsc_rgn_sizs(dsc);
	for (uint8_t rgn_idx = 0; rgn_idx < rgn_nb; rgn_idx++) {
		dsc_rgn_sizs[rgn_idx] = rgn_sizs[rgn_idx];
	}
	u8 *dsc_elm_sizs = _dsc_elm_sizs(dsc, rgn_nb);
	for (uint8_t arr_idx = 0; arr_idx < arr_nb; arr_idx++) {
		dsc_elm_sizs[arr_idx] = elm_sizs[arr_idx];
	}
}

/*
 * Verify that @dsc matches with what is expected.
 */
static inline void _dsc_chk(
	tb_sgm_dsc *dsc,
	u64 elm_max,
	u64 dat_siz,
	u8 rgn_nb,
	const u64 *rgn_sizs,
	u8 arr_nb,
	const u8 *elm_sizs
)
{
	assert(dsc->elm_max == elm_max, "dsc mismatch : elm_max : expected %U, got %U.\n", elm_max, dsc->elm_max);
	assert(dsc->dat_siz == dat_siz, "dsc mismatch : dat_siz : expected %p, got %p.\n", dat_siz, dsc->dat_siz);
	assert(dsc->rgn_nb == rgn_nb, "dsc mismatch : rgn_nb : expected %u, got %u.\n", rgn_nb, dsc->rgn_nb);
	assert(dsc->arr_nb == arr_nb, "dsc mismatch : arr_nb : expected %u, got %u.\n", arr_nb, dsc->arr_nb);
	const u64 *dsc_rgn_sizs = _dsc_rgn_sizs(dsc);
	for (uint8_t rgn_idx = 0; rgn_idx < rgn_nb; rgn_idx++) {
		assert(dsc_rgn_sizs[rgn_idx] == rgn_sizs[rgn_idx], "dsc mismatch : rgn_sizs[%u] : expected %u, got %u.\n", rgn_idx, dsc_rgn_sizs[rgn_idx], rgn_sizs[rgn_idx]);
	}
	const u8 *dsc_elm_sizs = _dsc_elm_sizs(dsc, rgn_nb);
	for (uint8_t arr_idx = 0; arr_idx < arr_nb; arr_idx++) {
		assert(dsc_elm_sizs[arr_idx] == elm_sizs[arr_idx], "dsc mismatch : elm_sizs[%u] : expected %u, got %u.\n", arr_idx, dsc_elm_sizs[arr_idx], elm_sizs[arr_idx]);
	}
}

/*
 * Initialize @imp.
 */
static inline void _imp_ini(
	tb_sgm *sgm,
	void *imp,
	u64 imp_siz
)
{
	assert(imp_siz <= TB_SGM_SIZ_IMP);
	const u64 zer_siz = TB_SGM_SIZ_IMP - imp_siz;
	ns_mem_cpy(sgm->imp, imp, imp_siz);
	if (zer_siz) ns_mem_rst(ns_psum(sgm->imp, imp_siz), zer_siz); 
}

/*
 * Verify that @inp matches with what is expected.
 */
static inline void _imp_chk(
	tb_sgm *sgm,
	void *imp,
	u64 imp_siz
)
{
	assert(imp_siz <= TB_SGM_SIZ_IMP);
	for (u64 byt_idx = 0; byt_idx < imp_siz; byt_idx++) {
		assert(((volatile u8 *) sgm->imp)[byt_idx] == ((volatile u8 *) imp)[byt_idx], "imp mismatch at byte %u.\n", byt_idx);
	}
	for (u64 byt_idx = imp_siz; byt_idx < TB_SGM_SIZ_IMP; byt_idx++) {
		assert(((volatile u8 *) sgm->imp)[byt_idx] == 0, "imp mismatch at byte %u.\n", byt_idx);
	}
}

/*
 * Load a segment.
 * If it does not exist, create it and
 * initialize it with @imp_ini, @arr_nb, @elm_max and @elm_sizs.
 * Otherwise, check that @imp_ini, @arr_nb, @elm_max and @elm_sizs
 * do match with the loaded content.
 */
tb_sgm *tb_sgm_vopn(
	u8 crt,
	void *imp_ini,
	u64 imp_siz,
	u8 rgn_nb,
	const u64 *rgn_sizs,
	u8 arr_nb,
	u64 elm_max,
	const u8 *elm_sizs,
	const char *pth,
	va_list *args
)
{

	/* Construct. */
	const u8 att = crt ? NH_FIL_ATT_RWC : NH_FIL_ATT_RW; 
	tb_sgm *sgm = nh_all(sizeof(tb_sgm) + arr_nb * sizeof(void *));
	ns_stg *stg = sgm->stg = nh_stg_vopn(att, &sgm->res, pth, args);
	assert(stg, "storage %s open failed.\n", pth);

	/* Compute the size. */
	u64 dat_siz = 0;
	for (u8 rgn_idx = 0; rgn_idx < rgn_nb; rgn_idx++) {
		dat_siz += TB_SGM_SIZ_RGN(rgn_sizs[rgn_idx]);
	}
	for (u8 arr_idx = 0; arr_idx < arr_nb; arr_idx++) {
		dat_siz += TB_SGM_SIZ_ARR(elm_max, elm_sizs[arr_idx]);
	}
	const u64 siz_tgt = TB_SGM_OFF_DAT + dat_siz;

	/* Resize. */
	u64 siz_cur = ns_stg_siz(stg);
	assert(stg, "storage %s open failed.\n", pth);
	assert((siz_cur == 0) || (siz_cur == siz_tgt), "storage %s unexpected size, expected 0 or %U, got %U.\n", pth, siz_tgt, siz_cur);
	ns_stg_rsz(stg, siz_tgt);
	siz_cur = ns_stg_siz(stg);
	assert(siz_cur == siz_tgt, "storage %s resize failed, expected %U, got %U.\n", pth, siz_tgt, siz_cur);

	/* Map the metadata block as shareable RW. */
	const u64 att_rws = NS_STG_ATT_RED | NS_STG_ATT_WRT | NS_STG_ATT_SHR; 
	void *mtd = sgm->mtd = ns_stg_map(stg, 0, TB_SGM_OFF_MTD, TB_SGM_SIZ_MTD, att_rws); 
	assert(mtd);

	/* Get substructs. */
	tb_sgm_syn *syn = sgm->syn = ns_psum(mtd, TB_SGM_OFF_SYN);
	tb_sgm_dsc *dsc = sgm->dsc = ns_psum(mtd, TB_SGM_OFF_DSC);
	sgm->imp = ns_psum(mtd, TB_SGM_OFF_IMP);

	/* Determine if initialized. */
	const u8 do_ini = _syn_ini(syn);
	
	/* Initialize if required. */
	if (do_ini) {
		_dsc_ini(dsc, elm_max, dat_siz, rgn_nb, rgn_sizs, arr_nb, elm_sizs);
		_imp_ini(sgm, imp_ini, imp_siz);
		_sgm_ini_cpl(sgm);
	}

	/* Verify. */
	_dsc_chk(dsc, elm_max, dat_siz, rgn_nb, rgn_sizs, arr_nb, elm_sizs);
	_imp_chk(sgm, imp_ini, imp_siz);

	/* Assign size arrays. */
	sgm->rgn_sizs = _dsc_rgn_sizs(dsc);
	sgm->elm_sizs = _dsc_elm_sizs(dsc, rgn_nb);

	/* Map the data block. */
	void *dat = sgm->dat = ns_stg_map(stg, 0, TB_SGM_OFF_DAT, dat_siz, att_rws);
	assert(dat);

	/* Assign regions. */
	sgm->rgns = nh_all(sizeof(void *) * rgn_nb);
	void *rgn_stt = dat;
	u64 _dat_siz = 0;
	for (u8 rgn_idx = 0; rgn_idx < rgn_nb; rgn_idx++) {
		sgm->rgns[rgn_idx] = rgn_stt;
		const u64 siz = TB_SGM_SIZ_RGN(rgn_sizs[rgn_idx]);
		rgn_stt = ns_psum(rgn_stt, siz);
		_dat_siz += siz;
	}

	/* Assign arrays. */
	sgm->arrs = nh_all(sizeof(void *) * arr_nb);
	void *arr_stt = rgn_stt;
	for (u8 arr_idx = 0; arr_idx < arr_nb; arr_idx++) {
		sgm->arrs[arr_idx] = arr_stt;
		const u64 siz = TB_SGM_SIZ_ARR(elm_max, elm_sizs[arr_idx]);
		arr_stt = ns_psum(arr_stt, siz);
		_dat_siz += siz;
	}

	/* Check that we covered the required size. */
	assert(dat_siz == _dat_siz);

	/* No mapping or resize needed anymore. */
	ns_stg_cln(stg);

	/* Complete. */
	return sgm;

}

/*
 * Destruct @sgm.
 */
void tb_sgm_cls(
	_own_ tb_sgm *sgm
)
{

	/* Cache data before unmapping. */
	const u8 arr_nb = sgm->dsc->arr_nb;
	const u8 rgn_nb = sgm->dsc->rgn_nb;

	/* Sync. */
	ns_stg *stg = sgm->stg;
	ns_stg_syn(stg, sgm->mtd, TB_SGM_SIZ_MTD);
	ns_stg_syn(stg, sgm->dat, sgm->dsc->dat_siz);

	/* Unmap. */
	ns_stg_ump(stg, sgm->dat, sgm->dsc->dat_siz);
	ns_stg_ump(stg, sgm->mtd, TB_SGM_SIZ_MTD);

	/* Close. */
	nh_stg_cls(&sgm->res);

	/* Delete. */
	nh_fre(sgm->arrs, sizeof(void *) * arr_nb);
	nh_fre(sgm->rgns, sizeof(void *) * rgn_nb);
	nh_fre_(sgm);
}

/************
 * Read API *
 ************/

/*
 * If @sgm has been initialized with @elm_nb elements,
 * return 1.
 * Otherwise, return 0.
 */
u8 tb_sgm_rdy(
	tb_sgm *sgm,
	u64 elm_nb
)
{
	const u64 sgm_elm_nb = NS_RED_ONC(sgm->syn->elm_nb);
	assert(sgm_elm_nb <= sgm->dsc->elm_max);
	return (elm_nb <= sgm_elm_nb);
}

/*
 * Get read the @arr_nb read locations for @nb values
 * starting at @stt into @dst.
 * @red_nb can be null.
 */
void tb_sgm_red_rng(
	tb_sgm *sgm,
	u64 red_stt,
	u64 red_nb,
	const void **dst,
	u8 arr_nb
)
{
	check(arr_nb == sgm->dsc->arr_nb);
	check(arr_nb < (u64) (u32) -1);
	tb_sgm_syn *syn = sgm->syn;
	const u64 elm_end = red_stt + red_nb;
	const u64 sgm_end = NS_RED_ONC(syn->elm_nb);
	check(elm_end <= sgm_end);
	for (u8 arr_id = 0; arr_id < arr_nb; arr_id++) {
		dst[arr_id] = ns_psum(sgm->arrs[arr_id], red_stt * sgm->elm_sizs[arr_id]);  
	}
}

/*************
 * Write API *
 *************/

/*
 * Attempt to acquire @sgm's write privilege. 
 * If so, store its end offset at @offp, and return 0.
 * If someone already has it, return 1.
 */
uerr tb_sgm_wrt_get(
	tb_sgm *sgm,
	u64 *offp
)
{

	/* Require lock priv. */
	tb_sgm_syn *syn = sgm->syn;
	const u64 prv = ns_atm(a64, xch, aar, &syn->wrt, 1);

	/* If someone already has it, fail. */
	if (prv) return 1;

	/* If we got it, initialize the write procedure. */
	*offp = ns_atm(a64, red, acq, &syn->elm_nb);

	/* Report success. */
	return 0;

}

/*
 * Get the @arr_nb write locations for @wrt_nb values
 * of @sgm into @dst.
 * Return the offset of the first element to write.
 * Write priv must be owned.
 */
u64 tb_sgm_wrt_loc(
	tb_sgm *sgm,
	u64 wrt_nb,
	void **dst,
	u8 arr_nb
)
{
	check(arr_nb == sgm->dsc->arr_nb);
	check(wrt_nb < (u64) (u32) -1);
	tb_sgm_syn *syn = sgm->syn;
	check(NS_RED_ONC(syn->wrt));
	const u64 wrt_stt = NS_RED_ONC(syn->elm_nb);
	const u64 wrt_end = wrt_stt + wrt_nb;
	check(wrt_end <= sgm->dsc->elm_max);
	for (u8 arr_id = 0; arr_id < arr_nb; arr_id++) {
		dst[arr_id] = ns_psum(sgm->arrs[arr_id], wrt_stt * sgm->elm_sizs[arr_id]);  
	}
	return wrt_stt;
}

/*
 * Report @nb elements written.
 * Write priv must be owned.
 * Report the write.
 * Return the next write index.
 */
u64 tb_sgm_wrt_don(
	tb_sgm *sgm,
	u64 wrt_nb
)
{
	const u64 elm_nb = ns_atm(a64, add_red, rel, &sgm->syn->elm_nb, wrt_nb);
	assert(wrt_nb <= elm_nb);
	assert(elm_nb <= sgm->dsc->elm_max);
	return elm_nb;
}

/*
 * Complete the current write.
 * Updates previous indices for the written location.
 * Releases the write privileges.
 * Return 1 if the segment is complete, 0 otherwise. 
 */
u8 tb_sgm_wrt_cpl(
	tb_sgm *sgm
)
{
	/* Verify write data, determine fullness. */
	tb_sgm_syn *syn = sgm->syn;
	const u64 elm_nb = syn->elm_nb;
	const u64 elm_max = sgm->dsc->elm_max;
	assert(elm_nb <= elm_max);
	const u8 ful = (elm_nb == elm_max);

	/* Update nb and clear write. */
	const u64 prv = ns_atm(a64, xch, aar, &syn->wrt, 0);
	assert(prv == 1);

	/* Return the fullness. */
	return ful;
}

