/* Copyright 2025 Raphael Outhier - confidential - proprietary - no copy - no diffusion. */

#ifndef TB_TST_UTL_H
#define TB_TST_UTL_H

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

/* Yeah sorry blame me... */
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>

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

#endif /* TB_TST_UTL_H */
