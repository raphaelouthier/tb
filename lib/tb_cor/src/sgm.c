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
	const u8 do_ini = !ns_atm(aad, xch, aar, &syn->ini_res, 1);

	/* If initialization reserved by someone else,
	 * wait for completion, then ensure visibility. */
	if (!do_ini) {
		while (!ns_atm(aad, red, acq, &syn->ini_cpl));
		return 0;
	}

	/* If we must do the initialization, lock, 
	 * initialize @syn, and report that initialization
	 * must be done. */
	ns_atm(aad, wrt, rel, &syn->wrt, 1);
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
	const uad prv = ns_atm(aad, xch, aar, &sgm->syn->wrt, 0);
	assert(prv == 1);

	/* Report init done, unblock others. */
	ns_atm(aad, wrt, rel, &sgm->syn->ini_cpl, 1);

}

/*
 * Initialize @dsc.
 */
static inline void _dsc_ini(
	tb_sgm_dsc *dsc,
	u64 elm_max,
	uad dat_siz,
	u8 arr_nb,
	const u8 *elm_sizs
)
{
	dsc->elm_max = elm_max;
	dsc->dat_siz = dat_siz;
	dsc->arr_nb = arr_nb;
	for (uint8_t arr_idx = 0; arr_idx < arr_nb; arr_idx++) {
		dsc->elm_sizs[arr_idx] = elm_sizs[arr_idx];
	}
}

/*
 * Verify that @dsc matches with what is expected.
 */
static inline void _dsc_chk(
	tb_sgm_dsc *dsc,
	u64 elm_max,
	uad dat_siz,
	u8 arr_nb,
	const u8 *elm_sizs
)
{
	assert(dsc->elm_max == elm_max, "dsc mismatch : elm_max : expected %U, got %U.\n", elm_max, dsc->elm_max);
	assert(dsc->dat_siz == dat_siz, "dsc mismatch : dat_siz : expected %p, got %p.\n", dat_siz, dsc->dat_siz);
	assert(dsc->arr_nb == arr_nb, "dsc mismatch : arr_nb : expected %u, got %u.\n", arr_nb, dsc->arr_nb);
	for (uint8_t arr_idx = 0; arr_idx < arr_nb; arr_idx++) {
		assert(dsc->elm_sizs[arr_idx] == elm_sizs[arr_idx], "dsc mismatch : elm_sizs[%u] : expected %u, got %u.\n", arr_idx, dsc->elm_sizs[arr_idx], elm_sizs[arr_idx]);
	}
}

/*
 * Initialize @imp.
 */
static inline void _imp_ini(
	tb_sgm *sgm,
	void *imp,
	uad imp_siz
)
{
	assert(imp_siz <= TB_SGM_SIZ_IMP);
	const uad zer_siz = TB_SGM_SIZ_IMP - imp_siz;
	ns_mem_cpy(sgm->imp, imp, imp_siz);
	if (zer_siz) ns_mem_rst(ns_psum(sgm->imp, imp_siz), zer_siz); 
}

/*
 * Verify that @inp matches with what is expected.
 */
static inline void _imp_chk(
	tb_sgm *sgm,
	void *imp,
	uad imp_siz
)
{
	assert(imp_siz <= TB_SGM_SIZ_IMP);
	for (uad byt_idx = 0; byt_idx < imp_siz; byt_idx++) {
		assert(((volatile u8 *) sgm->imp)[byt_idx] == ((volatile u8 *) imp)[byt_idx], "imp mismatch at byte %u.\n", byt_idx);
	}
	for (uad byt_idx = imp_siz; byt_idx < TB_SGM_SIZ_IMP; byt_idx++) {
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
	void *imp_ini,
	uad imp_siz,
	u8 arr_nb,
	u64 elm_max,
	const u8 *elm_sizs,
	const char *pth,
	va_list *args
)
{

	/* Construct. */
	tb_sgm *sgm = nh_all(sizeof(tb_sgm) + arr_nb * sizeof(void *));
	ns_stg *stg = sgm->stg = nh_stg_vopn(NH_FIL_ATT_RWC, &sgm->res, pth, args);
	assert(stg, "storage %s open failed.\n", pth);

	/* Compute the size. */
	uad dat_siz = 0;
	for (u8 arr_idx = 0; arr_idx < arr_nb; arr_idx++) {
		dat_siz += TB_SGM_SIZ_ARR(elm_max, elm_sizs[arr_idx]);
	}
	const uad siz_tgt = TB_SGM_OFF_DAT + dat_siz;

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
		_dsc_ini(dsc, elm_max, dat_siz, arr_nb, elm_sizs);
		_imp_ini(sgm, imp_ini, imp_siz);
		_sgm_ini_cpl(sgm);
	}

	/* Verify. */
	_dsc_chk(dsc, elm_max, dat_siz, arr_nb, elm_sizs);
	_imp_chk(sgm, imp_ini, imp_siz);

	/* Map the data block. */
	void *dat = sgm->dat = ns_stg_map(stg, 0, TB_SGM_OFF_DAT, dat_siz, att_rws);
	debug("dat siz %p, %UMiB, %UGiB.\n", dat_siz, dat_siz / (1024 * 1024), dat_siz / (1024 * 1024 * 1024));
	debug("BND %p -> %p\n", dat, ns_psum(dat, dat_siz));
	assert(dat);

	/* Assign arrays. */
	void *arr_stt = dat;
	uad _dat_siz = 0;
	for (u8 arr_idx = 0; arr_idx < arr_nb; arr_idx++) {
		sgm->arrs[arr_idx] = arr_stt;
		const uad siz = TB_SGM_SIZ_ARR(elm_max, elm_sizs[arr_idx]);
		arr_stt = ns_psum(arr_stt, siz);
		_dat_siz += siz;
	}
	assert(dat_siz == _dat_siz);

	/* Complete. */
	return sgm;

}

/*
 * Destruct @sgm.
 */
void tb_sgm_dtr(
	_own_ tb_sgm *sgm
)
{

	/* Cache data before unmapping. */
	const u8 arr_nb = sgm->dsc->arr_nb;

	/* Sync. */
	ns_stg *stg = sgm->stg;
	ns_stg_syn(stg, sgm->mtd, TB_SGM_SIZ_MTD);
	ns_stg_syn(stg, sgm->dat, sgm->dsc->dat_siz);

	/* Unmap. */
	ns_stg_ump(stg, sgm->mtd, TB_SGM_SIZ_MTD);
	ns_stg_ump(stg, sgm->dat, sgm->dsc->dat_siz);

	/* Close. */
	nh_stg_cls(&sgm->res);

	/* Delete. */
	nh_fre(sgm, sizeof(tb_sgm) + arr_nb * sizeof(void *));
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
	uad elm_nb
)
{
	const uad sgm_elm_nb = NS_RED_ONC(sgm->syn->elm_nb);
	assert(sgm_elm_nb <= sgm->dsc->elm_max);
	return (elm_nb <= sgm_elm_nb);
}

/*
 * Get read the @arr_nb read locations for @nb values
 * starting at @stt into @dst.
 */
void tb_sgm_red_rng(
	tb_sgm *sgm,
	u64 red_stt,
	u64 red_nb,
	void **dst,
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
		dst[arr_id] = ns_psum(sgm->arrs[arr_id], red_stt * sgm->dsc->elm_sizs[arr_id]);  
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
	const aad prv = ns_atm(aad, xch, aar, &syn->wrt, 1);

	/* If someone already has it, fail. */
	if (prv) return 1;

	/* If we got it, initialize the write procedure. */
	*offp = syn->elm_nb;

	/* Report success. */
	return 0;

}

/*
 * Get the @arr_nb write locations for @wrt_nb values
 * of @sgm into @dst.
 * Return the offset of the first element to write.
 * Write priv must be owned.
 */
uad tb_sgm_wrt_loc(
	tb_sgm *sgm,
	uad wrt_nb,
	void **dst,
	u8 arr_nb
)
{
	check(arr_nb == sgm->dsc->arr_nb);
	check(wrt_nb < (uad) (u32) -1);
	tb_sgm_syn *syn = sgm->syn;
	check(NS_RED_ONC(syn->wrt));
	const uad wrt_stt = NS_RED_ONC(syn->elm_nb);
	const uad wrt_end = wrt_stt + wrt_nb;
	check(wrt_end <= sgm->dsc->elm_max);
	for (u8 arr_id = 0; arr_id < arr_nb; arr_id++) {
		dst[arr_id] = ns_psum(sgm->arrs[arr_id], wrt_stt * sgm->dsc->elm_sizs[arr_id]);  
	}
	return wrt_stt;
}

/*
 * Report @nb elements written.
 * Write priv must be owned.
 * Report the write.
 * Return the next write index.
 */
uad tb_sgm_wrt_don(
	tb_sgm *sgm,
	uad wrt_nb
)
{
	const uad elm_nb = ns_atm(aad, add_red, rel, &sgm->syn->elm_nb, wrt_nb);
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
	const uad elm_nb = syn->elm_nb;
	const uad elm_max = sgm->dsc->elm_max;
	assert(elm_nb <= elm_max);
	const u8 ful = (elm_nb == elm_max);

	/* Update nb and clear write. */
	const uad prv = ns_atm(aad, xch, aar, &syn->wrt, 0);
	assert(prv == 1);

	/* Return the fullness. */
	return ful;
}

