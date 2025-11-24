/* Copyright 2025 Raphael Outhier - confidential - proprietary - no copy - no diffusion. */

#include <tb_tst/tb_tst.all.h>

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
void tb_tst_sgm(
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
