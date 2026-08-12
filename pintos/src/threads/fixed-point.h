#ifndef THREADS_FIXED_POINT_H
#define THREADS_FIXED_POINT_H

/* Fixed-point real arithmetic, used by the advanced (4.4BSD)
   scheduler to represent load_avg and recent_cpu without using
   floating point in the kernel.

   We use a 17.14 signed fixed-point format: the low
   FP_SHIFT_AMOUNT bits are the fractional part, and the rest
   (including the sign bit) are the integer part.  A fixed_t is
   just an int -- P.Q, with P = 17, Q = 14, in the pintos manual's
   terminology. */

typedef int fixed_t;

#define FP_SHIFT_AMOUNT 14

/* Convert a value to fixed-point. */
#define FP_CONST(A) ((fixed_t) ((A) << FP_SHIFT_AMOUNT))

/* Add two fixed-point values. */
#define FP_ADD(A, B) ((A) + (B))

/* Add a fixed-point value A and an int value B. */
#define FP_ADD_MIX(A, B) ((A) + ((B) << FP_SHIFT_AMOUNT))

/* Subtract two fixed-point values. */
#define FP_SUB(A, B) ((A) - (B))

/* Subtract an int value B from a fixed-point value A. */
#define FP_SUB_MIX(A, B) ((A) - ((B) << FP_SHIFT_AMOUNT))

/* Multiply a fixed-point value A by an int value B. */
#define FP_MULT_MIX(A, B) ((A) * (B))

/* Divide a fixed-point value A by an int value B. */
#define FP_DIV_MIX(A, B) ((A) / (B))

/* Multiply two fixed-point values.  The multiplication is done in
   64 bits to avoid overflow, since both operands already carry
   FP_SHIFT_AMOUNT fractional bits and their product would carry
   2 * FP_SHIFT_AMOUNT bits before being scaled back down. */
#define FP_MULT(A, B) ((fixed_t) (((int64_t) (A)) * (B) >> FP_SHIFT_AMOUNT))

/* Divide two fixed-point values, likewise done in 64 bits so that
   the numerator can be pre-scaled up without losing bits. */
#define FP_DIV(A, B) \
  ((fixed_t) ((((int64_t) (A)) << FP_SHIFT_AMOUNT) / (B)))

/* Get the integer part (truncated toward zero) of a fixed-point
   value. */
#define FP_INT_PART(A) ((A) >> FP_SHIFT_AMOUNT)

/* Round a fixed-point value to the nearest integer. */
#define FP_ROUND(A)                                                \
  ((A) >= 0                                                        \
   ? ((A) + (1 << (FP_SHIFT_AMOUNT - 1))) >> FP_SHIFT_AMOUNT       \
   : ((A) - (1 << (FP_SHIFT_AMOUNT - 1))) >> FP_SHIFT_AMOUNT)

#endif /* threads/fixed-point.h */
