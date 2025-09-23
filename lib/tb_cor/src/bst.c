/* Copyright 2025 Raphael Outhier - confidential - proprietary - no copy - no diffusion. */

#include <sp_cor/sp_cor.all.h>

/***********
 * Locking *
 ***********/

/*
 * Lock @mtd.
 * Basic spinlock implementation.
 */
static inline void _mtd_lck(
	sp_bst_mtd *mtd
)
{
	while (1) {
		aad red = 0;
		u8 don = __atomic_compare_exchange_n(&mtd->lck.v, &red, 1, 0, __ATOMIC_ACQUIRE, __ATOMIC_ACQUIRE);
		if (don) break;
		WAIT_EVENT();
	}
}

/*
 * Unlock @mtd.
 */
static inline void _mtd_ulk(
	sp_bst_mtd *mtd
)
{
	assert(NS_RED_ONC(mtd->lck.v) == 1);
	__atomic_store_n(&mtd->lck.v, 0, __ATOMIC_RELEASE);
	SEND_EVENT();
}

/*******
 * API *
 *******/

/*
 * Initialize the metadata section if required.
 */
static inline uerr _ini_chk(
	sp_bst *bst,
	const char *mkp,
	const char *sym,
	u32 yer,
	const char **rsn
)
{
	sp_bst_mtd *mtd = bst->mtd;

	/* Compute year-based constants. */
	const u64 mid_stt = sp_mid(yer, 0, 0, 0, 0);
	const u64 mid_max = (ns_is_leap_year(yer) ? 366 : 365) * 24 * 60;

	/* Lock the metadata. */
	uerr err = 0;
	_mtd_lck(mtd);

	/* If initialized, verify. */
	const u8 ini = mtd->ini;
	if (mtd->ini) {
		#define H(c, v) ({if (c) {err = 1; *rsn = v; goto end;}})
		H((ns_str_cmp(mkp, (const char *) mtd->mkp) != 0), "mkp"); 
		H((ns_str_cmp(sym, (const char *) mtd->sym) != 0), "sym");
		H((mtd->yer != yer), "year");
		H((mid_stt != mtd->mid_stt), "mid_stt");
		H((mid_max != mtd->mid_max), "mid_max");
	}
	
	/* If not initialzied, init. */
	else {
		NS_WRT_ONC(mtd->mid_max, mid_max);
		NS_WRT_ONC(mtd->mid_nb, 0);
		NS_WRT_ONC(mtd->wrt, 0);
		NS_WRT_ONC(mtd->ini, 1);
		sp_str_cpy((char *) mtd->mkp, mkp);
		sp_str_cpy((char *) mtd->sym, sym);
		NS_WRT_ONC(mtd->mid_stt, mid_stt);
		NS_WRT_ONC(mtd->yer, yer);
	}

	/* Unlock. */
	_mtd_ulk(mtd);

	/* Synchronize metadata. */
	if (!ini) ns_stg_syn(bst->stg, bst->mtd, SP_BST_BLK_SIZ_MTD); 

	/* Complete. */
	end:
	return err;
}
	

/*
 * Construct and load a buffered storage.
 */
_own_ sp_bst *sp_bst_ctr(
	const char *dir,
	const char *mkp,
	const char *sym,
	u32 yer
)
{

	/* Construct. */
	nh_all__(sp_bst, bst);
	ns_stg *stg = bst->stg = nh_stg_fcrt_opn(&bst->res, "%s/%s.%s.%u", dir, mkp, sym, yer); 
	assert(stg, "storage %s/%s.%s.%u open failed.\n", dir, mkp, sym, yer);
	bst->wrt_nb = 0;

	/* Resize. */
	ns_stg_rsz(stg, SP_BST_STG_SIZ);

	/* Map the init page as shareable RW. */
	const u64 att_rws = NS_STG_ATT_RED | NS_STG_ATT_WRT | NS_STG_ATT_SHR; 
	bst->mtd = ns_stg_map(stg, 0, SP_BST_BLK_OFF_MTD, SP_BST_BLK_SIZ_MTD, att_rws); 
	assert(bst->mtd);

	/* Init the metadata or verify it. */
	const char *rsn = "unk";
	const uerr err = _ini_chk(bst, mkp, sym, yer, &rsn);
	assert(!err, "metadata mismatch for '%s:%s.%U' : %s.\n", mkp, sym, yer);

	/* Map data sections as shareable RW. */
	bst->vals = ns_stg_map(stg, 0, SP_BST_BLK_OFF_VAL, SP_BST_BLK_SIZ_VAL, att_rws); 
	assert(bst->vals);
	bst->vols = ns_stg_map(stg, 0, SP_BST_BLK_OFF_VOL, SP_BST_BLK_SIZ_VOL, att_rws); 
	assert(bst->vols);
	bst->prvs = ns_stg_map(stg, 0, SP_BST_BLK_OFF_PRV, SP_BST_BLK_SIZ_PRV, att_rws); 
	assert(bst->prvs);
	
	/* Complete. */
	return bst;

}

/*
 * Destruct @bst.
 */
void sp_bst_dtr(
	_own_ sp_bst *bst
)
{

	/* Sync. */
	ns_stg *stg = bst->stg;
	ns_stg_syn(stg, bst->mtd, SP_BST_BLK_SIZ_MTD); 
	ns_stg_syn(stg, bst->vals, SP_BST_BLK_SIZ_VAL); 
	ns_stg_syn(stg, bst->vols, SP_BST_BLK_SIZ_VOL); 
	ns_stg_syn(stg, bst->prvs, SP_BST_BLK_SIZ_PRV); 

	/* Unmap. */
	ns_stg_ump(stg, bst->mtd, SP_BST_BLK_SIZ_MTD); 
	ns_stg_ump(stg, bst->vals, SP_BST_BLK_SIZ_VAL); 
	ns_stg_ump(stg, bst->vols, SP_BST_BLK_SIZ_VOL); 
	ns_stg_ump(stg, bst->prvs, SP_BST_BLK_SIZ_PRV); 

	/* Close. */
	nh_stg_cls(&bst->res);

	/* Delete. */
	nh_fre_(bst);
}

/************
 * Read API *
 ************/

/*
 * If @bst has been initialized with data required to do a
 * read of the time interval [@start, @end[, return 1.
 * otherwise, return 0.
 */
u8 sp_bst_rdy(
	sp_bst *bst,
	u64 mid_stt,
	u64 mid_end
)
{
	sp_bst_mtd *mtd = bst->mtd;
	_mtd_lck(mtd);
	const u64 bst_mid_stt = mtd->mid_stt;
	const u64 bst_mid_end = bst_mid_stt + mtd->mid_nb;
	check(bst_mid_stt <= bst_mid_end);
	check(bst_mid_stt <= mid_stt);
	_mtd_ulk(mtd);
	return (mid_end <= bst_mid_end);
}

/*
 * Get read locations for @nb values at @mid.
 * Reads potentially outdated array sizes.
 */
void sp_bst_red_rng(
	sp_bst *bst,
	u64 mid,
	u64 nb,
	sf_sig_f64 *vals,
	sf_sig_f64 *vols
)
{
	check(nb < (u64) (u32) -1);
	sp_bst_mtd *mtd = bst->mtd;
	const u64 mid_stt = mtd->mid_stt;
	check(mid_stt <= mid);
	const u64 mid_off = (mid - mid_stt);
	check(mid_off + (u64) nb <= mtd->mid_max);
	const u64 mid_end = mid + nb;
	check(mid <= mid_end);
	check(mid_end <= mid_stt + mtd->mid_nb);
	sf_sig_f64_pushs(vals, bst->vals + mid_off, nb); 
	sf_sig_f64_pushs(vols, bst->vols + mid_off, nb); 
}

/*
 * Determine the value of @bst at the highest mid <= @mid.
 * If it exists, store its descriptor at provided locations
 * and return 0.
 * If it doesn't exist, return 1.
 */
uerr sp_bst_red_val(
	sp_bst *bst,
	u64 mid,
	f64 *valp,
	f64 *volp,
	u64 *midp
)
{
	sp_bst_mtd *mtd = bst->mtd;
	debug("%s/%s/%u.\n", mtd->mkp, mtd->sym, mtd->yer); 
	const u64 mid_stt = mtd->mid_stt;
	check(mid_stt <= mid);
	const u64 mid_off = (mid - mid_stt);
	check(mid_off < mtd->mid_max);
	const u32 prv_id = bst->prvs[mid_off];
	if (prv_id == (u32) -1) return 1; 
	check(prv_id <= mid_off);
	debug("prv_id %U.\n", prv_id);
	const f64 val = bst->vals[prv_id];
	const f64 vol = bst->vols[prv_id];
	check(vol > 0);
	check(val > 0);
	*valp = val;
	*volp = vol;
	*midp = prv_id;
	return 0;
}

/*************
 * Write API *
 *************/

/*
 * Attempt to acquire @bst's write privilege. 
 * If so, store its end mid at @midp, and return 0.
 * If someone already has it, return 1.
 */
uerr sp_bst_wrt_get(
	sp_bst *bst,
	u64 *midp
)
{
	sp_bst_mtd *mtd = bst->mtd;
	_mtd_lck(mtd);
	uerr err = (NS_RED_ONC(mtd->wrt) != 0);
	if (err) goto end;
	NS_WRT_ONC(mtd->wrt, 1);
	*midp = mtd->mid_stt + mtd->mid_nb;
	bst->wrt_nb = 0;
	end:;
	_mtd_ulk(mtd);
	return err;
}

/*
 * Get the write locations for @nb values of @bst.
 * Return the mid of the first element to write.
 * Write priv must be owned.
 */
u64 sp_bst_wrt_loc(
	sp_bst *bst,
	u64 nb,
	f64 **valp,
	f64 **volp
)
{
	check(nb < (u64) (u32) -1);
	sp_bst_mtd *mtd = bst->mtd;
	check(NS_RED_ONC(mtd->wrt));
	const u64 wrt_off = mtd->mid_nb + bst->wrt_nb;
	check(bst->wrt_nb <= bst->wrt_nb + nb);
	check(wrt_off + (u64) nb <= mtd->mid_max);
	*volp = bst->vols + wrt_off; 
	*valp = bst->vals + wrt_off; 
	return mtd->mid_stt + wrt_off;
}

/*
 * Report @nb elements written.
 * Write priv must be owned.
 * Report the write.
 * Return the next write index.
 */
u64 sp_bst_wrt_don(
	sp_bst *bst,
	u64 nb
)
{
	check(nb < (u64) (u32) -1);
	bst->wrt_nb += (u64) nb;
	return bst->mtd->mid_stt + bst->mtd->mid_nb + bst->wrt_nb;
}

/*
 * Complete the current write.
 * Updates previous indices for the written location.
 * Releases the write privileges.
 * Return 1 if the segment is complete, 0 otherwise. 
 */
u8 sp_bst_wrt_cpl(
	sp_bst *bst
)
{
	debug("CPL\n");
	/* Get the number of written bytes. */
	u64 nb = bst->wrt_nb;

	/* Update previous indices.
	 * We wrote those values, no sync needed. */
	const u64 wrt_off = bst->mtd->mid_nb;
	check(wrt_off + (u64) nb <= bst->mtd->mid_max);
	const f64 *volp = bst->vols + wrt_off; 
	u32 *prvp = bst->prvs + wrt_off; 
	u32 prv_id = (u32) -1;
	if (wrt_off != 0) {
		prv_id = prvp[-1]; 
	}
	for (u32 i = 0; i < nb; i++) {
		const f64 vol = *(volp++);
		check(vol >= 0);
		if (vol != 0.) prv_id = (u32) wrt_off + i;
		*(prvp++) = prv_id;
	}

	/* Update nb and clear write. */
	sp_bst_mtd *mtd = bst->mtd;
	_mtd_lck(mtd);
	check(NS_RED_ONC(mtd->wrt));
	NS_WRT_ONC(mtd->wrt, 0);
	NS_WRT_ONC(mtd->mid_nb, mtd->mid_nb + nb);
	check(mtd->mid_nb <= mtd->mid_max);
	const u8 ful = mtd->mid_nb == mtd->mid_max;
	_mtd_ulk(mtd);

	/* Return the fullness. */
	return ful;
}

