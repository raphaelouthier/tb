/* Copyright 2025 Raphael Outhier - confidential - proprietary - no copy - no diffusion. */

#include <tb_cor/tb_cor.all.h>


/*
 * Segment testing.
 */
static inline void _tst_sgm(
	nh_tst_sys *sys,
	u64 sed
)
{
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

