/* Copyright 2025 Raphael Outhier - confidential - proprietary - no copy - no diffusion. */

#include <tb_cor/tb_cor.all.h>

/***********
 * Locking *
 ***********/

/*
 * Lock @syn.
 * Basic spinlock implementation.
 */
static inline void _syn_lck(
	tb_sgm_syn *syn
)
{
	while (1) {
		aad red = 0;
		u8 don = __atomic_compare_exchange_n(&syn->lck.v, &red, 1, 0, __ATOMIC_ACQUIRE, __ATOMIC_ACQUIRE);
		if (don) break;
		WAIT_EVENT();
	}
}

/*
 * Unlock @syn.
 */
static inline void _syn_ulk(
	tb_sgm_syn *syn
)
{
	assert(NS_RED_ONC(syn->lck.v) == 1);
	__atomic_store_n(&syn->lck.v, 0, __ATOMIC_RELEASE);
	SEND_EVENT();
}

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
	const u8 do_ini = !aad_xchg(&syn->ini_res, 1);

	/* If initialization reserved by someone else,
	 * wait for completion, then ensure visibility. */
	if (!do_ini) {
		while (!aad_red(&syn->ini_cpl));
		_syn_lck(syn);
		_syn_ulk(syn);
		return 0;
	}

	/* If we must do the initialization, lock, 
	 * initialize @syn, and report that initialization
	 * must be done. */
	syn->lck.u = 0;
	_syn_lck(syn);
	syn->elm_nb = 0;
	syn->wrt = 0;
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
	_syn_ulk(sgm->syn);

	/* Report init done, unblock others. */
	aad_wrt(&sgm->syn->ini_cpl, 1);

}

/*
 * Initialize @dsc.
 */
static inline void _dsc_ini(
	tb_sgm_dsc *dsc,
	u64 elm_max,
	uad dat_siz,
	u8 arr_nb,
	u8 *elm_sizs
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
	u8 *elm_sizs
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

#if 0
/*
 * Initialize the metadata section if required.
 */
static inline uerr _ini_chk(
	tb_sgm *sgm,
	const char *mkp,
	const char *sym,
	u32 yer,
	const char **rsn
)
{
	tb_sgm_syn *syn = sgm->syn;

	/* Compute year-based constants. */
	const u64 mid_stt = sp_mid(yer, 0, 0, 0, 0);
	const u64 mid_max = (ns_is_leap_year(yer) ? 366 : 365) * 24 * 60;

	/* Lock the metadata. */
	uerr err = 0;
	_syn_lck(syn);

	/* If initialized, verify. */
	const u8 ini = syn->ini;
	if (syn->ini) {
		#define H(c, v) ({if (c) {err = 1; *rsn = v; goto end;}})
		H((ns_str_cmp(mkp, (const char *) syn->mkp) != 0), "mkp"); 
		H((ns_str_cmp(sym, (const char *) syn->sym) != 0), "sym");
		H((syn->yer != yer), "year");
		H((mid_stt != syn->mid_stt), "mid_stt");
		H((mid_max != syn->mid_max), "mid_max");
	}
	
	/* If not initialzied, init. */
	else {
		NS_WRT_ONC(syn->mid_max, mid_max);
		NS_WRT_ONC(syn->mid_nb, 0);
		NS_WRT_ONC(syn->wrt, 0);
		NS_WRT_ONC(syn->ini, 1);
		sp_str_cpy((char *) syn->mkp, mkp);
		sp_str_cpy((char *) syn->sym, sym);
		NS_WRT_ONC(syn->mid_stt, mid_stt);
		NS_WRT_ONC(syn->yer, yer);
	}

	/* Unlock. */
	_syn_ulk(syn);

	/* Synchronize metadata. */
	if (!ini) ns_stg_syn(sgm->stg, sgm->syn, tb_sgm_BLK_SIZ_syn); 

	/* Complete. */
	end:
	return err;
}

#endif
	
/*
 * Load a segment.
 * If it does not exist, create it and
 * initialize it with @imp_ini, @arr_nb, @elm_max and @elm_sizs.
 * Otherwise, check that @imp_ini, @arr_nb, @elm_max and @elm_sizs
 * do match with the loaded content.
 */
tb_sgm *tb_sgm_opn(
	const char *pth,
	void *imp_ini,
	uad imp_siz,
	u8 arr_nb,
	u64 elm_max,
	u8 *elm_sizs
)
{

	/* Construct. */
	tb_sgm *sgm = nh_all(sizeof(tb_sgm) + arr_nb * sizeof(void *));
	ns_stg *stg = sgm->stg = nh_stg_crt_opn(&sgm->res, pth);
	assert(stg, "storage %s open failed.\n", pth);
	sgm->wrt_nb = 0;

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
	}

	/* Verify. */
	_dsc_chk(dsc, elm_max, dat_siz, arr_nb, elm_sizs);
	_imp_chk(sgm, imp_ini, imp_siz);

	/* Map the data block. */
	void *dat = sgm->dat = ns_stg_map(stg, 0, TB_SGM_OFF_DAT, dat_siz, att_rws);
	assert(dat);

	/* Assign arrays. */
	void *arr_stt = dat;
	uad _dat_siz;
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
	u64 elm_nb
)
{
	tb_sgm_syn *syn = sgm->syn;
	_syn_lck(syn);
	const u64 sgm_elm_nb = NS_RED_ONC(sgm->syn->elm_nb);
	assert(sgm_elm_nb <= sgm->dsc->elm_max);
	_syn_ulk(syn);
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
	tb_sgm_syn *syn = sgm->syn;
	_syn_lck(syn);
	uerr err = (NS_RED_ONC(syn->wrt) != 0);
	if (err) goto end;
	NS_WRT_ONC(syn->wrt, 1);
	*offp = syn->elm_nb;
	sgm->wrt_nb = 0;
	end:;
	_syn_ulk(syn);
	return err;
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
u64 tb_sgm_wrt_don(
	tb_sgm *sgm,
	u64 wrt_nb
)
{
	check(wrt_nb < (u64) (u32) -1);
	sgm->wrt_nb += (u64) wrt_nb;
	check(sgm->wrt_nb < sgm->dsc->elm_max);
	return NS_RED_ONC(sgm->syn->elm_nb) + sgm->wrt_nb;
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
	/* Verify write data, prepare the write. */
	tb_sgm_syn *syn = sgm->syn;
	const u64 wrt_nb = NS_RED_ONC(sgm->wrt_nb);
	const u64 wrt_stt = syn->elm_nb;
	const u64 wrt_end = wrt_stt + wrt_nb;
	check(wrt_end <= wrt_stt);
	const u64 elm_max = sgm->dsc->elm_max;
	check(wrt_end <= elm_max);
	const u8 ful = (wrt_end == elm_max);

	/* Update nb and clear write. */
	_syn_lck(syn);
	check(NS_RED_ONC(syn->wrt));
	NS_WRT_ONC(syn->wrt, 0);
	NS_WRT_ONC(syn->elm_nb, wrt_end);
	_syn_ulk(syn);

	/* Return the fullness. */
	return ful;
}

