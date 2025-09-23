/* Copyright 2025 Raphael Outhier - confidential - proprietary - no copy - no diffusion. */

#ifndef SP_COR_BST_H
#define SP_COR_BST_H

/*******
 * Doc *
 *******/

/*
 * The buffered storage library stores a year's worth
 * of per-minute values on disk in a way that allows
 * multiple providers to coexist with the
 * same backing storage, and to synchronize the addition
 * of new data.
 *
 * For every possible minute in a year, it stores the
 * volume of transactions at this minute, and the
 * weighted average of transaction prices. 
 *
 * It uses shared memory mapped files as a base
 * memory accesses, and atomics to synchronize
 * the writes. 
 */

/*********
 * Types *
 *********/

types(
	sp_bst_mtd,
	sp_bst
);

/**************
 * Structures *
 **************/

/*
 * Storage metadata VA.
 * Fits in one page.
 * Mapped as shared RW.
 * Assumes cache line size of max 256 bytes.
 * If greater, then bump the padding and re-download
 * your data. 
 */
struct sp_bst_mtd {

	/* Offset 0 : shareable mutable data.
	 * Subject to snooping. */
	
	/* Lock area. */
	union {
		uad a; 
		aad v; 
		u64 u; 
	} lck;

	/* Effective max size of history in number of minutes.
	 * Leap years are taken into account. */ 
	volatile u64 mid_max;

	/* Effective number of minutes stored in memory. */
	volatile u64 mid_nb;

	/* Set <=> someone is writing. */
	volatile u64 wrt;

	/* Initialized ? */
	volatile u8 ini;

	/* Offset 33 : pad until 256. */
	u8 pad[223];

	/* Offset 256 : descriptor area. Initialized once.
	 * Never found uninitialized after locking, unless
	 * never initialized and the owner must do it. */

	/* Marketplace. */
	volatile sp_str mkp;

	/* Symbol. */
	volatile sp_str sym;

	/* Start mid. */
	volatile u64 mid_stt;

	/* Year. */
	volatile u32 yer;


};

/*
 * Storage data.
 */
struct sp_bst {

	/* Resource. */
	nh_res res;

	/* Storage. */
	ns_stg *stg;

	/* Mapped metadata. */
	sp_bst_mtd *mtd;

	/* Mapped volumes array. */
	f64 *vols;

	/* Mapped values array. */
	f64 *vals;

	/* Valid predecessor indices. */
	u32 *prvs;

	/* Number of writes performed by the current request. */
	u64 wrt_nb;

};

/*
 * Sizes and offsets.
 */

/* Page size. Just needs to be a multiple of the machine's
 * actual page size. */
#define SP_BST_PAG_SIZ ((uad) (1 << 16)) 

/* Round up to a page. */
#define SP_BST_RND(x) ((x + (SP_BST_PAG_SIZ - 1)) & ~(SP_BST_PAG_SIZ - 1))

/* Max number of entries in each array, max number of minutes in a year. */
#define SP_BST_MID_PER_YER ((uad) (366 * 24 * 60))

/* Size of the metadata block. 64K to accomodate the largest configs. */
#define SP_BST_BLK_SIZ_MTD SP_BST_PAG_SIZ

/* Size of the values block. */
#define SP_BST_BLK_SIZ_VAL SP_BST_RND(((uad) (sizeof(f64) * SP_BST_MID_PER_YER)))

/* Size of the volumes block. */
#define SP_BST_BLK_SIZ_VOL SP_BST_RND(((uad) (sizeof(f64) * SP_BST_MID_PER_YER)))

/* Size of the previous indices block. */
#define SP_BST_BLK_SIZ_PRV SP_BST_RND(((uad) (sizeof(u32) * SP_BST_MID_PER_YER)))

/* Start offset of the metadata page size. */
#define SP_BST_BLK_OFF_MTD (0)

/* Start offset of the values block. */
#define SP_BST_BLK_OFF_VAL (SP_BST_BLK_OFF_MTD + SP_BST_BLK_SIZ_MTD)

/* Start offset of the volumes block. */
#define SP_BST_BLK_OFF_VOL (SP_BST_BLK_OFF_VAL + SP_BST_BLK_SIZ_VAL)

/* Start offset of the previous indices block. */
#define SP_BST_BLK_OFF_PRV (SP_BST_BLK_OFF_VOL + SP_BST_BLK_SIZ_VOL)

/* Total size. */
#define SP_BST_STG_SIZ (SP_BST_BLK_OFF_PRV + SP_BST_BLK_SIZ_PRV)

/*******
 * API *
 *******/

/*
 * Construct and load a buffered storage.
 */
_own_ sp_bst *sp_bst_ctr(
	const char *dir,
	const char *mkp,
	const char *sym,
	u32 yer
);

/*
 * Destruct @bst.
 */
void sp_bst_dtr(
	_own_ sp_bst *bst
);

/************
 * Read API *
 ************/

/*
 * Return @bst's marketplace,
 */
static inline const char *sp_bst_mkp(
	sp_bst *bst
) {return (const char *) bst->mtd->mkp;}	

/*
 * Return @bst's marketplace,
 */
static inline const char *sp_bst_sym(
	sp_bst *bst
) {return (const char *) bst->mtd->sym;}	

/*
 * Return @bst's start mid.
 */
static inline u64 sp_bst_stt(
	sp_bst *bst
) {return bst->mtd->mid_stt;}

/*
 * Return @bst's end mid.
 * No locking done, possibly sees outdated values.
 * Will need to do the write sequence to see updated values.
 */
static inline u64 sp_bst_end(
	sp_bst *bst
) {return bst->mtd->mid_stt + bst->mtd->mid_nb;}

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

#endif /* SP_COR_BST_H */
