/* Copyright 2025 Raphael Outhier - confidential - proprietary - no copy - no diffusion. */

#ifndef TB_LOG_H
#define TB_LOG_H

/*
 * Master log flag.
 * Enable all logs.
 */
extern u8 tb_log_all;

/*
 * Core log flag.
 */
extern u8 tb_cor_log_flg;

/*
 * Initialize the log environment for a component.
 */
#define tb_cmp_log_dec(cmp) extern u8 tb_##cmp##_log_flg;
#define tb_cmp_log_def(cmp) u8 tb_##cmp##_log_flg = 0;
#define tb_lib_log_dec(cmp, lib) extern u8 tb_##cmp##_##lib##_log_flg;
#define tb_lib_log_def(cmp, lib) u8 tb_##cmp##_##lib##_log_flg = 0;

/*
 * Set a log flag.
 */

/*
 * Log if the log flag is set.
 */
#ifdef TB_NOLOG
#define tb_log_cmp_log(cmp, ...) ()
#define tb_log_lib_log(cmp, lib, ...) ()
#else
#define tb_log_cmp(cmp, ...) ((tb_log_all || tb_##cmp##_log_flg) ? debug("["#cmp"] " __VA_ARGS__) : (void) 0 )
#define tb_log_lib(cmp, lib, ...) ((tb_log_all || tb_##cmp##_log_flg || tb_##cmp##_##lib##_log_flg) ? debug("["#cmp"] ["#lib"] " __VA_ARGS__) : (void) 0 )
#endif

#endif /* TB_LOG_H */

