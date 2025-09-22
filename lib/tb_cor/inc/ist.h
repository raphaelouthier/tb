/* Copyright 2025 Raphael Outhier - confidential - proprietary - no copy - no diffusion. */

#ifndef TB_IST_H
#define TB_IST_H

/*
 * An instrument is something that can be traded.
 * The instrument library identifies instruments in a
 * unique manner which allows their efficient indexing.
 */

/*********
 * Types *
 *********/

types(
	tb_ist,
	tb_ist_sys
);

/**************
 * Structures *
 **************/

/*
 * Instrument classes.
 */

/* Share. */
#define TB_IST_CLS_SHR 0

/* Currency. */
#define TB_IST_CLS_CCY 1

/* Number of classes. */
#define TB_IST_CLS_NB 2

/*
 * Instrument.
 * Once created, an instrument is never fred.
 */
struct tb_ist {

	/* Name. */
	tb_str nam;

	/* Class. */
	u8 cls;

	/* Per-class data. */
	union {

		/* Share data. */
		struct {

			/* Shares instruments of the same system. */
			ns_mapn_str shrs;

			/* Marketplace. */
			tb_mkp *mkp;

			/* Symbol. */
			tb_str sym; 

		} shr;

		/* Currency data. */
		struct {

			/* Currency instruments of the same system. */
			ns_mapn_str ccys;

			/* Marketplace. */
			tb_mkp *mkp;

			/* Currency. */
			tb_ccy *ccy; 

		} ccy;

	};

	/* Usage counter. */
	u32 uctr;

};

/*
 * Instrument tracker.
 */
struct tb_ist_sys {

	/* Lock. */
	nh_spn lck;

	/* Share instruments. */
	ns_map_str shrs;

	/* Currency instruments. */
	ns_map_str ccys;

};

/*******
 * API *
 *******/

/*
 * Deinitialize the instrument tracking system.
 * Deletes all active instruments.
 * Only here for valgrind to get off our backs.
 */
void tb_ist_cln(
	void
);

/*
 * Return a share instrument identifier.
 */
tb_ist *tb_ist_ctr_shr(
	tb_mkp *mkp,
	const char *sym
);

/*
 * Return a currency instrument identifier.
 */
tb_ist *tb_ist_ctr_ccy(
	tb_mkp *mkp,
	tb_ccy *ccy
);

/*
 * Print log data for an instrument.
 */
void tb_ist_log(
	ns_stm *stm,
	va_list *args
);
#define tb_ist_dsc(ist) &tb_ist_log, (ist)

#endif /* TB_IST_H */

