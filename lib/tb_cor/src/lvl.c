/* Copyright 2025 Raphael Outhier - confidential - proprietary - no copy - no diffusion. */

#include <tb_cor/tb_cor.all.h>

/******************************
 * Arrays for dynamic queries *
 ******************************/

const u64 *const (tb_lvl_to_rgn_sizs[3]) = {
	(u64 []) {TB_LVL_RGN_SIZ_SYN},
	(u64 []) {TB_LVL_RGN_SIZ_SYN, TB_LVL_RGN_SIZ_OBS},
	(u64 []) {TB_LVL_RGN_SIZ_SYN, TB_LVL_RGN_SIZ_OBS},
};

const u8 tb_lvl_to_arr_nbr[TB_LVL_NB] = {
	5, /* time, bid, ask, vol, val. */
	3, /* time, prc, vol. */
	6, /* time, ord_id, trd_id, ord_typ, ord_val, ord_vol. */
};

const u8 *const (tb_lvl_to_arr_elm_sizs[TB_LVL_NB]) = {
	(u8 []) {8, 8, 8, 8, 8},
	(u8 []) {8, 8, 8},
	(u8 []) {8, 8, 8, 1, 8, 8},
};

