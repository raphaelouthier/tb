/* Copyright 2025 Raphael Outhier - confidential - proprietary - no copy - no diffusion. */

#ifndef TB_COR_MA_H
#define TB_COR_MA_H

/*
 * The money amount library handles the representation
 * of an amount of money whose track should be kept,
 * and that should not be subject to numerical
 * approximations.
 * Instead of using a floating point representation,
 * it uses a s64 as the integer part and a u16 as the
 * decimal part.
 */

/*********
 * Types *
 *********/

types(
	sp_ma
);

/**************
 * Structures *
 **************/

/* Resolution of the decimal part. */
#define TB_MA_DEC_RES 1000

/*
 * A money amount is composed of :
 * - a signed integer part.
 * - a decimal part with w resolution of 4 digits.
 */
struct sp_ma {

	/* Integer part. */
	s64 itg;

	/* Decimal part. */
	u16 dec;

};

/*******
 * API *
 *******/

/*
 * Ma constructor from components.
 */
static inline sp_ma sp_ma_int(
	s64 itg,
	u16 dec
) {return (sp_ma) {itg, dec};}

/*
 * Ma constructor from float.
 */
static inline sp_ma sp_ma_flt(
	f64 val
)
{
	val *= (f64) TB_MA_DEC_RES;
	if (val >= 0.) {
		s64 v = (s64) (val + 0.5);
		return (sp_ma) {v / TB_MA_DEC_RES, (u16) (((u64) v) % TB_MA_DEC_RES)};	
	} else {
		u64 v = (u64) -(s64) (val - 0.5);
		u64 itg = v / TB_MA_DEC_RES;
		u16 dec = (u16) (v % TB_MA_DEC_RES);
		if (dec) {
			itg++;
			dec = (u16) ((u16) TB_MA_DEC_RES - dec);
		}
		return (sp_ma) {- (s64) itg, dec}; 
	}
}

/*
 * Export to float.
 */
static inline f64 sp_ma_to_flt(
	sp_ma ma
) {
	return (f64) ma.itg + (((f64) ma.dec) / TB_MA_DEC_RES);
}

/*
 * Return a descriptor of @ma using @gen.
 */
ct_val *sp_ma_ct_exp(
	sp_ma *ma
);

/*
 * Import a money amount at @ma from @val and return 0.
 * If the import fails, log and return 1.
 */ 
uerr sp_ma_ct_imp(
	sp_ma *ma,
	ct_val *val
);

/*
 * Print log data for @ma.
 */
void sp_ma_log(
	ns_stm *stm,
	va_list *args
);
#define sp_ma_dsc(ma) &sp_ma_log, ((sp_ma []) {ma})

/*
 * Return a null amount.
 */
static inline sp_ma sp_ma_0(
	void
) {return (sp_ma) {0, 0};}

/*
 * Return the sign of @a0 - @a1 :
 * - if @a0 is superior to @a1, return 1.
 * - if @a0 is inferior to @a1, return -1
 * - if @a0 is equal to @a1, return 0.
 */
static inline s8 sp_ma_cmp(
	sp_ma a0,
	sp_ma a1
) {
	if (a0.itg != a1.itg) return (a0.itg > a1.itg) ? 1 : -1;
	else if (a0.dec != a1.dec) return (a0.dec > a1.dec) ? 1 : -1;
	else return 0;
}
static inline u8 sp_ma_g(
	sp_ma a0,
	sp_ma a1
) {return sp_ma_cmp(a0, a1) == 1;}
static inline u8 sp_ma_ge(
	sp_ma a0,
	sp_ma a1
) {return sp_ma_cmp(a0, a1) != -1;}
static inline u8 sp_ma_s(
	sp_ma a0,
	sp_ma a1
) {return sp_ma_cmp(a0, a1) == -1;}
static inline u8 sp_ma_se(
	sp_ma a0,
	sp_ma a1
) {return sp_ma_cmp(a0, a1) != 1;}
static inline u8 sp_ma_p(
	sp_ma ma
) {return sp_ma_g(ma, sp_ma_0());}
static inline u8 sp_ma_pz(
	sp_ma ma
) {return sp_ma_ge(ma, sp_ma_0());}
static inline u8 sp_ma_n(
	sp_ma ma
) {return sp_ma_s(ma, sp_ma_0());}
static inline u8 sp_ma_nz(
	sp_ma ma
) {return sp_ma_se(ma, sp_ma_0());}
static inline u8 sp_ma_e(
	sp_ma a0,
	sp_ma a1
) {return sp_ma_cmp(a0, a1) == 0;}
static inline u8 sp_ma_z(
	sp_ma ma
) {return sp_ma_e(ma, sp_ma_0());}

/*
 * Return @a0 - a1.
 */
static inline sp_ma sp_ma_dif(
	sp_ma a0,
	sp_ma a1
)
{
	s32 dec = (s32) a0.dec - (s32) a1.dec;
	s64 itg = a0.itg - a1.itg;
	if (dec < 0) {
		itg--;
		dec += TB_MA_DEC_RES;
		check(dec > 0);
	}
	check(dec >= 0, dec < (u16) TB_MA_DEC_RES);
	return (sp_ma) {itg, (u16) dec};
}

/*
 * Return 0 - @a0.
 */
static inline sp_ma sp_ma_neg(
	sp_ma a0
) {return sp_ma_dif(sp_ma_0(), a0);}

/*
 * Return @a0 + a1.
 */
static inline sp_ma sp_ma_sum(
	sp_ma a0,
	sp_ma a1
)
{
	u32 dec = (u32) a0.dec + (u32) a1.dec;
	s64 itg = a0.itg + a1.itg;
	if (dec >= TB_MA_DEC_RES) {
		itg++;
		dec -= TB_MA_DEC_RES;
		check(dec < TB_MA_DEC_RES);
	}
	return (sp_ma) {itg, (u16) dec};
}

/*
 * Return @ma * @coeff.
 */
static inline sp_ma sp_ma_mul(
	sp_ma ma,
	u32 coeff
)
{
	u64 dec = (u64) ma.dec * (u64) coeff;
	return (sp_ma) {ma.itg * (s64) coeff + (s64) (dec / (u64) TB_MA_DEC_RES), (u16) (dec % (u64) TB_MA_DEC_RES)};
}
#endif /* TB_COR_MA_H */
