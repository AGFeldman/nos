/*! \file fixed-point.h
 *
 * Declarations and implementation for fixed-point real arithmetic.
 */

#ifndef FIXED_POINT_H
#define FIXED_POINT_H

/* Use a 17.14 fixed-point number representation */
#define FIXED_POINT_F 16384  /* This is 2^14 */

typedef int FPNUM;  /* Fixed-point number */


FPNUM fixed_point_itofp(int n);
int fixed_point_fptoi(FPNUM x);
int fixed_point_fptoi_truncate(FPNUM x);
FPNUM fixed_point_add(FPNUM x, FPNUM y);
FPNUM fixed_point_sub(FPNUM x, FPNUM y);
FPNUM fixed_point_fp_plus_i(FPNUM x, int n);
FPNUM fixed_point_fp_minus_i(FPNUM x, int n);
FPNUM fixed_point_mul(FPNUM x, FPNUM y);
FPNUM fixed_point_fp_times_i(FPNUM x, int n);
FPNUM fixed_point_div(FPNUM x, FPNUM y);
FPNUM fixed_point_frac(int n, int m);
FPNUM fixed_point_fp_div_i(FPNUM x, int n);


/* Returns the fixed-point representation of integer n. */
inline FPNUM fixed_point_itofp(int n) {
    return n * FIXED_POINT_F;
}

/* Returns the integer represented by the fixed point number x, rounding to
 * the nearest integer. */
inline int fixed_point_fptoi(FPNUM x) {
    if (x >= 0) {
        return (x + FIXED_POINT_F / 2) / FIXED_POINT_F;
    }
    return (x - FIXED_POINT_F / 2) / FIXED_POINT_F;
}

/* Returns the integer represented by the fixed point number x, rounding down
 * (truncating). */
inline int fixed_point_fptoi_truncate(FPNUM x) {
    int res = x / FIXED_POINT_F;
    if (res < 0) {
        res++;
    }
    return res;
}

/* Returns the sum of two fixed-point numbers as a fixed-point number. */
inline FPNUM fixed_point_add(FPNUM x, FPNUM y) {
    return x + y;
}

/* Returns the difference x-y of two fixed-point numbers as a fixed-point
 * number. */
inline FPNUM fixed_point_sub(FPNUM x, FPNUM y) {
    return x - y;
}

/* Returns the sum of a fixed-point number x and an integer n.
 * Returns the sum as a fixed-point number. */
inline FPNUM fixed_point_fp_plus_i(FPNUM x, int n) {
    return x + n * FIXED_POINT_F;
}

/* Returns the difference x-n of a fixed-point number x and an integer n.
 * Returns the difference as a fixed-point number. */
inline FPNUM fixed_point_fp_minus_i(FPNUM x, int n) {
    return x - n * FIXED_POINT_F;
}

/* Returns the product of two fixed-point numbers as a fixed-point number. */
inline FPNUM fixed_point_mul(FPNUM x, FPNUM y) {
    return ((int64_t) x ) * y / FIXED_POINT_F;
}

/* Returns the product of a fixed-point number x and an integer n.
 * Returns the product as a fixed-point number. */
inline FPNUM fixed_point_fp_times_i(FPNUM x, int n) {
    return x * n;
}

/* Returns the quotient x/y of two fixed point numbers as a fixed-point number.
 * */
inline FPNUM fixed_point_div(FPNUM x, FPNUM y) {
    return ((int64_t) x) * FIXED_POINT_F / y;
}

/* Returns the fraction n/m as a fixed-point number.
 * n and m are integers. */
inline FPNUM fixed_point_frac(int n, int m) {
    return ((int64_t) n) * FIXED_POINT_F / m;
}

/* Returns the quotient x/n of a fixed-point number x and an integer n.
 * Returns the quotient as a fixed-point number. */
inline FPNUM fixed_point_fp_div_i(FPNUM x, int n) {
    return x / n;
}

#endif /* threads/fixed-point.h */
