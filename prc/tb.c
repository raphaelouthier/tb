/* Copyright 2025 Raphael Outhier - confidential - proprietary - no copy - no diffusion. */

#include <tb_tst/tb_tst.all.h>

/**********************
 * Reproduction tests *
 **********************/

/*
 * Entrypoint for tests that already failed in the past.
 */
static inline void tb_tst_rpr(
	nh_tst_sys *sys,
	u64 sed
) {}

/**********
 * Muxers *
 **********/

#define tst(nam, ...) ({tst_cnt++; tb_tst_##nam(sys, sed, ##__VA_ARGS__); nh_tst_run(sys, thr_nb);})

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
		(0, flg, obk, (obk), "run orderbook computation tests."),
		(0, flg, lvl, (lvl), "run level constants check tests."),
		(0, flg, lv1, (lv1), "run level 1 reconstruction tests.")
	);
	u32 tst_cnt = 0;
	nh_tst_sys *sys = nh_tst_sys_ctr();
	if (rpr__flg) tst(rpr); 
	if (sgm__flg) tst(sgm, thr_nb, prc); 
	if (stg__flg) tst(stg, thr_nb, prc); 
	if (obk__flg) tst(obk, thr_nb, prc); 
	if (lvl__flg) tst(lvl, thr_nb, prc); 
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
	tst(obk, thr_nb, prc);
	tst(lvl, thr_nb, prc);
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
	NS_ARG_EXTR(
		"tb", argc, argv,
		return 1;,
		"tb entrypoint",
		(0, flg, vrb, (v), "Enable logs.")
	);
	if (vrb__flg) {
		tb_cor_log_flg = 1;	
	}
	u32 ret = 0;
	NS_ARG_SEL(argc, argv, "tb", , ret,
		("tst", _tst_main, "run tests.")
	);
	return ret;
}

