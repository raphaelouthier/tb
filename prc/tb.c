/* Copyright 2024 Raphael Outhier - confidential - proprietary - no copy - no diffusion. */

#include <tb_stg/tb_stg.all.h>

#include <stdlib.h>
#include <unistd.h>

#include <sys/types.h>

/*********
 * Paths *
 *********/

#define SGM_PTH "/home/bt/tb_tst_sgm"
#define STG_PTH "/tmp/tb_tst_stg"

/******************
 * Test utilities *
 ******************/

/*
 * Allocate a random data block of @dat_siz bytes.
 */
static inline void *_dat_all(
	u64 dat_siz,
	u64 sed
)
{
	u64 *dat = nh_all(dat_siz);
	u64 ini = sed;
	const uad elm_nb = dat_siz / 8;
	for (uad idx = 0; idx < elm_nb; idx++) {
		dat[idx] = ini;
		ini = ns_hsh_mas_gen(ini);
	}
	return dat;
}

/*
 * Allocate an increasing time array.
 */
static inline u64 *_tim_all(
	u64 nb,
	u64 sed
)
{
	u64 *tims = nh_xall(nb * sizeof(u64));
	u64 val0 = ns_hsh_u32_rng(sed, 1, (u32) -1, 1);
	for (u64 i = 0; i < nb; i++) {
		tims[i] = val0;
		val0 += 20;
	}
	return tims;
}


#define GAT_PAS(dsc) \
	assert(gat_ctr == itr_ctr, "gate entry error\n"); \
	itr_ctr++; \
	gat_ctr = ns_gat_pas(&dsc->syn->gat); \
	assert(gat_ctr == itr_ctr, "gate exit error\n");

/*******************************
 * Parallel testing primitives *
 *******************************/

#define TST_PRL(_prc, _fnc, _ini) ({ \
	/* If thread-based test : */ \
	if (!(_prc)) { \
\
		/* Create workers and execute on our end too. */ \
		u8 *__thr_blk = nh_all(1024 * (uad) wrk_nb); \
		for (u8 i = 0; i < wrk_nb; i++) { \
			const u8 tst_prl_mst = (i + 1) == wrk_nb; \
\
			/* Allocate a descriptor. */ \
			typeof(_ini) __arg = (_ini); \
\
			/* Run in a thread or ourselves. */ \
			if (tst_prl_mst) { \
				_fnc(__arg); \
			} else { \
				assert(!nh_thr_run( \
					ns_psum(__thr_blk, 1024 * (uad) (i)), \
					1024, \
					0, \
					(u32 (*)(void *)) &_fnc, \
					__arg \
				)); \
			} \
		} \
\
		/* Free the thread block. */ \
		nh_fre(__thr_blk, 1024 * (uad) wrk_nb); \
\
	} \
\
	/* If process-based testing, fork. */ \
	else { \
\
		/* Report which process is the original one. */ \
		u8 tst_prl_mst = 1; \
\
		/* Fork and determine if we're the master. */ \
		for (u8 i = 1; i < wrk_nb; i++) { \
			pid_t __pid = fork(); \
			if (__pid == 0) { \
				tst_prl_mst = 0; \
				break; \
			} \
		} \
\
		/* Allocate a descriptor. */ \
		typeof(_ini) __arg = (_ini); \
\
		/* Execute. */ \
		_fnc(__arg); \
\
		/* If we're not the master, exit now. */ \
		if (!tst_prl_mst) { \
			debug("Subprocess test done.\n"); \
			exit(0); \
		} \
	} \
})

/*********************
 * Segment testbench *
 *********************/

/*
 * Shared synchronized data.
 * Occupies the first region of the test segment.
 */
typedef struct {

	/* Gate. */
	ns_gat gat;

	/* Write perm acquisistion failure counter. */
	volatile aad fal_ctr;

	/* Counter. */
	volatile aad val_ctr;

	/* Error. */
	volatile aad err_ctr;

} tb_tst_sgm_syn;

/*
 * Segment test per-thread descriptor.
 */
typedef struct {

	/* Seed. */
	u64 sed;

	/* Segment implementation size. */
	u64 imp_siz;

	/* Random data. */
	u64 *dat;

	/* Storage path. */
	const char *pth;

	/* Number of workers. */
	u8 wrk_nb;

	/* Syn data. */
	tb_tst_sgm_syn *syn;

} tb_tst_sgm_dsc;

/*
 * Region sizes
 */
const u64 rgn_sizs[10] = {
	1024, /* Sync region. */
	10, 3, 1025,
	2048, 4096, 65536,
	65537, 1 << 22, 65535
};

/*
 * Element sizes.
 */
const u8 _sgm_elm_sizs[255] = {
	      0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
	0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
	0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
	0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,
	0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,
	0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f,
	0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
	0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f,
	0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47,
	0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f,
	0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57,
	0x58, 0x59, 0x5a, 0x5b, 0x5c, 0x5d, 0x5e, 0x5f,
	0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67,
	0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f,
	0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77,
	0x78, 0x79, 0x7a, 0x7b, 0x7c, 0x7d, 0x7e, 0x7f,
	0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,
	0x88, 0x89, 0x8a, 0x8b, 0x8c, 0x8d, 0x8e, 0x8f,
	0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97,
	0x98, 0x99, 0x9a, 0x9b, 0x9c, 0x9d, 0x9e, 0x9f,
	0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7,
	0xa8, 0xa9, 0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf,
	0xb0, 0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7,
	0xb8, 0xb9, 0xba, 0xbb, 0xbc, 0xbd, 0xbe, 0xbf,
	0xc0, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7,
	0xc8, 0xc9, 0xca, 0xcb, 0xcc, 0xcd, 0xce, 0xcf,
	0xd0, 0xd1, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7,
	0xd8, 0xd9, 0xda, 0xdb, 0xdc, 0xdd, 0xde, 0xdf,
	0xe0, 0xe1, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7,
	0xe8, 0xe9, 0xea, 0xeb, 0xec, 0xed, 0xee, 0xef,
	0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7,
	0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff
};

#define SGM_ELM_NB 0x1ffff

/*
 * If @sgm is set, close it.
 * Then reload it and update @sgm.
 */

static inline void _sgm_lod(
	tb_sgm **sgmp,
	tb_tst_sgm_dsc *dsc
)
{

	if (*sgmp) {
		tb_sgm_cls(*sgmp);
	}

	/* Open. */
#define RGN_NB 10
#define ARR_NB 255
	*sgmp = tb_sgm_fopn(
		1,
		dsc->dat,
		dsc->imp_siz,
		RGN_NB,
		rgn_sizs,
		ARR_NB,
		SGM_ELM_NB,
		_sgm_elm_sizs,
		dsc->pth
	);

	dsc->syn = tb_sgm_rgn(*sgmp, 0); 
	dsc->syn->gat.wrk_nb = dsc->wrk_nb;

}

/*
 * Write regions of @dsc.
 */
static inline void _rgn_wrt(
	tb_tst_sgm_dsc *dsc,
	tb_sgm *sgm,
	u8 itr_nb
)
{
	u8 *src = (u8 *) dsc->dat + itr_nb;
	for (u8 i = 1; i < 10; i++) {
		u8 *dst = tb_sgm_rgn(sgm, i);
		assert(dst);
		u64 rgn_siz = rgn_sizs[i];
		while (rgn_siz--) {
			*(dst++) = *(src++); 
		}
	}
}

/*
 * Check regions of @dsc.
 */
static inline void _rgn_red(
	tb_tst_sgm_dsc *dsc,
	tb_sgm *sgm,
	u8 itr_nb
)
{
	u8 *src = (u8 *) dsc->dat + itr_nb;
	for (u8 i = 1; i < 10; i++) {
		u8 *dst = tb_sgm_rgn(sgm, i);
		assert(dst);
		u64 rgn_siz = rgn_sizs[i];
		while (rgn_siz--) {
			check(*(dst++) == *(src++)); 
		}
	}
}

/*
 * Per-thread entrypoint.
 */
static u32 _sgm_exc(
	tb_tst_sgm_dsc *dsc
)
{
	tb_sgm *sgm = 0;
	_sgm_lod(&sgm, dsc);
	assert(sgm);

	assert(aad_red_aar(&sgm->syn->ini_res) == 1);
	assert(aad_red_aar(&sgm->syn->ini_cpl) == 1);
	assert(sgm->syn->ini_res == 1);
	assert(sgm->syn->ini_cpl == 1);

	/* Iterate over the size range. */
	uad siz_crt = 1;
	const uad siz_max = SGM_ELM_NB;
	uad siz_sum = 0;
	uad gat_ctr = 0;
	void *dsts[ARR_NB];
	u8 itr = 0;
	uad itr_ctr = 0;
	while (siz_sum < siz_max) {
		itr++;
		debug("ITR %p\n", siz_crt);

		/* Reset the write failure counter. */
		aad_wrt_aar(&dsc->syn->fal_ctr, 0);
		
		/* Pass the gate. */
		GAT_PAS(dsc);

		/* Attempt to lock 100000 times.
		 * Verify that we only have one lock at a time. */
		uerr err = 0;
		u64 off = 0;
		for (u32 i = 0; i < 100000; i++) {
			//debug("\r%u", i);
			err = tb_sgm_wrt_get(sgm, &off);
			if (!err) {
				uad val = aad_red_inc_aar(&dsc->syn->val_ctr);
				aad_red_add_aar(&dsc->syn->err_ctr, val);
				assert(!val);
				val = aad_dec_red_aar(&dsc->syn->val_ctr);
				assert(!val);
				aad_red_add_aar(&dsc->syn->err_ctr, val);
				tb_sgm_wrt_cpl(sgm);
			} else {
				uad val = aad_red_aar(&dsc->syn->val_ctr);
				aad_red_add_aar(&dsc->syn->fal_ctr, val);
				val = aad_red_aar(&dsc->syn->val_ctr);
				aad_red_add_aar(&dsc->syn->fal_ctr, val);
			}
		}
		//debug("\n");

		/* If any error was detected, stop. */
		assert(!aad_red_aar(&dsc->syn->err_ctr));

		/* Pass the gate. */
		GAT_PAS(dsc);

		/* If no collision, report it. */
		if (dsc->wrk_nb != 1) {
			if (!dsc->syn->fal_ctr) {
				debug("NO COLLISION.\n");
			}
		}

		/* Lock. */ 
		if (dsc->wrk_nb == 1) {
			assert(!sgm->syn->wrt);
		}
		const u8 has_wrt = !tb_sgm_wrt_get(sgm, &off);
		if (dsc->wrk_nb == 1) {
			assert(has_wrt);
		}

		/* If write privileges acquired, add data. */
		if (has_wrt) {
			assert(off == siz_sum); 

			/* Write in up to 8 passes, reload in the middle. */
			const uad pas_nb = (siz_crt > 8) ? 8 : 1;
			const uad rld_pas = pas_nb >> 1;
			assert(!(siz_crt % pas_nb));
			const uad pas_siz = siz_crt / pas_nb;
			uad wrt_siz = 0;
			for (uad pas_idx = 0; pas_idx < pas_nb; pas_idx++) {

				/* Reload. */
				if (pas_idx == rld_pas) {
					_sgm_lod(&sgm, dsc); 
				}

				/* Get offsets. */
				tb_sgm_wrt_loc(sgm, pas_siz, dsts, ARR_NB);

				/* Check offsets and write. */ 
				for (u16 i = 0; i < ARR_NB; i++) {
					assert(dsts[i]);
					assert(dsts[i] == ns_psum(sgm->arrs[i], (off + wrt_siz) * (uad) (i + 1)));
					const uad len = pas_siz * (uad) (i + 1);
					const void *src = ns_psum(dsc->dat, (off + wrt_siz) * (uad) (i + 1));
					ns_mem_cpy(dsts[i], src, len);
				}

				/* Report write. */
				wrt_siz += pas_siz;
				tb_sgm_wrt_don(sgm, pas_siz);
				assert(tb_sgm_rdy(sgm, off + wrt_siz));
				assert(!tb_sgm_rdy(sgm, off + wrt_siz + 1));

			}

			assert(wrt_siz == siz_crt);

		}

		/* Otherwise, reload. */
		else {
			_sgm_lod(&sgm, dsc); 
		}

		/* Now, everyone writes regions. */
		_rgn_wrt(dsc, sgm, itr);

		/* Report write. */
		siz_sum += siz_crt;
		assert(!(siz_sum & (siz_sum + 1)));
		siz_crt <<= 1;

		/* Pass the gate. */
		GAT_PAS(dsc);

		/* Unlock. */
		if (has_wrt) tb_sgm_wrt_cpl(sgm);

		/* Pass the gate. */
		GAT_PAS(dsc);

		/* Verify regions. */
		_rgn_red(dsc, sgm, itr);

		/* Verify data in up to 8 passes. */
		{
			assert(tb_sgm_rdy(sgm, siz_sum));
			assert(!tb_sgm_rdy(sgm, siz_sum + 1));
			const uad pas_nb = (siz_sum > 8) ? 8 : 1;
			const uad pas_siz = siz_sum / pas_nb;
			uad pas_rem = siz_sum;
			uad ttl_red_siz = 0;
			while (pas_rem) {
				const uad red_siz = (pas_rem < pas_siz) ? pas_rem : pas_siz;

				/* Get offsets. */
				assert(tb_sgm_rdy(sgm, ttl_red_siz + red_siz));
				tb_sgm_red_rng(sgm, ttl_red_siz, red_siz, (const void **) dsts, ARR_NB);

				/* Check offsets and write. */ 
				for (u16 i = 0; i < ARR_NB; i++) {
					assert(dsts[i]);
					assert(dsts[i] == ns_psum(sgm->arrs[i], ttl_red_siz * (uad) (i + 1)));
					const uad len = red_siz * (uad) (i + 1);
					const void *src = ns_psum(dsc->dat, ttl_red_siz * (uad) (i + 1));
					assert(!ns_mem_cmp(dsts[i], src, len));
				}

				/* Report read. */
				SAFE_ADD(ttl_red_siz, red_siz);
				SAFE_SUB(pas_rem, red_siz);

			}

			assert(ttl_red_siz == siz_sum);

		}

		/* Pass the gate. */
		GAT_PAS(dsc);

	}

	tb_sgm_cls(sgm);

	assert(siz_sum == siz_max);
	return 0;
}

static inline tb_tst_sgm_dsc *_sgm_dsc_gen(
	u64 sed,
	void *dat,
	const char *stg_pth,
	u8 wrk_nb
)
{
	/* Allocate the descriptor. */
	nh_all__(tb_tst_sgm_dsc, dsc);
	dsc->sed = sed;
	dsc->imp_siz = ns_hsh_u32_rng(sed, 128, 1025, 1);
	dsc->dat = dat;
	dsc->pth = stg_pth;
	dsc->wrk_nb = wrk_nb;
	dsc->syn = 0;
	assert(dsc->imp_siz <= 1024);
	assert(1 <= dsc->imp_siz);

	/* Complete. */
	return dsc;
}

/*
 * Segment testing.
 */
static inline void _tst_sgm(
	nh_tst_sys *sys,
	u64 sed,
	u8 wrk_nb,
	u8 prc
)
{

	/* Clean if needed. */
	nh_fs_del_stg(SGM_PTH);

	/* Use 255 arrays, ~100K elements,
	 * 25MiB of random data. */
	const uad elm_max = SGM_ELM_NB;
	const uad dat_siz = uad_alu((uad) elm_max * 256, 6);
	void *dat = _dat_all(dat_siz, sed);

	/* Parallel testing. */
	TST_PRL(prc, _sgm_exc, _sgm_dsc_gen(sed, dat, SGM_PTH, wrk_nb));	

	/* Clean. */
	nh_fs_del_stg(SGM_PTH);

	/* Free the data block. */
	nh_fre(dat, dat_siz);

}

/*********************
 * Storage testbench *
 *********************/

/*
 * Shared synchronized data.
 * Occupies the first region of the test segment.
 */
typedef struct {

	/* Gate. */
	ns_gat gat;

} tb_tst_stg_syn;

/*
 * Segment test per-thread descriptor.
 */
typedef struct {

	/* Seed. */
	u64 sed;

	/* Segment implementation size. */
	u64 imp_siz;

	/* Random data. */
	u64 *dat;

	/* Times. */
	u64 *tims;

	/* Storage path. */
	const char *pth;

	/* Number of workers. */
	u8 wrk_nb;

	/* Syn data. */
	tb_tst_stg_syn *syn;

	/* Marketplace. */
	const char *mkp;

	/* Marketplace. */
	const char *ist;

	/* Level. */
	u8 lvl;

	/* Are we the ones with write privs ? */
	u8 wrt;

	/* Write key if @wrt is set. */
	u64 key;

} tb_tst_stg_dsc;

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
		u64 _tim = ((uint64_t *) srcs[0])[0];
		if (dsc->wrt) {
			tb_stg_wrt(idx, stp_elm_nb, srcs, arr_nb);
		}
		wrt_id += stp_elm_nb;

		/* Everyone unloads every 7 steps. */ 
		const u8 unl = !(stp_id % 7);
		if (unl) {
			tb_stg_cls(idx, dsc->key);
			tb_stg_dtr(sys);
			idx = 0;
			sys = 0;
		}

		GAT_PAS(dsc);

		/* Everyone reloads if required. */
		if (unl) {
			_stg_lod(&sys, &idx, dsc);
			assert(sys);
			assert(idx);
		}

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
			assert((!blk_id) || (itb[blk_id - 1][1] < itb[blk_id][0]));
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

		/* Everyone checks that they can access each block,
		 * and that the block's data is the expected one. */

		assert(!tb_stg_lod(idx, tim_stt - 1));
		assert(!tb_stg_lod(idx, tim_end + 1));

		/* Check that start and end blocks can be accessed. */
		tb_stg_blk *blk = 0;
	    blk = tb_stg_lod(idx, tim_stt);
		assert(blk);
		tb_stg_unl(blk);
		blk = tb_stg_lod(idx, tim_end);
		assert(blk);
		tb_stg_unl(blk);

		/* Generate the expected values. */
		_get_dat(srcs, tims, dsc->dat, arr_nb, 0, _elm_sizs);

		/* Iterate over all blocks. */
		u64 red_id = 0;
		tb_stg_red(idx, tim_stt, tim_end, dsts, arr_nb) {
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
	return 0;
	_stg_exc_lvl(dsc, 1);
	_stg_exc_lvl(dsc, 2);
	return 0;
}

/*
 * Storage testing.
 */
static inline void _tst_stg(
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


/**********************
 * Reproduction tests *
 **********************/

/*
 * Entrypoint for tests that already failed in the past.
 */
static inline void _tst_rpr(
	nh_tst_sys *sys,
	u64 sed
) {}

/**********
 * Muxers *
 **********/

#define tst(nam, ...) ({tst_cnt++; _tst_##nam(sys, sed, ##__VA_ARGS__); nh_tst_run(sys, thr_nb);})

/*
 * Run one test.
 */
static u32 _mux_one(
	u32 argc,
	char **argv,
	u64 sed,
	u8 thr_nb,
	u8 prc
)
{
	NS_ARG_OPTS(
		"one", argc, argv,
		return 1;,
		"execute a single test.",
		(0, flg, rpr, (rpr), "run reproduction tests."),
		(0, flg, sgm, (sgm), "run segment tests."),
		(0, flg, stg, (stg), "run storage tests.")
	);
	u32 tst_cnt = 0;
	nh_tst_sys *sys = nh_tst_sys_ctr();
	if (rpr__flg) tst(rpr); 
	if (sgm__flg) tst(sgm, thr_nb, prc); 
	if (stg__flg) tst(stg, thr_nb, prc); 
	assert(nh_tst_don(sys));
	debug("tb tests : %u testbenches ran, %U sequences, %U unit tests, %U errors.\n", tst_cnt, sys->seq_cnt, sys->unt_cnt, sys->err_cnt);
	u32 ret = sys->err_cnt != 0;
	nh_tst_sys_dtr(sys);
	return ret;
}

/*
 * Run all tests.
 */
static u32 _mux_all(
	u32 argc,
	char **argv,
	u64 sed,
	u8 thr_nb,
	u8 prc
)
{
	u32 tst_cnt = 0;
	nh_tst_sys *sys = nh_tst_sys_ctr();
	tst(rpr);
	tst(sgm, thr_nb, prc);
	tst(stg, thr_nb, prc);
	assert(nh_tst_don(sys));
	debug("tb tests : %u testbenches ran, %U sequences, %U unit tests, %U errors.\n", tst_cnt, sys->seq_cnt, sys->unt_cnt, sys->err_cnt);
	u32 ret = sys->err_cnt != 0;
	nh_tst_sys_dtr(sys);
	return ret;
}

/********
 * Test *
 ********/

/*
 * TB test main.
 */
static u32 _tst_main(
	u32 argc,
	char **argv
)
{
	NS_ARG_EXTR(
		"tst", argc, argv,
		return 1;,
		" test entrypoint",
		(0, u64, sed, (s, sed, seed), "seed."),
		(0, (dbu8, 1, 1, 255), nb_thr, (n, nb), "number of threads (default 1)."),
		(0, flg, thr, (thr), "run parallel tests using thread instead of processes."),
		(0, flg, err, (e, err), "halt on error.")
	);

	/* Make NT abort on error if required. */
	if (err__flg) nh_tst_aoe = 1;

	/* If no seed provided, choose one arbitrarily. */
	if (!sed__flg) {
		sed = nh_tst_sed_gen();
	}

	/* If specified, use threads. Otherwise, use processes. */
	const u8 prc = !thr__flg;
	
	info("Running TB tests with %u threads.\n", nb_thr); 
	info("Seed : %H.\n", sed);

	u32 ret = 1;
	NS_ARG_SEL(argc, argv, "tb", (sed, nb_thr, prc), ret,
		("one", _mux_one, "run a specified test."),
		("all", _mux_all, "run all tests.")
	);
	
	/* Complete. */
	return ret;

}

/********
 * Main *
 ********/

/*
 * TB main.
 */
u32 prc_tb_main(
	u32 argc,
	char **argv
)
{
	NS_ARG_INI(argc, argv);
	u32 ret = 0;
	NS_ARG_SEL(argc, argv, "tb", , ret,
		("tst", _tst_main, "run tests.")
	);
	return ret;
}

