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

	/* Maximal number of elements in memory. */
	volatile u64 elm_nb;

	/* Effective number of elements in memory. */
	volatile u64 elm_max;

	/* Set <=> someone is writing. */
	volatile u64 wrt;

	/* Initialized ? */
	volatile u8 ini;

};

/*
 * Array descriptor.
 */
struct tb_sgm_arr {

	/* Offset. */
	u64 off;

	/* Element size. */
	u8 siz;

};

/*
 * Storage description data.
 * Fits in one page.
 * Mapped as shared RW.
 */
struct tb_sgm_dsc {

	/* Number of arrays. */
	u8 arr_nb;

	/* Arrays. */
	tb_sgm_arr arrs[];

};

/*
 * Storage data.
 */
struct tb_sgm {

	/* Resource. */
	nh_res res;

	/* Storage. */
	ns_stg *stg;

	/* Synchronization data. */
	tb_sgm_stg *stg;

	/* Descriptor. */
	tb_sgm_dsc *dsc;

	/* Implementation descriptor. */
	void *imp;

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


/* Offset of the synchronization block. */ 
#define TB_SGM_OFF_SYN 0

/* Size of the synchronization block. */
#define TB_SGM_SIZ_SYN 1024

/* Offset of the synchronization block. */ 
#define TB_SGM_OFF_DSC (TB_SGM_OFF_SYN + TB_SGM_SIZ_SYN)

/* Size of the synchronization block. */
#define TB_SGM_SIZ_DSC 1024

/* Offset of the synchronization block. */ 
#define TB_SGM_OFF_IMP (TB_SGM_OFF_DSC + TB_SGM_SIZ_DSC)

/* Size of the synchronization block. */
#define TB_SGM_SIZ_IMP 1024

/* Offset of the data block. */
#define TB_SGM_OFF_DAT (TB_SGM_OFF_IMP + TB_SGM_SIZ_IMP)

/* Size of an array block. */
#define TB_SGM_SIZ_ARR(elm_nbr, elm_siz) TB_SHM_PAG_RND((elm_nbr * elm_siz))

/*****************
 * Lifecycle API *
 *****************/

/*
 * Load a segment.
 * If it does not exist, create it and
 * initialize it with @imp, @arr_nb and @elm_sizs.
 * Otherwise, check that @imp, @arr@nb and @elm_sizs
 * do match with the loaded content.
 */
tb_sgm *tb_sgm_opn(
	const char *pth,
	void *ini,
	uad ini_siz,
	u8 arr_nb,
	u8 *elm_sizs
);

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
static inline void *tb_sgm_arr(
	tb_sgm *sgm,
	u8 idx
)
{
	check(idx < tb_sgm_arr_nb(sgm));
	return sgm->arrs[idx]; 
}

/*
 * If @bst has been initialized with data until @mid_end,
 * return 1.
 * Otherwise, return 0.
 */
u8 sp_bst_rdy(
	sp_bst *bst,
	u64 mid_stt,
	u64 mid_end
);

/*
 * Get read locations for @nb values at @mid.
 */
void sp_bst_red_rng(
	sp_bst *bst,
	u64 mid,
	u64 nb,
	sf_sig_f64 *vals,
	sf_sig_f64 *vols
);

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
);

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
);


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
);

/*
 * Report @nb elements written.
 * Write priv must be owned.
 * Report the write.
 * Return the next write index.
 */
u64 sp_bst_wrt_don(
	sp_bst *bst,
	u64 nb
);

/*
 * Complete the current write.
 * Updates previous indices for the written location.
 * Releases the write privileges.
 * Return 1 if the segment is complete, 0 otherwise. 
 */
u8 sp_bst_wrt_cpl(
	sp_bst *bst
);


#endif /* TB_COR_SGM_H */
