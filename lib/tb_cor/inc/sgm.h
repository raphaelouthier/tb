/* Copyright 2025 Raphael Outhier - confidential - proprietary - no copy - no diffusion. */

#ifndef TB_COR_SGM_H
#define TB_COR_SGM_H

/*******
 * Doc *
 *******/

/*
 * The segment library stores shared data
 * in fixed size files
 *
 * It uses shared memory mapped files as a base
 * memory accesses, and atomics to synchronize
 * the writes. 
 *
 * It provides the following types of storage areas :
 * - shared memory regions of specified sizes, whose
 *   management is left to the user.
 * - shared synchronous memory arrays which can be read
 *   by multiple entities at a time, and incrementally
 *   written only once by a single writer at a time.
 *   Arrays are synchronous in the sense that they all
 *   have the same :
 *   - number of elements.
 *   - maximal number of elements.
 *   but array element size can vary from one array to
 *   the other.
 */

/*********
 * Types *
 *********/

types(
	tb_sgm_syn,
	tb_sgm_dsc,
	tb_sgm
);

/**************
 * Structures *
 **************/

/*
 * Storage synchronization data.
 * Fits in one page.
 * Mapped as shared RW.
 */
struct tb_sgm_syn {

	/* Set <=> initialization is reserved. */
	volatile aad ini_res;

	/* Set <=> initialization is done. */
	volatile aad ini_cpl;

	/* Set <=> someone is writing. */
	volatile aad wrt;

	/*
	 * Effective number of elements in memory.
	 * Readable by anyome, writeable only by the holder
	 * of @wrt.
	 */
	volatile aad elm_nb;

};

/*
 * Storage description data.
 * Fits in one page.
 * Mapped as shared RW.
 */
struct tb_sgm_dsc {

	/* Maximal number of elements in memory. */
	uad elm_max;

	/* Data block total size. */
	uad dat_siz;

	/* Number of regions. */
	u8 rgn_nb;

	/* Number of arrays. */
	u8 arr_nb;
	
	/* U64 aligned. */
	u64 pad;

	/*
	 * Next :
	 * u64 rgn_sizs[rgn_nb];
	 * u8 arr_sizs[rgn_nb];
	 */

};

/*
 * Storage data.
 */
struct tb_sgm {

	/* Storage resource. */
	nh_res res;

	/* Storage. */
	ns_stg *stg;

	/* Metadata. */
	void *mtd;

	/* Synchronization data. */
	tb_sgm_syn *syn;

	/* Descriptor. */
	tb_sgm_dsc *dsc;

	/* Region sizes (points to @dsc). */
	const u64 *rgn_sizs;

	/* Array element sizes (points to @dsc). */
	const u8 *elm_sizs;

	/* Implementation descriptor. */
	void *imp;

	/* Data section. */
	void *dat;

	/* Regions VAs. */
	void **rgns;

	/* Arrays VAs. */
	void **arrs;

};

/*
 * Sizes and offsets.
 */

/* Page size. Just needs to be a multiple of the machine's
 * actual page size. */
#define TB_SGM_PAG_SIZ ((uad) (1 << 16)) 

/* Round up to a page. */
#define TB_SGM_PAG_RND(x) (((x) + (TB_SGM_PAG_SIZ - 1)) & ~(TB_SGM_PAG_SIZ - 1))

/* Offset of the metadata block. */
#define TB_SGM_OFF_MTD 0

/* Offset of the synchronization block in the metadata block. */ 
#define TB_SGM_OFF_SYN 0

/* Size of the synchronization block. */
#define TB_SGM_SIZ_SYN 1024

/* Offset of the synchronization block in the metadata block. */ 
#define TB_SGM_OFF_DSC (TB_SGM_OFF_SYN + TB_SGM_SIZ_SYN)

/* Size of the synchronization block. */
#define TB_SGM_SIZ_DSC 1024

/* Offset of the synchronization block in the metadata block. */ 
#define TB_SGM_OFF_IMP (TB_SGM_OFF_DSC + TB_SGM_SIZ_DSC)

/* Size of the synchronization block. */
#define TB_SGM_SIZ_IMP 1024

/* Size of the metadata block. */
#define TB_SGM_SIZ_MTD (TB_SGM_OFF_IMP + TB_SGM_SIZ_IMP - TB_SGM_OFF_MTD)

/* Offset of the data block. */
#define TB_SGM_OFF_DAT TB_SGM_PAG_RND((TB_SGM_OFF_IMP + TB_SGM_SIZ_IMP))

/* Size of a region. */
#define TB_SGM_SIZ_RGN(rgn_siz) TB_SGM_PAG_RND(rgn_siz)

/* Size of an array block. */
#define TB_SGM_SIZ_ARR(elm_nbr, elm_siz) TB_SGM_PAG_RND((elm_nbr * elm_siz))

/*****************
 * Lifecycle API *
 *****************/

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
	uad imp_siz,
	u8 rgn_nb,
	const u64 *rgn_sizs,
	u8 arr_nb,
	uad elm_max,
	const u8 *elm_sizs,
	const char *pth,
	va_list *args
);
static inline tb_sgm *tb_sgm_fopn(
	u8 crt,
	void *imp_ini,
	uad imp_siz,
	u8 rgn_nb,
	const u64 *rgn_sizs,
	u8 arr_nb,
	uad elm_max,
	const u8 *elm_sizs,
	const char *pth,
	...
)
{
	va_list args;
	va_start(args, pth);
	tb_sgm *sgm = tb_sgm_vopn(
		crt,
		imp_ini,
		imp_siz,
		rgn_nb,
		rgn_sizs,
		arr_nb,
		elm_max,
		elm_sizs,
		pth,
		&args
	);
	va_end(args);
	return sgm;
}

/*
 * Close and delete @sgm.
 */
void tb_sgm_cls(
	tb_sgm *sgm
);

/**************
 * Region API *
 **************/

/*
 * Return @sgm's @idx-th region.
 */
static inline void *tb_sgm_rgn(
	tb_sgm *sgm,
	u8 idx
)
{
	assert(idx < sgm->dsc->rgn_nb);
	return sgm->rgns[idx];
}

/************
 * Read API *
 ************/

/*
 * Return @sgm's number of arrays.
 */
static inline u8 tb_sgm_arr_nb(
	tb_sgm *sgm
) {return sgm->dsc->arr_nb;}	

/*
 * Return @sgm's number of elements.
 */
static inline uad tb_sgm_elm_nb(
	tb_sgm *sgm
) {return ns_atm(aad, red, acq, &sgm->syn->elm_nb);}

/*
 * Return @sgm's maximal number of elements.
 */
static inline uad tb_sgm_elm_max(
	tb_sgm *sgm
) {return sgm->dsc->elm_max;}

/*
 * Return the start of @sgm's @idx-th array.
 */
static inline void *tb_sgm_arr_stt(
	tb_sgm *sgm,
	u8 idx
)
{
	check(idx < tb_sgm_arr_nb(sgm));
	return sgm->arrs[idx]; 
}

/*
 * If @sgm has been initialized with @elm_nb elements,
 * return 1.
 * Otherwise, return 0.
 */
u8 tb_sgm_rdy(
	tb_sgm *sgm,
	uad elm_nb
);

/*
 * Get the @arr_nb read locations for @nb values
 * starting at @stt into @dst.
 * @red_nb can be null.
 */
void tb_sgm_red_rng(
	tb_sgm *sgm,
	uad stt,
	uad nb,
	void **dst,
	u8 arr_nb
);

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
	uad *offp
);

/*
 * Get the @arr_nb write locations for @elm_nb values
 * of @sgm into @dst.
 * Return the offset of the first element to write.
 * Write priv must be owned.
 */
uad tb_sgm_wrt_loc(
	tb_sgm *sgm,
	uad elm_nb,
	void **dst,
	u8 arr_nb
);

/*
 * Report @nb elements written.
 * Write priv must be owned.
 * Return the next write index.
 */
uad tb_sgm_wrt_don(
	tb_sgm *sgm,
	uad wrt_nb
);

/*
 * Complete the current write.
 * Updates previous indices for the written location.
 * Releases the write privileges.
 * Return 1 if the segment is complete, 0 otherwise. 
 */
u8 tb_sgm_wrt_cpl(
	tb_sgm *sgm
);

#endif /* TB_COR_SGM_H */
