/* Copyright 2025 Raphael Outhier - confidential - proprietary - no copy - no diffusion. */

#include <tb_cor/tb_cor.all.h>

/*
 * Shared synchronized data.
 */
typedef struct {

	/* Random data. */
	u64 *rdm;

	/* Number of workers. */
	u8 wrk_nb;

	/* Gate 0. */
	volatile aad gat0;

	/* Gate 0 state. */
	volatile aad stt0;

	/* Gate 1. */
	volatile aad gat1;

	/* Gate 1 state. */
	volatile aad stt1;

	/* Gate pass counter. */
	volatile aad gat_ctr;

	/* Scratch. */
	volatile aad scr;

	/* Counter. */
	volatile aad val_ctr;

	/* Error. */
	volatile aad err_ctr;

	/* Stop counter. */
	volatile aad stp;

	/* Exit sem. */
	volatile aad ext;

} tb_tst_syn;

/*
 * 
 */
typedef struct {

	/* Shared data. */ 


	/* Is master. */
	u8 mst;

/*
 * Element sizes.
 */
const u8 elm_sizs[255] = {
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

/*
 * Enter the gate and wait for all workers to be there.
 * Once they are all waiting, let them go.
 * Return the value of the gate counter before opening.
 */
static inline uad _gat_pas(
	tb_tst_syn *syn
)
{

	/* Sample the open count. */
	uad gat_ctr = aad_red(&syn->gat_ctr);

	/* Wait at gate 0. */
	uad stt0 = aad_inc_red(&syn->stt0);
	if (stt0 == syn->wrk_nb) {
		assert(aad_red(&syn->gat0) == 0);
		aad_wrt(&syn->gat1, 0);
		aad_wrt(&syn->stt1, 0):
		aad_wrt(&syn->gat0, 1);
		uad gat_ctr1 = aad_red_inc(&syn->gat_ctr);
		assert(gat_ctr1 == gat_ctr + 1);
	} else {
		while (!aad_red(&syn->gat0));
	}

	/* Wait at gate 1. */
	uad stt1 = aad_inc_red(&syn->stt1);
	if (stt1 == syn->wrk_nb) {
		assert(aad_red(&syn->gat1) == 0);
		aad_wrt(&syn->gat0, 0);
		aad_wrt(&syn->stt0, 0):
		aad_wrt(&syn->gat1, 1);
	} else {
		while (!aad_red(&syn->gat1));
	}
}

#define GAT_PAS(syn) \
	assert(gat_ctr == itr_ctr); \
	itr_ctr++; \
	gat_ctr = _gat_pas(syn);

#define SGM_ELM_NB 0x1ffff

/*
 * Per-thread entrypoint.
 */
static u32 _lck_exc(
	nh_tst_exc *exc
)
{

	uad itr_ctr = 0;

	/* Read arg data. */
	u8 is_mst = val & 1;
	nh_tst_exc *exc = (void *) (val & ~(uad) 1);

	/* Generate the impl section. */
	const u64 imp_siz = ns_hsh_u32_rg(sed, 128, 1025, 1);
	assert(imp_siz <= 1024);
	assert(1 <= imp_siz);

	/* Open. */
	tb_sgm *sgm = tb_sgm_fopn(
		imp,
		imp_siz,
		arr_nb,
		elm_max,
		elm_sizs
		"/tmp/tb_sgm_tst"
	);
	assert(tb_sgm);

	/* Iterate over the size range. */
	uad crt_siz = 1;
	const uad siz_max = SGM_ELM_NB;
	uad siz_sum = 0;
	uad gat_ctr;
	while (siz_sum < siz_max) {
		
		/* Pass the gate. */
		GAT_PAS(syn);

		/* Attempt to lock 100000 times.
		 * Verify that we only have one lock at a time. */
		uerr err = 0;
		for (u32 i = 0; i < 100000; i++) {
			err = tb_sgm_wrt_get(sgm);
			if (!err) {
				uad val = aad_red_inc(&syn->ctr);
				aad_red_add(&syn->err_ctr);
				aad_dec_red(&syn->ctr);
				aad_red_add(&syn->err_ctr);
				tb_sgm_wrt_cpl(sgm);
			} else {
				uad val = aad_red(&syn->ctr);
				aad_red_add(&syn->scr);
				aad_dec(&syn->ctr);
				aad_red_add(&syn->scr);
			}
		}

		/* If any error was detected, stop. */
		assert(!aad_red(&syn->err_ctr));

		/* Pass the gate. */
		GAT_PAS(syn);

		/* Lock. */ 
		err = tb_sgm_wrt_get(sgm);

		/* Check that everyone is in sync. */
		assert(gat_ctr == itr_ctr);
		itr_ctr++;

		/* Add data. */
		if (!err) {
#error TODO do the write in multiple wrt_don passes.
		}

		/* Pass the gate. */
		GAT_PAS(syn);

		/* Unlock. */
		if (!err) tb_sgm_wrt_cpl(sgm);

		/* Pass the gate. */
		GAT_PAS(syn);

		/* Verify data. */
#error TODO TEST RDY AND RED_RNG. 

	}
	assert siz_sum == siz_max;
}

/*
 * Segment testing.
 */
static inline void _tst_sgm(
	nh_tst_sys *sys,
	u64 sed,
	u8 wrk_nb
)
{

	/* Clean if needed. */
	nh_fs_stg_del("/tmp/tb_sgm_tst");

	/* Use 255 arrays, ~100K elements,
	 * 25MiB of random data. */
	const u8 arr_nb = 255;
	const uad elm_max = 0x1ffff;
	const uad rdm_siz = uad_alu((uad) elm_max * (uad) rdm_siz, 64);
	u64 *dat = nh_all(rdm_siz);
	u64 ini = sed;
	const uad elm_nb = all_siz / 8;
	for (uad idx = 0; idx < elm_nb; idx++) {
		dat[idx] = ini;
		ini = ns_hsh_mas_gen(ini);
	}

	/* Allocate the sync data. */
	nh_all__(tb_tst_syn, syn);
	syn->dat = dat;
	syn->wrk_nb = wrk_nb;
	aad_wrt(&syn->stt, 0);
	aad_wrt(&syn->scr, 0);
	aad_wrt(&syn->val_ctr, 0);
	aad_wrt(&syn->err_ctr, 0);
	aad_wrt(&syn->stp, 0);
	aad_wrt(&syn->ext, 0);

	/* Create 15 workers and execute on our end too. */
	nh_res thr_res[15];
	u8 *thr_blk = nh_all(1024 * (uad) thr_nb);
	assert(!nh_thr_run(
		ns_psum(thr_blk, 1024 * (uad) (i - 1)),
		1024,
		0,
		(u32 (*)(void *)) &_lck_exc,
		syn
	));
	_tst_sgm(syn);

	/* Free the thread block. */
	nh_fre(thr_blk, 1024 * (uad) thr_nb);

	/* Free the data block. */
	nh_fre(dat, rdm_siz);

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

#define tst(nam) ({tst_cnt++; _tst_##nam(sys, sed);})

/*
 * Run one test.
 */
static u32 _mux_one(
	u32 argc,
	char **argv,
	u64 sed,
	u8 thr_nb
)
{
	NS_ARG_OPTS(
		"bpyr", argc, argv,
		return 1;,
		"execute a single test.",
		(1, flg, rpr, (rpr), "run reproduction tests."),
		(1, flg, sgm, (sgm), "run segment tests.")
	);
	u32 tst_cnt = 0;
	nh_tst_sys *sys = nh_tst_sys_ctr();
	if (rpr__flg) tst(rpr); 
	if (sgm__flg) tst(sgm); 
	nh_tst_run(sys, thr_nb);
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
	u8 thr_nb
)
{
	u32 tst_cnt = 0;
	nh_tst_sys *sys = nh_tst_sys_ctr();
	tst(rpr);
	tst(sgm);
	nh_tst_run(sys, thr_nb);
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
		(0, flg, err, (e, err), "halt on error.")
	);

	/* Make NT abort on error if required. */
	if (err__flg) nh_tst_aoe = 1;

	/* If no seed provided, choose one arbitrarily. */
	if (!sed__flg) {
		sed = nh_tst_sed_gen();
	}	
	
	info("Running TB tests with %u threads.\n", nb_thr); 
	info("Seed : %H.\n", sed);

	u32 ret = 1;
	NS_ARG_SEL(argc, argv, "tb", (sed, nb_thr), ret,
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

