/* Copyright 2024 Raphael Outhier - confidential - proprietary - no copy - no diffusion. */

#include <tb_cor/tb_cor.all.h>

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
		if (i & 1) {
			val0 += 20;
		}
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

/************
 * Lv1 test *
 ************/

/*
 * Orderbook update.
 */
typedef struct {

	/* Price of update. */
	f64 val;

	/* Volume of update. */
	f64 vol;

	/* Time of update. */
	u64 tim;

} tb_tst_lv1_upd ;

/*
 * Orderbook generation context.
 */
typedef struct {

	/*
	 * Request.
	 */

	/* Seed. */
	u64 sed;

	/* Number of time units. */
	u64 unt_nbr;

	/* Start time. */
	u64 tim_stt;

	/* Time increment. */
	u64 tim_inc;

	/* Price minimum. */
	f64 prc_min;

	/* Total number of ticks in the orderbook. */
	u64 tck_nbr;

	/* Number of time beats per unit. */
	u64 tim_stp;

	/* Ticks per price unit. */
	u64 tck_rat;

	/*
	 * Generation parameters.
	 */

	/* Generation base volume. */
	f64 gen_vol;

	/* Orderbook total reset period. */
	u64 obk_rst_prd;

	/* Orderbook volume reset period. */
	u64 obk_vol_rst_prd;

	/* Orderbook normal bid period. 1. */

	/* Orderbook exceptional bid period. */
	u64 obk_bid_exc_prd;

	/* Orderbook bid reset period. */
	u64 obk_bid_rst_prd;

	/* Orderbook normal ask period. 1. */

	/* Orderbook exceptional ask period. */
	u64 obk_ask_exc_prd;

	/* Orderbook ask reset period. */
	u64 obk_ask_rst_prd;

	/*
	 * Generation stats.
	 */

	/* Current time. */
	u64 tim_cur;

	/* Current tick. */
	u64 tck_cur;

	/* Previous tick. */
	u64 tck_prv;

	/* Current generation step. */
	u64 gen_idx;

	/* Generation seed. */
	u64 gen_sed;

	/* Tick min. */
	u64 tck_min;

	/* Tick max. */
	u64 tck_max;

	/* Did an orderbook reset happen at last orderbook update ? */
	u8 obk_rst;

	/* Number of times the orderbook was reset. */
	u64 obk_rst_ctr;

	/* Number of times the orderbook was reset before a delay. */
	u64 obk_rst_dly;

	/* Number of times the orderbook was reset before a delay spanning at least one complete cell. */
	u64 obk_rst_skp;

	/*
	 * Generation-output.
	 */

	/* Number of updates. */
	u64 upd_nbr;

	/* Maximal number of updates. */
	u64 upd_max;

	/* Updates array. */
	tb_tst_lv1_upd *upds;

	/* Bid array. */
	u64 *bid_arr;

	/* Ask array[unt_nbr]. */
	u64 *ask_arr;

	/* Global heatmap [unt_nbr * tim_stp][tck_nbr]. */
	f64 *hmp;

	/* Init heatmap. Each cell has the resting value for its price/time. */
	f64 *hmp_ini;

	/* Time of last update. */
	u64 tim_end;

} tb_tst_lv1_ctx;

/*
 * Update @ctx's current tick.
 */
static inline void _tck_upd(
	tb_tst_lv1_ctx *ctx
)
{
	u64 tck_cur = ctx->tck_cur; 
	u64 gen_sed = ctx->gen_sed;
	ctx->tck_prv = ctx->tck_cur;

	/* Determine if we should go up or down. */
	u8 up_ok = tck_cur + 1 < ctx->tck_max;
	u8 dwn_ok = ctx->tck_min < tck_cur;
	assert(up_ok || dwn_ok);
	u8 is_dwn;
	if (!up_ok) {
		is_dwn = 1;
	} else if (!dwn_ok) {
		is_dwn = 0;
	} else {
		is_dwn = ((gen_sed >> 32) & 0xff) < 0x80;
	}
	gen_sed = ns_hsh_mas_gen(gen_sed);

	/* Determine the price increment logarithmically. */
	u8 inc_sed = gen_sed & 0xff;
	u8 inc_ord = 0;
	if (inc_sed < (1 << 1)) {
		inc_ord = 7;
	} else if (inc_sed < (1 << 2)) {
		inc_ord = 6;
	} else if (inc_sed < (1 << 3)) {
		inc_ord = 5;
	} else if (inc_sed < (1 << 4)) {
		inc_ord = 4;
	} else if (inc_sed < (1 << 5)) {
		inc_ord = 3;
	} else {
		inc_ord = 2;
	}
	check(inc_ord);
	u64 inc_max = 1 << inc_ord;
	assert(inc_max < ctx->tck_min); 
	assert(inc_max < inc_max + ctx->tck_max);
	gen_sed = ctx->gen_sed = ns_hsh_mas_gen(gen_sed);

	/* Determine the actual increment. */
	const u64 inc_val = ns_hsh_u64_rng(gen_sed, 0, inc_max, 1);

	/* Determine the new target price. */
	if (is_dwn) {
		tck_cur = tck_cur - inc_val;
		if (tck_cur < ctx->tck_min) tck_cur = ctx->tck_min;
	} else {
		tck_cur = tck_cur + inc_val;
		if (tck_cur >= ctx->tck_max) tck_cur = ctx->tck_max - 1;
	}
	ctx->gen_sed = ns_hsh_mas_gen(gen_sed);
	ctx->tck_cur = tck_cur;

}

/*
 * Update @obk after @ctx's price was updated.
 */
static inline void _obk_upd(
	tb_tst_lv1_ctx *ctx,
	f64 *obk
)
{

	/* Flags. */
	ctx->obk_rst = 0;

	/* Constants. */
	const f64 gen_vol = ctx->gen_vol; 

	/* Read the current and previous prices. */
	const u64 tck_cur = ctx->tck_cur;
	const u64 tck_prv = ctx->tck_prv;
	debug("tck_cur %U.\n", tck_cur);
	debug("tck_prv %U.\n", tck_prv);
	const u64 tck_min = ctx->tck_min;
	const u64 tck_max = ctx->tck_max;
	assert(tck_min > 400);
	#define OBK(idx) (obk[((idx) - tck_min)])
	#define OBK_WRT(idx, val) ({ \
		if ((tck_min <= idx) && (idx < tck_max)) { \
			OBK(idx) = (val); \
		} \
	})

	/* Read the seed. */
	u64 gen_sed = ctx->gen_sed;
	u64 gen_idx = ctx->gen_idx;

	/* Reset ticks in between. */
	{
		assert(!(OBK(tck_prv)));
		u64 rst_stt = tck_cur; 
		u64 rst_end = tck_prv; 
		if (rst_end < rst_stt) {
			_swap(rst_end, rst_stt);
		}
		for (;rst_stt <= rst_end; rst_stt++) {
			OBK(rst_stt) = 0;
		}
		assert(!(OBK(tck_cur)));
	}

	/* Reset around the current tick. */
	{
		if (gen_sed & (1 << 20)) {
			u8 rst_up = (gen_sed & 0x7);
			u8 rst_dwn = ((gen_sed >> 3) & 0x7);
			for (
				u64 rst_stt = (tck_cur - rst_dwn), rst_end = (tck_cur + rst_up);
				rst_stt <= rst_end;
				rst_stt++
			) {
				if ((tck_min <= rst_stt) && (rst_stt < tck_max)) {
					OBK(rst_stt) = 0;
				}	
			}
		}
	}
	gen_sed = ns_hsh_mas_gen(gen_sed);

	/* Reset the orderbook if required. */ 
	if (!(gen_idx % ctx->obk_rst_prd)) {
		for (u64 idx = tck_min; idx < tck_max; idx++) {
			OBK(idx) = 0;
		}

		/* Skip all other generation steps. */
		goto end;
	}

	/* Normal and exceptional steps. */
	static u64 nrm_stps[4] = {
		1,
		1,
		5,
		2		
	};

	/* Apply normal bids and ask updates on 10 ticks around the current price. */
	if (1) { //!(gen_idx % ctx->obk_ask_nrm_prd)) {
		f64 ask_max = ns_hsh_f64_rng(gen_sed, gen_vol / 2, gen_vol, gen_vol / 100);
		const u64 stp = nrm_stps[(gen_idx) % 4];
		for (u64 idx = tck_cur, ctr = 0; ((idx += stp) < tck_max) && (ctr++ < 10);) {
			OBK(idx) = ask_max;
			ask_max /= 1.1;
		}	
		gen_sed = ns_hsh_mas_gen(gen_sed);
	}
	if (1) { //!(gen_idx % ctx->obk_bid_nrm_prd)) {
		f64 bid_max = -ns_hsh_f64_rng(gen_sed, gen_vol / 2, gen_vol, gen_vol / 100);
		const u64 stp = nrm_stps[(gen_idx) % 4];
		for (u64 idx = tck_cur, ctr = 0; (tck_min <= (idx -= stp)) && (ctr++ < 10);) {
			OBK(idx) = bid_max;
			bid_max /= 1.1;
		}	
		gen_sed = ns_hsh_mas_gen(gen_sed);
	}

	static u64 exc_stps[3] = {
		100,
		200,
		400
	};
	/* Apply exceptional bids and asks on the whole orderbook. */
	if (!(gen_idx % ctx->obk_ask_exc_prd)) {
		f64 ask_max = ns_hsh_f64_rng(gen_sed, gen_vol / 2, gen_vol, gen_vol / 100);
		const u64 stp = exc_stps[(gen_idx / ctx->obk_ask_exc_prd) % 3];
		for (u64 idx = tck_cur; (idx += stp) < tck_max;) {
			OBK(idx) = ask_max;
			ask_max /= 1.1;
		}	
		gen_sed = ns_hsh_mas_gen(gen_sed);
	}
	if (!(gen_idx % ctx->obk_bid_exc_prd)) {
		f64 bid_max = -ns_hsh_f64_rng(gen_sed, gen_vol / 2, gen_vol, gen_vol / 100);
		const u64 stp = exc_stps[(gen_idx / ctx->obk_bid_exc_prd) % 3];
		for (u64 idx = tck_cur; tck_min <= (idx -= stp);) {
			OBK(idx) = bid_max;
			bid_max /= 1.1;
		}	
		gen_sed = ns_hsh_mas_gen(gen_sed);
	}

	static u64 rst_stps[3] = {
		12,
		29,
		51
	};
	/* Apply resets on the whole orderbook. */
	if (!(gen_idx % ctx->obk_ask_rst_prd)) {
		const u64 stp = rst_stps[(gen_idx / ctx->obk_ask_rst_prd) % 3];
		for (u64 idx = tck_cur; (idx += stp) < tck_max;) {
			OBK(idx) = 0;
		}	
		gen_sed = ns_hsh_mas_gen(gen_sed);
	}
	if (!(gen_idx % ctx->obk_bid_rst_prd)) {
		const u64 stp = rst_stps[(gen_idx / ctx->obk_bid_rst_prd) % 3];
		for (u64 idx = tck_cur; tck_min <= (idx -= stp);) {
			OBK(idx) = 0;
		}	
		gen_sed = ns_hsh_mas_gen(gen_sed);
	}

	/* Save post generation params. */	
	end:;
	ctx->gen_sed = gen_sed;
	ctx->gen_idx = gen_idx + 1;

}

/*
 * Add @obk to @ctx's heatmap at column @col_idx.
 */
static inline void _hmp_add(
	tb_tst_lv1_ctx *ctx,
	f64 *obk,
	u64 col_idx,
	u64 nxt_idx
)
{
	
	const u64 tck_nbr = ctx->tck_nbr;
	const u64 hmp_stt = col_idx * tck_nbr; 
	for (u64 tck_idx = tck_nbr; tck_idx--;) {
		ctx->hmp[hmp_stt + tck_idx] += obk[tck_idx];
	}
	if (col_idx != nxt_idx) {
		assert(nxt_idx == col_idx + 1);
		const u64 hmp_nxt = nxt_idx * tck_nbr; 
		for (u64 tck_idx = tck_nbr; tck_idx--;) {
			ctx->hmp_ini[hmp_nxt + tck_idx] += obk[tck_idx];
		}
	}
}

#define TCK_TO_PRC(tck_idx, min, stp) \
	((f64) min + (f64) (stp) * (f64) (tck_idx))

#define TCK_PER_UNT 100
#define TIM_STP 10

/*
 * Generate a sequence of orderbook updates as
 * specified by @ctx.
 */
static inline void _tck_gen(
	tb_tst_lv1_ctx *ctx
)
{
	assert(ctx->unt_nbr < 10000);

	/*
	 * Constants.
	 */
	

	/* Skip between 1 and 100 beats time every 43 iterations. */
	const u64 skp_prd = 43;

	/* Read request data. */
	const u64 unt_nbr = ctx->unt_nbr; 
	const u64 tim_stt = ctx->tim_stt; 
	const u64 tim_inc = ctx->tim_inc; 
	const f64 prc_min = ctx->prc_min;
	const u64 tck_nbr = ctx->tck_nbr;
	const u64 tim_stp = ctx->tim_stp;
	const u64 aid_wid = tim_inc * tim_stp;
	const u64 tck_rat = ctx->tck_rat;

	/* 100 ticks per price unit. */
	const f64 prc_cfc = tck_rat;
	const f64 prc_stp = 1 / prc_cfc;
	assert(prc_cfc == (f64) 1 / prc_stp);

	/* Allocate the updates array. */ 	
	ctx->upd_max = ctx->unt_nbr * tim_stp; 
	ctx->upds = nh_all(ctx->upd_max * sizeof(tb_tst_lv1_upd));
	ctx->upd_nbr = 0;

	/* Helper to resize the updates arrays. */
	#define _upd_rsz(nb) ({ \
		if (ctx->upd_max < (nb)) { \
			const u64 _upd_max = 2 * ctx->upd_max; \
			assert((nb) == ctx->upd_max + 1); \
			tb_tst_lv1_upd *_upds = nh_all(_upd_max * sizeof(tb_tst_lv1_upd)); \
			ns_mem_cpy(_upds, ctx->upds, ctx->upd_max * sizeof(tb_tst_lv1_upd)); \
			nh_fre(ctx->upds, ctx->upd_max * sizeof(tb_tst_lv1_upd)); \
			ctx->upds = _upds; \
			ctx->upd_nbr = (nb); \
			ctx->upd_max = _upd_max; \
		} \
	})

	/* Generate the range. */
	const u64 tck_min = ctx->tck_min = (u64) ((f64) ((f64) prc_min * (f64) prc_cfc));
	const u64 tck_max = ctx->tck_max = tck_min + tck_nbr;
	check(tck_min < tck_max);

	/* Allocate two orderbooks. */
	u64 obk_siz = tck_nbr * sizeof(f64);
	f64 *const obk0 = nh_all(obk_siz);
	f64 *const obk1 = nh_all(obk_siz);

	/* Allocate the bitmap. */
	u64 bmp_siz = tck_nbr;
	u8 *bmp = nh_all(bmp_siz);

	/* Choose the initial tick. */
	ctx->gen_sed = ns_hsh_mas_gen(ctx->sed);
	ctx->tck_cur = ctx->tck_prv = ns_hsh_u64_rng(ctx->gen_sed, tck_min, tck_max + 1, 1);
	ctx->gen_idx = 0;

	/* Generate the initial state. */
	f64 *obk_prv = obk0;
	f64 *obk_cur = obk1;
	for (u64 i = 0; i < tck_nbr; i++) obk_prv[i] = 0;
	_obk_upd(ctx, obk_prv);
	ns_mem_cpy(obk_cur, obk_prv, obk_siz);

	/* Allocate the bid/ask arrays. */
	ctx->bid_arr = nh_all(unt_nbr * sizeof(u64));
	ctx->ask_arr = nh_all(unt_nbr * sizeof(u64));

	/* Allocate the heatmap and its initial copy. */
	const u64 hmp_nbr = unt_nbr * tck_nbr; 
	ctx->hmp = nh_all(hmp_nbr * sizeof(f64));
	ctx->hmp_ini = nh_all(hmp_nbr * sizeof(f64));
	ns_mem_rst(ctx->hmp, hmp_nbr * sizeof(f64));
	ns_mem_rst(ctx->hmp_ini, hmp_nbr * sizeof(f64));

	{
		/* Verify that each tick was accounted for, determine
		 * the current best bid. */
		u64 bst_bid = 0;
		u64 bst_ask = (u64) -1;
		for (u64 idx = 0; idx < tck_nbr; idx++) {
			const f64 vol = obk_cur[idx];
			if (!vol) continue;
			const u8 is_bid = vol < 0;
			const u64 tck_idx = tck_min + idx;
			assert(tck_idx);
			assert(tck_idx != (u64) -1);
			if (is_bid) {
				assert(bst_ask == (u64) -1); // Bid found at tick below first ask.
				if (bst_bid < tck_idx) bst_bid = tck_idx;
			} else {
				if (bst_ask != (u64) -1) bst_ask = tck_idx;
			}
		}
		assert(bst_bid < bst_ask);
		ctx->bid_arr[0] = bst_bid;
		ctx->ask_arr[0] = bst_ask;
	}

	/* Tick update offsets. */
	static u64 prms[8] = {
		1,
		3,
		19,
		31,
		59,
		241,
		701,
		947
	};
	u8 prm_idx = 0;

	/* Generate as many updates as requested. */ 
	u64 tim_lst = tim_stt;
	u64 tim_cur = tim_lst;
	const u64 tim_end = tim_stt + tim_inc * tim_stp * unt_nbr;
	assert(tim_stt == tim_cur);
	assert(tim_cur < tim_end);
	assert(!(tim_stt % (tim_stp * tim_inc)));
	assert(!(tim_end % (tim_stp * tim_inc)));
	u64 itr_idx = 0;
	#define TIM_TO_AID(tim) ((((tim) - tim_stt)) / (aid_wid))
	while (tim_cur < tim_end) {
		debug("itr %U.\n", itr_idx);

		/* Periodically let some time pass, up to 10 columns. */
		if (!(itr_idx % skp_prd)) {

			/* Determine how much time should pass. */
			u64 skp_nb = ns_hsh_u8_rng(ctx->gen_sed, 1, 30, 1);
			if (ctx->obk_rst) {
				SAFE_INCR(ctx->obk_rst_dly);
				if (skp_nb > 20) {
					SAFE_INCR(ctx->obk_rst_skp);
				}
			}

			/* Iteratively add the current orderbook to
			 * the heatmap and add time increment. */
			while (skp_nb--) {
				const u64 aid_cur = TIM_TO_AID(tim_cur);
				const u64 aid_nxt = TIM_TO_AID(tim_cur + tim_stp);
				assert(aid_cur < unt_nbr);
				assert(aid_nxt < unt_nbr);
				debug("SKP %U.\n", aid_cur);
				_hmp_add(ctx, obk_cur, aid_cur, aid_nxt);
				tim_cur += tim_inc;
				assert(tim_cur <= tim_end);
				if (tim_cur == tim_end) {
					goto out;
				}
			}

		}

		/* Enlarge and propagate the bid/ask curve. */
		check(tim_cur != 0);
		const u64 aid_lst = TIM_TO_AID(tim_lst);
		const u64 aid_prv = TIM_TO_AID(tim_cur - 1);
		const u64 aid_cur = TIM_TO_AID(tim_cur);
		const u64 aid_nxt = TIM_TO_AID(tim_cur + tim_stp);
		assert(aid_lst < unt_nbr);
		assert(aid_prv < unt_nbr);
		assert(aid_nxt < unt_nbr);
		assert(aid_cur < unt_nbr);
		if (aid_lst != aid_prv) {
			for (u64 idx = aid_prv + 1; idx <= aid_lst; idx++) {
				ctx->bid_arr[idx] = ctx->bid_arr[aid_prv];
				ctx->ask_arr[idx] = ctx->ask_arr[aid_prv];
			}
		}
		if (aid_prv != aid_cur) {
			ctx->bid_arr[aid_cur] = 0;
			ctx->ask_arr[aid_cur] = (u64) -1;
		}
		tim_lst = tim_cur;

		/* Generate the new current tick. */
		_tck_upd(ctx);

		/* Generate a new orderbook state. */
		_obk_upd(ctx, obk_cur);

		/* Traverse the orderbooks in a different order. */
		ns_mem_rst(bmp, bmp_siz);
		u64 prm = prms[prm_idx];
		check(prm % tck_nbr);
		check((prm == 1) || (tck_nbr % prm));
		for (u64 idx = 0; idx < tck_nbr; idx++) {

			/* Reorder. */
			const u64 tck_idx = (itr_idx + prm * idx) % tck_nbr; 

			/* Report tick processed. */
			assert(!bmp[tck_idx]);
			bmp[tck_idx] = 1;

			/* Read prev and current volume. */
			const f64 vol_prv = obk_prv[tck_idx];
			const f64 vol_cur = obk_cur[tck_idx];
			if (vol_prv == vol_cur) continue;
			obk_prv[tck_idx] = vol_cur;

			/* If required, generate a tick update. */ 
			u64 upd_nbr = ctx->upd_nbr;
			_upd_rsz(upd_nbr + 1);
			check(upd_nbr < ctx->upd_max);
			ctx->upds[upd_nbr].tim = tim_cur;	
			ctx->upds[upd_nbr].val = TCK_TO_PRC(tck_idx, prc_min, prc_stp);
			ctx->upds[upd_nbr].vol = vol_cur;
			ctx->upd_nbr++;
			
		}
		
		/* Verify that each tick was accounted for, determine
		 * the current best bid. */
		{
			u64 bst_bid = 0;
			u64 bst_ask = (u64) -1;
			for (u64 idx = 0; idx < tck_nbr; idx++) {
				assert(bmp[idx] == 1);
				const f64 vol = obk_cur[idx];
				assert(obk_prv[idx] == vol);
				if (!vol) continue;
				const u8 is_bid = vol < 0;
				const u64 tck_idx = tck_min + idx;
				assert(tck_idx);
				assert(tck_idx != (u64) -1);
				if (is_bid) {
					assert(bst_ask == (u64) -1); // Bid found at tick below first ask.
					if (bst_bid < tck_idx) bst_bid = tck_idx;
				} else {
					if (bst_ask != (u64) -1) bst_ask = tck_idx;
				}
			}
			assert(bst_bid < bst_ask);

			/* Incorporate the best bid and best ask. */
			if (ctx->bid_arr[aid_cur] < bst_bid) ctx->bid_arr[aid_cur] = bst_bid;
			if (ctx->ask_arr[aid_cur] > bst_ask) ctx->ask_arr[aid_cur] = bst_ask;
		}

		/* Update the number of remaining updates. */
		assert(ctx->upd_nbr <= ctx->upd_max);

		/* Incorporate the current column to the heatmap,
		 * update the current time. */
		debug("ADD %U.\n", aid_cur);
		_hmp_add(ctx, obk_cur, aid_cur, aid_nxt);
		tim_cur += tim_inc;
		itr_idx++;

	}
	out:;
	assert(tim_cur == tim_end);

	/* Compute the actual average of hashmap cells. */
	for (u64 hmp_idx = hmp_nbr; hmp_idx--;) {
		ctx->hmp[hmp_idx] /= (f64) tim_stp;
	}

	/* Release. */
	nh_fre(obk0, obk_siz);
	nh_fre(obk1, obk_siz);
	nh_fre(bmp, bmp_siz);

	/* Transmit generation data. */
	assert(tim_cur > tim_stt);
	assert(tim_cur - tim_inc >= tim_stt);
	ctx->tim_end = tim_cur - tim_inc;

}

/*
 * Free all generated tick data.
 */
static inline void _tck_del(
	tb_tst_lv1_ctx *ctx
)
{
	nh_fre(ctx->upds, ctx->upd_max * sizeof(tb_tst_lv1_upd));
	nh_fre(ctx->bid_arr, ctx->unt_nbr * sizeof(u64));
	nh_fre(ctx->ask_arr, ctx->unt_nbr * sizeof(u64));
	const u64 hmp_nbr = ctx->unt_nbr * ctx->tck_nbr; 
	nh_fre(ctx->hmp, hmp_nbr * sizeof(f64));
	nh_fre(ctx->hmp_ini, hmp_nbr * sizeof(f64));
}

/*
 * Export tick data.
 */
static inline void _tck_exp(
	tb_tst_lv1_ctx *ctx
)
{
	const u64 siz_x = ctx->unt_nbr;
	const u64 siz_y = ctx->tck_nbr;
	const u64 elm_nb = siz_x * siz_y;
	debug("exporting heatmap of size %Ux%U (volume ref %d) at /tmp/hmp.\n", siz_x, siz_y, ctx->gen_vol);
	nh_res res;
	ns_stg *stg = nh_stg_opn(NH_FIL_ATT_RWC, &res, "/tmp/hmp");	
	ns_stg_rsz(stg, elm_nb * sizeof(f64)); 
	f64 *dst = ns_stg_map(stg, 0, 0, elm_nb * sizeof(f64), NS_STG_ATT_RWS); 
	ns_mem_cpy(dst, ctx->hmp, elm_nb * sizeof(f64));
	ns_stg_syn(stg, dst, elm_nb * sizeof(f64));
	ns_stg_ump(stg, dst, elm_nb * sizeof(f64));
	nh_stg_cls(&res);
}

/*
 * Entrypoint for lv1 tests.
 */
static inline void _tst_lv1(
	nh_tst_sys *sys,
	u64 sed,
	u8 wrk_nb,
	u8 prc
)
{

	/* Generation parameters. */
	#define TIM_INC 1000000
	#define UNT_NBR 1000
	#define TCK_NBR 1024
	#define PRC_MIN 10000
	#define HMP_DIM_TCK 100
	#define HMP_DIM_TIM 100
	#define BAC_SIZ 200
	#define TCK_RAT 100
	const u64 cel_dim_tim = TIM_INC * TIM_STP;
	const f64 prc_stp = (f64) 1 / (f64) TCK_RAT;
	const u64 tim_stt = ns_hsh_u32_rng(sed, (HMP_DIM_TIM + 1) * cel_dim_tim, (u32) -1, cel_dim_tim);

	/* Generation settings. */ 
	nh_all__(tb_tst_lv1_ctx, ctx);
	ctx->sed = sed;
	ctx->unt_nbr = UNT_NBR;
	ctx->tim_stt = tim_stt;
	assert(!(ctx->tim_stt % 100));
	ctx->tim_inc = TIM_INC;
	ctx->prc_min = PRC_MIN;
	ctx->tck_nbr = TCK_NBR;
	ctx->tim_stp = TIM_STP;
	ctx->tck_rat = TCK_RAT;
	ctx->gen_vol = 10000;
	ctx->obk_rst_prd = 100;
	ctx->obk_vol_rst_prd = 43;
	ctx->obk_vol_rst_prd = 47;
	ctx->obk_bid_exc_prd = 8;
	ctx->obk_ask_exc_prd = 9;
	ctx->obk_bid_rst_prd = 23;
	ctx->obk_ask_rst_prd = 27;

	/* Generate tick data. */
	_tck_gen(ctx);

	/* Read post generation parameters. */
	const u64 tim_end = ctx->tim_end;
	const u64 upd_nb = ctx->upd_nbr;
	tb_tst_lv1_upd *upds = ctx->upds;

	/* Allocate buffers to contain two entire orderbooks,
	 * which prevents us from overflowing when adding
	 * updates as the current time (not more than TCK_NBR). */
	u64 *tims = nh_all(2 * TCK_NBR * sizeof(u64));
	f64 *prcs = nh_all(2 * TCK_NBR * sizeof(f64));
	f64 *vols = nh_all(2 * TCK_NBR * sizeof(f64));

	/* Export tick data. */
	_tck_exp(ctx);

	/*
	 * Create a reconstructor covering a 
	 * 100 * 100 heatmap window
	 * and a 200 ticks bid-ask prediction.
	 */
	tb_lv1_hst *hst = tb_lv1_ctr(
		cel_dim_tim,
		TCK_PER_UNT,
		HMP_DIM_TCK,
		HMP_DIM_TIM,
		BAC_SIZ
	);

	/* Set initial prices in groups of 19 by step of 19. */
	assert(tck_nbr & 19);
	assert(19 % tck_nbr);
	for (u32 i = 0; i < TCK_NBR;) {
		u32 j = 0;
		for (; (j < 19) && (i < tck_nbr); i++; j++) {
			u32 ri = (19 * i) % tck_nbr;
			prcs[j] = TCK_TO_PRC(ri, PRC_MIN, prc_stp); 
			vols[j] = ctx->hmp_ini[ri];
		}
		tb_lv1_add(hst, 19, 0, prcs, vols);
	}

	#define STP_NB 24
	static u64 add_stps[STP_NB] = {
		1, 2, 1, 5, 1, 1,
		2, 1, 10, 1, 3, 2,
		5, 1, 43, 1, 1, 2,
		1, 10, 1, 1, 2, 127
	};

	/* Determine the initial times. */
	assert(tim_stt > HMP_DIM_TIM * cel_dim_tim);
	u64 tim_cur = tim_stt;
	u64 tim_cln = tim_stt - HMP_DIM_TIM * cel_dim_tim;
	u64 tim_bac = tim_stt + BAC_SIZ * cel_dim_tim;

	/* Index of orders to be inserted in the heatmap and bac region. */
	u64 cur_idx = 0;
	u64 bac_idx = 0;
	
	/*
	 * The way we generate orderbook updates is fundamentally
	 * prone to causing incoherence in the bid-ask spread.
	 * Since we generate a new orderbook, then traverse ticks
	 * in a randomized way to create updates, then it is possible
	 * that if we only apply half of those updates, we end up
	 * with alternating bid and asks.
	 * We do this anyway because it adds randomness, and because
	 * since all those updates have the same time, they must
	 * be added altogether before processing, which will
	 * allow the processing stage to see all of them, and hence
	 * to see a coherent bid/ask spread.
	 * One nuance is that best bids and best asks are done in
	 * a step by step manner, which causes the intermediate
	 * states to be incoherent.
	 * For this reason, TODO mute debug messages about
	 * alternating bid and ask levels during testing.
	 */

	/* Generate and compare the heatmap and bac. */
	u64 itr_idx = 0;
	while (tim_cur < tim_end) {

#error TODO INCORPORATE UPDATES IN BID/ASK CURVES.
#error WE NEED BID_INI AND ASK_INI LIKE WE DID FOR HMP_INI.
		
		/* Add the required number of updates, keep track of the bac time. */
		const u64 bac_prv;
		const u64 add_nb = add_stps[itr_idx % STP_NB];
		assert(bac_idx + 1 < upd_nb);
		assert(bac_tim < upds[bac_idx + 1].tim); 
		u64 i = 0;
		for (; (i < add_nb) && (bac_idx < upd_nb); (i++), (bac_idx++)) {
			prcs[i] = upds[bac_idx].val;
			vols[i] = upds[bac_idx].vol;
			const u64 tim_nxt = tims[i] = upds[bac_idx].tim;
			assert(tim_bac <= tim_nxt);
			#error TODO INCORPORATE INTO BID_ASK.
		}

		/* Add all updates. */
		tb_lv1_add(hst, i, tims, prcs, vols);

		/* Also add all successor updates at the same time. */
		i = 0;
		while (bac_idx < upd_nb) {
			const u64 tim_nxt = upds[bac_idx].tim; 
			assert(tim_bac <= tim_nxt)
			if (tim_bac != tim_nxt) break;
			tims[i] = tim_nxt;
			prcs[i] = upds[bac_idx].val;
			vols[i] = upds[bac_idx].vol;
			#error TODO INCORPORATE INTO BID_ASK.
			bac_idx++;
			i++;
		}

		/* Add extra updates. */
		if (i) {
			tb_lv1_add(hst, i, tims, prcs, vols);
		}

		/* Process. */
		tb_lv1_prc(hst);

		/* Update the current time and clone time. */
		assert(tim_cur < tim_bac - BAC_SIZ * cel_dim_tim); 
		tim_cur = tim_bac - BAC_SIZ * cel_dim_tim;
		assert(tim_cln < tim_cur - HMP_DIM_TIM * cel_dim_tim);
		tim_cln = tim_cur - HMP_DIM_TIM * cel_dim_tim;

		/* Incorporate all orders < current time in the
		 * heatmap. */
		while (1) {
			const u64 tim_nxt = upds[cur_idx].tim; 
			if (tim_nxt >= tim_cur) break;
			#error TODO INCORPORATE INTO HEATMAP.
			cur_idx++;

		}
		const u64 bac_prv;
		const u64 add_nb = add_stps[itr_idx % STP_NB];
		assert(bac_idx + 1 < upd_nb);
		assert(bac_tim < upds[bac_idx + 1].tim); 
		u64 i = 0;
		for (; (i < add_nb) && (bac_idx < upd_nb); (i++), (bac_idx++)) {
			prcs[i] = upds[bac_idx].val;
			vols[i] = upds[bac_idx].vol;
			const u64 tim_nxt = tims[i] = upds[bac_idx].tim;
			assert(tim_bac <= tim_nxt);
		}

		/* Verify the heatmap. */
#error TODO.

		/* Verify bid-ask curves. */

		/* Cleanup one time out of 10. */
		if (!itr_idx % 20) {
			tb_lv1_cln(hst);
		}

	}	

	/* Free buffers. */ 
	nh_fre(tims, 2 * TCK_NBR * sizeof(u64));
	nh_fre(prcs, 2 * TCK_NBR * sizeof(f64));
	nh_fre(vols, 2 * TCK_NBR * sizeof(f64));

	/* Delete the history. */
	tb_lv1_dtr(hst);

	/* Delete tick data. */
	_tck_del(ctx);

	/* Free settings. */ 
	nh_fre_(ctx);

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
		(0, flg, stg, (stg), "run storage tests."),
		(0, flg, lv1, (lv1), "run storage tests.")
	);
	u32 tst_cnt = 0;
	nh_tst_sys *sys = nh_tst_sys_ctr();
	if (rpr__flg) tst(rpr); 
	if (sgm__flg) tst(sgm, thr_nb, prc); 
	if (stg__flg) tst(stg, thr_nb, prc); 
	if (lv1__flg) tst(lv1, thr_nb, prc); 
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
	tst(lv1, thr_nb, prc);
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

