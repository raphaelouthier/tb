/* Copyright 2025 Raphael Outhier - confidential - proprietary - no copy - no diffusion. */

#ifndef TB_COR_SGM_H
#define TB_COR_SGM_H

/*******
 * Doc *
 *******/

/*
 * The segment library stores data in fixed size files
 * and supports single-writer multi-reader requests.
 *
 * It uses shared memory mapped files as a base
 * memory accesses, and atomics to synchronize
 * the writes. 
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

	/* Lock area. */
	union {
		uad a; 
		aad v; 
		u64 u; 
	} lck;

	/* Effective number of elements in memory. */
	volatile u64 elm_nb;

	/* Set <=> someone is writing. */
	volatile u64 wrt;

	/* Set <=> initialization is reserved. */
	volatile aad ini_res;

	/* Set <=> initialization is done. */
	volatile aad ini_cpl;

};

/*
 * Storage description data.
 * Fits in one page.
 * Mapped as shared RW.
 */
struct tb_sgm_dsc {

	/* Maximal number of elements in memory. */
	u64 elm_max;

	/* Data block total size. */
	uad dat_siz;

	/* Number of arrays. */
	u8 arr_nb;

	/* Arrays. */
	u8 elm_sizs[];

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

	/* Implementation descriptor. */
	void *imp;

	/* Data section. */
	void *dat;

	/* Number of writes performed by the current write request. */
	u64 wrt_nb;

	/* Array VAs. */
	void *arrs[];

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
	void *imp_ini,
	uad imp_siz,
	u8 arr_nb,
	u64 elm_max,
	u8 *elm_sizs,
	const char *pth,
	va_list *args
);
static inline tb_sgm *tb_sgm_fopn(
	void *imp_ini,
	uad imp_siz,
	u8 arr_nb,
	u64 elm_max,
	u8 *elm_sizs,
	const char *pth,
	...
)
{
	va_list args;
	va_start(args, pth);
	tb_sgm *sgm = tb_sgm_vopn(imp_ini, imp_siz, arr_nb, elm_max, elm_sizs, pth, &args);
	va_end(args);
	return sgm;
}

/*
 * Close and delete @sgm.
 */
void tb_sgm_cls(
	tb_sgm *sgm
);

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
	u64 elm_nb
);

/*
 * Get the @arr_nb read locations for @nb values
 * starting at @stt into @dst.
 */
void tb_sgm_red_rng(
	tb_sgm *sgm,
	u64 stt,
	u64 nb,
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
	u64 *offp
);

/*
 * Get the @arr_nb write locations for @elm_nb values
 * of @sgm into @dst.
 * Return the offset of the first element to write.
 * Write priv must be owned.
 */
u64 tb_sgm_wrt_loc(
	tb_sgm *sgm,
	u64 elm_nb,
	void **dst,
	u8 arr_nb
);

/*
 * Report @nb elements written.
 * Write priv must be owned.
 * Return the next write index.
 */
u64 tb_sgm_wrt_don(
	tb_sgm *sgm,
	u64 nb
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
