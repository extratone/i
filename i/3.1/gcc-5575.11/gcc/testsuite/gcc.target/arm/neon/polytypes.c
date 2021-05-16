/* Check that NEON polynomial vector types are suitably incompatible with
   integer vector types of the same layout.  */

/* { dg-do compile } */
/* { dg-require-effective-target arm_neon_ok } */
/* { dg-options "-mfpu=neon -mfloat-abi=softfp -fno-lax-vector-conversions" } */

#include <arm_neon.h>

void s64_8 (int8x8_t a) {}
void u64_8 (uint8x8_t a) {}
void p64_8 (poly8x8_t a) {}
void s64_16 (int16x4_t a) {}
void u64_16 (uint16x4_t a) {}
void p64_16 (poly16x4_t a) {}

void s128_8 (int8x16_t a) {}
void u128_8 (uint8x16_t a) {}
void p128_8 (poly8x16_t a) {}
void s128_16 (int16x8_t a) {}
void u128_16 (uint16x8_t a) {}
void p128_16 (poly16x8_t a) {}

void foo ()
{
  poly8x8_t v64_8;
  poly16x4_t v64_16;
  poly8x16_t v128_8;
  poly16x8_t v128_16;

  s64_8 (v64_8); /* { dg-error "use -flax-vector-conversions.*incompatible type for argument 1 of 's64_8'" } */
  u64_8 (v64_8); /* { dg-error "incompatible type for argument 1 of 'u64_8'" } */
  p64_8 (v64_8);

  s64_16 (v64_16); /* { dg-error "incompatible type for argument 1 of 's64_16'" } */
  u64_16 (v64_16); /* { dg-error "incompatible type for argument 1 of 'u64_16'" } */
  p64_16 (v64_16);

  s128_8 (v128_8); /* { dg-error "incompatible type for argument 1 of 's128_8'" } */
  u128_8 (v128_8); /* { dg-error "incompatible type for argument 1 of 'u128_8'" } */
  p128_8 (v128_8);

  s128_16 (v128_16); /* { dg-error "incompatible type for argument 1 of 's128_16'" } */
  u128_16 (v128_16); /* { dg-error "incompatible type for argument 1 of 'u128_16'" } */
  p128_16 (v128_16);
}

