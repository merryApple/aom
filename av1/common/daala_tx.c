#include "av1/common/daala_tx.h"
#include "av1/common/odintrin.h"

/* clang-format off */

#define OD_RSHIFT1(_a) (((_a) + ((_a) < 0)) >> 1)
#define OD_PAVG(_a, _b) (((_a) + (_b) + 1) >> 1)

/* TODO: Daala DCT overflow checks need to be ported as a later test */
# if defined(OD_DCT_CHECK_OVERFLOW)
# else
#  define OD_DCT_OVERFLOW_CHECK(val, scale, offset, idx)
# endif

#define OD_FDCT_2_PR(p0, p1) \
  /* Embedded 2-point orthonormal Type-II fDCT. */ \
  do { \
    /* 13573/32768 ~= Tan[pi/8] ~= 0.414213562373095 */ \
    OD_DCT_OVERFLOW_CHECK(p1, 13573, 16384, 100); \
    p0 -= (p1*13573 + 16384) >> 15; \
    /* 5793/8192 ~= Sin[pi/4] ~= 0.707106781186547 */ \
    OD_DCT_OVERFLOW_CHECK(p0, 5793, 4096, 101); \
    p1 += (p0*5793 + 4096) >> 13; \
    /* 3393/8192 ~= Tan[pi/8] ~= 0.414213562373095 */ \
    OD_DCT_OVERFLOW_CHECK(p1, 3393, 4096, 102); \
    p0 -= (p1*3393 + 4096) >> 13; \
  } \
  while (0)

#define OD_IDCT_2_PR(p0, p1) \
  /* Embedded 2-point orthonormal Type-II iDCT. */ \
  do { \
    /* 3393/8192 ~= Tan[pi/8] ~= 0.414213562373095 */ \
    p0 += (p1*3393 + 4096) >> 13; \
    /* 5793/8192 ~= Sin[pi/4] ~= 0.707106781186547 */ \
    p1 -= (p0*5793 + 4096) >> 13; \
    /* 13573/32768 ~= Tan[pi/8] ~= 0.414213562373095 */ \
    p0 += (p1*13573 + 16384) >> 15; \
  } \
  while (0)

#define OD_FDCT_2_FLAT(p0, p1) \
  /* Embedded 2-point orthonormal Type-II fDCT with flattened rotations. */ \
  do { \
    int t_; \
    t_ = (p0 - p1 + 1) >> 1; \
    /* 46341/32768 ~= 2*Sin[Pi/4] = 1.4142135623730951 */ \
    p0 = (p1*46341 + 16384) >> 15; \
    /* 46341/32768 ~= 2*Cos[Pi/4] = 1.4142135623730951 */ \
    p1 = (t_*46341 + 16384) >> 15; \
    p0 += p1; \
  } \
  while (0)

#define OD_IDCT_2_FLAT(p0, p1) \
  /* Embedded 2-point orthonormal Type-II iDCT with flattened rotations. */ \
  do { \
    int t_; \
    t_ = p0 + p1; \
    /* 11585/8192 ~= 2*Sin[Pi/4] = 1.4142135623730951 */ \
    p1 = (p0*11585 + 4096) >> 13; \
    /* 11585/16384 ~= Cos[Pi/4] = 0.7071067811865475 */ \
    p0 = (t_*11585 + 8192) >> 14; \
    p1 -= p0; \
  } \
  while (0)

#define OD_FDCT_2_ASYM_PR(p0, p1, p1h) \
  /* Embedded 2-point asymmetric Type-II fDCT. */ \
  do { \
    p0 += p1h; \
    p1 = p0 - p1; \
  } \
  while (0)

#define OD_IDCT_2_ASYM_PR(p0, p1, p1h) \
  /* Embedded 2-point asymmetric Type-II iDCT. */ \
  do { \
    p1 = p0 - p1; \
    p1h = OD_RSHIFT1(p1); \
    p0 -= p1h; \
  } \
  while (0)

#define OD_FDCT_2_ASYM_FLAT OD_FDCT_2_ASYM_PR
#define OD_IDCT_2_ASYM_FLAT OD_IDCT_2_ASYM_PR

#define OD_FDST_2_PR(p0, p1) \
  /* Embedded 2-point orthonormal Type-IV fDST. */ \
  do { \
    /* 10947/16384 ~= Tan[3*Pi/16] ~= 0.668178637919299 */ \
    OD_DCT_OVERFLOW_CHECK(p1, 10947, 8192, 103); \
    p0 -= (p1*10947 + 8192) >> 14; \
    /* 473/512 ~= Sin[3*Pi/8] ~= 0.923879532511287 */ \
    OD_DCT_OVERFLOW_CHECK(p0, 473, 256, 104); \
    p1 += (p0*473 + 256) >> 9; \
    /* 10947/16384 ~= Tan[3*Pi/16] ~= 0.668178637919299 */ \
    OD_DCT_OVERFLOW_CHECK(p1, 10947, 8192, 105); \
    p0 -= (p1*10947 + 8192) >> 14; \
  } \
  while (0)

#define OD_IDST_2_PR(p0, p1) \
  /* Embedded 2-point orthonormal Type-IV iDST. */ \
  do { \
    /* 10947/16384 ~= Tan[3*Pi/16]) ~= 0.668178637919299 */ \
    p0 += (p1*10947 + 8192) >> 14; \
    /* 473/512 ~= Sin[3*Pi/8] ~= 0.923879532511287 */ \
    p1 -= (p0*473 + 256) >> 9; \
    /* 10947/16384 ~= Tan[3*Pi/16] ~= 0.668178637919299 */ \
    p0 += (p1*10947 + 8192) >> 14; \
  } \
  while (0)

#define OD_FDST_2_FLAT(p0, p1) \
  do { \
    int t_; \
    int u_; \
    t_ = (p0 + p1 + 1) >> 1; \
    /* 21407/16384 ~= Sin[3*Pi/8] + Cos[3*Pi/8] ~= 1.3065629648763766 */ \
    u_ = (p0*21407 + 8192) >> 14; \
    /* 8867/16384 ~= Sin[3*Pi/8] - Cos[3*Pi/8] ~= 0.541196100146197 */ \
    p0 = (p1*8867 + 8192) >> 14; \
    /* 3135/4096 ~= 2*Cos[3*Pi/8] ~= 0.7653668647301796 */ \
    t_ = (t_*3135 + 2048) >> 12; \
    p0 += t_; \
    p1 = u_ - t_; \
  } \
  while (0)

#define OD_IDST_2_FLAT OD_FDST_2_FLAT

#define OD_FDST_2_ASYM_PR(p0, p1) \
  /* Embedded 2-point asymmetric Type-IV fDST. */ \
  do { \
    /* 11507/16384 ~= 4*Sin[Pi/8] - 2*Tan[Pi/8] ~= 0.702306604714169 */ \
    OD_DCT_OVERFLOW_CHECK(p1, 11507, 8192, 187); \
    p0 -= (p1*11507 + 8192) >> 14; \
    /* 669/1024 ~= Cos[Pi/8]/Sqrt[2] ~= 0.653281482438188 */ \
    OD_DCT_OVERFLOW_CHECK(p0, 669, 512, 188); \
    p1 += (p0*669 + 512) >> 10; \
    /* 4573/4096 ~= 4*Sin[Pi/8] - Tan[Pi/8] ~= 1.11652016708726 */ \
    OD_DCT_OVERFLOW_CHECK(p1, 4573, 2048, 189); \
    p0 -= (p1*4573 + 2048) >> 12; \
  } \
  while (0)

#define OD_IDST_2_ASYM_PR(p0, p1) \
  /* Embedded 2-point asymmetric Type-IV iDST. */ \
  do { \
    /* 4573/4096 ~= 4*Sin[Pi/8] - Tan[Pi/8] ~= 1.11652016708726 */ \
    p0 += (p1*4573 + 2048) >> 12; \
    /* 669/1024 ~= Cos[Pi/8]/Sqrt[2] ~= 0.653281482438188 */ \
    p1 -= (p0*669 + 512) >> 10; \
    /* 11507/16384 ~= 4*Sin[Pi/8] - 2*Tan[Pi/8] ~= 0.702306604714169 */ \
    p0 += (p1*11507 + 8192) >> 14; \
  } \
  while (0)

#define OD_FDST_2_ASYM_FLAT(p0, p0h, p1) \
  /* Embedded 2-point asymmetric Type-IV fDST with flattened rotations. */ \
  do { \
    int t_; \
    int u_; \
    t_ = p0h + p1; \
    /* 15137/16384 ~= (Cos[3*Pi/8] + Sin[3*Pi/8])/Sqrt[2] = 0.9238795325112867 */ \
    u_ = (p0*15137 + 8192) >> 14; \
    /* 3135/4096 ~= (Cos[3*Pi/8] - Sin[3*Pi/8])*Sqrt[2] = 0.7653668647301795 */ \
    p0 = (p1*3135 + 2048) >> 12; \
    /* 8867/16384 ~= Cos[3*Pi/8]*Sqrt[2] = 0.5411961001461971 */ \
    t_ = (t_*8867 + 8192) >> 14; \
    p0 += t_; \
    p1 = u_ - t_; \
  } \
  while (0)

#define OD_IDST_2_ASYM_FLAT(p0, p1) \
  /* Embedded 2-point asymmetric Type-IV iDST with flattened rotations. */ \
  do { \
    int t_; \
    int u_; \
    t_ = (p0 + p1 + 1) >> 1; \
    /* 3135/4096 ~= (Cos[Pi/8] - Sin[Pi/8])*Sqrt[2] = 0.7653668647301795 */ \
    u_ = (p1*3135 + 2048) >> 12; \
    /* 15137/16384 ~= (Cos[Pi/8] + Sin[Pi/8])/Sqrt[2] = 0.9238795325112867 */ \
    p1 = (p0*15137 + 8192) >> 14; \
    /* 8867/8192 ~= 2*Cos[3*Pi/8]*Sqrt[2] = 1.082392200292394 */ \
    t_ = (t_*8867 + 4096) >> 13; \
    p0 = u_ + t_; \
    p1 -= OD_RSHIFT1(t_); \
  } \
  while (0)

#define OD_FDCT_4_PR(q0, q2, q1, q3) \
  /* Embedded 4-point orthonormal Type-II fDCT. */ \
  do { \
    int q2h; \
    int q3h; \
    q3 = q0 - q3; \
    q3h = OD_RSHIFT1(q3); \
    q0 -= q3h; \
    q2 += q1; \
    q2h = OD_RSHIFT1(q2); \
    q1 = q2h - q1; \
    OD_FDCT_2_ASYM_PR(q0, q2, q2h); \
    OD_FDST_2_ASYM_PR(q3, q1); \
  } \
  while (0)

#define OD_IDCT_4_PR(q0, q2, q1, q3) \
  /* Embedded 4-point orthonormal Type-II iDCT. */ \
  do { \
    int q1h; \
    int q3h; \
    OD_IDST_2_ASYM_PR(q3, q2); \
    OD_IDCT_2_ASYM_PR(q0, q1, q1h); \
    q3h = OD_RSHIFT1(q3); \
    q0 += q3h; \
    q3 = q0 - q3; \
    q2 = q1h - q2; \
    q1 -= q2; \
  } \
  while (0)

#define OD_FDCT_4_ASYM_PR(q0, q2, q2h, q1, q3, q3h) \
  /* Embedded 4-point asymmetric Type-II fDCT. */ \
  do { \
    q0 += q3h; \
    q3 = q0 - q3; \
    q1 = q2h - q1; \
    q2 = q1 - q2; \
    OD_FDCT_2_PR(q0, q2); \
    OD_FDST_2_PR(q3, q1); \
  } \
  while (0)

#define OD_IDCT_4_ASYM_PR(q0, q2, q1, q1h, q3, q3h) \
  /* Embedded 4-point asymmetric Type-II iDCT. */ \
  do { \
    OD_IDST_2_PR(q3, q2); \
    OD_IDCT_2_PR(q0, q1); \
    q1 = q2 - q1; \
    q1h = OD_RSHIFT1(q1); \
    q2 = q1h - q2; \
    q3 = q0 - q3; \
    q3h = OD_RSHIFT1(q3); \
    q0 -= q3h; \
  } \
  while (0)

#define OD_FDCT_4_FLAT(q0, q1, q2, q3) \
  /* Embedded 4-point orthonormal Type-II fDCT with flattened rotations. */ \
  do { \
    int q1h; \
    int q3h; \
    q3 = q0 - q3; \
    q3h = OD_RSHIFT1(q3); \
    q0 -= q3h; \
    q1 += q2; \
    q1h = OD_RSHIFT1(q1); \
    q2 -= q1h; \
    OD_FDCT_2_ASYM_FLAT(q0, q1, q1h); \
    OD_FDST_2_ASYM_FLAT(q3, q3h, q2); \
  } \
  while (0)

#define OD_IDCT_4_FLAT(q0, q2, q1, q3) \
  /* Embedded 4-point orthonormal Type-II iDCT with flattened rotations. */ \
  do { \
    int q1h; \
    OD_IDST_2_ASYM_FLAT(q3, q2); \
    OD_IDCT_2_ASYM_FLAT(q0, q1, q1h); \
    q2 += q1h; \
    q1 -= q2; \
    q0 += OD_RSHIFT1(q3); \
    q3 = q0 - q3; \
  } \
  while (0)

#define OD_FDCT_4_ASYM_FLAT(q0, q1, q1h, q2, q3, q3h) \
  /* Embedded 4-point asymmetric Type-II fDCT with flattened rotations. */ \
  do { \
    q0 += q3h; \
    q3 = q0 - q3; \
    q2 -= q1h; \
    q1 += q2; \
    OD_FDCT_2_FLAT(q0, q1); \
    OD_FDST_2_FLAT(q3, q2); \
  } \
  while (0)

#define OD_IDCT_4_ASYM_FLAT(q0, q2, q1, q1h, q3, q3h) \
  /* Embedded 4-point asymmetric Type-II iDCT with flattened rotations. */ \
  do { \
    OD_IDST_2_FLAT(q3, q2); \
    OD_IDCT_2_FLAT(q0, q1); \
    q1 -= q2; \
    q1h = OD_RSHIFT1(q1); \
    q2 += q1h; \
    q3 = q0 - q3; \
    q3h = OD_RSHIFT1(q3); \
    q0 -= q3h; \
  } \
  while (0)

#define OD_FDST_4_PR(q0, q2, q1, q3) \
  /* Embedded 4-point orthonormal Type-IV fDST. */ \
  do { \
    int q0h; \
    int q1h; \
    /* 13573/32768 ~= Tan[Pi/8] ~= 0.414213562373095 */ \
    OD_DCT_OVERFLOW_CHECK(q1, 13573, 16384, 190); \
    q2 += (q1*13573 + 16384) >> 15; \
    /* 5793/8192 ~= Sin[Pi/4] ~= 0.707106781186547 */ \
    OD_DCT_OVERFLOW_CHECK(q2, 5793, 4096, 191); \
    q1 -= (q2*5793 + 4096) >> 13; \
    /* 3393/8192 ~= Tan[Pi/8] ~= 0.414213562373095 */ \
    OD_DCT_OVERFLOW_CHECK(q1, 3393, 4096, 192); \
    q2 += (q1*3393 + 4096) >> 13; \
    q0 += q2; \
    q0h = OD_RSHIFT1(q0); \
    q2 = q0h - q2; \
    q1 += q3; \
    q1h = OD_RSHIFT1(q1); \
    q3 -= q1h; \
    /* 537/1024 ~= (1/Sqrt[2] - Cos[3*Pi/16]/2)/Sin[3*Pi/16] ~=
        0.524455699240090 */ \
    OD_DCT_OVERFLOW_CHECK(q1, 537, 512, 193); \
    q2 -= (q1*537 + 512) >> 10; \
    /* 1609/2048 ~= Sqrt[2]*Sin[3*Pi/16] ~= 0.785694958387102 */ \
    OD_DCT_OVERFLOW_CHECK(q2, 1609, 1024, 194); \
    q1 += (q2*1609 + 1024) >> 11; \
    /* 7335/32768 ~= (1/Sqrt[2] - Cos[3*Pi/16])/Sin[3*Pi/16] ~=
        0.223847182092655 */ \
    OD_DCT_OVERFLOW_CHECK(q1, 7335, 16384, 195); \
    q2 += (q1*7335 + 16384) >> 15; \
    /* 5091/8192 ~= (1/Sqrt[2] - Cos[7*Pi/16]/2)/Sin[7*Pi/16] ~=
        0.6215036383171189 */ \
    OD_DCT_OVERFLOW_CHECK(q0, 5091, 4096, 196); \
    q3 += (q0*5091 + 4096) >> 13; \
    /* 5681/4096 ~= Sqrt[2]*Sin[7*Pi/16] ~= 1.38703984532215 */ \
    OD_DCT_OVERFLOW_CHECK(q3, 5681, 2048, 197); \
    q0 -= (q3*5681 + 2048) >> 12; \
    /* 4277/8192 ~= (1/Sqrt[2] - Cos[7*Pi/16])/Sin[7*Pi/16] ~=
        0.52204745462729 */ \
    OD_DCT_OVERFLOW_CHECK(q0, 4277, 4096, 198); \
    q3 += (q0*4277 + 4096) >> 13; \
  } \
  while (0)

#define OD_IDST_4_PR(q0, q2, q1, q3) \
  /* Embedded 4-point orthonormal Type-IV iDST. */ \
  do { \
    int q0h; \
    int q2h; \
    /* 4277/8192 ~= (1/Sqrt[2] - Cos[7*Pi/16])/Sin[7*Pi/16] ~=
        0.52204745462729 */ \
    q3 -= (q0*4277 + 4096) >> 13; \
    /* 5681/4096 ~= Sqrt[2]*Sin[7*Pi/16] ~= 1.38703984532215 */ \
    q0 += (q3*5681 + 2048) >> 12; \
    /* 5091/8192 ~= (1/Sqrt[2] - Cos[7*Pi/16]/2)/Sin[7*Pi/16] ~=
        0.6215036383171189 */ \
    q3 -= (q0*5091 + 4096) >> 13; \
    /* 7335/32768 ~= (1/Sqrt[2] - Cos[3*Pi/16])/Sin[3*Pi/16] ~=
        0.223847182092655 */ \
    q1 -= (q2*7335 + 16384) >> 15; \
    /* 1609/2048 ~= Sqrt[2]*Sin[3*Pi/16] ~= 0.785694958387102 */ \
    q2 -= (q1*1609 + 1024) >> 11; \
    /* 537/1024 ~= (1/Sqrt[2] - Cos[3*Pi/16]/2)/Sin[3*Pi/16] ~=
        0.524455699240090 */ \
    q1 += (q2*537 + 512) >> 10; \
    q2h = OD_RSHIFT1(q2); \
    q3 += q2h; \
    q2 -= q3; \
    q0h = OD_RSHIFT1(q0); \
    q1 = q0h - q1; \
    q0 -= q1; \
    /* 3393/8192 ~= Tan[Pi/8] ~= 0.414213562373095 */ \
    q1 -= (q2*3393 + 4096) >> 13; \
    /* 5793/8192 ~= Sin[Pi/4] ~= 0.707106781186547 */ \
    q2 += (q1*5793 + 4096) >> 13; \
    /* 13573/32768 ~= Tan[Pi/8] ~= 0.414213562373095 */ \
    q1 -= (q2*13573 + 16384) >> 15; \
  } \
  while (0)

#define OD_FDST_4_FLAT(q0, q1, q2, q3) \
  /* Embedded 4-point orthonormal Type-IV fDST with flattened rotations. */ \
  do { \
    int t_; \
    int u_; \
    t_ = q0 - q3; \
    /* 13623/16384 ~= (Sin[7*Pi/16] + Cos[7*Pi/16])/Sqrt[2] ~=
        0.8314696123025451 */ \
    u_ = (13623*q3 + 8192) >> 14; \
    /* 18205/16384 ~= (Sin[7*Pi/16] - Cos[7*Pi/16])*Sqrt[2] ~=
        1.1111404660392046 */ \
    q3 = (18205*q0 + 8192) >> 14; \
    /* 9041/32768 ~= Cos[7*Pi/16]*Sqrt[2] ~= 0.275899379282943 */ \
    t_ = (9041*t_ + 16384) >> 15; \
    q0 = u_ + OD_RSHIFT1(t_); \
    q3 += t_; \
    t_ = q1 + q2; \
    /* 16069/16384 ~= (Sin[5*Pi/16] + Cos[5*Pi/16])/Sqrt[2] ~=
        0.9807852804032304 */ \
    u_ = (16069*q1 + 8192) >> 14; \
    /* 12785/32768 ~= (Sin[5*Pi/16] - Cos[5*Pi/16])*Sqrt[2] ~=
        0.3901806440322566 */ \
    q1 = (12785*q2 + 16384) >> 15; \
    /* 12873/16384 ~= Cos[5*Pi/16]*Sqrt[2] ~= 0.7856949583871021 */ \
    t_ = (12873*t_ + 8192) >> 14; \
    q2 = u_ - OD_RSHIFT1(t_); \
    q1 += t_; \
    q2 += OD_RSHIFT1(q3); \
    q3 -= q2; \
    q0 += OD_RSHIFT1(q1); \
    q1 -= q0; \
    t_ = (q1 + q2 + 1) >> 1; \
    /* 11585/8192 ~= 2*Sin[Pi/4] ~= 1.4142135623730951 */ \
    q1 = (11585*q2 + 4096) >> 13; \
    /* 11585/8192 ~= 2*Cos[Pi/4] ~= 1.4142135623730951 */ \
    q2 = (11585*t_ + 4096) >> 13; \
    q1 -= q2; \
  } \
  while (0)

#define OD_IDST_4_FLAT(q0, q1, q2, q3) \
  /* Embedded 4-point orthonormal Type-IV fDST with flattened rotations. */ \
  do { \
    int t_; \
    int u_; \
    int q2h; \
    int q3h; \
    t_ = (q1 + q2 + 1) >> 1; \
    /* 11585/8192 ~= 2*Sin[Pi/4] ~= 1.4142135623730951 */ \
    q2 = (11585*q1 + 4096) >> 13; \
    /* 11585/8192 ~= 2*Cos[Pi/4] ~= 1.4142135623730951 */ \
    q1 = (11585*t_ + 4096) >> 13; \
    q2 -= q1; \
    q2 += q0; \
    q2h = OD_RSHIFT1(q2); \
    q0 -= q2h; \
    q3 += q1; \
    q3h = OD_RSHIFT1(q3); \
    q1 -= q3h; \
    t_ = q1 + q2h; \
    /* 16069/16384 ~= (Sin[5*Pi/16] + Cos[5*Pi/16])/Sqrt[2] ~=
        0.9807852804032304 */ \
    u_ = (16069*q2 + 8192) >> 14; \
    /* 12785/32768 ~= (Sin[5*Pi/16] - Cos[5*Pi/16])*Sqrt[2] ~=
        0.3901806440322566 */ \
    q2 = (12785*q1 + 16384) >> 15; \
    /* 12873/16384 ~= Cos[5*Pi/16]*Sqrt[2] ~= 0.7856949583871021 */ \
    t_ = (12873*t_ + 8192) >> 14; \
    q1 = u_ - t_; \
    q2 += t_; \
    t_ = q0 - q3h; \
    /* 13623/16384 ~= (Sin[7*Pi/16] + Cos[7*Pi/16])/Sqrt[2] ~=
        0.8314696123025451 */ \
    u_ = (13623*q3 + 8192) >> 14; \
    /* 18205/16384 ~= (Sin[7*Pi/16] - Cos[7*Pi/16])*Sqrt[2] ~=
        1.1111404660392046 */ \
    q3 = (18205*q0 + 8192) >> 14; \
    /* 9041/32768 ~= Cos[7*Pi/16]*Sqrt[2] = 0.275899379282943 */ \
    t_ = (9041*t_ + 16384) >> 15; \
    q0 = u_ + t_; \
    q3 += t_; \
  } \
  while (0)

#define OD_FDST_4_ASYM_PR(t0, t0h, t2, t1, t3) \
  /* Embedded 4-point asymmetric Type-IV fDST. */ \
  do { \
    /* 7489/8192 ~= Tan[Pi/8] + Tan[Pi/4]/2 ~= 0.914213562373095 */ \
    OD_DCT_OVERFLOW_CHECK(t1, 7489, 4096, 106); \
    t2 -= (t1*7489 + 4096) >> 13; \
    /* 11585/16384 ~= Sin[Pi/4] ~= 0.707106781186548 */ \
    OD_DCT_OVERFLOW_CHECK(t1, 11585, 8192, 107); \
    t1 += (t2*11585 + 8192) >> 14; \
    /* -19195/32768 ~= Tan[Pi/8] - Tan[Pi/4] ~= -0.585786437626905 */ \
    OD_DCT_OVERFLOW_CHECK(t1, 19195, 16384, 108); \
    t2 += (t1*19195 + 16384) >> 15; \
    t3 += OD_RSHIFT1(t2); \
    t2 -= t3; \
    t1 = t0h - t1; \
    t0 -= t1; \
    /* 6723/8192 ~= Tan[7*Pi/32] ~= 0.820678790828660 */ \
    OD_DCT_OVERFLOW_CHECK(t0, 6723, 4096, 109); \
    t3 += (t0*6723 + 4096) >> 13; \
    /* 8035/8192 ~= Sin[7*Pi/16] ~= 0.980785280403230 */ \
    OD_DCT_OVERFLOW_CHECK(t3, 8035, 4096, 110); \
    t0 -= (t3*8035 + 4096) >> 13; \
    /* 6723/8192 ~= Tan[7*Pi/32] ~= 0.820678790828660 */ \
    OD_DCT_OVERFLOW_CHECK(t0, 6723, 4096, 111); \
    t3 += (t0*6723 + 4096) >> 13; \
    /* 8757/16384 ~= Tan[5*Pi/32] ~= 0.534511135950792 */ \
    OD_DCT_OVERFLOW_CHECK(t1, 8757, 8192, 112); \
    t2 += (t1*8757 + 8192) >> 14; \
    /* 6811/8192 ~= Sin[5*Pi/16] ~= 0.831469612302545 */ \
    OD_DCT_OVERFLOW_CHECK(t2, 6811, 4096, 113); \
    t1 -= (t2*6811 + 4096) >> 13; \
    /* 8757/16384 ~= Tan[5*Pi/32] ~= 0.534511135950792 */ \
    OD_DCT_OVERFLOW_CHECK(t1, 8757, 8192, 114); \
    t2 += (t1*8757 + 8192) >> 14; \
  } \
  while (0)

#define OD_IDST_4_ASYM_PR(t0, t0h, t2, t1, t3) \
  /* Embedded 4-point asymmetric Type-IV iDST. */ \
  do { \
    /* 8757/16384 ~= Tan[5*Pi/32] ~= 0.534511135950792 */ \
    t1 -= (t2*8757 + 8192) >> 14; \
    /* 6811/8192 ~= Sin[5*Pi/16] ~= 0.831469612302545 */ \
    t2 += (t1*6811 + 4096) >> 13; \
    /* 8757/16384 ~= Tan[5*Pi/32] ~= 0.534511135950792 */ \
    t1 -= (t2*8757 + 8192) >> 14; \
    /* 6723/8192 ~= Tan[7*Pi/32] ~= 0.820678790828660 */ \
    t3 -= (t0*6723 + 4096) >> 13; \
    /* 8035/8192 ~= Sin[7*Pi/16] ~= 0.980785280403230 */ \
    t0 += (t3*8035 + 4096) >> 13; \
    /* 6723/8192 ~= Tan[7*Pi/32] ~= 0.820678790828660 */ \
    t3 -= (t0*6723 + 4096) >> 13; \
    t0 += t2; \
    t0h = OD_RSHIFT1(t0); \
    t2 = t0h - t2; \
    t1 += t3; \
    t3 -= OD_RSHIFT1(t1); \
    /* -19195/32768 ~= Tan[Pi/8] - Tan[Pi/4] ~= -0.585786437626905 */ \
    t1 -= (t2*19195 + 16384) >> 15; \
    /* 11585/16384 ~= Sin[Pi/4] ~= 0.707106781186548 */ \
    t2 -= (t1*11585 + 8192) >> 14; \
    /* 7489/8192 ~= Tan[Pi/8] + Tan[Pi/4]/2 ~= 0.914213562373095 */ \
    t1 += (t2*7489 + 4096) >> 13; \
  } \
  while (0)

#define OD_FDST_4_ASYM_FLAT(q0, q0h, q1, q2, q2h, q3) \
  /* Embedded 4-point asymmetric Type-IV fDST with flattened rotations. */ \
  do { \
    int t_; \
    int u_; \
    t_ = q0h - q3; \
    /* 38531/32768 ~= Sin[7*Pi/16] + Cos[7*Pi/16] ~= 1.1758756024193586 */ \
    u_ = (q3*38531 + 16384) >> 15; \
    /* 12873/16384 ~= Sin[7*Pi/16] - Cos[7*Pi/16] ~= 0.7856949583871022 */ \
    q3 = (q0*12873 + 8192) >> 14; \
    /* 12785/32768 ~= 2*Cos[7*Pi/16] ~= 0.3901806440322565 */ \
    t_ = (t_*12785 + 16384) >> 15; \
    q0 = u_ + OD_RSHIFT1(t_); \
    q3 += t_; \
    t_ = q1 + q2h; \
    /* 45451/32768 ~= Sin[5*Pi/16] + Cos[5*Pi/16] ~= 1.3870398453221475 */ \
    u_ = (q1*45451 + 16384) >> 15; \
    /* 9041/32768 ~= Sin[5*Pi/16] - Cos[5*Pi/16] ~= 0.27589937928294306 */ \
    q1 = (q2*9041 + 16384) >> 15; \
    /* 18205/16384 ~= 2*Cos[5*Pi/16] ~= 1.1111404660392044 */ \
    t_ = (t_*18205 + 8192) >> 14; \
    q1 += t_; \
    q2 = u_ - OD_RSHIFT1(t_); \
    q2 += OD_RSHIFT1(q3); \
    q3 -= q2; \
    q0 += OD_RSHIFT1(q1); \
    q1 -= q0; \
    t_ = (q1 + q2 + 1) >> 1; \
    /* 11585/8192 ~= 2*Sin[Pi/4] ~= 1.4142135623730951 */ \
    q1 = (q2*11585 + 4096) >> 13; \
    /* 11585/8192 ~= 2*Cos[Pi/4] ~= 1.4142135623730951 */ \
    q2 = (t_*11585 + 4096) >> 13; \
    q1 -= q2; \
  } \
  while (0)

#define OD_IDST_4_ASYM_FLAT(q0, q2, q1, q3) \
  do { \
    int t_; \
    int u_; \
    int q1h; \
    int q3h; \
    t_ = (q1 + q2 + 1) >> 1; \
    /* 11585/8192 ~= 2*Sin[Pi/4] ~= 1.4142135623730951 */ \
    q1 = (q2*11585 + 4096) >> 13; \
    /* 11585/8192 ~= 2*Cos[Pi/4] ~= 1.4142135623730951 */ \
    q2 = (t_*11585 + 4096) >> 13; \
    q1 -= q2; \
    q1 += q0; \
    q1h = OD_RSHIFT1(q1); \
    q0 -= q1h; \
    q3 += q2; \
    q3h = OD_RSHIFT1(q3); \
    q2 -= q3h; \
    t_ = q1h + q2; \
    /* 45451/32768 ~= Sin[5*Pi/16] + Cos[5*Pi/16] ~= 1.3870398453221475 */ \
    u_ = (q1*45451 + 16384) >> 15; \
    /* 9041/32768 ~= Sin[5*Pi/16] - Cos[5*Pi/16] ~= 0.27589937928294306 */ \
    q1 = (q2*9041 + 16384) >> 15; \
    /* 18205/16384 ~= 2*Cos[5*Pi/16] ~= 1.1111404660392044 */ \
    t_ = (t_*18205 + 8192) >> 14; \
    q1 += OD_RSHIFT1(t_); \
    q2 = u_ - t_; \
    t_ = q0 - q3h; \
    /* 38531/32768 ~= Sin[7*Pi/16] + Cos[7*Pi/16] = 1.1758756024193586 */ \
    u_ = (q3*38531 + 16384) >> 15; \
    /* 12873/16384 ~= Sin[7*Pi/16] - Cos[7*Pi/16] = 0.7856949583871022 */ \
    q3 = (q0*12873 + 8192) >> 14; \
    /* 12785/32768 ~= 2*Cos[7*Pi/16] = 0.3901806440322565 */ \
    t_ = (t_*12785 + 16384) >> 15; \
    q3 += OD_RSHIFT1(t_); \
    q0 = u_ + t_; \
  } \
  while (0)

#define OD_FDCT_8_PR(r0, r4, r2, r6, r1, r5, r3, r7) \
  /* Embedded 8-point orthonormal Type-II fDCT. */ \
  do { \
    int r4h; \
    int r5h; \
    int r6h; \
    int r7h; \
    r7 = r0 - r7; \
    r7h = OD_RSHIFT1(r7); \
    r0 -= r7h; \
    r6 += r1; \
    r6h = OD_RSHIFT1(r6); \
    r1 = r6h - r1; \
    r5 = r2 - r5; \
    r5h = OD_RSHIFT1(r5); \
    r2 -= r5h; \
    r4 += r3; \
    r4h = OD_RSHIFT1(r4); \
    r3 = r4h - r3; \
    OD_FDCT_4_ASYM_PR(r0, r4, r4h, r2, r6, r6h); \
    OD_FDST_4_ASYM_PR(r7, r7h, r3, r5, r1); \
  } \
  while (0)

#define OD_IDCT_8_PR(r0, r4, r2, r6, r1, r5, r3, r7) \
  /* Embedded 8-point orthonormal Type-II iDCT. */ \
  do { \
    int r1h; \
    int r3h; \
    int r5h; \
    int r7h; \
    OD_IDST_4_ASYM_PR(r7, r7h, r5, r6, r4); \
    OD_IDCT_4_ASYM_PR(r0, r2, r1, r1h, r3, r3h); \
    r0 += r7h; \
    r7 = r0 - r7; \
    r6 = r1h - r6; \
    r1 -= r6; \
    r5h = OD_RSHIFT1(r5); \
    r2 += r5h; \
    r5 = r2 - r5; \
    r4 = r3h - r4; \
    r3 -= r4; \
  } \
  while (0)

#define OD_FDCT_8_FLAT(r0, r1, r2, r3, r4, r5, r6, r7) \
  /* Embedded 8-point orthonormal Type-II fDCT with flattened rotations. */ \
  do { \
    int r1h; \
    int r3h; \
    int r5h; \
    int r7h; \
    r7 = r0 - r7; \
    r7h = OD_RSHIFT1(r7); \
    r0 -= r7h; \
    r1 += r6; \
    r1h = OD_RSHIFT1(r1); \
    r6 -= r1h; \
    r5 = r2 - r5; \
    r5h = OD_RSHIFT1(r5); \
    r2 -= r5h; \
    r3 += r4; \
    r3h = OD_RSHIFT1(r3); \
    r4 -= r3h; \
    OD_FDCT_4_ASYM_FLAT(r0, r1, r1h, r2, r3, r3h); \
    OD_FDST_4_ASYM_FLAT(r7, r7h, r6, r5, r5h, r4); \
  } \
  while (0)

#define OD_IDCT_8_FLAT(r0, r4, r2, r6, r1, r5, r3, r7) \
  /* Embedded 8-point orthonormal Type-II iDCT with flattened rotations. */ \
  do { \
    int r1h; \
    int r3h; \
    OD_IDST_4_ASYM_FLAT(r7, r5, r6, r4); \
    OD_IDCT_4_ASYM_FLAT(r0, r2, r1, r1h, r3, r3h); \
    r4 += r3h; \
    r3 -= r4; \
    r2 += OD_RSHIFT1(r5); \
    r5 = r2 - r5; \
    r6 += r1h; \
    r1 -= r6; \
    r0 += OD_RSHIFT1(r7); \
    r7 = r0 - r7; \
  } \
  while (0)

#define OD_FDCT_8_ASYM_PR(r0, r4, r4h, r2, r6, r6h, r1, r5, r5h, r3, r7, r7h) \
  /* Embedded 8-point asymmetric Type-II fDCT. */ \
  do { \
    r0 += r7h; \
    r7 = r0 - r7; \
    r1 = r6h - r1; \
    r6 -= r1; \
    r2 += r5h; \
    r5 = r2 - r5; \
    r3 = r4h - r3; \
    r4 -= r3; \
    OD_FDCT_4_PR(r0, r4, r2, r6); \
    OD_FDST_4_PR(r7, r3, r5, r1); \
  } \
  while (0)

#define OD_IDCT_8_ASYM_PR(r0, r4, r2, r6, r1, r1h, r5, r5h, r3, r3h, r7, r7h) \
  /* Embedded 8-point asymmetric Type-II iDCT. */ \
  do { \
    OD_IDST_4_PR(r7, r5, r6, r4); \
    OD_IDCT_4_PR(r0, r2, r1, r3); \
    r7 = r0 - r7; \
    r7h = OD_RSHIFT1(r7); \
    r0 -= r7h; \
    r1 += r6; \
    r1h = OD_RSHIFT1(r1); \
    r6 = r1h - r6; \
    r5 = r2 - r5; \
    r5h = OD_RSHIFT1(r5); \
    r2 -= r5h; \
    r3 += r4; \
    r3h = OD_RSHIFT1(r3); \
    r4 = r3h - r4; \
  } \
  while (0)

#define OD_FDCT_8_ASYM_FLAT(r0, r1, r1h, r2, r3, r3h, r4, r5, r5h, r6, r7, r7h) \
  /* Embedded 8-point asymmetric Type-II fDCT. */ \
  do { \
    r0 += r7h; \
    r7 = r0 - r7; \
    r4 -= r3h; \
    r3 += r4; \
    r2 += r5h; \
    r5 = r2 - r5; \
    r6 -= r1h; \
    r1 += r6; \
    OD_FDCT_4_FLAT(r0, r1, r2, r3); \
    OD_FDST_4_FLAT(r7, r6, r5, r4); \
  } \
  while (0)

#define OD_IDCT_8_ASYM_FLAT(r0, r4, r2, r6, r1, r1h, r5, r5h, r3, r3h, r7, r7h) \
  /* Embedded 8-point asymmetric Type-II iDCT with flattened rotations. */ \
  do { \
    OD_IDST_4_FLAT(r7, r5, r6, r4); \
    OD_IDCT_4_FLAT(r0, r2, r1, r3); \
    r7 = r0 - r7; \
    r7h = OD_RSHIFT1(r7); \
    r0 -= r7h; \
    r1 -= r6; \
    r1h = OD_RSHIFT1(r1); \
    r6 += r1h; \
    r5 = r2 - r5; \
    r5h = OD_RSHIFT1(r5); \
    r2 -= r5h; \
    r3 -= r4; \
    r3h = OD_RSHIFT1(r3); \
    r4 += r3h; \
  } \
  while (0)

#define OD_FDST_8_PR(t0, t4, t2, t6, t1, t5, t3, t7)  \
  /* Embedded 8-point orthonormal Type-IV fDST. */ \
  do { \
    int t0h; \
    int t2h; \
    int t5h; \
    int t7h; \
    /* 13573/32768 ~= Tan[Pi/8] ~= 0.414213562373095 */ \
    OD_DCT_OVERFLOW_CHECK(t1, 13573, 16384, 115); \
    t6 -= (t1*13573 + 16384) >> 15; \
    /* 11585/16384 ~= Sin[Pi/4] ~= 0.707106781186547 */ \
    OD_DCT_OVERFLOW_CHECK(t6, 11585, 8192, 116); \
    t1 += (t6*11585 + 8192) >> 14; \
    /* 13573/32768 ~= Tan[Pi/8] ~= 0.414213562373095 */ \
    OD_DCT_OVERFLOW_CHECK(t1, 13573, 16384, 117); \
    t6 -= (t1*13573 + 16384) >> 15; \
    /* 21895/32768 ~= Tan[3*Pi/16] ~= 0.668178637919299 */ \
    OD_DCT_OVERFLOW_CHECK(t2, 21895, 16384, 118); \
    t5 -= (t2*21895 + 16384) >> 15; \
    /* 15137/16384 ~= Sin[3*Pi/8] ~= 0.923879532511287 */ \
    OD_DCT_OVERFLOW_CHECK(t5, 15137, 8192, 119); \
    t2 += (t5*15137 + 8192) >> 14; \
    /* 10947/16384 ~= Tan[3*Pi/16] ~= 0.668178637919299 */ \
    OD_DCT_OVERFLOW_CHECK(t2, 10947, 8192, 120); \
    t5 -= (t2*10947 + 8192) >> 14; \
    /* 3259/16384 ~= Tan[Pi/16] ~= 0.198912367379658 */ \
    OD_DCT_OVERFLOW_CHECK(t3, 3259, 8192, 121); \
    t4 -= (t3*3259 + 8192) >> 14; \
    /* 3135/8192 ~= Sin[Pi/8] ~= 0.382683432365090 */ \
    OD_DCT_OVERFLOW_CHECK(t4, 3135, 4096, 122); \
    t3 += (t4*3135 + 4096) >> 13; \
    /* 3259/16384 ~= Tan[Pi/16] ~= 0.198912367379658 */ \
    OD_DCT_OVERFLOW_CHECK(t3, 3259, 8192, 123); \
    t4 -= (t3*3259 + 8192) >> 14; \
    t7 += t1; \
    t7h = OD_RSHIFT1(t7); \
    t1 -= t7h; \
    t2 = t3 - t2; \
    t2h = OD_RSHIFT1(t2); \
    t3 -= t2h; \
    t0 -= t6; \
    t0h = OD_RSHIFT1(t0); \
    t6 += t0h; \
    t5 = t4 - t5; \
    t5h = OD_RSHIFT1(t5); \
    t4 -= t5h; \
    t1 += t5h; \
    t5 = t1 - t5; \
    t4 += t0h; \
    t0 -= t4; \
    t6 -= t2h; \
    t2 += t6; \
    t3 -= t7h; \
    t7 += t3; \
    /* TODO: Can we move this into another operation */ \
    t7 = -t7; \
    /* 7425/8192 ~= Tan[15*Pi/64] ~= 0.906347169019147 */ \
    OD_DCT_OVERFLOW_CHECK(t7, 7425, 4096, 124); \
    t0 -= (t7*7425 + 4096) >> 13; \
    /* 8153/8192 ~= Sin[15*Pi/32] ~= 0.995184726672197 */ \
    OD_DCT_OVERFLOW_CHECK(t0, 8153, 4096, 125); \
    t7 += (t0*8153 + 4096) >> 13; \
    /* 7425/8192 ~= Tan[15*Pi/64] ~= 0.906347169019147 */ \
    OD_DCT_OVERFLOW_CHECK(t7, 7425, 4096, 126); \
    t0 -= (t7*7425 + 4096) >> 13; \
    /* 4861/32768 ~= Tan[3*Pi/64] ~= 0.148335987538347 */ \
    OD_DCT_OVERFLOW_CHECK(t1, 4861, 16384, 127); \
    t6 -= (t1*4861 + 16384) >> 15; \
    /* 1189/4096 ~= Sin[3*Pi/32] ~= 0.290284677254462 */ \
    OD_DCT_OVERFLOW_CHECK(t6, 1189, 2048, 128); \
    t1 += (t6*1189 + 2048) >> 12; \
    /* 4861/32768 ~= Tan[3*Pi/64] ~= 0.148335987538347 */ \
    OD_DCT_OVERFLOW_CHECK(t1, 4861, 16384, 129); \
    t6 -= (t1*4861 + 16384) >> 15; \
    /* 2455/4096 ~= Tan[11*Pi/64] ~= 0.599376933681924 */ \
    OD_DCT_OVERFLOW_CHECK(t5, 2455, 2048, 130); \
    t2 -= (t5*2455 + 2048) >> 12; \
    /* 7225/8192 ~= Sin[11*Pi/32] ~= 0.881921264348355 */ \
    OD_DCT_OVERFLOW_CHECK(t2, 7225, 4096, 131); \
    t5 += (t2*7225 + 4096) >> 13; \
    /* 2455/4096 ~= Tan[11*Pi/64] ~= 0.599376933681924 */ \
    OD_DCT_OVERFLOW_CHECK(t5, 2455, 2048, 132); \
    t2 -= (t5*2455 + 2048) >> 12; \
    /* 11725/32768 ~= Tan[7*Pi/64] ~= 0.357805721314524 */ \
    OD_DCT_OVERFLOW_CHECK(t3, 11725, 16384, 133); \
    t4 -= (t3*11725 + 16384) >> 15; \
    /* 5197/8192 ~= Sin[7*Pi/32] ~= 0.634393284163645 */ \
    OD_DCT_OVERFLOW_CHECK(t4, 5197, 4096, 134); \
    t3 += (t4*5197 + 4096) >> 13; \
    /* 11725/32768 ~= Tan[7*Pi/64] ~= 0.357805721314524 */ \
    OD_DCT_OVERFLOW_CHECK(t3, 11725, 16384, 135); \
    t4 -= (t3*11725 + 16384) >> 15; \
  } \
  while (0)

#define OD_IDST_8_PR(t0, t4, t2, t6, t1, t5, t3, t7) \
  /* Embedded 8-point orthonormal Type-IV iDST. */ \
  do { \
    int t0h; \
    int t2h; \
    int t5h_; \
    int t7h_; \
    /* 11725/32768 ~= Tan[7*Pi/64] ~= 0.357805721314524 */ \
    t1 += (t6*11725 + 16384) >> 15; \
    /* 5197/8192 ~= Sin[7*Pi/32] ~= 0.634393284163645 */ \
    t6 -= (t1*5197 + 4096) >> 13; \
    /* 11725/32768 ~= Tan[7*Pi/64] ~= 0.357805721314524 */ \
    t1 += (t6*11725 + 16384) >> 15; \
    /* 2455/4096 ~= Tan[11*Pi/64] ~= 0.599376933681924 */ \
    t2 += (t5*2455 + 2048) >> 12; \
    /* 7225/8192 ~= Sin[11*Pi/32] ~= 0.881921264348355 */ \
    t5 -= (t2*7225 + 4096) >> 13; \
    /* 2455/4096 ~= Tan[11*Pi/64] ~= 0.599376933681924 */ \
    t2 += (t5*2455 + 2048) >> 12; \
    /* 4861/32768 ~= Tan[3*Pi/64] ~= 0.148335987538347 */ \
    t3 += (t4*4861 + 16384) >> 15; \
    /* 1189/4096 ~= Sin[3*Pi/32] ~= 0.290284677254462 */ \
    t4 -= (t3*1189 + 2048) >> 12; \
    /* 4861/32768 ~= Tan[3*Pi/64] ~= 0.148335987538347 */ \
    t3 += (t4*4861 + 16384) >> 15; \
    /* 7425/8192 ~= Tan[15*Pi/64] ~= 0.906347169019147 */ \
    t0 += (t7*7425 + 4096) >> 13; \
    /* 8153/8192 ~= Sin[15*Pi/32] ~= 0.995184726672197 */ \
    t7 -= (t0*8153 + 4096) >> 13; \
    /* 7425/8192 ~= Tan[15*Pi/64] ~= 0.906347169019147 */ \
    t0 += (t7*7425 + 4096) >> 13; \
    /* TODO: Can we move this into another operation */ \
    t7 = -t7; \
    t7 -= t6; \
    t7h_ = OD_RSHIFT1(t7); \
    t6 += t7h_; \
    t2 -= t3; \
    t2h = OD_RSHIFT1(t2); \
    t3 += t2h; \
    t0 += t1; \
    t0h = OD_RSHIFT1(t0); \
    t1 -= t0h; \
    t5 = t4 - t5; \
    t5h_ = OD_RSHIFT1(t5); \
    t4 -= t5h_; \
    t1 += t5h_; \
    t5 = t1 - t5; \
    t3 -= t0h; \
    t0 += t3; \
    t6 += t2h; \
    t2 = t6 - t2; \
    t4 += t7h_; \
    t7 -= t4; \
    /* 3259/16384 ~= Tan[Pi/16] ~= 0.198912367379658 */ \
    t1 += (t6*3259 + 8192) >> 14; \
    /* 3135/8192 ~= Sin[Pi/8] ~= 0.382683432365090 */ \
    t6 -= (t1*3135 + 4096) >> 13; \
    /* 3259/16384 ~= Tan[Pi/16] ~= 0.198912367379658 */ \
    t1 += (t6*3259 + 8192) >> 14; \
    /* 10947/16384 ~= Tan[3*Pi/16] ~= 0.668178637919299 */ \
    t5 += (t2*10947 + 8192) >> 14; \
    /* 15137/16384 ~= Sin[3*Pi/8] ~= 0.923879532511287 */ \
    t2 -= (t5*15137 + 8192) >> 14; \
    /* 21895/32768 ~= Tan[3*Pi/16] ~= 0.668178637919299 */ \
    t5 += (t2*21895 + 16384) >> 15; \
    /* 13573/32768 ~= Tan[Pi/8] ~= 0.414213562373095 */ \
    t3 += (t4*13573 + 16384) >> 15; \
    /* 11585/16384 ~= Sin[Pi/4] ~= 0.707106781186547 */ \
    t4 -= (t3*11585 + 8192) >> 14; \
    /* 13573/32768 ~= Tan[Pi/8] ~= 0.414213562373095 */ \
    t3 += (t4*13573 + 16384) >> 15; \
  } \
  while (0)

#define OD_FDST_8_FLAT(r0, r1, r2, r3, r4, r5, r6, r7) \
  /* Embedded 8-point Type-IV fDST with flattened rotations. */ \
  do { \
    int t_; \
    int u_; \
    int r0h; \
    int r2h; \
    int r5h; \
    int r7h; \
    t_ = r3 - r4; \
    /* 23059/16384 ~= Sin[9*Pi/32] + Cos[9*Pi/32] ~= 1.4074037375263826 */ \
    u_ = (23059*r4 + 8192) >> 14; \
    /* 2271/16384 ~= Sin[9*Pi/32] - Cos[9*Pi/32] ~= 0.1386171691990915 */ \
    r4 = (2271*r3 + 8192) >> 14; \
    /* 5197/8192 ~= Cos[9*Pi/32] ~= 0.6343932841636455 */ \
    t_ = (5197*t_ + 4096) >> 13; \
    r3 = u_ + t_; \
    r4 += t_; \
    t_ = r2 + r5; \
    /* 22173/16384 ~= Sin[11*Pi/32] + Cos[11*Pi/32] ~= 1.3533180011743526 */ \
    u_ = (22173*r2 + 8192) >> 14; \
    /* 3363/8192 ~= Sin[11*Pi/32] - Cos[11*Pi/32] ~= 0.4105245275223574 */ \
    r2 = (3363*r5 + 4096) >> 13; \
    /* 15447/32768 ~= Cos[11*Pi/32] ~= 0.47139673682599764 */ \
    t_ = (15447*t_ + 16384) >> 15; \
    r2 += t_; \
    r5 = u_ - t_; \
    t_ = r1 - r6; \
    /* 40869/32768 ~= Sin[13*Pi/32] + Cos[13*Pi/32] ~= 1.247225012986671 */ \
    u_ = (40869*r6 + 16384) >> 15; \
    /* 21845/32768 ~= Sin[13*Pi/32] - Cos[13*Pi/32] ~= 0.6666556584777465 */ \
    r6 = (21845*r1 + 16384) >> 15; \
    /* 1189/4096 ~= Cos[13*Pi/32] ~= 0.29028467725446233 */ \
    t_ = (1189*t_ + 2048) >> 12; \
    r1 = u_ + t_; \
    r6 += t_; \
    t_ = r0 + r7; \
    /* 17911/16384 ~= Sin[15*Pi/32] + Cos[15*Pi/32] ~= 1.0932018670017576 */ \
    u_ = (17911*r0 + 8192) >> 14; \
    /* 14699/16384 ~= Sin[15*Pi/32] - Cos[15*Pi/32] ~= 0.8971675863426363 */ \
    r0 = (14699*r7 + 8192) >> 14; \
    /* 803/8192 ~= Cos[15*Pi/32] ~= 0.0980171403295606 */ \
    t_ = (803*t_ + 4096) >> 13; \
    r0 += t_; \
    r7 = u_ - t_; \
    r2 -= r1; \
    r2h = OD_RSHIFT1(r2); \
    r1 += r2h; \
    r5 += r6; \
    r5h = OD_RSHIFT1(r5); \
    r6 -= r5h; \
    r0 += r3; \
    r0h = OD_RSHIFT1(r0); \
    r3 -= r0h; \
    r7 -= r4; \
    r7h = OD_RSHIFT1(r7); \
    r4 += r7h; \
    r3 += r5h; \
    r5 -= r3; \
    r1 -= r0h; \
    r0 += r1; \
    r4 += r2h; \
    r2 -= r4; \
    r6 += r7h; \
    r7 -= r6; \
    t_ = (r4 - r3 + 1) >> 1; \
    /* 21407/16384 ~= Sin[3*Pi/8] + Cos[3*Pi/8] ~= 1.3065629648763766 */ \
    u_ = (21407*r3 + 8192) >> 14; \
    /* 8867/16384 ~= Sin[3*Pi/8] - Cos[3*Pi/8] ~= 0.5411961001461969 */ \
    r3 = (8867*r4 + 8192) >> 14; \
    /* 3135/4096 ~= 2*Cos[3*Pi/8] ~= 0.7653668647301796 */ \
    t_ = (3135*t_ + 2048) >> 12; \
    r3 += t_; \
    r4 = u_ + t_; \
    t_ = (r2 - r5 + 1) >> 1; \
    /* 21407/16384 ~= Sin[3*Pi/8] + Cos[3*Pi/8] ~= 1.3065629648763766 */ \
    u_ = (21407*r2 + 8192) >> 14; \
    /* 8867/16384 ~= Sin[3*Pi/8] - Cos[3*Pi/8] ~= 0.5411961001461969 */ \
    r2 = (8867*r5 + 8192) >> 14; \
    /* 3135/4096 ~= 2*Cos[3*Pi/8] ~= 0.7653668647301796 */ \
    t_ = (3135*t_ + 2048) >> 12; \
    r5 = t_ - u_; \
    r2 -= t_; \
    t_ = (r6 - r1 + 1) >> 1; \
    /* 11585/8192 ~= 2*Sin[Pi/4] ~= 1.4142135623730951 */ \
    r6 = (11585*r1 + 4096) >> 13; \
    /* 11585/8192 ~= 2*Cos[Pi/4] ~= 1.4142135623730951 */ \
    r1 = (11585*t_ + 4096) >> 13; \
    r6 += r1; \
  } \
  while (0)

#define OD_IDST_8_FLAT(r0, r4, r2, r6, r1, r5, r3, r7) \
  /* Embedded 8-point Type-IV iDST with flattened rotations. */ \
  do { \
    int t_; \
    int u_; \
    int r0h; \
    int r2h; \
    int r5h; \
    int r7h; \
    t_ = (r1 + r6 + 1) >> 1; \
    /* 11585/8192 ~= 2*Sin[Pi/4] ~= 1.4142135623730951 */ \
    r1 = (11585*r6 + 4096) >> 13; \
    /* 11585/8192 ~= 2*Cos[Pi/4] ~= 1.4142135623730951 */ \
    r6 = (11585*t_ + 4096) >> 13; \
    r1 -= r6; \
    t_ = (r5 - r2 + 1) >> 1; \
    /* 21407/16384 ~= Sin[3*Pi/8] + Cos[3*Pi/8] ~= 1.3065629648763766 */ \
    u_ = (21407*r5 + 8192) >> 14; \
    /* 8867/16384 ~= Sin[3*Pi/8] - Cos[3*Pi/8] ~= 0.5411961001461969 */ \
    r5 = (8867*r2 + 8192) >> 14; \
    /* 3135/4096 ~= 2*Cos[3*Pi/8] ~= 0.7653668647301796 */ \
    t_ = (3135*t_ + 2048) >> 12; \
    r5 -= t_; \
    r2 = t_ - u_; \
    t_ = (r3 + r4 + 1) >> 1; \
    /* 21407/16384 ~= Sin[3*Pi/8] + Cos[3*Pi/8] ~= 1.3065629648763766 */ \
    u_ = (21407*r4 + 8192) >> 14; \
    /* 8867/16384 ~= Sin[3*Pi/8] - Cos[3*Pi/8] ~= 0.5411961001461969 */ \
    r4 = (8867*r3 + 8192) >> 14; \
    /* 3135/4096 ~= 2*Cos[3*Pi/8] ~= 0.7653668647301796 */ \
    t_ = (3135*t_ + 2048) >> 12; \
    r3 = u_ - t_; \
    r4 += t_; \
    r7 += r6; \
    r7h = OD_RSHIFT1(r7); \
    r6 -= r7h; \
    r2 += r4; \
    r2h = OD_RSHIFT1(r2); \
    r4 -= r2h; \
    r0 -= r1; \
    r0h = OD_RSHIFT1(r0); \
    r1 += r0h; \
    r5 += r3; \
    r5h = OD_RSHIFT1(r5); \
    r3 -= r5h; \
    r4 -= r7h; \
    r7 += r4; \
    r6 += r5h; \
    r5 -= r6; \
    r3 += r0h; \
    r0 -= r3; \
    r1 -= r2h; \
    r2 += r1; \
    t_ = r0 + r7; \
    /* 17911/16384 ~= Sin[15*Pi/32] + Cos[15*Pi/32] ~= 1.0932018670017576 */ \
    u_ = (17911*r0 + 8192) >> 14; \
    /* 14699/16384 ~= Sin[15*Pi/32] - Cos[15*Pi/32] ~= 0.8971675863426363 */ \
    r0 = (14699*r7 + 8192) >> 14; \
    /* 803/8192 ~= Cos[15*Pi/32] ~= 0.0980171403295606 */ \
    t_ = (803*t_ + 4096) >> 13; \
    r7 = u_ - t_; \
    r0 += t_; \
    t_ = r1 - r6; \
    /* 40869/32768 ~= Sin[13*Pi/32] + Cos[13*Pi/32] ~= 1.247225012986671 */ \
    u_ = (40869*r6 + 16384) >> 15; \
    /* 21845/32768 ~= Sin[13*Pi/32] - Cos[13*Pi/32] ~= 0.6666556584777465 */ \
    r6 = (21845*r1 + 16384) >> 15; \
    /* 1189/4096 ~= Cos[13*Pi/32] ~= 0.29028467725446233 */ \
    t_ = (1189*t_ + 2048) >> 12; \
    r1 = u_ + t_; \
    r6 += t_; \
    t_ = r2 + r5; \
    /* 22173/16384 ~= Sin[11*Pi/32] + Cos[11*Pi/32] ~= 1.3533180011743526 */ \
    u_ = (22173*r2 + 8192) >> 14; \
    /* 3363/8192 ~= Sin[11*Pi/32] - Cos[11*Pi/32] ~= 0.4105245275223574 */ \
    r2 = (3363*r5 + 4096) >> 13; \
    /* 15447/32768 ~= Cos[11*Pi/32] ~= 0.47139673682599764 */ \
    t_ = (15447*t_ + 16384) >> 15; \
    r5 = u_ - t_; \
    r2 += t_; \
    t_ = r3 - r4; \
    /* 23059/16384 ~= Sin[9*Pi/32] + Cos[9*Pi/32] ~= 1.4074037375263826 */ \
    u_ = (23059*r4 + 8192) >> 14; \
    /* 2271/16384 ~= Sin[9*Pi/32] - Cos[9*Pi/32] ~= 0.1386171691990915 */ \
    r4 = (2271*r3 + 8192) >> 14; \
    /* 5197/8192 ~= Cos[9*Pi/32] ~= 0.6343932841636455 */ \
    t_ = (5197*t_ + 4096) >> 13; \
    r3 = u_ + t_; \
    r4 += t_; \
  } \
  while (0)

/* Rewrite this so that t0h can be passed in. */
#define OD_FDST_8_ASYM_PR(t0, t4, t2, t6, t1, t5, t3, t7) \
  /* Embedded 8-point asymmetric Type-IV fDST. */ \
  do { \
    int t0h; \
    int t2h; \
    int t5h; \
    int t7h; \
    /* 1035/2048 ~= (Sqrt[2] - Cos[7*Pi/32])/(2*Sin[7*Pi/32]) */ \
    OD_DCT_OVERFLOW_CHECK(t1, 1035, 1024, 199); \
    t6 += (t1*1035 + 1024) >> 11; \
    /* 3675/4096 ~= Sqrt[2]*Sin[7*Pi/32] */ \
    OD_DCT_OVERFLOW_CHECK(t6, 3675, 2048, 200); \
    t1 -= (t6*3675 + 2048) >> 12; \
    /* 851/8192 ~= (Cos[7*Pi/32] - 1/Sqrt[2])/Sin[7*Pi/32] */ \
    OD_DCT_OVERFLOW_CHECK(t1, 851, 4096, 201); \
    t6 -= (t1*851 + 4096) >> 13; \
    /* 4379/8192 ~= (Sqrt[2] - Sin[5*Pi/32])/(2*Cos[5*Pi/32]) */ \
    OD_DCT_OVERFLOW_CHECK(t2, 4379, 4096, 202); \
    t5 += (t2*4379 + 4096) >> 13; \
    /* 10217/8192 ~= Sqrt[2]*Cos[5*Pi/32] */ \
    OD_DCT_OVERFLOW_CHECK(t5, 10217, 4096, 203); \
    t2 -= (t5*10217 + 4096) >> 13; \
    /* 4379/16384 ~= (1/Sqrt[2] - Sin[5*Pi/32])/Cos[5*Pi/32] */ \
    OD_DCT_OVERFLOW_CHECK(t2, 4379, 8192, 204); \
    t5 += (t2*4379 + 8192) >> 14; \
    /* 12905/16384 ~= (Sqrt[2] - Cos[3*Pi/32])/(2*Sin[3*Pi/32]) */ \
    OD_DCT_OVERFLOW_CHECK(t3, 12905, 8192, 205); \
    t4 += (t3*12905 + 8192) >> 14; \
    /* 3363/8192 ~= Sqrt[2]*Sin[3*Pi/32] */ \
    OD_DCT_OVERFLOW_CHECK(t4, 3363, 4096, 206); \
    t3 -= (t4*3363 + 4096) >> 13; \
    /* 3525/4096 ~= (Cos[3*Pi/32] - 1/Sqrt[2])/Sin[3*Pi/32] */ \
    OD_DCT_OVERFLOW_CHECK(t3, 3525, 2048, 207); \
    t4 -= (t3*3525 + 2048) >> 12; \
    /* 5417/8192 ~= (Sqrt[2] - Sin[Pi/32])/(2*Cos[Pi/32]) */ \
    OD_DCT_OVERFLOW_CHECK(t0, 5417, 4096, 208); \
    t7 += (t0*5417 + 4096) >> 13; \
    /* 5765/4096 ~= Sqrt[2]*Cos[Pi/32] */ \
    OD_DCT_OVERFLOW_CHECK(t7, 5765, 2048, 209); \
    t0 -= (t7*5765 + 2048) >> 12; \
    /* 2507/4096 ~= (1/Sqrt[2] - Sin[Pi/32])/Cos[Pi/32] */ \
    OD_DCT_OVERFLOW_CHECK(t0, 2507, 2048, 210); \
    t7 += (t0*2507 + 2048) >> 12; \
    t0 += t1; \
    t0h = OD_RSHIFT1(t0); \
    t1 -= t0h; \
    t2 -= t3; \
    t2h = OD_RSHIFT1(t2); \
    t3 += t2h; \
    t5 -= t4; \
    t5h = OD_RSHIFT1(t5); \
    t4 += t5h; \
    t7 += t6; \
    t7h = OD_RSHIFT1(t7); \
    t6 = t7h - t6; \
    t4 = t7h - t4; \
    t7 -= t4; \
    t1 += t5h; \
    t5 = t1 - t5; \
    t6 += t2h; \
    t2 = t6 - t2; \
    t3 -= t0h; \
    t0 += t3; \
    /* 3259/16384 ~= Tan[Pi/16] ~= 0.198912367379658 */ \
    OD_DCT_OVERFLOW_CHECK(t6, 3259, 8192, 211); \
    t1 += (t6*3259 + 8192) >> 14; \
    /* 3135/8192 ~= Sin[Pi/8] ~= 0.382683432365090 */ \
    OD_DCT_OVERFLOW_CHECK(t1, 3135, 4096, 212); \
    t6 -= (t1*3135 + 4096) >> 13; \
    /* 3259/16384 ~= Tan[Pi/16] ~= 0.198912367379658 */ \
    OD_DCT_OVERFLOW_CHECK(t6, 3259, 8192, 213); \
    t1 += (t6*3259 + 8192) >> 14; \
    /* 2737/4096 ~= Tan[3*Pi/16] ~= 0.668178637919299 */ \
    OD_DCT_OVERFLOW_CHECK(t2, 2737, 2048, 214); \
    t5 += (t2*2737 + 2048) >> 12; \
    /* 473/512 ~= Sin[3*Pi/8] ~= 0.923879532511287 */ \
    OD_DCT_OVERFLOW_CHECK(t5, 473, 256, 215); \
    t2 -= (t5*473 + 256) >> 9; \
    /* 2737/4096 ~= Tan[3*Pi/16] ~= 0.668178637919299 */ \
    OD_DCT_OVERFLOW_CHECK(t2, 2737, 2048, 216); \
    t5 += (t2*2737 + 2048) >> 12; \
    /* 3393/8192 ~= Tan[Pi/8] ~= 0.414213562373095 */ \
    OD_DCT_OVERFLOW_CHECK(t4, 3393, 4096, 217); \
    t3 += (t4*3393 + 4096) >> 13; \
    /* 5793/8192 ~= Sin[Pi/4] ~= 0.707106781186547 */ \
    OD_DCT_OVERFLOW_CHECK(t3, 5793, 4096, 218); \
    t4 -= (t3*5793 + 4096) >> 13; \
    /* 3393/8192 ~= Tan[Pi/8] ~= 0.414213562373095 */ \
    OD_DCT_OVERFLOW_CHECK(t4, 3393, 4096, 219); \
    t3 += (t4*3393 + 4096) >> 13; \
  } \
  while (0)

#define OD_IDST_8_ASYM_PR(t0, t4, t2, t6, t1, t5, t3, t7) \
  /* Embedded 8-point asymmetric Type-IV iDST. */ \
  do { \
    int t0h; \
    int t2h; \
    int t5h__; \
    int t7h__; \
    /* 3393/8192 ~= Tan[Pi/8] ~= 0.414213562373095 */ \
    t6 -= (t1*3393 + 4096) >> 13; \
    /* 5793/8192 ~= Sin[Pi/4] ~= 0.707106781186547 */ \
    t1 += (t6*5793 + 4096) >> 13; \
    /* 3393/8192 ~= Tan[Pi/8] ~= 0.414213562373095 */ \
    t6 -= (t1*3393 + 4096) >> 13; \
    /* 2737/4096 ~= Tan[3*Pi/16] ~= 0.668178637919299 */ \
    t5 -= (t2*2737 + 2048) >> 12; \
    /* 473/512 ~= Sin[3*Pi/8] ~= 0.923879532511287 */ \
    t2 += (t5*473 + 256) >> 9; \
    /* 2737/4096 ~= Tan[3*Pi/16] ~= 0.668178637919299 */ \
    t5 -= (t2*2737 + 2048) >> 12; \
    /* 3259/16384 ~= Tan[Pi/16] ~= 0.198912367379658 */ \
    t4 -= (t3*3259 + 8192) >> 14; \
    /* 3135/8192 ~= Sin[Pi/8] ~= 0.382683432365090 */ \
    t3 += (t4*3135 + 4096) >> 13; \
    /* 3259/16384 ~= Tan[Pi/16] ~= 0.198912367379658 */ \
    t4 -= (t3*3259 + 8192) >> 14; \
    t0 -= t6; \
    t0h = OD_RSHIFT1(t0); \
    t6 += t0h; \
    t2 = t3 - t2; \
    t2h = OD_RSHIFT1(t2); \
    t3 -= t2h; \
    t5 = t4 - t5; \
    t5h__ = OD_RSHIFT1(t5); \
    t4 -= t5h__; \
    t7 += t1; \
    t7h__ = OD_RSHIFT1(t7); \
    t1 = t7h__ - t1; \
    t3 = t7h__ - t3; \
    t7 -= t3; \
    t1 -= t5h__; \
    t5 += t1; \
    t6 -= t2h; \
    t2 += t6; \
    t4 += t0h; \
    t0 -= t4; \
    /* 2507/4096 ~= (1/Sqrt[2] - Sin[Pi/32])/Cos[Pi/32] */ \
    t7 -= (t0*2507 + 2048) >> 12; \
    /* 5765/4096 ~= Sqrt[2]*Cos[Pi/32] */ \
    t0 += (t7*5765 + 2048) >> 12; \
    /* 5417/8192 ~= (Sqrt[2] - Sin[Pi/32])/(2*Cos[Pi/32]) */ \
    t7 -= (t0*5417 + 4096) >> 13; \
    /* 3525/4096 ~= (Cos[3*Pi/32] - 1/Sqrt[2])/Sin[3*Pi/32] */ \
    t1 += (t6*3525 + 2048) >> 12; \
    /* 3363/8192 ~= Sqrt[2]*Sin[3*Pi/32] */ \
    t6 += (t1*3363 + 4096) >> 13; \
    /* 12905/16384 ~= (1/Sqrt[2] - Cos[3*Pi/32]/1)/Sin[3*Pi/32] */ \
    t1 -= (t6*12905 + 8192) >> 14; \
    /* 4379/16384 ~= (1/Sqrt[2] - Sin[5*Pi/32])/Cos[5*Pi/32] */ \
    t5 -= (t2*4379 + 8192) >> 14; \
    /* 10217/8192 ~= Sqrt[2]*Cos[5*Pi/32] */ \
    t2 += (t5*10217 + 4096) >> 13; \
    /* 4379/8192 ~= (Sqrt[2] - Sin[5*Pi/32])/(2*Cos[5*Pi/32]) */ \
    t5 -= (t2*4379 + 4096) >> 13; \
    /* 851/8192 ~= (Cos[7*Pi/32] - 1/Sqrt[2])/Sin[7*Pi/32] */ \
    t3 += (t4*851 + 4096) >> 13; \
    /* 3675/4096 ~= Sqrt[2]*Sin[7*Pi/32] */ \
    t4 += (t3*3675 + 2048) >> 12; \
    /* 1035/2048 ~= (Sqrt[2] - Cos[7*Pi/32])/(2*Sin[7*Pi/32]) */ \
    t3 -= (t4*1035 + 1024) >> 11; \
  } \
  while (0)

#define OD_FDST_8_ASYM_FLAT(r0, r0h, r1, r2, r2h, r3, \
  r4, r4h, r5, r6, r6h, r7) \
  /* Embedded 8-point asymmetric Type-IV fDST with flattened rotations. */ \
  do { \
    int t_; \
    int u_; \
    int r5h; \
    int r7h; \
    t_ = r3 - r4h; \
    /* 16305/16384 ~= (Sin[9*Pi/32] + Cos[9*Pi/32])/Sqrt[2] ~=
        0.9951847266721969 */ \
    u_ = (16305*r4 + 8192) >> 14; \
    /* 803/4096 ~= (Sin[9*Pi/32] - Cos[9*Pi/32])*Sqrt[2] ~=
        0.1960342806591213 */ \
    r4 = (803*r3 + 2048) >> 12; \
    /* 14699/16384 ~= Cos[9*Pi/32]*Sqrt[2] ~= 0.8971675863426364 */ \
    t_ = (14699*t_ + 8192) >> 14; \
    r3 = u_ + t_; \
    r4 += t_; \
    t_ = r2h + r5; \
    /* 31357/32768 ~= (Sin[11*Pi/32] + Cos[11*Pi/32])/Sqrt[2]
        ~= 0.9569403357322087 */ \
    u_ = (31357*r2 + 16384) >> 15; \
    /* 1189/2048 ~= (Sin[11*Pi/32] - Cos[11*Pi/32])*Sqrt[2] ~=
        0.5805693545089248 */ \
    r2 = (1189*r5 + 1024) >> 11; \
    /* 21845/32768 ~= Cos[11*Pi/32] ~= 0.6666556584777465 */ \
    t_ = (21845*t_ + 16384) >> 15; \
    r2 += t_; \
    r5 = u_ - t_; \
    t_ = r1 - r6h; \
    /* 28899/32768 ~= (Sin[13*Pi/32] + Cos[13*Pi/32])/Sqrt[2] ~=
        0.8819212643483548 */ \
    u_ = (28899*r6 + 16384) >> 15; \
    /* 30893/32768 ~= (Sin[13*Pi/32] - Cos[13*Pi/32])*Sqrt[2] ~=
        0.942793473651995297112775 */ \
    r6 = (30893*r1 + 16384) >> 15; \
    /* 3363/8192 ~= Cos[13*Pi/32]*Sqrt[2] ~= 0.41052452752235735 */ \
    t_ = (3363*t_ + 4096) >> 13; \
    r1 = u_ + t_; \
    r6 += t_; \
    t_ = r0h + r7; \
    /* 12665/16384 ~= (Sin[15*Pi/32] + Cos[15*Pi/32])/Sqrt[2] ~=
        0.773010453362737 */ \
    u_ = (12665*r0 + 8192) >> 14; \
    /* 5197/4096 ~= (Sin[15*Pi/32] - Cos[15*Pi/32])*Sqrt[2] ~=
        1.268786568327291 */ \
    r0 = (5197*r7 + 2048) >> 12; \
    /* 2271/16384 ~= Cos[15*Pi/32]*Sqrt[2] ~= 0.13861716919909148 */ \
    t_ = (2271*t_ + 8192) >> 14; \
    r0 += t_; \
    r7 = u_ - t_; \
    r2 -= r1; \
    r2h = OD_RSHIFT1(r2); \
    r1 += r2h; \
    r5 += r6; \
    r5h = OD_RSHIFT1(r5); \
    r6 -= r5h; \
    r0 += r3; \
    r0h = OD_RSHIFT1(r0); \
    r3 -= r0h; \
    r7 -= r4; \
    r7h = OD_RSHIFT1(r7); \
    r4 += r7h; \
    r3 += r5h; \
    r5 -= r3; \
    r1 -= r0h; \
    r0 += r1; \
    r4 += r2h; \
    r2 -= r4; \
    r6 += r7h; \
    r7 -= r6; \
    t_ = (r4 - r3 + 1) >> 1; \
    /* 21407/16384 ~= Sin[3*Pi/8] + Cos[3*Pi/8] ~= 1.3065629648763766 */ \
    u_ = (21407*r3 + 8192) >> 14; \
    /* 8867/16384 ~= Sin[3*Pi/8] - Cos[3*Pi/8] ~= 0.5411961001461969 */ \
    r3 = (8867*r4 + 8192) >> 14; \
    /* 3135/4096 ~= 2*Cos[3*Pi/8] ~= 0.7653668647301796 */ \
    t_ = (3135*t_ + 2048) >> 12; \
    r3 += t_; \
    r4 = u_ + t_; \
    t_ = (r2 - r5 + 1) >> 1; \
    /* 21407/16384 ~= Sin[3*Pi/8] + Cos[3*Pi/8] ~= 1.3065629648763766 */ \
    u_ = (21407*r2 + 8192) >> 14; \
    /* 8867/16384 ~= Sin[3*Pi/8] - Cos[3*Pi/8] ~= 0.5411961001461969 */ \
    r2 = (8867*r5 + 8192) >> 14; \
    /* 3135/4096 ~= 2*Cos[3*Pi/8] ~= 0.7653668647301796 */ \
    t_ = (3135*t_ + 2048) >> 12; \
    r5 = t_ - u_; \
    r2 -= t_; \
    t_ = (r6 - r1 + 1) >> 1; \
    /* 11585/8192 ~= 2*Sin[Pi/4] ~= 1.4142135623730951 */ \
    r6 = (11585*r1 + 4096) >> 13; \
    /* 11585/8192 ~= 2*Cos[Pi/4] ~= 1.4142135623730951 */ \
    r1 = (11585*t_ + 4096) >> 13; \
    r6 += r1; \
  } \
  while (0)

#define OD_IDST_8_ASYM_FLAT(r0, r4, r2, r6, r1, r5, r3, r7) \
  /* Embedded 8-point asymmetric Type-IV iDST with flattened rotations. */ \
  do { \
    int t_; \
    int u_; \
    int r0h; \
    int r2h; \
    int r5h; \
    int r7h; \
    t_ = (r1 + r6 + 1) >> 1; \
    /* 11585/8192 ~= 2*Sin[Pi/4] ~= 1.4142135623730951 */ \
    r1 = (11585*r6 + 4096) >> 13; \
    /* 11585/8192 ~= 2*Cos[Pi/4] ~= 1.4142135623730951 */ \
    r6 = (11585*t_ + 4096) >> 13; \
    r1 -= r6; \
    t_ = (r5 - r2 + 1) >> 1; \
    /* 21407/16384 ~= Sin[3*Pi/8] + Cos[3*Pi/8] ~= 1.3065629648763766 */ \
    u_ = (21407*r5 + 8192) >> 14; \
    /* 8867/16384 ~= Sin[3*Pi/8] - Cos[3*Pi/8] ~= 0.5411961001461969 */ \
    r5 = (8867*r2 + 8192) >> 14; \
    /* 3135/4096 ~= 2*Cos[3*Pi/8] ~= 0.7653668647301796 */ \
    t_ = (3135*t_ + 2048) >> 12; \
    r5 -= t_; \
    r2 = t_ - u_; \
    t_ = (r3 + r4 + 1) >> 1; \
    /* 21407/16384 ~= Sin[3*Pi/8] + Cos[3*Pi/8] ~= 1.3065629648763766 */ \
    u_ = (21407*r4 + 8192) >> 14; \
    /* 8867/16384 ~= Sin[3*Pi/8] - Cos[3*Pi/8] ~= 0.5411961001461969 */ \
    r4 = (8867*r3 + 8192) >> 14; \
    /* 3135/4096 ~= 2*Cos[3*Pi/8] ~= 0.7653668647301796 */ \
    t_ = (3135*t_ + 2048) >> 12; \
    r3 = u_ - t_; \
    r4 += t_; \
    r7 += r6; \
    r7h = OD_RSHIFT1(r7); \
    r6 -= r7h; \
    r2 += r4; \
    r2h = OD_RSHIFT1(r2); \
    r4 -= r2h; \
    r0 -= r1; \
    r0h = OD_RSHIFT1(r0); \
    r1 += r0h; \
    r5 += r3; \
    r5h = OD_RSHIFT1(r5); \
    r3 -= r5h; \
    r4 -= r7h; \
    r7 += r4; \
    r6 += r5h; \
    r5 -= r6; \
    r3 += r0h; \
    r0 -= r3; \
    r1 -= r2h; \
    r2 += r1; \
    t_ = r0 + r7; \
    /* 12665/16384 ~= (Sin[15*Pi/32] + Cos[15*Pi/32])/Sqrt[2] ~=
        0.773010453362737 */ \
    u_ = (12665*r0 + 8192) >> 14; \
    /* 5197/4096 ~= (Sin[15*Pi/32] - Cos[15*Pi/32])*Sqrt[2] ~=
        1.268786568327291 */ \
    r0 = (5197*r7 + 2048) >> 12; \
    /* 2271/16384 ~= Cos[15*Pi/32]*Sqrt[2] ~= 0.13861716919909148 */ \
    t_ = (2271*t_ + 8192) >> 14; \
    r7 = u_ - OD_RSHIFT1(t_); \
    r0 += t_; \
    t_ = r1 - r6; \
    /* 28899/32768 ~= (Sin[13*Pi/32] + Cos[13*Pi/32])/Sqrt[2] ~=
        0.8819212643483548 */ \
    u_ = (28899*r6 + 16384) >> 15; \
    /* 30893/32768 ~= (Sin[13*Pi/32] - Cos[13*Pi/32])*Sqrt[2] ~=
        0.942793473651995297112775 */ \
    r6 = (30893*r1 + 16384) >> 15; \
    /* 3363/8192 ~= Cos[13*Pi/32]*Sqrt[2] ~= 0.41052452752235735 */ \
    t_ = (3363*t_ + 4096) >> 13; \
    r1 = u_ + OD_RSHIFT1(t_); \
    r6 += t_; \
    t_ = r2 + r5; \
    /* 31357/32768 ~= (Sin[11*Pi/32] + Cos[11*Pi/32])/Sqrt[2]
        ~= 0.9569403357322087 */ \
    u_ = (31357*r2 + 16384) >> 15; \
    /* 1189/2048 ~= (Sin[11*Pi/32] - Cos[11*Pi/32])*Sqrt[2] ~=
        0.5805693545089248 */ \
    r2 = (1189*r5 + 1024) >> 11; \
    /* 21845/32768 ~= Cos[11*Pi/32] ~= 0.6666556584777465 */ \
    t_ = (21845*t_ + 16384) >> 15; \
    r5 = u_ - OD_RSHIFT1(t_); \
    r2 += t_; \
    t_ = r3 - r4; \
    /* 16305/16384 ~= (Sin[9*Pi/32] + Cos[9*Pi/32])/Sqrt[2] ~=
        0.9951847266721969 */ \
    u_ = (16305*r4 + 8192) >> 14; \
    /* 803/4096 ~= (Sin[9*Pi/32] - Cos[9*Pi/32])*Sqrt[2] ~=
        0.1960342806591213 */ \
    r4 = (803*r3 + 2048) >> 12; \
    /* 14699/16384 ~= Cos[9*Pi/32]*Sqrt[2] ~= 0.8971675863426364 */ \
    t_ = (14699*t_ + 8192) >> 14; \
    r3 = u_ + OD_RSHIFT1(t_); \
    r4 += t_; \
  } \
  while (0)

#define OD_FDCT_16_PR(s0, s8, s4, sc, s2, sa, s6, se, \
  s1, s9, s5, sd, s3, sb, s7, sf) \
  /* Embedded 16-point orthonormal Type-II fDCT. */ \
  do { \
    int s8h; \
    int sah; \
    int sch; \
    int seh; \
    int sfh; \
    sf = s0 - sf; \
    sfh = OD_RSHIFT1(sf); \
    s0 -= sfh; \
    se += s1; \
    seh = OD_RSHIFT1(se); \
    s1 = seh - s1; \
    sd = s2 - sd; \
    s2 -= OD_RSHIFT1(sd); \
    sc += s3; \
    sch = OD_RSHIFT1(sc); \
    s3 = sch - s3; \
    sb = s4 - sb; \
    s4 -= OD_RSHIFT1(sb); \
    sa += s5; \
    sah = OD_RSHIFT1(sa); \
    s5 = sah - s5; \
    s9 = s6 - s9; \
    s6 -= OD_RSHIFT1(s9); \
    s8 += s7; \
    s8h = OD_RSHIFT1(s8); \
    s7 = s8h - s7; \
    OD_FDCT_8_ASYM_PR(s0, s8, s8h, s4, sc, sch, s2, sa, sah, s6, se, seh); \
    OD_FDST_8_ASYM_PR(sf, s7, sb, s3, sd, s5, s9, s1); \
  } \
  while (0)

#define OD_IDCT_16_PR(s0, s8, s4, sc, s2, sa, s6, se, \
  s1, s9, s5, sd, s3, sb, s7, sf) \
  /* Embedded 16-point orthonormal Type-II iDCT. */ \
  do { \
    int s1h; \
    int s3h; \
    int s5h; \
    int s7h; \
    int sfh; \
    OD_IDST_8_ASYM_PR(sf, sb, sd, s9, se, sa, sc, s8); \
    OD_IDCT_8_ASYM_PR(s0, s4, s2, s6, s1, s1h, s5, s5h, s3, s3h, s7, s7h); \
    sfh = OD_RSHIFT1(sf); \
    s0 += sfh; \
    sf = s0 - sf; \
    se = s1h - se; \
    s1 -= se; \
    s2 += OD_RSHIFT1(sd); \
    sd = s2 - sd; \
    sc = s3h - sc; \
    s3 -= sc; \
    s4 += OD_RSHIFT1(sb); \
    sb = s4 - sb; \
    sa = s5h - sa; \
    s5 -= sa; \
    s6 += OD_RSHIFT1(s9); \
    s9 = s6 - s9; \
    s8 = s7h - s8; \
    s7 -= s8; \
  } \
  while (0)

#define OD_FDCT_16_FLAT(s0, s1, s2, s3, s4, s5, s6, s7, \
 s8, s9, sa, sb, sc, sd, se, sf) \
  /* Embedded 16-point orthonormal Type-II fDCT with flattened rotations. */ \
  do { \
    int s1h; \
    int s3h; \
    int s5h; \
    int s7h; \
    int s9h; \
    int sbh; \
    int sdh; \
    int sfh; \
    sf = s0 - sf; \
    sfh = OD_RSHIFT1(sf); \
    s0 -= sfh; \
    s1 += se; \
    s1h = OD_RSHIFT1(s1); \
    se -= s1h; \
    sd = s2 - sd; \
    sdh = OD_RSHIFT1(sd); \
    s2 -= sdh; \
    s3 += sc; \
    s3h = OD_RSHIFT1(s3); \
    sc -= s3h; \
    sb = s4 - sb; \
    sbh = OD_RSHIFT1(sb); \
    s4 -= sbh; \
    s5 += sa; \
    s5h = OD_RSHIFT1(s5); \
    sa -= s5h; \
    s9 = s6 - s9; \
    s9h = OD_RSHIFT1(s9); \
    s6 -= s9h; \
    s7 += s8; \
    s7h = OD_RSHIFT1(s7); \
    s8 -= s7h; \
    OD_FDCT_8_ASYM_FLAT(s0, s1, s1h, s2, s3, s3h, s4, s5, s5h, s6, s7, s7h); \
    OD_FDST_8_ASYM_FLAT(sf, sfh, se, sd, sdh, sc, sb, sbh, sa, s9, s9h, s8); \
  } \
  while (0)

#define OD_IDCT_16_FLAT(s0, s8, s4, sc, s2, sa, s6, se, \
 s1, s9, s5, sd, s3, sb, s7, sf) \
  /* Embedded 16-point orthonormal Type-II iDCT with flattened rotations. */ \
  do { \
    int s1h; \
    int s3h; \
    int s5h; \
    int s7h; \
    OD_IDST_8_ASYM_FLAT(sf, sb, sd, s9, se, sa, sc, s8); \
    OD_IDCT_8_ASYM_FLAT(s0, s4, s2, s6, s1, s1h, s5, s5h, s3, s3h, s7, s7h); \
    s8 += s7h; \
    s7 -= s8; \
    s6 += OD_RSHIFT1(s9); \
    s9 = s6 - s9; \
    sa += s5h; \
    s5 -= sa; \
    s4 += OD_RSHIFT1(sb); \
    sb = s4 - sb; \
    sc += s3h; \
    s3 -= sc; \
    s2 += OD_RSHIFT1(sd); \
    sd = s2 - sd; \
    se += s1h; \
    s1 -= se; \
    s0 += OD_RSHIFT1(sf); \
    sf = s0 - sf; \
  } \
  while (0)

#define OD_FDCT_16_ASYM_PR(t0, t8, t8h, t4, tc, tch, t2, ta, tah, t6, te, teh, \
  t1, t9, t9h, t5, td, tdh, t3, tb, tbh, t7, tf, tfh) \
  /* Embedded 16-point asymmetric Type-II fDCT. */ \
  do { \
    t0 += tfh; \
    tf = t0 - tf; \
    t1 -= teh; \
    te += t1; \
    t2 += tdh; \
    td = t2 - td; \
    t3 -= tch; \
    tc += t3; \
    t4 += tbh; \
    tb = t4 - tb; \
    t5 -= tah; \
    ta += t5; \
    t6 += t9h; \
    t9 = t6 - t9; \
    t7 -= t8h; \
    t8 += t7; \
    OD_FDCT_8_PR(t0, t8, t4, tc, t2, ta, t6, te); \
    OD_FDST_8_PR(tf, t7, tb, t3, td, t5, t9, t1); \
  } \
  while (0)

#define OD_IDCT_16_ASYM_PR(t0, t8, t4, tc, t2, ta, t6, te, \
  t1, t1h, t9, t9h, t5, t5h, td, tdh, t3, t3h, tb, tbh, t7, t7h, tf, tfh) \
  /* Embedded 16-point asymmetric Type-II iDCT. */ \
  do { \
    OD_IDST_8_PR(tf, tb, td, t9, te, ta, tc, t8); \
    OD_IDCT_8_PR(t0, t4, t2, t6, t1, t5, t3, t7); \
    t1 -= te; \
    t1h = OD_RSHIFT1(t1); \
    te += t1h; \
    t9 = t6 - t9; \
    t9h = OD_RSHIFT1(t9); \
    t6 -= t9h; \
    t5 -= ta; \
    t5h = OD_RSHIFT1(t5); \
    ta += t5h; \
    td = t2 - td; \
    tdh = OD_RSHIFT1(td); \
    t2 -= tdh; \
    t3 -= tc; \
    t3h = OD_RSHIFT1(t3); \
    tc += t3h; \
    tb = t4 - tb; \
    tbh = OD_RSHIFT1(tb); \
    t4 -= tbh; \
    t7 -= t8; \
    t7h = OD_RSHIFT1(t7); \
    t8 += t7h; \
    tf = t0 - tf; \
    tfh = OD_RSHIFT1(tf); \
    t0 -= tfh; \
  } \
  while (0)

#define OD_FDCT_16_ASYM_FLAT(t0, t8, t8h, t4, tc, tch, t2, ta, tah, t6, \
  te, teh, t1, t9, t9h, t5, td, tdh, t3, tb, tbh, t7, tf, tfh) \
  /* Embedded 16-point asymmetric Type-II fDCT with flattened rotations. */ \
  do { \
    t0 += tfh; \
    tf = t0 - tf; \
    t1 -= teh; \
    te += t1; \
    t2 += tdh; \
    td = t2 - td; \
    t3 -= tch; \
    tc += t3; \
    t4 += tbh; \
    tb = t4 - tb; \
    t5 -= tah; \
    ta += t5; \
    t6 += t9h; \
    t9 = t6 - t9; \
    t7 -= t8h; \
    t8 += t7; \
    OD_FDCT_8_FLAT(t0, t8, t4, tc, t2, ta, t6, te); \
    OD_FDST_8_FLAT(tf, t7, tb, t3, td, t5, t9, t1); \
  } \
  while (0)

#define OD_IDCT_16_ASYM_FLAT(t0, t8, t4, tc, t2, ta, t6, te, \
  t1, t1h, t9, t9h, t5, t5h, td, tdh, t3, t3h, tb, tbh, t7, t7h, tf, tfh) \
  /* Embedded 16-point asymmetric Type-II iDCT with flattened rotations. */ \
  do { \
    OD_IDST_8_FLAT(tf, tb, td, t9, te, ta, tc, t8); \
    OD_IDCT_8_FLAT(t0, t4, t2, t6, t1, t5, t3, t7); \
    t1 -= te; \
    t1h = OD_RSHIFT1(t1); \
    te += t1h; \
    t9 = t6 - t9; \
    t9h = OD_RSHIFT1(t9); \
    t6 -= t9h; \
    t5 -= ta; \
    t5h = OD_RSHIFT1(t5); \
    ta += t5h; \
    td = t2 - td; \
    tdh = OD_RSHIFT1(td); \
    t2 -= tdh; \
    t3 -= tc; \
    t3h = OD_RSHIFT1(t3); \
    tc += t3h; \
    tb = t4 - tb; \
    tbh = OD_RSHIFT1(tb); \
    t4 -= tbh; \
    t7 -= t8; \
    t7h = OD_RSHIFT1(t7); \
    t8 += t7h; \
    tf = t0 - tf; \
    tfh = OD_RSHIFT1(tf); \
    t0 -= tfh; \
  } \
  while (0)

#define OD_FDST_16_PR(s0, s8, s4, sc, s2, sa, s6, se, \
  s1, s9, s5, sd, s3, sb, s7, sf) \
  /* Embedded 16-point orthonormal Type-IV fDST. */ \
  do { \
    int s0h; \
    int s2h; \
    int sdh; \
    int sfh; \
    /* 13573/32768 ~= Tan[Pi/8] ~= 0.414213562373095 */ \
    OD_DCT_OVERFLOW_CHECK(s3, 13573, 16384, 220); \
    s1 += (se*13573 + 16384) >> 15; \
    /* 11585/16384 ~= Sin[Pi/4] ~= 0.707106781186547 */ \
    OD_DCT_OVERFLOW_CHECK(s1, 11585, 8192, 221); \
    se -= (s1*11585 + 8192) >> 14; \
    /* 13573/32768 ~= Tan[Pi/8] ~= 0.414213562373095 */ \
    OD_DCT_OVERFLOW_CHECK(s3, 13573, 16384, 222); \
    s1 += (se*13573 + 16384) >> 15; \
    /* 21895/32768 ~= Tan[3*Pi/16] ~= 0.668178637919299 */ \
    OD_DCT_OVERFLOW_CHECK(s2, 21895, 16384, 223); \
    sd += (s2*21895 + 16384) >> 15; \
    /* 15137/16384 ~= Sin[3*Pi/8] ~= 0.923879532511287 */ \
    OD_DCT_OVERFLOW_CHECK(sd, 15137, 16384, 224); \
    s2 -= (sd*15137 + 8192) >> 14; \
    /* 21895/32768 ~= Tan[3*Pi/16] ~= 0.668178637919299 */ \
    OD_DCT_OVERFLOW_CHECK(s2, 21895, 16384, 225); \
    sd += (s2*21895 + 16384) >> 15; \
    /* 3259/16384 ~= Tan[Pi/16] ~= 0.198912367379658 */ \
    OD_DCT_OVERFLOW_CHECK(s3, 3259, 8192, 226); \
    sc += (s3*3259 + 8192) >> 14; \
    /* 3135/8192 ~= Sin[Pi/8] ~= 0.382683432365090 */ \
    OD_DCT_OVERFLOW_CHECK(sc, 3135, 4096, 227); \
    s3 -= (sc*3135 + 4096) >> 13; \
    /* 3259/16384 ~= Tan[Pi/16] ~= 0.198912367379658 */ \
    OD_DCT_OVERFLOW_CHECK(s3, 3259, 8192, 228); \
    sc += (s3*3259 + 8192) >> 14; \
    /* 13573/32768 ~= Tan[Pi/8] ~= 0.414213562373095 */ \
    OD_DCT_OVERFLOW_CHECK(s5, 13573, 16384, 229); \
    sa += (s5*13573 + 16384) >> 15; \
    /* 11585/16384 ~= Sin[Pi/4] ~= 0.707106781186547 */ \
    OD_DCT_OVERFLOW_CHECK(sa, 11585, 8192, 230); \
    s5 -= (sa*11585 + 8192) >> 14; \
    /* 13573/32768 ~= Tan[Pi/8] ~= 0.414213562373095 */ \
    OD_DCT_OVERFLOW_CHECK(s5, 13573, 16384, 231); \
    sa += (s5*13573 + 16384) >> 15; \
    /* 13573/32768 ~= Tan[pi/8] ~= 0.414213562373095 */ \
    OD_DCT_OVERFLOW_CHECK(s9, 13573, 16384, 232); \
    s6 += (s9*13573 + 16384) >> 15; \
    /* 11585/16384 ~= Sin[pi/4] ~= 0.707106781186547 */ \
    OD_DCT_OVERFLOW_CHECK(s6, 11585, 8192, 233); \
    s9 -= (s6*11585 + 8192) >> 14; \
    /* 13573/32768 ~= Tan[pi/8] ~= 0.414213562373095 */ \
    OD_DCT_OVERFLOW_CHECK(s9, 13573, 16384, 234); \
    s6 += (s9*13573 + 16384) >> 15; \
    sf += se; \
    sfh = OD_RSHIFT1(sf); \
    se = sfh - se; \
    s0 += s1; \
    s0h = OD_RSHIFT1(s0); \
    s1 = s0h - s1; \
    s2 = s3 - s2; \
    s2h = OD_RSHIFT1(s2); \
    s3 -= s2h; \
    sd -= sc; \
    sdh = OD_RSHIFT1(sd); \
    sc += sdh; \
    sa = s4 - sa; \
    s4 -= OD_RSHIFT1(sa); \
    s5 += sb; \
    sb = OD_RSHIFT1(s5) - sb; \
    s8 += s6; \
    s6 -= OD_RSHIFT1(s8); \
    s7 = s9 - s7; \
    s9 -= OD_RSHIFT1(s7); \
    /* 6723/8192 ~= Tan[7*Pi/32] ~= 0.820678790828660 */ \
    OD_DCT_OVERFLOW_CHECK(sb, 6723, 4096, 235); \
    s4 += (sb*6723 + 4096) >> 13; \
    /* 16069/16384 ~= Sin[7*Pi/16] ~= 0.980785280403230 */ \
    OD_DCT_OVERFLOW_CHECK(s4, 16069, 8192, 236); \
    sb -= (s4*16069 + 8192) >> 14; \
    /* 6723/8192 ~= Tan[7*Pi/32]) ~= 0.820678790828660 */ \
    OD_DCT_OVERFLOW_CHECK(sb, 6723, 4096, 237); \
    s4 += (sb*6723 + 4096) >> 13; \
    /* 8757/16384 ~= Tan[5*Pi/32]) ~= 0.534511135950792 */ \
    OD_DCT_OVERFLOW_CHECK(s5, 8757, 8192, 238); \
    sa += (s5*8757 + 8192) >> 14; \
    /* 6811/8192 ~= Sin[5*Pi/16] ~= 0.831469612302545 */ \
    OD_DCT_OVERFLOW_CHECK(sa, 6811, 4096, 239); \
    s5 -= (sa*6811 + 4096) >> 13; \
    /* 8757/16384 ~= Tan[5*Pi/32] ~= 0.534511135950792 */ \
    OD_DCT_OVERFLOW_CHECK(s5, 8757, 8192, 240); \
    sa += (s5*8757 + 8192) >> 14; \
    /* 2485/8192 ~= Tan[3*Pi/32] ~= 0.303346683607342 */ \
    OD_DCT_OVERFLOW_CHECK(s9, 2485, 4096, 241); \
    s6 += (s9*2485 + 4096) >> 13; \
    /* 4551/8192 ~= Sin[3*Pi/16] ~= 0.555570233019602 */ \
    OD_DCT_OVERFLOW_CHECK(s6, 4551, 4096, 242); \
    s9 -= (s6*4551 + 4096) >> 13; \
    /* 2485/8192 ~= Tan[3*Pi/32] ~= 0.303346683607342 */ \
    OD_DCT_OVERFLOW_CHECK(s9, 2485, 4096, 243); \
    s6 += (s9*2485 + 4096) >> 13; \
    /* 3227/32768 ~= Tan[Pi/32] ~= 0.09849140335716425 */ \
    OD_DCT_OVERFLOW_CHECK(s8, 3227, 16384, 244); \
    s7 += (s8*3227 + 16384) >> 15; \
    /* 6393/32768 ~= Sin[Pi/16] ~= 0.19509032201612825 */ \
    OD_DCT_OVERFLOW_CHECK(s7, 6393, 16384, 245); \
    s8 -= (s7*6393 + 16384) >> 15; \
    /* 3227/32768 ~= Tan[Pi/32] ~= 0.09849140335716425 */ \
    OD_DCT_OVERFLOW_CHECK(s8, 3227, 16384, 246); \
    s7 += (s8*3227 + 16384) >> 15; \
    s1 -= s2h; \
    s2 += s1; \
    se += sdh; \
    sd = se - sd; \
    s3 += sfh; \
    sf -= s3; \
    sc = s0h - sc; \
    s0 -= sc; \
    sb += OD_RSHIFT1(s8); \
    s8 = sb - s8; \
    s4 += OD_RSHIFT1(s7); \
    s7 -= s4; \
    s6 += OD_RSHIFT1(s5); \
    s5 = s6 - s5; \
    s9 -= OD_RSHIFT1(sa); \
    sa += s9; \
    s8 += s0; \
    s0 -= OD_RSHIFT1(s8); \
    sf += s7; \
    s7 = OD_RSHIFT1(sf) - s7; \
    s1 -= s6; \
    s6 += OD_RSHIFT1(s1); \
    s9 += se; \
    se = OD_RSHIFT1(s9) - se; \
    s2 += sa; \
    sa = OD_RSHIFT1(s2) - sa; \
    s5 += sd; \
    sd -= OD_RSHIFT1(s5); \
    s4 = sc - s4; \
    sc -= OD_RSHIFT1(s4); \
    s3 -= sb; \
    sb += OD_RSHIFT1(s3); \
    /* 2799/4096 ~= (1/Sqrt[2] - Cos[31*Pi/64]/2)/Sin[31*Pi/64] */ \
    OD_DCT_OVERFLOW_CHECK(sf, 2799, 2048, 247); \
    s0 -= (sf*2799 + 2048) >> 12; \
    /* 2893/2048 ~= Sqrt[2]*Sin[31*Pi/64] */ \
    OD_DCT_OVERFLOW_CHECK(s0, 2893, 1024, 248); \
    sf += (s0*2893 + 1024) >> 11; \
    /* 5397/8192 ~= (Cos[Pi/4] - Cos[31*Pi/64])/Sin[31*Pi/64] */ \
    OD_DCT_OVERFLOW_CHECK(sf, 5397, 4096, 249); \
    s0 -= (sf*5397 + 4096) >> 13; \
    /* 41/64 ~= (1/Sqrt[2] - Cos[29*Pi/64]/2)/Sin[29*Pi/64] */ \
    OD_DCT_OVERFLOW_CHECK(s1, 41, 32, 250); \
    se += (s1*41 + 32) >> 6; \
    /* 2865/2048 ~= Sqrt[2]*Sin[29*Pi/64] */ \
    OD_DCT_OVERFLOW_CHECK(se, 2865, 1024, 251); \
    s1 -= (se*2865 + 1024) >> 11; \
    /* 4641/8192 ~= (1/Sqrt[2] - Cos[29*Pi/64])/Sin[29*Pi/64] */ \
    OD_DCT_OVERFLOW_CHECK(s1, 4641, 4096, 252); \
    se += (s1*4641 + 4096) >> 13; \
    /* 2473/4096 ~= (1/Sqrt[2] - Cos[27*Pi/64]/2)/Sin[27*Pi/64] */ \
    OD_DCT_OVERFLOW_CHECK(s2, 2473, 2048, 253); \
    sd += (s2*2473 + 2048) >> 12; \
    /* 5619/4096 ~= Sqrt[2]*Sin[27*Pi/64] */ \
    OD_DCT_OVERFLOW_CHECK(sd, 5619, 2048, 254); \
    s2 -= (sd*5619 + 2048) >> 12; \
    /* 7839/16384 ~= (1/Sqrt[2] - Cos[27*Pi/64])/Sin[27*Pi/64] */ \
    OD_DCT_OVERFLOW_CHECK(s2, 7839, 8192, 255); \
    sd += (s2*7839 + 8192) >> 14; \
    /* 5747/8192 ~= (1/Sqrt[2] - Cos[7*Pi/64]/2)/Sin[7*Pi/64] */ \
    OD_DCT_OVERFLOW_CHECK(s3, 5747, 4096, 256); \
    sc -= (s3*5747 + 4096) >> 13; \
    /* 3903/8192 ~= Sqrt[2]*Sin[7*Pi/64] ~= */ \
    OD_DCT_OVERFLOW_CHECK(sc, 3903, 4096, 257); \
    s3 += (sc*3903 + 4096) >> 13; \
    /* 5701/8192 ~= (1/Sqrt[2] - Cos[7*Pi/64])/Sin[7*Pi/64] */ \
    OD_DCT_OVERFLOW_CHECK(s3, 5701, 4096, 258); \
    sc += (s3*5701 + 4096) >> 13; \
    /* 4471/8192 ~= (1/Sqrt[2] - Cos[23*Pi/64]/2)/Sin[23*Pi/64] */ \
    OD_DCT_OVERFLOW_CHECK(s4, 4471, 4096, 259); \
    sb += (s4*4471 + 4096) >> 13; \
    /* 1309/1024 ~= Sqrt[2]*Sin[23*Pi/64] */ \
    OD_DCT_OVERFLOW_CHECK(sb, 1309, 512, 260); \
    s4 -= (sb*1309 + 512) >> 10; \
    /* 5067/16384 ~= (1/Sqrt[2] - Cos[23*Pi/64])/Sin[23*Pi/64] */ \
    OD_DCT_OVERFLOW_CHECK(s4, 5067, 8192, 261); \
    sb += (s4*5067 + 8192) >> 14; \
    /* 2217/4096 ~= (1/Sqrt[2] - Cos[11*Pi/64]/2)/Sin[11*Pi/64] */ \
    OD_DCT_OVERFLOW_CHECK(s5, 2217, 2048, 262); \
    sa -= (s5*2217 + 2048) >> 12; \
    /* 1489/2048 ~= Sqrt[2]*Sin[11*Pi/64] ~= 0.72705107329128 */ \
    OD_DCT_OVERFLOW_CHECK(sa, 1489, 1024, 263); \
    s5 += (sa*1489 + 1024) >> 11; \
    /* 75/256 ~= (1/Sqrt[2] - Cos[11*Pi/64])/Sin[11*Pi/64] */ \
    OD_DCT_OVERFLOW_CHECK(s5, 75, 128, 264); \
    sa += (s5*75 + 128) >> 8; \
    /* 2087/4096 ~= (1/Sqrt[2] - Cos[19*Pi/64]/2)/Sin[19*Pi/64] */ \
    OD_DCT_OVERFLOW_CHECK(s9, 2087, 2048, 265); \
    s6 -= (s9*2087 + 2048) >> 12; \
    /* 4653/4096 ~= Sqrt[2]*Sin[19*Pi/64] */ \
    OD_DCT_OVERFLOW_CHECK(s6, 4653, 2048, 266); \
    s9 += (s6*4653 + 2048) >> 12; \
    /* 4545/32768 ~= (1/Sqrt[2] - Cos[19*Pi/64])/Sin[19*Pi/64] */ \
    OD_DCT_OVERFLOW_CHECK(s9, 4545, 16384, 267); \
    s6 -= (s9*4545 + 16384) >> 15; \
    /* 2053/4096 ~= (1/Sqrt[2] - Cos[15*Pi/64]/2)/Sin[15*Pi/64] */ \
    OD_DCT_OVERFLOW_CHECK(s8, 2053, 2048, 268); \
    s7 += (s8*2053 + 2048) >> 12; \
    /* 1945/2048 ~= Sqrt[2]*Sin[15*Pi/64] */ \
    OD_DCT_OVERFLOW_CHECK(s7, 1945, 1024, 269); \
    s8 -= (s7*1945 + 1024) >> 11; \
    /* 1651/32768 ~= (1/Sqrt[2] - Cos[15*Pi/64])/Sin[15*Pi/64] */ \
    OD_DCT_OVERFLOW_CHECK(s8, 1651, 16384, 270); \
    s7 -= (s8*1651 + 16384) >> 15; \
  } \
  while (0)

#define OD_IDST_16_PR(s0, s8, s4, sc, s2, sa, s6, se, \
  s1, s9, s5, sd, s3, sb, s7, sf) \
  /* Embedded 16-point orthonormal Type-IV iDST. */ \
  do { \
    int s0h; \
    int s4h; \
    int sbh; \
    int sfh; \
    /* 1651/32768 ~= (1/Sqrt[2] - Cos[15*Pi/64])/Sin[15*Pi/64] */ \
    se += (s1*1651 + 16384) >> 15; \
    /* 1945/2048 ~= Sqrt[2]*Sin[15*Pi/64] */ \
    s1 += (se*1945 + 1024) >> 11; \
    /* 2053/4096 ~= (1/Sqrt[2] - Cos[15*Pi/64]/2)/Sin[15*Pi/64] */ \
    se -= (s1*2053 + 2048) >> 12; \
    /* 4545/32768 ~= (1/Sqrt[2] - Cos[19*Pi/64])/Sin[19*Pi/64] */ \
    s6 += (s9*4545 + 16384) >> 15; \
    /* 4653/32768 ~= Sqrt[2]*Sin[19*Pi/64] */ \
    s9 -= (s6*4653 + 2048) >> 12; \
    /* 2087/4096 ~= (1/Sqrt[2] - Cos[19*Pi/64]/2)/Sin[19*Pi/64] */ \
    s6 += (s9*2087 + 2048) >> 12; \
    /* 75/256 ~= (1/Sqrt[2] - Cos[11*Pi/64])/Sin[11*Pi/64] */ \
    s5 -= (sa*75 + 128) >> 8; \
    /* 1489/2048 ~= Sqrt[2]*Sin[11*Pi/64] */ \
    sa -= (s5*1489 + 1024) >> 11; \
    /* 2217/4096 ~= (1/Sqrt[2] - Cos[11*Pi/64]/2)/Sin[11*Pi/64] */ \
    s5 += (sa*2217 + 2048) >> 12; \
    /* 5067/16384 ~= (1/Sqrt[2] - Cos[23*Pi/64])/Sin[23*Pi/64] */ \
    sd -= (s2*5067 + 8192) >> 14; \
    /* 1309/1024 ~= Sqrt[2]*Sin[23*Pi/64] */ \
    s2 += (sd*1309 + 512) >> 10; \
    /* 4471/8192 ~= (1/Sqrt[2] - Cos[23*Pi/64]/2)/Sin[23*Pi/64] */ \
    sd -= (s2*4471 + 4096) >> 13; \
    /* 5701/8192 ~= (1/Sqrt[2] - Cos[7*Pi/64])/Sin[7*Pi/64] */  \
    s3 -= (sc*5701 + 4096) >> 13; \
    /* 3903/8192 ~= Sqrt[2]*Sin[7*Pi/64] */ \
    sc -= (s3*3903 + 4096) >> 13; \
    /* 5747/8192 ~= (1/Sqrt[2] - Cos[7*Pi/64]/2)/Sin[7*Pi/64] */ \
    s3 += (sc*5747 + 4096) >> 13; \
    /* 7839/16384 ~= (1/Sqrt[2] - Cos[27*Pi/64])/Sin[27*Pi/64] */ \
    sb -= (s4*7839 + 8192) >> 14; \
    /* 5619/4096 ~= Sqrt[2]*Sin[27*Pi/64] */ \
    s4 += (sb*5619 + 2048) >> 12; \
    /* 2473/4096 ~= (1/Sqrt[2] - Cos[27*Pi/64]/2)/Sin[27*Pi/64] */ \
    sb -= (s4*2473 + 2048) >> 12; \
    /* 4641/8192 ~= (1/Sqrt[2] - Cos[29*Pi/64])/Sin[29*Pi/64] */ \
    s7 -= (s8*4641 + 4096) >> 13; \
    /* 2865/2048 ~= Sqrt[2]*Sin[29*Pi/64] */ \
    s8 += (s7*2865 + 1024) >> 11; \
    /* 41/64 ~= (1/Sqrt[2] - Cos[29*Pi/64]/2)/Sin[29*Pi/64] */ \
    s7 -= (s8*41 + 32) >> 6; \
    /* 5397/8192 ~= (Cos[Pi/4] - Cos[31*Pi/64])/Sin[31*Pi/64] */ \
    s0 += (sf*5397 + 4096) >> 13; \
    /* 2893/2048 ~= Sqrt[2]*Sin[31*Pi/64] */ \
    sf -= (s0*2893 + 1024) >> 11; \
    /* 2799/4096 ~= (1/Sqrt[2] - Cos[31*Pi/64]/2)/Sin[31*Pi/64] */ \
    s0 += (sf*2799 + 2048) >> 12; \
    sd -= OD_RSHIFT1(sc); \
    sc += sd; \
    s3 += OD_RSHIFT1(s2); \
    s2 = s3 - s2; \
    sb += OD_RSHIFT1(sa); \
    sa -= sb; \
    s5 = OD_RSHIFT1(s4) - s5; \
    s4 -= s5; \
    s7 = OD_RSHIFT1(s9) - s7; \
    s9 -= s7; \
    s6 -= OD_RSHIFT1(s8); \
    s8 += s6; \
    se = OD_RSHIFT1(sf) - se; \
    sf -= se; \
    s0 += OD_RSHIFT1(s1); \
    s1 -= s0; \
    s5 -= s9; \
    s9 += OD_RSHIFT1(s5); \
    sa = s6 - sa; \
    s6 -= OD_RSHIFT1(sa); \
    se += s2; \
    s2 -= OD_RSHIFT1(se); \
    s1 = sd - s1; \
    sd -= OD_RSHIFT1(s1); \
    s0 += s3; \
    s0h = OD_RSHIFT1(s0); \
    s3 = s0h - s3; \
    sf += sc; \
    sfh = OD_RSHIFT1(sf); \
    sc -= sfh; \
    sb = s7 - sb; \
    sbh = OD_RSHIFT1(sb); \
    s7 -= sbh; \
    s4 -= s8; \
    s4h = OD_RSHIFT1(s4); \
    s8 += s4h; \
    /* 3227/32768 ~= Tan[Pi/32] ~= 0.09849140335716425 */ \
    se -= (s1*3227 + 16384) >> 15; \
    /* 6393/32768 ~= Sin[Pi/16] ~= 0.19509032201612825 */ \
    s1 += (se*6393 + 16384) >> 15; \
    /* 3227/32768 ~= Tan[Pi/32] ~= 0.09849140335716425 */ \
    se -= (s1*3227 + 16384) >> 15; \
    /* 2485/8192 ~= Tan[3*Pi/32] ~= 0.303346683607342 */ \
    s6 -= (s9*2485 + 4096) >> 13; \
    /* 4551/8192 ~= Sin[3*Pi/16] ~= 0.555570233019602 */ \
    s9 += (s6*4551 + 4096) >> 13; \
    /* 2485/8192 ~= Tan[3*Pi/32] ~= 0.303346683607342 */ \
    s6 -= (s9*2485 + 4096) >> 13; \
    /* 8757/16384 ~= Tan[5*Pi/32] ~= 0.534511135950792 */ \
    s5 -= (sa*8757 + 8192) >> 14; \
    /* 6811/8192 ~= Sin[5*Pi/16] ~= 0.831469612302545 */ \
    sa += (s5*6811 + 4096) >> 13; \
    /* 8757/16384 ~= Tan[5*Pi/32]) ~= 0.534511135950792 */ \
    s5 -= (sa*8757 + 8192) >> 14; \
    /* 6723/8192 ~= Tan[7*Pi/32]) ~= 0.820678790828660 */ \
    s2 -= (sd*6723 + 4096) >> 13; \
    /* 16069/16384 ~= Sin[7*Pi/16] ~= 0.980785280403230 */ \
    sd += (s2*16069 + 8192) >> 14; \
    /* 6723/8192 ~= Tan[7*Pi/32] ~= 0.820678790828660 */ \
    s2 -= (sd*6723 + 4096) >> 13; \
    s9 += OD_RSHIFT1(se); \
    se = s9 - se; \
    s6 += OD_RSHIFT1(s1); \
    s1 -= s6; \
    sd = OD_RSHIFT1(sa) - sd; \
    sa -= sd; \
    s2 += OD_RSHIFT1(s5); \
    s5 = s2 - s5; \
    s3 -= sbh; \
    sb += s3; \
    sc += s4h; \
    s4 = sc - s4; \
    s8 = s0h - s8; \
    s0 -= s8; \
    s7 = sfh - s7; \
    sf -= s7; \
    /* 13573/32768 ~= Tan[pi/8] ~= 0.414213562373095 */ \
    s6 -= (s9*13573 + 16384) >> 15; \
    /* 11585/16384 ~= Sin[pi/4] ~= 0.707106781186547 */ \
    s9 += (s6*11585 + 8192) >> 14; \
    /* 13573/32768 ~= Tan[pi/8] ~= 0.414213562373095 */ \
    s6 -= (s9*13573 + 16384) >> 15; \
    /* 13573/32768 ~= Tan[pi/8] ~= 0.414213562373095 */ \
    s5 -= (sa*13573 + 16384) >> 15; \
    /* 11585/16384 ~= Sin[pi/4] ~= 0.707106781186547 */ \
    sa += (s5*11585 + 8192) >> 14; \
    /* 13573/32768 ~= Tan[pi/8] ~= 0.414213562373095 */ \
    s5 -= (sa*13573 + 16384) >> 15; \
    /* 3259/16384 ~= Tan[Pi/16] ~= 0.198912367379658 */ \
    s3 -= (sc*3259 + 8192) >> 14; \
    /* 3135/8192 ~= Sin[Pi/8] ~= 0.382683432365090 */ \
    sc += (s3*3135 + 4096) >> 13; \
    /* 3259/16384 ~= Tan[Pi/16] ~= 0.198912367379658 */ \
    s3 -= (sc*3259 + 8192) >> 14; \
    /* 21895/32768 ~= Tan[3*Pi/16] ~= 0.668178637919299 */ \
    sb -= (s4*21895 + 16384) >> 15; \
    /* 15137/16384 ~= Sin[3*Pi/8] ~= 0.923879532511287 */ \
    s4 += (sb*15137 + 8192) >> 14; \
    /* 21895/32768 ~= Tan[3*Pi/16] ~= 0.668178637919299 */ \
    sb -= (s4*21895 + 16384) >> 15; \
    /* 13573/32768 ~= Tan[pi/8] ~= 0.414213562373095 */ \
    s8 -= (s7*13573 + 16384) >> 15; \
    /* 11585/16384 ~= Sin[pi/4] ~= 0.707106781186547 */ \
    s7 += (s8*11585 + 8192) >> 14; \
    /* 13573/32768 ~= Tan[pi/8] ~= 0.414213562373095 */ \
    s8 -= (s7*13573 + 16384) >> 15; \
  } \
  while (0)

#define OD_FDST_16_FLAT(s0, s8, s4, sc, s2, sa, s6, se, \
  s1, s9, s5, sd, s3, sb, s7, sf) \
  /* Embedded 16-point orthonormal Type-IV fDST with flattened rotations. */ \
  do { \
    int t_; \
    int u_; \
    int s0h; \
    int s4h; \
    int sbh; \
    int sfh; \
    t_ = s1 + se; \
    /* 32729/32768 ~= (Sin[17*Pi/64] + Cos[17*Pi/64])/Sqrt[2] ~=
        0.9987954562051723 */ \
    u_ = (se*32729 + 16384) >> 15; \
    /* 201/2048 ~= (Sin[17*Pi/64] - Cos[17*Pi/64])*Sqrt[2] ~=
        0.09813534865483615 */ \
    se = (s1*201 + 1024) >> 11; \
    /* 31121/32768 ~= Cos[17*Pi/64]*Sqrt[2] = 0.9497277818777543 */ \
    t_ = (t_*31121 + 16384) >> 15; \
    se += t_; \
    s1 = u_ - OD_RSHIFT1(t_); \
    t_ = s6 - s9; \
    /* 32413/32768 ~= (Sin[19*Pi/64] + Cos[19*Pi/64])/Sqrt[2] ~=
        0.9891765099647809 */ \
    u_ = (s9*32413 + 16384) >> 15; \
    /* 601/2048 ~= (Sin[19*Pi/64] - Cos[19*Pi/64])*Sqrt[2]
        ~= 0.29346094891072355 */ \
    s9 = (s6*601 + 1024) >> 11; \
    /* 27605/32768 ~= Cos[19*Pi/64]*Sqrt[2] = 0.8424460355094193 */ \
    t_ = (t_*27605 + 16384) >> 15; \
    s9 += t_; \
    s6 = u_ + OD_RSHIFT1(t_); \
    t_ = s5 + sa; \
    /* 15893/16384 ~= (Sin[21*Pi/64] + Cos[21*Pi/64])/Sqrt[2] ~=
        0.970031253194544 */ \
    u_ = (sa*15893 + 8192) >> 14; \
    /* 3981/8192 ~= (Sin[21*Pi/64] - Cos[21*Pi/64])*Sqrt[2] ~=
        0.48596035980652796 */ \
    sa = (s5*3981 + 4096) >> 13; \
    /* 1489/2048 ~= Cos[21*Pi/64]*Sqrt[2] ~= 0.72705107329128 */ \
    t_ = (t_*1489 + 1024) >> 11; \
    sa += t_; \
    s5 = OD_RSHIFT1(t_) - u_; \
    t_ = sd - s2; \
    /* 30853/32768 ~= (Sin[23*Pi/64] + Cos[23*Pi/64])/Sqrt[2] ~=
        0.9415440651830208 */ \
    u_ = (sd*30853 + 16384) >> 15; \
    /* 11039/16384 ~= (Sin[23*Pi/64] - Cos[23*Pi/64])*Sqrt[2] ~=
        0.6737797067844402 */ \
    sd = (s2*11039 + 8192) >> 14; \
    /* 19813/32768 ~= Cos[23*Pi/64]*Sqrt[2] ~= 0.6046542117908008 */ \
    t_ = (t_*19813 + 16384) >> 15; \
    sd -= t_; \
    s2 = OD_RSHIFT1(t_) - u_; \
    t_ = s3 + sc; \
    /* 14811/16384 ~= (Sin[25*Pi/64] + Cos[25*Pi/64])/Sqrt[2] ~=
        0.9039892931234433 */ \
    u_ = (sc*14811 + 8192) >> 14; \
    /* 7005/8192 ~= (Sin[25*Pi/64] - Cos[25*Pi/64])*Sqrt[2] ~=
        0.8551101868605642 */ \
    sc = (s3*7005 + 4096) >> 13; \
    /* 3903/8192 ~= Cos[25*Pi/64]*Sqrt[2] ~= 0.47643419969316125 */ \
    t_ = (t_*3903 + 4096) >> 13; \
    sc += t_; \
    s3 = u_ - OD_RSHIFT1(t_); \
    t_ = sb - s4; \
    /* 14053/16384 ~= (Sin[27*Pi/64] + Cos[27*Pi/64])/Sqrt[2] ~=
        0.857728610000272 */ \
    u_ = (sb*14053 + 8192) >> 14; \
    /* 8423/8192 ~= (Sin[27*Pi/64] - Cos[27*Pi/64])*Sqrt[2] ~=
        1.0282054883864435 */ \
    sb = (s4*8423 + 4096) >> 13; \
    /* 2815/8192 ~= Cos[27*Pi/64]*Sqrt[2] = 0.34362586580705035 */ \
    t_ = (t_*2815 + 4096) >> 13; \
    sb -= t_; \
    s4 = OD_RSHIFT1(t_) - u_; \
    t_ = s7 + s8; \
    /* 1645/2048 ~= (Sin[29*Pi/64] + Cos[29*Pi/64])/Sqrt[2] ~=
        0.8032075314806449 */ \
    u_ = (s8*1645 + 1024) >> 11; \
    /* 305/256 ~= (Sin[29*Pi/64] - Cos[29*Pi/64])*Sqrt[2] ~=
        1.1913986089848667 */ \
    s8 = (s7*305 + 128) >> 8; \
    /* 425/2048 ~= Cos[29*Pi/64]*Sqrt[2] ~= 0.20750822698821159 */ \
    t_ = (t_*425 + 1024) >> 11; \
    s8 += t_; \
    s7 = u_ - OD_RSHIFT1(t_); \
    t_ = s0 - sf; \
    /* 24279/32768 ~= (Sin[31*Pi/64] + Cos[31*Pi/64])/Sqrt[2] ~=
        0.7409511253549591 */ \
    u_ = (sf*24279 + 16384) >> 15; \
    /* 44011/32768 ~= (Sin[31*Pi/64] - Cos[31*Pi/64])*Sqrt[2] ~=
        1.3431179096940369 */ \
    sf = (s0*44011 + 16384) >> 15; \
    /* 1137/16384 ~= Cos[31*Pi/64]*Sqrt[2] ~= 0.0693921705079406 */ \
    t_ = (t_*1137 + 8192) >> 14; \
    s0 = u_ + OD_RSHIFT1(t_); \
    sf += t_; \
    s3 -= OD_RSHIFT1(sd); \
    sd += s3; \
    s2 += OD_RSHIFT1(sc); \
    sc -= s2; \
    s5 -= OD_RSHIFT1(sb); \
    sb += s5; \
    s4 -= OD_RSHIFT1(sa); \
    sa += s4; \
    s1 += OD_RSHIFT1(sf); \
    sf -= s1; \
    s7 -= OD_RSHIFT1(s9); \
    s9 += s7; \
    s6 -= OD_RSHIFT1(s8); \
    s8 += s6; \
    s0 += OD_RSHIFT1(se); \
    se -= s0; \
    sa -= s9; \
    s9 += OD_RSHIFT1(sa); \
    s5 += s6; \
    s6 -= OD_RSHIFT1(s5); \
    s1 -= s2; \
    s2 += OD_RSHIFT1(s1); \
    se += sd; \
    sd -= OD_RSHIFT1(se); \
    s0 += sc; \
    s0h = OD_RSHIFT1(s0); \
    sc -= s0h; \
    sf -= s3; \
    sfh = OD_RSHIFT1(sf); \
    s3 += sfh; \
    sb += s7; \
    sbh = OD_RSHIFT1(sb); \
    s7 -= sbh; \
    s4 += s8; \
    s4h = OD_RSHIFT1(s4); \
    s8 -= s4h; \
    t_ = OD_PAVG(s1, se); \
    /* 9633/8192 ~= Sin[7*Pi/16] + Cos[7*Pi/16] ~= 1.1758756024193586 */ \
    u_ = (s1*9633 + 4096) >> 13; \
    /* 12873/16384 ~= Sin[7*Pi/16] - Cos[7*Pi/16] ~= 0.7856949583871022 */ \
    s1 = (se*12873 + 8192) >> 14; \
    /* 12785/32768 ~= 2*Cos[7*Pi/16] ~= 0.3901806440322565 */ \
    t_ = (t_*12785 + 16384) >> 15; \
    s1 += t_; \
    se = u_ - t_; \
    t_ = s6 + s9; \
    /* 45451/32768 ~= Sin[5*Pi/16] + Cos[5*Pi/16] ~= 1.3870398453221475 */ \
    u_ = (s9*45451 + 16384) >> 15; \
    /* 9041/32768 ~= Sin[5*Pi/16] - Cos[5*Pi/16] ~= 0.27589937928294306 */ \
    s9 = (s6*9041 + 16384) >> 15; \
    /* 18205/32768 ~= Cos[5*Pi/16] ~= 0.5555702330196022 */ \
    t_ = (t_*18205 + 16384) >> 15; \
    s9 += t_; \
    s6 = u_ - t_; \
    t_ = OD_PAVG(s5, sa); \
    /* 22725/16384 ~= Sin[5*Pi/16] + Cos[5*Pi/16] ~= 1.3870398453221475 */ \
    u_ = (sa*22725 + 8192) >> 14; \
    /* 9041/32768 ~= Sin[5*Pi/16] - Cos[5*Pi/16] ~= 0.27589937928294306 */ \
    sa = (s5*9041 + 16384) >> 15; \
    /* 18205/16384 ~= 2*Cos[5*Pi/16] ~= 1.1111404660392044 */ \
    t_ = (t_*18205 + 8192) >> 14; \
    sa += t_; \
    s5 = t_ - u_; \
    t_ = s2 + sd; \
    /* 38531/32768 ~= Sin[7*Pi/16] + Cos[7*Pi/16] ~= 1.1758756024193586 */ \
    u_ = (s2*38531 + 16384) >> 15; \
    /* 12873/16384 ~= Sin[7*Pi/16] - Cos[7*Pi/16] ~= 0.7856949583871022 */ \
    s2 = (sd*12873 + 8192) >> 14; \
    /* 6393/32768 ~= Cos[7*Pi/16] ~= 0.19509032201612825 */ \
    t_ = (t_*6393 + 16384) >> 15; \
    s2 += t_; \
    sd = u_ - t_; \
    s3 -= s4h; \
    s4 += s3; \
    s8 -= s0h; \
    s0 += s8; \
    s7 += sfh; \
    sf -= s7; \
    sc += sbh; \
    sb -= sc; \
    s6 += OD_RSHIFT1(se) ;\
    se -= s6; \
    s9 -= OD_RSHIFT1(s1); \
    s1 += s9; \
    sd -= OD_RSHIFT1(s5); \
    s5 += sd; \
    s2 -= OD_RSHIFT1(sa); \
    sa += s2; \
    t_ = OD_PAVG(s3, sc); \
    /* 21407/16384 ~= Sin[3*Pi/8] + Cos[3*Pi/8] ~= 1.3065629648763766 */ \
    u_ = (s3*21407 + 8192) >> 14; \
    /* 8867/16384 ~= Sin[3*Pi/8] - Cos[3*Pi/8] ~= 0.5411961001461969 */ \
    s3 = (sc*8867 + 8192) >> 14; \
    /* 3135/4096 ~= 2*Cos[3*Pi/8] ~= 0.7653668647301796 */ \
    t_ = (t_*3135 + 2048) >> 12; \
    s3 += t_; \
    sc = u_ - t_; \
    t_ = OD_PAVG(s4, sb); \
    /* 21407/16384 ~= Sin[3*Pi/8] + Cos[3*Pi/8] ~= 1.3065629648763766 */ \
    u_ = (s4*21407 + 8192) >> 14; \
    /* 8867/16384 ~= Sin[3*Pi/8] - Cos[3*Pi/8] ~= 0.5411961001461969 */ \
    s4 = (sb*8867 + 8192) >> 14; \
    /* 3135/4096 ~= 2*Cos[3*Pi/8] ~= 0.7653668647301796 */ \
    t_ = (t_*3134 + 2048) >> 12; \
    s4 += t_; \
    sb = u_ - t_; \
    t_ = OD_PAVG(s5, sa); \
    /* 11585/8192 ~= Sin[Pi/4] + Cos[Pi/4] ~= 1.4142135623730951 */ \
    u_ = (sa*11585 + 4096) >> 13; \
    /* 11585/8192 ~= 2*Cos[Pi/4] ~= 1.4142135623730951 */ \
    sa = (t_*11585 + 4096) >> 13; \
    s5 = sa - u_; \
    t_ = OD_PAVG(s6, -s9); \
    /* 11585/8192 ~= Sin[Pi/4] + Cos[Pi/4] ~= 1.4142135623730951 */ \
    s6 = (s9*11585 + 4096) >> 13; \
    /* 11585/8192 ~= 2*Cos[Pi/4] ~= 1.4142135623730951 */ \
    s9 = (t_*11585 + 4096) >> 13; \
    s6 += s9; \
    t_ = OD_PAVG(s7, -s8); \
    /* 11585/8192 ~= Sin[Pi/4] + Cos[Pi/4] ~= 1.4142135623730951 */ \
    s7 = (s8*11585 + 4096) >> 13; \
    /* 11585/8192 ~= 2*Cos[Pi/4] ~= 1.4142135623730951 */ \
    s8 = (t_*11585 + 4096) >> 13; \
    s7 += s8; \
  } \
  while (0)

#define OD_IDST_16_FLAT(s0, s1, s2, s3, s4, s5, s6, s7, \
  s8, s9, sa, sb, sc, sd, se, sf) \
  /* Embedded 16-point orthonormal Type-IV iDST with flattened rotations. */ \
  do { \
    int t_; \
    int u_; \
    int s0h; \
    int s1h; \
    int s2h; \
    int s3h; \
    int s4h; \
    int s5h; \
    int s6h; \
    int s7h; \
    int sbh; \
    int sfh; \
    t_ = OD_PAVG(s6, s9); \
    /* 11585/8192 ~= Sin[Pi/4] + Cos[Pi/4] ~= 1.4142135623730951 */ \
    s9 = (s6*11585 + 4096) >> 13; \
    /* 11585/8192 ~= 2*Cos[Pi/4] ~= 1.4142135623730951 */ \
    s6 = (t_*11585 + 4096) >> 13; \
    s9 -= s6; \
    t_ = OD_PAVG(s5, sa); \
    /* 11585/8192 ~= Sin[Pi/4] + Cos[Pi/4] ~= 1.4142135623730951 */ \
    sa = (s5*11585 + 4096) >> 13; \
    /* 11585/8192 ~= 2*Cos[Pi/4] ~= 1.4142135623730951 */ \
    s5 = (t_*11585 + 4096) >> 13; \
    sa -= s5; \
    t_ = OD_PAVG(s7, s8); \
    /* 11585/8192 ~= Sin[Pi/4] + Cos[Pi/4] ~= 1.4142135623730951 */ \
    s8 = (s7*11585 + 4096) >> 13; \
    /* 11585/8192 ~= 2*Cos[Pi/4] ~= 1.4142135623730951 */ \
    s7 = (t_*11585 + 4096) >> 13; \
    s8 -= s7; \
    t_ = OD_PAVG(s3, sc); \
    /* 21407/16384 ~= Sin[3*Pi/8] + Cos[3*Pi/8] ~= 1.3065629648763766 */ \
    u_ = (s3*21407 + 8192) >> 14; \
    /* 8867/16384 ~= Sin[3*Pi/8] - Cos[3*Pi/8] ~= 0.5411961001461969 */ \
    s3 = (sc*8867 + 8192) >> 14; \
    /* 3135/4096 ~= 2*Cos[3*Pi/8] ~= 0.7653668647301796 */ \
    t_ = (t_*3135 + 2048) >> 12; \
    s3 += t_; \
    sc = u_ - t_; \
    t_ = OD_PAVG(sb, -s4); \
    /* 21407/16384 ~= Sin[3*Pi/8] + Cos[3*Pi/8] ~= 1.3065629648763766 */ \
    u_ = (sb*21407 + 8192) >> 14; \
    /* 8867/16384 ~= Sin[3*Pi/8] - Cos[3*Pi/8] ~= 0.5411961001461969 */ \
    sb = (s4*8867 + 8192) >> 14; \
    /* 3135/4096 ~= 2*Cos[3*Pi/8] ~= 0.7653668647301796 */ \
    t_ = (t_*3135 + 2048) >> 12; \
    sb -= t_; \
    s4 = t_ - u_; \
    sa += s2; \
    s2 -= OD_RSHIFT1(sa); \
    s5 -= sd; \
    sd += OD_RSHIFT1(s5); \
    s1 -= s9; \
    s9 += OD_RSHIFT1(s1); \
    se += s6; \
    s6 -= OD_RSHIFT1(se); \
    sb += sc; \
    sbh = OD_RSHIFT1(sb); \
    sc -= sbh; \
    sf += s7; \
    sfh = OD_RSHIFT1(sf); \
    s7 -= sfh; \
    s0 -= s8; \
    s0h = OD_RSHIFT1(s0); \
    s8 += s0h; \
    s4 += s3; \
    s4h = OD_RSHIFT1(s4); \
    s3 -= s4h; \
    t_ = sd - s2; \
    /* 38531/32768 ~= Sin[7*Pi/16] + Cos[7*Pi/16] ~= 1.1758756024193586 */ \
    u_ = (sd*38531 + 16384) >> 15; \
    /* 12873/16384 ~= Sin[7*Pi/16] - Cos[7*Pi/16] ~= 0.7856949583871022 */ \
    sd = (s2*12873 + 8192) >> 14; \
    /* 6393/32768 ~= Cos[7*Pi/16] ~= 0.19509032201612825 */ \
    t_ = (t_*6393 + 16384) >> 15; \
    sd -= t_; \
    s2 = t_ - u_; \
    t_ = OD_PAVG(s5, -sa); \
    /* 22725/16384 ~= Sin[5*Pi/16] + Cos[5*Pi/16] ~= 1.3870398453221475 */ \
    u_ = (s5*22725 + 8192) >> 14; \
    /* 9041/32768 ~= Sin[5*Pi/16] - Cos[5*Pi/16] ~= 0.27589937928294306 */ \
    s5 = (sa*9041 + 16384) >> 15; \
    /* 18205/16384 ~= 2*Cos[5*Pi/16] ~= 1.1111404660392044 */ \
    t_ = (t_*18205 + 8192) >> 14; \
    s5 -= t_; \
    sa = t_ - u_; \
    t_ = s6 + s9; \
    /* 45451/32768 ~= Sin[5*Pi/16] + Cos[5*Pi/16] ~= 1.3870398453221475 */ \
    u_ = (s9*45451 + 16384) >> 15; \
    /* 9041/32768 ~= Sin[5*Pi/16] - Cos[5*Pi/16] ~= 0.27589937928294306 */ \
    s9 = (s6*9041 + 16384) >> 15; \
    /* 18205/32768 ~= Cos[5*Pi/16] ~= 0.5555702330196022 */ \
    t_ = (t_*18205 + 16384) >> 15; \
    s9 += t_; \
    s6 = u_ - t_; \
    t_ = OD_PAVG(s1, se); \
    /* 9633/8192 ~= Sin[7*Pi/16] + Cos[7*Pi/16] ~= 1.1758756024193586 */ \
    u_ = (s1*9633 + 4096) >> 13; \
    /* 12873/16384 ~= Sin[7*Pi/16] - Cos[7*Pi/16] ~= 0.7856949583871022 */ \
    s1 = (se*12873 + 8192) >> 14; \
    /* 12785/32768 ~= 2*Cos[7*Pi/16] ~= 0.3901806440322565 */ \
    t_ = (t_*12785 + 16384) >> 15; \
    s1 += t_; \
    se = u_ - t_; \
    s8 -= s4h; \
    s4 += s8; \
    s7 += sbh; \
    sb -= s7; \
    s3 -= sfh; \
    sf += s3; \
    sc += s0h; \
    s0 -= sc; \
    sd += OD_RSHIFT1(se); \
    se -= sd; \
    s2 += OD_RSHIFT1(s1); \
    s1 -= s2; \
    s6 -= OD_RSHIFT1(s5); \
    s5 += s6; \
    s9 -= OD_RSHIFT1(sa); \
    sa += s9; \
    s0 -= se; \
    s0h = OD_RSHIFT1(s0); \
    se += s0h; \
    s1 -= sf; \
    s1h = OD_RSHIFT1(s1); \
    sf += s1h; \
    s2 += sc; \
    s2h = OD_RSHIFT1(s2); \
    sc -= s2h; \
    s3 += sd; \
    s3h = OD_RSHIFT1(s3); \
    sd -= s3h; \
    s4 -= sa; \
    s4h = OD_RSHIFT1(s4); \
    sa += s4h; \
    s5 -= sb; \
    s5h = OD_RSHIFT1(s5); \
    sb += s5h; \
    s6 += s8; \
    s6h = OD_RSHIFT1(s6); \
    s8 -= s6h; \
    s7 += s9; \
    s7h = OD_RSHIFT1(s7); \
    s9 -= s7h; \
    t_ = se - s1h; \
    /* 32729/32768 ~= (Sin[17*Pi/64] + Cos[17*Pi/64])/Sqrt[2] ~=
        0.9987954562051723 */ \
    u_ = (s1*32729 + 16384) >> 15; \
    /* 201/2048 ~= (Sin[17*Pi/64] - Cos[17*Pi/64])*Sqrt[2] ~=
        0.09813534865483615 */ \
    s1 = (se*201 + 1024) >> 11; \
    /* 31121/32768 ~= Cos[17*Pi/64]*Sqrt[2] ~=
        0.9497277818777543 */ \
    t_ = (t_*31121 + 16384) >> 15; \
    s1 += t_; \
    se = u_ + t_; \
    t_ = s6h + s9; \
    /* 32413/32768 ~= (Sin[19*Pi/64] + Cos[19*Pi/64])/Sqrt[2] ~=
        0.9891765099647809 */ \
    u_ = (s6*32413 + 16384) >> 15; \
    /* 601/2048 ~= (Sin[19*Pi/64] - Cos[19*Pi/64])*Sqrt[2] ~=
        0.29346094891072355 */ \
    s6 = (s9*601 + 1024) >> 11; \
    /* 27605/32768 ~= Cos[19*Pi/64]*Sqrt[2] ~= 0.8424460355094193 */ \
    t_ = (t_*27605 + 16384) >> 15; \
    s6 += t_; \
    s9 = u_ - t_; \
    t_ = sa - s5h; \
    /* 15893/16384 ~= (Sin[21*Pi/64] + Cos[21*Pi/64])/Sqrt[2] ~=
        0.970031253194544 */ \
    u_ = (s5*15893 + 8192) >> 14; \
    /* 3981/8192 ~= (Sin[21*Pi/64] - Cos[21*Pi/64])*Sqrt[2] ~=
        0.48596035980652796 */ \
    s5 = (sa*3981 + 4096) >> 13; \
    /* 1489/2048 ~= Cos[21*Pi/64]*Sqrt[2] ~= 0.72705107329128 */ \
    t_ = (t_*1489 + 1024) >> 11; \
    s5 += t_; \
    sa = u_ + t_; \
    t_ = s2h + sd; \
    /* 30853/32768 ~= (Sin[23*Pi/64] + Cos[23*Pi/64])/Sqrt[2] ~=
        0.9415440651830208 */ \
    u_ = (s2*30853 + 16384) >> 15; \
    /* 11039/16384 ~= (Sin[23*Pi/64] - Cos[23*Pi/64])*Sqrt[2] ~=
        0.6737797067844402 */ \
    s2 = (sd*11039 + 8192) >> 14; \
    /* 19813/32768 ~= Cos[23*Pi/64]*Sqrt[2] ~= 0.6046542117908008 */ \
    t_ = (t_*19813 + 16384) >> 15; \
    s2 += t_; \
    sd = u_ - t_; \
    t_ = sc - s3h; \
    /* 14811/16384 ~= (Sin[25*Pi/64] + Cos[25*Pi/64])/Sqrt[2] ~=
        0.9039892931234433 */ \
    u_ = (s3*14811 + 8192) >> 14; \
    /* 7005/8192 ~= (Sin[25*Pi/64] - Cos[25*Pi/64])*Sqrt[2] ~=
        0.8551101868605642 */ \
    s3 = (sc*7005 + 4096) >> 13; \
    /* 3903/8192 ~= Cos[25*Pi/64]*Sqrt[2] ~= 0.47643419969316125 */ \
    t_ = (t_*3903 + 4096) >> 13; \
    s3 += t_; \
    sc = u_ + t_; \
    t_ = s4h + sb; \
    /* 14053/16384 ~= (Sin[27*Pi/64] + Cos[27*Pi/64])/Sqrt[2] ~=
        0.857728610000272 */ \
    u_ = (s4*14053 + 8192) >> 14; \
    /* 8423/8192 ~= (Sin[27*Pi/64] - Cos[27*Pi/64])*Sqrt[2] ~=
        1.0282054883864435 */ \
    s4 = (sb*8423 + 4096) >> 13; \
    /* 2815/8192 ~= Cos[27*Pi/64]*Sqrt[2] ~= 0.34362586580705035 */ \
    t_ = (t_*2815 + 4096) >> 13; \
    s4 += t_; \
    sb = u_ - t_; \
    t_ = s8 - s7h; \
    /* 1645/2048 ~= (Sin[29*Pi/64] + Cos[29*Pi/64])/Sqrt[2] ~=
        0.8032075314806449 */ \
    u_ = (s7*1645 + 1024) >> 11; \
    /* 305/256 ~= (Sin[29*Pi/64] - Cos[29*Pi/64])*Sqrt[2] ~=
        1.1913986089848667 */ \
    s7 = (s8*305 + 128) >> 8; \
    /* 425/2048 ~= Cos[29*Pi/64]*Sqrt[2] ~= 0.20750822698821159 */ \
    t_ = (t_*425 + 1024) >> 11; \
    s7 += t_; \
    s8 = u_ + t_; \
    t_ = s0h + sf; \
    /* 24279/32768 ~= (Sin[31*Pi/64] + Cos[31*Pi/64])/Sqrt[2] ~=
        0.7409511253549591 */ \
    u_ = (s0*24279 + 16384) >> 15; \
    /* 44011/32768 ~= (Sin[31*Pi/64] - Cos[31*Pi/64])*Sqrt[2] ~=
        1.3431179096940369 */ \
    s0 = (sf*44011 + 16384) >> 15; \
    /* 1137/16384 ~= Cos[31*Pi/64]*Sqrt[2] ~= 0.06939217050794069 */ \
    t_ = (t_*1137 + 8192) >> 14; \
    s0 += t_; \
    sf = u_ - t_; \
  } \
  while (0)

/* TODO: rewrite this to match OD_FDST_16. */
#define OD_FDST_16_ASYM_PR(t0, t0h, t8, t4, t4h, tc, t2, ta, t6, te, \
  t1, t9, t5, td, t3, tb, t7, t7h, tf) \
  /* Embedded 16-point asymmetric Type-IV fDST. */ \
  do { \
    int t2h; \
    int t3h; \
    int t6h; \
    int t8h; \
    int t9h; \
    int tch; \
    int tdh; \
    /* TODO: Can we move these into another operation */ \
    t8 = -t8; \
    t9 = -t9; \
    ta = -ta; \
    tb = -tb; \
    td = -td; \
    /* 13573/16384 ~= 2*Tan[Pi/8] ~= 0.828427124746190 */ \
    OD_DCT_OVERFLOW_CHECK(te, 13573, 8192, 136); \
    t1 -= (te*13573 + 8192) >> 14; \
    /* 11585/32768 ~= Sin[Pi/4]/2 ~= 0.353553390593274 */ \
    OD_DCT_OVERFLOW_CHECK(t1, 11585, 16384, 137); \
    te += (t1*11585 + 16384) >> 15; \
    /* 13573/16384 ~= 2*Tan[Pi/8] ~= 0.828427124746190 */ \
    OD_DCT_OVERFLOW_CHECK(te, 13573, 8192, 138); \
    t1 -= (te*13573 + 8192) >> 14; \
    /* 4161/16384 ~= Tan[3*Pi/16] - Tan[Pi/8] ~= 0.253965075546204 */ \
    OD_DCT_OVERFLOW_CHECK(td, 4161, 8192, 139); \
    t2 += (td*4161 + 8192) >> 14; \
    /* 15137/16384 ~= Sin[3*Pi/8] ~= 0.923879532511287 */ \
    OD_DCT_OVERFLOW_CHECK(t2, 15137, 8192, 140); \
    td -= (t2*15137 + 8192) >> 14; \
    /* 14341/16384 ~= Tan[3*Pi/16] + Tan[Pi/8]/2 ~= 0.875285419105846 */ \
    OD_DCT_OVERFLOW_CHECK(td, 14341, 8192, 141); \
    t2 += (td*14341 + 8192) >> 14; \
    /* 14341/16384 ~= Tan[3*Pi/16] + Tan[Pi/8]/2 ~= 0.875285419105846 */ \
    OD_DCT_OVERFLOW_CHECK(t3, 14341, 8192, 142); \
    tc -= (t3*14341 + 8192) >> 14; \
    /* 15137/16384 ~= Sin[3*Pi/8] ~= 0.923879532511287 */ \
    OD_DCT_OVERFLOW_CHECK(tc, 15137, 8192, 143); \
    t3 += (tc*15137 + 8192) >> 14; \
    /* 4161/16384 ~= Tan[3*Pi/16] - Tan[Pi/8] ~= 0.253965075546204 */ \
    OD_DCT_OVERFLOW_CHECK(t3, 4161, 8192, 144); \
    tc -= (t3*4161 + 8192) >> 14; \
    te = t0h - te; \
    t0 -= te; \
    tf = OD_RSHIFT1(t1) - tf; \
    t1 -= tf; \
    /* TODO: Can we move this into another operation */ \
    tc = -tc; \
    t2 = OD_RSHIFT1(tc) - t2; \
    tc -= t2; \
    t3 = OD_RSHIFT1(td) - t3; \
    td = t3 - td; \
    /* 7489/8192 ~= Tan[Pi/8] + Tan[Pi/4]/2 ~= 0.914213562373095 */ \
    OD_DCT_OVERFLOW_CHECK(t6, 7489, 4096, 145); \
    t9 -= (t6*7489 + 4096) >> 13; \
    /* 11585/16384 ~= Sin[Pi/4] ~= 0.707106781186548 */ \
    OD_DCT_OVERFLOW_CHECK(t9, 11585, 8192, 146); \
    t6 += (t9*11585 + 8192) >> 14; \
    /* -19195/32768 ~= Tan[Pi/8] - Tan[Pi/4] ~= -0.585786437626905 */ \
    OD_DCT_OVERFLOW_CHECK(t6, 19195, 16384, 147); \
    t9 += (t6*19195 + 16384) >> 15; \
    t8 += OD_RSHIFT1(t9); \
    t9 -= t8; \
    t6 = t7h - t6; \
    t7 -= t6; \
    /* 6723/8192 ~= Tan[7*Pi/32] ~= 0.820678790828660 */ \
    OD_DCT_OVERFLOW_CHECK(t7, 6723, 4096, 148); \
    t8 += (t7*6723 + 4096) >> 13; \
    /* 16069/16384 ~= Sin[7*Pi/16] ~= 0.980785280403230 */ \
    OD_DCT_OVERFLOW_CHECK(t8, 16069, 8192, 149); \
    t7 -= (t8*16069 + 8192) >> 14; \
    /* 6723/8192 ~= Tan[7*Pi/32]) ~= 0.820678790828660 */ \
    OD_DCT_OVERFLOW_CHECK(t7, 6723, 4096, 150); \
    t8 += (t7*6723 + 4096) >> 13; \
    /* 17515/32768 ~= Tan[5*Pi/32]) ~= 0.534511135950792 */ \
    OD_DCT_OVERFLOW_CHECK(t6, 17515, 16384, 151); \
    t9 += (t6*17515 + 16384) >> 15; \
    /* 13623/16384 ~= Sin[5*Pi/16] ~= 0.831469612302545 */ \
    OD_DCT_OVERFLOW_CHECK(t9, 13623, 8192, 152); \
    t6 -= (t9*13623 + 8192) >> 14; \
    /* 17515/32768 ~= Tan[5*Pi/32] ~= 0.534511135950792 */ \
    OD_DCT_OVERFLOW_CHECK(t6, 17515, 16384, 153); \
    t9 += (t6*17515 + 16384) >> 15; \
    /* 13573/16384 ~= 2*Tan[Pi/8] ~= 0.828427124746190 */ \
    OD_DCT_OVERFLOW_CHECK(ta, 13573, 8192, 154); \
    t5 += (ta*13573 + 8192) >> 14; \
    /* 11585/32768 ~= Sin[Pi/4]/2 ~= 0.353553390593274 */ \
    OD_DCT_OVERFLOW_CHECK(t5, 11585, 16384, 155); \
    ta -= (t5*11585 + 16384) >> 15; \
    /* 13573/16384 ~= 2*Tan[Pi/8] ~= 0.828427124746190 */ \
    OD_DCT_OVERFLOW_CHECK(ta, 13573, 8192, 156); \
    t5 += (ta*13573 + 8192) >> 14; \
    tb += OD_RSHIFT1(t5); \
    t5 = tb - t5; \
    ta += t4h; \
    t4 -= ta; \
    /* 2485/8192 ~= Tan[3*Pi/32] ~= 0.303346683607342 */ \
    OD_DCT_OVERFLOW_CHECK(t5, 2485, 4096, 157); \
    ta += (t5*2485 + 4096) >> 13; \
    /* 18205/32768 ~= Sin[3*Pi/16] ~= 0.555570233019602 */ \
    OD_DCT_OVERFLOW_CHECK(ta, 18205, 16384, 158); \
    t5 -= (ta*18205 + 16384) >> 15; \
    /* 2485/8192 ~= Tan[3*Pi/32] ~= 0.303346683607342 */ \
    OD_DCT_OVERFLOW_CHECK(t5, 2485, 4096, 159); \
    ta += (t5*2485 + 4096) >> 13; \
    /* 6723/8192 ~= Tan[7*Pi/32] ~= 0.820678790828660 */ \
    OD_DCT_OVERFLOW_CHECK(t4, 6723, 4096, 160); \
    tb -= (t4*6723 + 4096) >> 13; \
    /* 16069/16384 ~= Sin[7*Pi/16] ~= 0.980785280403230 */ \
    OD_DCT_OVERFLOW_CHECK(tb, 16069, 8192, 161); \
    t4 += (tb*16069 + 8192) >> 14; \
    /* 6723/8192 ~= Tan[7*Pi/32] ~= 0.820678790828660 */ \
    OD_DCT_OVERFLOW_CHECK(t4, 6723, 4096, 162); \
    tb -= (t4*6723 + 4096) >> 13; \
    /* TODO: Can we move this into another operation */ \
    t5 = -t5; \
    tc -= tf; \
    tch = OD_RSHIFT1(tc); \
    tf += tch; \
    t3 += t0; \
    t3h = OD_RSHIFT1(t3); \
    t0 -= t3h; \
    td -= t1; \
    tdh = OD_RSHIFT1(td); \
    t1 += tdh; \
    t2 += te; \
    t2h = OD_RSHIFT1(t2); \
    te -= t2h; \
    t8 += t4; \
    t8h = OD_RSHIFT1(t8); \
    t4 = t8h - t4; \
    t7 = tb - t7; \
    t7h = OD_RSHIFT1(t7); \
    tb = t7h - tb; \
    t6 -= ta; \
    t6h = OD_RSHIFT1(t6); \
    ta += t6h; \
    t9 = t5 - t9; \
    t9h = OD_RSHIFT1(t9); \
    t5 -= t9h; \
    t0 -= t7h; \
    t7 += t0; \
    tf += t8h; \
    t8 -= tf; \
    te -= t6h; \
    t6 += te; \
    t1 += t9h; \
    t9 -= t1; \
    tb -= tch; \
    tc += tb; \
    t4 += t3h; \
    t3 -= t4; \
    ta -= tdh; \
    td += ta; \
    t5 = t2h - t5; \
    t2 -= t5; \
    /* TODO: Can we move these into another operation */ \
    t8 = -t8; \
    t9 = -t9; \
    ta = -ta; \
    tb = -tb; \
    tc = -tc; \
    td = -td; \
    tf = -tf; \
    /* 7799/8192 ~= Tan[31*Pi/128] ~= 0.952079146700925 */ \
    OD_DCT_OVERFLOW_CHECK(tf, 7799, 4096, 163); \
    t0 -= (tf*7799 + 4096) >> 13; \
    /* 4091/4096 ~= Sin[31*Pi/64] ~= 0.998795456205172 */ \
    OD_DCT_OVERFLOW_CHECK(t0, 4091, 2048, 164); \
    tf += (t0*4091 + 2048) >> 12; \
    /* 7799/8192 ~= Tan[31*Pi/128] ~= 0.952079146700925 */ \
    OD_DCT_OVERFLOW_CHECK(tf, 7799, 4096, 165); \
    t0 -= (tf*7799 + 4096) >> 13; \
    /* 2417/32768 ~= Tan[3*Pi/128] ~= 0.0737644315224493 */ \
    OD_DCT_OVERFLOW_CHECK(te, 2417, 16384, 166); \
    t1 += (te*2417 + 16384) >> 15; \
    /* 601/4096 ~= Sin[3*Pi/64] ~= 0.146730474455362 */ \
    OD_DCT_OVERFLOW_CHECK(t1, 601, 2048, 167); \
    te -= (t1*601 + 2048) >> 12; \
    /* 2417/32768 ~= Tan[3*Pi/128] ~= 0.0737644315224493 */ \
    OD_DCT_OVERFLOW_CHECK(te, 2417, 16384, 168); \
    t1 += (te*2417 + 16384) >> 15; \
    /* 14525/32768 ~= Tan[17*Pi/128] ~= 0.443269513890864 */ \
    OD_DCT_OVERFLOW_CHECK(t8, 14525, 16384, 169); \
    t7 -= (t8*14525 + 16384) >> 15; \
    /* 3035/4096 ~= Sin[17*Pi/64] ~= 0.740951125354959 */ \
    OD_DCT_OVERFLOW_CHECK(t7, 3035, 2048, 170); \
    t8 += (t7*3035 + 2048) >> 12; \
    /* 7263/16384 ~= Tan[17*Pi/128] ~= 0.443269513890864 */ \
    OD_DCT_OVERFLOW_CHECK(t8, 7263, 8192, 171); \
    t7 -= (t8*7263 + 8192) >> 14; \
    /* 6393/8192 ~= Tan[27*Pi/128] ~= 0.780407659653944 */ \
    OD_DCT_OVERFLOW_CHECK(td, 6393, 4096, 172); \
    t2 -= (td*6393 + 4096) >> 13; \
    /* 3973/4096 ~= Sin[27*Pi/64] ~= 0.970031253194544 */ \
    OD_DCT_OVERFLOW_CHECK(t2, 3973, 2048, 173); \
    td += (t2*3973 + 2048) >> 12; \
    /* 6393/8192 ~= Tan[27*Pi/128] ~= 0.780407659653944 */ \
    OD_DCT_OVERFLOW_CHECK(td, 6393, 4096, 174); \
    t2 -= (td*6393 + 4096) >> 13; \
    /* 9281/16384 ~= Tan[21*Pi/128] ~= 0.566493002730344 */ \
    OD_DCT_OVERFLOW_CHECK(ta, 9281, 8192, 175); \
    t5 -= (ta*9281 + 8192) >> 14; \
    /* 7027/8192 ~= Sin[21*Pi/64] ~= 0.857728610000272 */ \
    OD_DCT_OVERFLOW_CHECK(t5, 7027, 4096, 176); \
    ta += (t5*7027 + 4096) >> 13; \
    /* 9281/16384 ~= Tan[21*Pi/128] ~= 0.566493002730344 */ \
    OD_DCT_OVERFLOW_CHECK(ta, 9281, 8192, 177); \
    t5 -= (ta*9281 + 8192) >> 14; \
    /* 11539/16384 ~= Tan[25*Pi/128] ~= 0.704279460865044 */ \
    OD_DCT_OVERFLOW_CHECK(tc, 11539, 8192, 178); \
    t3 -= (tc*11539 + 8192) >> 14; \
    /* 7713/8192 ~= Sin[25*Pi/64] ~= 0.941544065183021 */ \
    OD_DCT_OVERFLOW_CHECK(t3, 7713, 4096, 179); \
    tc += (t3*7713 + 4096) >> 13; \
    /* 11539/16384 ~= Tan[25*Pi/128] ~= 0.704279460865044 */ \
    OD_DCT_OVERFLOW_CHECK(tc, 11539, 8192, 180); \
    t3 -= (tc*11539 + 8192) >> 14; \
    /* 10375/16384 ~= Tan[23*Pi/128] ~= 0.633243016177569 */ \
    OD_DCT_OVERFLOW_CHECK(tb, 10375, 8192, 181); \
    t4 -= (tb*10375 + 8192) >> 14; \
    /* 7405/8192 ~= Sin[23*Pi/64] ~= 0.903989293123443 */ \
    OD_DCT_OVERFLOW_CHECK(t4, 7405, 4096, 182); \
    tb += (t4*7405 + 4096) >> 13; \
    /* 10375/16384 ~= Tan[23*Pi/128] ~= 0.633243016177569 */ \
    OD_DCT_OVERFLOW_CHECK(tb, 10375, 8192, 183); \
    t4 -= (tb*10375 + 8192) >> 14; \
    /* 8247/16384 ~= Tan[19*Pi/128] ~= 0.503357699799294 */ \
    OD_DCT_OVERFLOW_CHECK(t9, 8247, 8192, 184); \
    t6 -= (t9*8247 + 8192) >> 14; \
    /* 1645/2048 ~= Sin[19*Pi/64] ~= 0.803207531480645 */ \
    OD_DCT_OVERFLOW_CHECK(t6, 1645, 1024, 185); \
    t9 += (t6*1645 + 1024) >> 11; \
    /* 8247/16384 ~= Tan[19*Pi/128] ~= 0.503357699799294 */ \
    OD_DCT_OVERFLOW_CHECK(t9, 8247, 8192, 186); \
    t6 -= (t9*8247 + 8192) >> 14; \
  } \
  while (0)

#define OD_IDST_16_ASYM_PR(t0, t0h, t8, t4, tc, t2, t2h, ta, t6, te, teh, \
  t1, t9, t5, td, t3, tb, t7, tf) \
  /* Embedded 16-point asymmetric Type-IV iDST. */ \
  do { \
    int t1h_; \
    int t3h_; \
    int t4h; \
    int t6h; \
    int t9h_; \
    int tbh_; \
    int tch; \
    /* 8247/16384 ~= Tan[19*Pi/128] ~= 0.503357699799294 */ \
    t6 += (t9*8247 + 8192) >> 14; \
    /* 1645/2048 ~= Sin[19*Pi/64] ~= 0.803207531480645 */ \
    t9 -= (t6*1645 + 1024) >> 11; \
    /* 8247/16384 ~= Tan[19*Pi/128] ~= 0.503357699799294 */ \
    t6 += (t9*8247 + 8192) >> 14; \
    /* 10375/16384 ~= Tan[23*Pi/128] ~= 0.633243016177569 */ \
    t2 += (td*10375 + 8192) >> 14; \
    /* 7405/8192 ~= Sin[23*Pi/64] ~= 0.903989293123443 */ \
    td -= (t2*7405 + 4096) >> 13; \
    /* 10375/16384 ~= Tan[23*Pi/128] ~= 0.633243016177569 */ \
    t2 += (td*10375 + 8192) >> 14; \
    /* 11539/16384 ~= Tan[25*Pi/128] ~= 0.704279460865044 */ \
    tc += (t3*11539 + 8192) >> 14; \
    /* 7713/8192 ~= Sin[25*Pi/64] ~= 0.941544065183021 */ \
    t3 -= (tc*7713 + 4096) >> 13; \
    /* 11539/16384 ~= Tan[25*Pi/128] ~= 0.704279460865044 */ \
    tc += (t3*11539 + 8192) >> 14; \
    /* 9281/16384 ~= Tan[21*Pi/128] ~= 0.566493002730344 */ \
    ta += (t5*9281 + 8192) >> 14; \
    /* 7027/8192 ~= Sin[21*Pi/64] ~= 0.857728610000272 */ \
    t5 -= (ta*7027 + 4096) >> 13; \
    /* 9281/16384 ~= Tan[21*Pi/128] ~= 0.566493002730344 */ \
    ta += (t5*9281 + 8192) >> 14; \
    /* 6393/8192 ~= Tan[27*Pi/128] ~= 0.780407659653944 */ \
    t4 += (tb*6393 + 4096) >> 13; \
    /* 3973/4096 ~= Sin[27*Pi/64] ~= 0.970031253194544 */ \
    tb -= (t4*3973 + 2048) >> 12; \
    /* 6393/8192 ~= Tan[27*Pi/128] ~= 0.780407659653944 */ \
    t4 += (tb*6393 + 4096) >> 13; \
    /* 7263/16384 ~= Tan[17*Pi/128] ~= 0.443269513890864 */ \
    te += (t1*7263 + 8192) >> 14; \
    /* 3035/4096 ~= Sin[17*Pi/64] ~= 0.740951125354959 */ \
    t1 -= (te*3035 + 2048) >> 12; \
    /* 14525/32768 ~= Tan[17*Pi/128] ~= 0.443269513890864 */ \
    te += (t1*14525 + 16384) >> 15; \
    /* 2417/32768 ~= Tan[3*Pi/128] ~= 0.0737644315224493 */ \
    t8 -= (t7*2417 + 16384) >> 15; \
    /* 601/4096 ~= Sin[3*Pi/64] ~= 0.146730474455362 */ \
    t7 += (t8*601 + 2048) >> 12; \
    /* 2417/32768 ~= Tan[3*Pi/128] ~= 0.0737644315224493 */ \
    t8 -= (t7*2417 + 16384) >> 15; \
    /* 7799/8192 ~= Tan[31*Pi/128] ~= 0.952079146700925 */ \
    t0 += (tf*7799 + 4096) >> 13; \
    /* 4091/4096 ~= Sin[31*Pi/64] ~= 0.998795456205172 */ \
    tf -= (t0*4091 + 2048) >> 12; \
    /* 7799/8192 ~= Tan[31*Pi/128] ~= 0.952079146700925 */ \
    t0 += (tf*7799 + 4096) >> 13; \
    /* TODO: Can we move these into another operation */ \
    t1 = -t1; \
    t3 = -t3; \
    t5 = -t5; \
    t9 = -t9; \
    tb = -tb; \
    td = -td; \
    tf = -tf; \
    t4 += ta; \
    t4h = OD_RSHIFT1(t4); \
    ta = t4h - ta; \
    tb -= t5; \
    tbh_ = OD_RSHIFT1(tb); \
    t5 += tbh_; \
    tc += t2; \
    tch = OD_RSHIFT1(tc); \
    t2 -= tch; \
    t3 -= td; \
    t3h_ = OD_RSHIFT1(t3); \
    td += t3h_; \
    t9 += t8; \
    t9h_ = OD_RSHIFT1(t9); \
    t8 -= t9h_; \
    t6 -= t7; \
    t6h = OD_RSHIFT1(t6); \
    t7 += t6h; \
    t1 += tf; \
    t1h_ = OD_RSHIFT1(t1); \
    tf -= t1h_; \
    te -= t0; \
    teh = OD_RSHIFT1(te); \
    t0 += teh; \
    ta += t9h_; \
    t9 = ta - t9; \
    t5 -= t6h; \
    t6 += t5; \
    td = teh - td; \
    te = td - te; \
    t2 = t1h_ - t2; \
    t1 -= t2; \
    t7 += t4h; \
    t4 -= t7; \
    t8 -= tbh_; \
    tb += t8; \
    t0 += tch; \
    tc -= t0; \
    tf -= t3h_; \
    t3 += tf; \
    /* TODO: Can we move this into another operation */ \
    ta = -ta; \
    /* 6723/8192 ~= Tan[7*Pi/32] ~= 0.820678790828660 */ \
    td += (t2*6723 + 4096) >> 13; \
    /* 16069/16384 ~= Sin[7*Pi/16] ~= 0.980785280403230 */ \
    t2 -= (td*16069 + 8192) >> 14; \
    /* 6723/8192 ~= Tan[7*Pi/32] ~= 0.820678790828660 */ \
    td += (t2*6723 + 4096) >> 13; \
    /* 2485/8192 ~= Tan[3*Pi/32] ~= 0.303346683607342 */ \
    t5 -= (ta*2485 + 4096) >> 13; \
    /* 18205/32768 ~= Sin[3*Pi/16] ~= 0.555570233019602 */ \
    ta += (t5*18205 + 16384) >> 15; \
    /* 2485/8192 ~= Tan[3*Pi/32] ~= 0.303346683607342 */ \
    t5 -= (ta*2485 + 4096) >> 13; \
    t2 += t5; \
    t2h = OD_RSHIFT1(t2); \
    t5 -= t2h; \
    ta = td - ta; \
    td -= OD_RSHIFT1(ta); \
    /* 13573/16384 ~= 2*Tan[Pi/8] ~= 0.828427124746190 */ \
    ta -= (t5*13573 + 8192) >> 14; \
    /* 11585/32768 ~= Sin[Pi/4]/2 ~= 0.353553390593274 */ \
    t5 += (ta*11585 + 16384) >> 15; \
    /* 13573/16384 ~= 2*Tan[Pi/8] ~= 0.828427124746190 */ \
    ta -= (t5*13573 + 8192) >> 14; \
    /* 17515/32768 ~= Tan[5*Pi/32] ~= 0.534511135950792 */ \
    t9 -= (t6*17515 + 16384) >> 15; \
    /* 13623/16384 ~= Sin[5*Pi/16] ~= 0.831469612302545 */ \
    t6 += (t9*13623 + 8192) >> 14; \
    /* 17515/32768 ~= Tan[5*Pi/32]) ~= 0.534511135950792 */ \
    t9 -= (t6*17515 + 16384) >> 15; \
    /* 6723/8192 ~= Tan[7*Pi/32]) ~= 0.820678790828660 */ \
    t1 -= (te*6723 + 4096) >> 13; \
    /* 16069/16384 ~= Sin[7*Pi/16] ~= 0.980785280403230 */ \
    te += (t1*16069 + 8192) >> 14; \
    /* 6723/8192 ~= Tan[7*Pi/32]) ~= 0.820678790828660 */ \
    t1 -= (te*6723 + 4096) >> 13; \
    te += t6; \
    teh = OD_RSHIFT1(te); \
    t6 = teh - t6; \
    t9 += t1; \
    t1 -= OD_RSHIFT1(t9); \
    /* -19195/32768 ~= Tan[Pi/8] - Tan[Pi/4] ~= -0.585786437626905 */ \
    t9 -= (t6*19195 + 16384) >> 15; \
    /* 11585/16384 ~= Sin[Pi/4] ~= 0.707106781186548 */ \
    t6 -= (t9*11585 + 8192) >> 14; \
    /* 7489/8192 ~= Tan[Pi/8] + Tan[Pi/4]/2 ~= 0.914213562373095 */ \
    t9 += (t6*7489 + 4096) >> 13; \
    tb = tc - tb; \
    tc = OD_RSHIFT1(tb) - tc; \
    t3 += t4; \
    t4 = OD_RSHIFT1(t3) - t4; \
    /* TODO: Can we move this into another operation */ \
    t3 = -t3; \
    t8 += tf; \
    tf = OD_RSHIFT1(t8) - tf; \
    t0 += t7; \
    t0h = OD_RSHIFT1(t0); \
    t7 = t0h - t7; \
    /* 4161/16384 ~= Tan[3*Pi/16] - Tan[Pi/8] ~= 0.253965075546204 */ \
    t3 += (tc*4161 + 8192) >> 14; \
    /* 15137/16384 ~= Sin[3*Pi/8] ~= 0.923879532511287 */ \
    tc -= (t3*15137 + 8192) >> 14; \
    /* 14341/16384 ~= Tan[3*Pi/16] + Tan[Pi/8]/2 ~= 0.875285419105846 */ \
    t3 += (tc*14341 + 8192) >> 14; \
    /* 14341/16384 ~= Tan[3*Pi/16] + Tan[Pi/8]/2 ~= 0.875285419105846 */ \
    t4 -= (tb*14341 + 8192) >> 14; \
    /* 15137/16384 ~= Sin[3*Pi/8] ~= 0.923879532511287 */ \
    tb += (t4*15137 + 8192) >> 14; \
    /* 4161/16384 ~= Tan[3*Pi/16] - Tan[Pi/8] ~= 0.253965075546204 */ \
    t4 -= (tb*4161 + 8192) >> 14; \
    /* 13573/16384 ~= 2*Tan[Pi/8] ~= 0.828427124746190 */ \
    t8 += (t7*13573 + 8192) >> 14; \
    /* 11585/32768 ~= Sin[Pi/4]/2 ~= 0.353553390593274 */ \
    t7 -= (t8*11585 + 16384) >> 15; \
    /* 13573/16384 ~= 2*Tan[Pi/8] ~= 0.828427124746190 */ \
    t8 += (t7*13573 + 8192) >> 14; \
    /* TODO: Can we move these into another operation */ \
    t1 = -t1; \
    t5 = -t5; \
    t9 = -t9; \
    tb = -tb; \
    td = -td; \
  } \
  while (0)

#define OD_FDST_16_ASYM_FLAT(s0, s0h, s8, s4, s4h, sc, s2, s2h, sa, s6, s6h, \
  se, s1, s1h, s9, s5, s5h, sd, s3, s3h, sb, s7, s7h, sf) \
  /* Embedded 16-point asymmetric Type-IV fDST with flattened rotations. */ \
  do { \
    int t_; \
    int u_; \
    int sbh; \
    int sfh; \
    t_ = s1h + se; \
    /* 46285/32768 ~= Sin[17*Pi/64] + Cos[17*Pi/64] ~= 1.4125100802019774 */ \
    u_ = (se*46285 + 16384) >> 15; \
    /* 1137/16384 ~= Sin[17*Pi/64] - Cos[17*Pi/64] ~= 0.06939217050794078 */ \
    se = (s1*1137 + 8192) >> 14; \
    /* 44011/32768 ~= Cos[17*Pi/64]*2 ~= 1.3431179096940367 */ \
    t_ = (t_*44011 + 16384) >> 15; \
    se += t_; \
    s1 = u_ - OD_RSHIFT1(t_); \
    t_ = s6h - s9; \
    /* 45839/32768 ~= Sin[19*Pi/64] + Cos[19*Pi/64] ~= 1.3989068359730783 */ \
    u_ = (s9*45839 + 16384) >> 15; \
    /* 425/2048 ~= Sin[19*Pi/64] - Cos[19*Pi/64] ~= 0.20750822698821159 */ \
    s9 = (425*s6 + 1024) >> 11; \
    /* 305/256 ~= Cos[19*Pi/64]*2 ~= 1.1913986089848667 */ \
    t_ = (305*t_ + 128) >> 8; \
    s9 += t_; \
    s6 = u_ + OD_RSHIFT1(t_); \
    t_ = s5h + sa; \
    /* 5619/4096 ~= Sin[21*Pi/64] + Cos[21*Pi/64] ~= 1.371831354193494 */ \
    u_ = (sa*5619 + 2048) >> 12; \
    /* 2815/8192 ~= Sin[21*Pi/64] - Cos[21*Pi/64] ~= 0.34362586580705046 */ \
    sa = (s5*2815 + 4096) >> 13; \
    /* 8423/8192 ~= Cos[21*Pi/64]*2 ~= 1.0282054883864433 */ \
    t_ = (t_*8423 + 4096) >> 13; \
    sa += t_; \
    s5 = OD_RSHIFT1(t_) - u_; \
    t_ = sd - s2h; \
    /* 2727/2048 ~= Sin[23*Pi/64] + Cos[23*Pi/64] ~= 1.3315443865537255 */ \
    u_ = (sd*2727 + 1024) >> 11; \
    /* 3903/8192 ~= Sin[23*Pi/64] - Cos[23*Pi/64] ~= 0.47643419969316125 */ \
    sd = (s2*3903 + 4096) >> 13; \
    /* 7005/8192 ~= Cos[23*Pi/64]*2 ~= 0.8551101868605642 */ \
    t_ = (t_*7005 + 4096) >> 13; \
    sd -= t_; \
    s2 = OD_RSHIFT1(t_) - u_; \
    t_ = s3h + sc; \
    /* 10473/8192 ~= Sin[25*Pi/64] + Cos[25*Pi/64] ~= 1.278433918575241 */ \
    u_ = (sc*10473 + 4096) >> 13; \
    /* 19813/32768 ~= Sin[25*Pi/64] - Cos[25*Pi/64] ~= 0.6046542117908007 */ \
    sc = (s3*19813 + 16384) >> 15; \
    /* 11039/16384 ~= Cos[25*Pi/64]*2 ~= 0.6737797067844401 */ \
    t_ = (t_*11039 + 8192) >> 14; \
    sc += t_; \
    s3 = u_ - OD_RSHIFT1(t_); \
    t_ = sb - s4h; \
    /* 9937/8192 ~= Sin[27*Pi/64] + Cos[27*Pi/64] ~= 1.213011433097808 */ \
    u_ = (sb*9937 + 4096) >> 13; \
    /* 1489/2048 ~= Sin[27*Pi/64] - Cos[27*Pi/64] ~= 0.72705107329128 */ \
    sb = (s4*1489 + 1024) >> 11; \
    /* 3981/8192 ~= Cos[27*Pi/64]*2 ~= 0.48596035980652774 */ \
    t_ = (t_*3981 + 4096) >> 13; \
    sb -= t_; \
    s4 = OD_RSHIFT1(t_) - u_; \
    t_ = s7h + s8; \
    /* 37221/32768 ~= Sin[29*Pi/64] + Cos[29*Pi/64] ~= 1.1359069844201428 */ \
    u_ = (s8*37221 + 16384) >> 15; \
    /* 27605/32768 ~= Sin[29*Pi/64] - Cos[29*Pi/64] ~= 0.8424460355094192 */ \
    s8 = (s7*27605 + 16384) >> 15; \
    /* 601/2048 ~= Cos[29*Pi/64]*2 ~= 0.2934609489107235 */ \
    t_ = (t_*601 + 1024) >> 11; \
    s8 += t_; \
    s7 = u_ - OD_RSHIFT1(t_); \
    t_ = s0h - sf; \
    /* 1073/1024 ~= Sin[31*Pi/64] + Cos[31*Pi/64] ~= 1.0478631305325905 */ \
    u_ = (sf*1073 + 512) >> 10; \
    /* 31121/32768 ~= Sin[31*Pi/64] - Cos[31*Pi/64] ~= 0.9497277818777544 */ \
    sf = (s0*31121 + 16384) >> 15; \
    /* 201/2048 ~= Cos[31*Pi/64]*2 ~= 0.09813534865483603 */ \
    t_ = (t_*201 + 1024) >> 11; \
    s0 = u_ + OD_RSHIFT1(t_); \
    sf += t_; \
    s3 -= OD_RSHIFT1(sd); \
    sd += s3; \
    s2 += OD_RSHIFT1(sc); \
    sc -= s2; \
    s5 -= OD_RSHIFT1(sb); \
    sb += s5; \
    s4 -= OD_RSHIFT1(sa); \
    sa += s4; \
    s1 += OD_RSHIFT1(sf); \
    sf -= s1; \
    s7 -= OD_RSHIFT1(s9); \
    s9 += s7; \
    s6 -= OD_RSHIFT1(s8); \
    s8 += s6; \
    s0 += OD_RSHIFT1(se); \
    se -= s0; \
    sa -= s9; \
    s9 += OD_RSHIFT1(sa); \
    s5 += s6; \
    s6 -= OD_RSHIFT1(s5); \
    s1 -= s2; \
    s2 += OD_RSHIFT1(s1); \
    se += sd; \
    sd -= OD_RSHIFT1(se); \
    s0 += sc; \
    s0h = OD_RSHIFT1(s0); \
    sc -= s0h; \
    sf -= s3; \
    sfh = OD_RSHIFT1(sf); \
    s3 += sfh; \
    sb += s7; \
    sbh = OD_RSHIFT1(sb); \
    s7 -= sbh; \
    s4 += s8; \
    s4h = OD_RSHIFT1(s4); \
    s8 -= s4h; \
    t_ = s1 + se; \
    /* 9633/8192 ~= Sin[7*Pi/16] + Cos[7*Pi/16] ~= 1.1758756024193586 */ \
    u_ = (s1*9633 + 4096) >> 13; \
    /* 12873/16384 ~= Sin[7*Pi/16] - Cos[7*Pi/16] ~= 0.7856949583871022 */ \
    s1 = (se*12873 + 8192) >> 14; \
    /* 6393/32768 ~= Cos[7*Pi/16] ~= 0.19509032201612825 */ \
    t_ = (t_*6393 + 16384) >> 15; \
    s1 += t_; \
    se = u_ - t_; \
    t_ = s6 + s9; \
    /* 22725/16384 ~= Sin[5*Pi/16] + Cos[5*Pi/16] ~= 1.3870398453221475 */ \
    u_ = (s9*22725 + 8192) >> 14; \
    /* 9041/32768 ~= Sin[5*Pi/16] - Cos[5*Pi/16] ~= 0.27589937928294306 */ \
    s9 = (s6*9041 + 16384) >> 15; \
    /* 18205/32768 ~= Cos[5*Pi/16] ~= 0.5555702330196022 */ \
    t_ = (t_*18205 + 16384) >> 15; \
    s9 += t_; \
    s6 = u_ - t_; \
    t_ = s5 + sa; \
    /* 11363/8192 ~= Sin[5*Pi/16] + Cos[5*Pi/16] ~= 1.3870398453221475 */ \
    u_ = (sa*11363 + 4096) >> 13; \
    /* 9041/32768 ~= Sin[5*Pi/16] - Cos[5*Pi/16] ~= 0.27589937928294306 */ \
    sa = (s5*9041 + 16384) >> 15; \
    /* 4551/8192 ~= Cos[5*Pi/16] ~= 0.5555702330196022 */ \
    t_ = (t_*4551 + 4096) >> 13; \
    sa += t_; \
    s5 = t_ - u_; \
    t_ = s2 + sd; \
    /* 9633/8192 ~= Sin[7*Pi/16] + Cos[7*Pi/16] ~= 1.1758756024193586 */ \
    u_ = (s2*9633 + 4096) >> 13; \
    /* 12873/16384 ~= Sin[7*Pi/16] - Cos[7*Pi/16] ~= 0.7856949583871022 */ \
    s2 = (sd*12873 + 8192) >> 14; \
    /* 6393/32768 ~= Cos[7*Pi/16] ~= 0.19509032201612825 */ \
    t_ = (t_*6393 + 16384) >> 15; \
    s2 += t_; \
    sd = u_ - t_; \
    s3 -= s4h; \
    s4 += s3; \
    s8 -= s0h; \
    s0 += s8; \
    s7 += sfh; \
    sf -= s7; \
    sc += sbh; \
    sb -= sc; \
    s6 += OD_RSHIFT1(se) ;\
    se -= s6; \
    s9 -= OD_RSHIFT1(s1); \
    s1 += s9; \
    sd -= OD_RSHIFT1(s5); \
    s5 += sd; \
    s2 -= OD_RSHIFT1(sa); \
    sa += s2; \
    t_ = s3 + sc; \
    /* 10703/8192 ~= Sin[3*Pi/8] + Cos[3*Pi/8] ~= 1.3065629648763766 */ \
    u_ = (s3*10703 + 4096) >> 13; \
    /* 8867/16348 ~= Sin[3*Pi/8] - Cos[3*Pi/8] ~= 0.5411961001461969 */ \
    s3 = (sc*8867 + 8192) >> 14; \
    /* 3135/8192 ~= Cos[3*Pi/8] ~= 0.3826834323650898 */ \
    t_ = (t_*3135 + 4096) >> 13; \
    s3 += t_; \
    sc = u_ - t_; \
    t_ = s4 + sb; \
    /* 10703/8192 ~= Sin[3*Pi/8] + Cos[3*Pi/8] ~= 1.3065629648763766 */ \
    u_ = (s4*10703 + 4096) >> 13; \
    /* 8867/16384 ~= Sin[3*Pi/8] - Cos[3*Pi/8] ~= 0.5411961001461969 */ \
    s4 = (sb*8867 + 8192) >> 14; \
    /* 3135/8192 ~= Cos[3*Pi/8] ~= 0.3826834323650898 */ \
    t_ = (t_*3135 + 4096) >> 13; \
    s4 += t_; \
    sb = u_ - t_; \
    t_ = s5 + sa; \
    /* 11585/8192 ~= Sin[Pi/4] + Cos[Pi/4] ~= 1.4142135623730951 */ \
    u_ = (sa*11585 + 4096) >> 13; \
    /* 11585/16384 ~= Cos[Pi/4] ~= 0.7071067811865475 */ \
    sa = (t_*11585 + 8192) >> 14; \
    s5 = sa - u_; \
    t_ = s6 - s9; \
    /* 11585/8192 ~= Sin[Pi/4] + Cos[Pi/4] ~= 1.4142135623730951 */ \
    s6 = (s9*11585 + 4096) >> 13; \
    /* 11585/16384 ~= Cos[Pi/4] ~= 0.7071067811865475 */ \
    s9 = (t_*11585 + 8192) >> 14; \
    s6 += s9; \
    t_ = s7 - s8; \
    /* 11585/8192 ~= Sin[Pi/4] + Cos[Pi/4] ~= 1.4142135623730951 */ \
    s7 = (s8*11585 + 4096) >> 13; \
    /* 11585/16384 ~= Cos[Pi/4] ~= 0.7071067811865475 */ \
    s8 = (t_*11585 + 8192) >> 14; \
    s7 += s8; \
  } \
  while (0)

#define OD_IDST_16_ASYM_FLAT(s0, s1, s2, s3, s4, s5, s6, s7, \
  s8, s9, sa, sb, sc, sd, se, sf) \
  /* Embedded 16-point asymmetric Type-IV iDST with flattened rotations. */ \
  do { \
    int t_; \
    int u_; \
    int s0h; \
    int s1h; \
    int s2h; \
    int s3h; \
    int s4h; \
    int s5h; \
    int s6h; \
    int s7h; \
    int sbh; \
    int sfh; \
    t_ = s6 + s9; \
    /* 11585/8192 ~= Sin[Pi/4] + Cos[Pi/4] ~= 1.4142135623730951 */ \
    s9 = (s6*11585 + 4096) >> 13; \
    /* 11585/16384 ~= Cos[Pi/4] ~= 0.7071067811865475 */ \
    s6 = (t_*11585 + 8192) >> 14; \
    s9 -= s6; \
    t_ = s5 + sa; \
    /* 11585/8192 ~= Sin[Pi/4] + Cos[Pi/4] ~= 1.4142135623730951 */ \
    sa = (s5*11585 + 4096) >> 13; \
    /* 11585/16384 ~= Cos[Pi/4] ~= 0.7071067811865475 */ \
    s5 = (t_*11585 + 8192) >> 14; \
    sa -= s5; \
    t_ = s7 + s8; \
    /* 11585/8192 ~= Sin[Pi/4] + Cos[Pi/4] ~= 1.4142135623730951 */ \
    s8 = (s7*11585 + 4096) >> 13; \
    /* 11585/16384 ~= Cos[Pi/4] ~= 0.7071067811865475 */ \
    s7 = (t_*11585 + 8192) >> 14; \
    s8 -= s7; \
    t_ = s3 - sc; \
    /* 10703/8192 ~= Sin[3*Pi/8] + Cos[3*Pi/8] ~= 1.3065629648763766 */ \
    u_ = (sc*10703 + 4096) >> 13; \
    /* 8867/16384 ~= Sin[3*Pi/8] - Cos[3*Pi/8] ~= 0.5411961001461969 */ \
    sc = (s3*8867 + 8192) >> 14; \
    /* 3135/8192 ~= Cos[3*Pi/8] ~= 0.3826834323650898 */ \
    t_ = (t_*3135 + 4096) >> 13; \
    sc += t_; \
    s3 = u_ + t_; \
    t_ = sb - s4; \
    /* 10703/8192 ~= Sin[3*Pi/8] + Cos[3*Pi/8] ~= 1.3065629648763766 */ \
    u_ = (sb*10703 + 4096) >> 13; \
    /* 8867/16384 ~= Sin[3*Pi/8] - Cos[3*Pi/8] ~= 0.5411961001461969 */ \
    sb = (s4*8867 + 8192) >> 14; \
    /* 3135/8192 ~= Cos[3*Pi/8] ~= 0.3826834323650898 */ \
    t_ = (t_*3135 + 4096) >> 13; \
    sb -= t_; \
    s4 = t_ - u_; \
    sa += s2; \
    s2 -= OD_RSHIFT1(sa); \
    s5 -= sd; \
    sd += OD_RSHIFT1(s5); \
    s1 -= s9; \
    s9 += OD_RSHIFT1(s1); \
    se += s6; \
    s6 -= OD_RSHIFT1(se); \
    sb += sc; \
    sbh = OD_RSHIFT1(sb); \
    sc -= sbh; \
    sf += s7; \
    sfh = OD_RSHIFT1(sf); \
    s7 -= sfh; \
    s0 -= s8; \
    s0h = OD_RSHIFT1(s0); \
    s8 += s0h; \
    s4 += s3; \
    s4h = OD_RSHIFT1(s4); \
    s3 -= s4h; \
    t_ = sd - s2; \
    /* 9633/8192 ~= Sin[7*Pi/16] + Cos[7*Pi/16] ~= 1.1758756024193586 */ \
    u_ = (sd*9633 + 4096) >> 13; \
    /* 12873/16384 ~= Sin[7*Pi/16] - Cos[7*Pi/16] ~= 0.7856949583871022 */ \
    sd = (s2*12873 + 8192) >> 14; \
    /* 6393/32768 ~= Cos[7*Pi/16] ~= 0.19509032201612825 */ \
    t_ = (t_*6393 + 16384) >> 15; \
    sd -= t_; \
    s2 = t_ - u_; \
    t_ = s5 - sa; \
    /* 11363/8192 ~= Sin[5*Pi/16] + Cos[5*Pi/16] ~= 1.3870398453221475 */ \
    u_ = (s5*11363 + 4096) >> 13; \
    /* 9041/32768 ~= Sin[5*Pi/16] - Cos[5*Pi/16] ~= 0.27589937928294306 */ \
    s5 = (sa*9041 + 16384) >> 15; \
    /* 4551/8192 ~= Cos[5*Pi/16] ~= 0.5555702330196022 */ \
    t_ = (t_*4551 + 4096) >> 13; \
    s5 -= t_; \
    sa = t_ - u_; \
    t_ = s6 + s9; \
    /* 22725/16384 ~= Sin[5*Pi/16] + Cos[5*Pi/16] ~= 1.3870398453221475 */ \
    u_ = (s9*22725 + 8192) >> 14; \
    /* 9041/32768 ~= Sin[5*Pi/16] - Cos[5*Pi/16] ~= 0.27589937928294306 */ \
    s9 = (s6*9041 + 16384) >> 15; \
    /* 18205/32768 ~= Cos[5*Pi/16] ~= 0.5555702330196022 */ \
    t_ = (t_*18205 + 16384) >> 15; \
    s9 += t_; \
    s6 = u_ - t_; \
    t_ = s1 + se; \
    /* 9633/8192 ~= Sin[7*Pi/16] + Cos[7*Pi/16] ~= 1.1758756024193586 */ \
    u_ = (s1*9633 + 4096) >> 13; \
    /* 12873/16384 ~= Sin[7*Pi/16] - Cos[7*Pi/16] ~= 0.7856949583871022 */ \
    s1 = (se*12873 + 8192) >> 14; \
    /* 6393/32768 ~= Cos[7*Pi/16] ~= 0.19509032201612825 */ \
    t_ = (t_*6393 + 16384) >> 15; \
    s1 += t_; \
    se = u_ - t_; \
    s8 -= s4h; \
    s4 += s8; \
    s7 += sbh; \
    sb -= s7; \
    s3 -= sfh; \
    sf += s3; \
    sc += s0h; \
    s0 -= sc; \
    sd += OD_RSHIFT1(se); \
    se -= sd; \
    s2 += OD_RSHIFT1(s1); \
    s1 -= s2; \
    s6 -= OD_RSHIFT1(s5); \
    s5 += s6; \
    s9 -= OD_RSHIFT1(sa); \
    sa += s9; \
    s0 -= se; \
    s0h = OD_RSHIFT1(s0); \
    se += s0h; \
    s1 -= sf; \
    s1h = OD_RSHIFT1(s1); \
    sf += s1h; \
    s2 += sc; \
    s2h = OD_RSHIFT1(s2); \
    sc -= s2h; \
    s3 += sd; \
    s3h = OD_RSHIFT1(s3); \
    sd -= s3h; \
    s4 -= sa; \
    s4h = OD_RSHIFT1(s4); \
    sa += s4h; \
    s5 -= sb; \
    s5h = OD_RSHIFT1(s5); \
    sb += s5h; \
    s6 += s8; \
    s6h = OD_RSHIFT1(s6); \
    s8 -= s6h; \
    s7 += s9; \
    s7h = OD_RSHIFT1(s7); \
    s9 -= s7h; \
    t_ = se - s1h; \
    /* 23143/32768 ~= (Sin[17*Pi/64] + Cos[17*Pi/64])/2 ~=
        0.7062550401009887 */ \
    u_ = (s1*23143 + 16384) >> 15; \
    /* 1137/8192 ~= (Sin[17*Pi/64] - Cos[17*Pi/64])*2 ~=
        0.13878434101588155 */ \
    s1 = (se*1137 + 4096) >> 13; \
    /* 44011/32768 ~= Cos[17*Pi/64]*2 ~= 1.3431179096940367 */ \
    t_ = (t_*44011 + 16384) >> 15; \
    s1 += t_; \
    se = u_ + OD_RSHIFT1(t_); \
    t_ = s6h + s9; \
    /* 2865/4096 ~= (Sin[19*Pi/64] + Cos[19*Pi/64])/2 ~= 0.6994534179865391 */ \
    u_ = (s6*2865 + 2048) >> 12; \
    /* 13599/32768 ~= (Sin[19*Pi/64] - Cos[19*Pi/64])*2 ~=
        0.41501645397642317 */ \
    s6 = (s9*13599 + 16384) >> 15; \
    /* 305/256 ~= Cos[19*Pi/64]*2 ~= 1.1913986089848667 */ \
    t_ = (t_*305 + 128) >> 8; \
    s6 += t_; \
    s9 = u_ - OD_RSHIFT1(t_); \
    t_ = sa - s5h; \
    /* 5619/8192 ~= (Sin[21*Pi/64] + Cos[21*Pi/64])/2 ~= 0.685915677096747 */ \
    u_ = (s5*5619 + 4096) >> 13; \
    /* 2815/4096 ~= (Sin[21*Pi/64] - Cos[21*Pi/64])*2 ~= 0.6872517316141009 */ \
    s5 = (sa*2815 + 2048) >> 12; \
    /* 8423/8192 ~= Cos[21*Pi/64]*2 ~= 1.0282054883864433 */ \
    t_ = (t_*8423 + 4096) >> 13; \
    s5 += t_; \
    sa = u_ + OD_RSHIFT1(t_); \
    t_ = s2h + sd; \
    /* 2727/4096 ~= (Sin[23*Pi/64] + Cos[23*Pi/64])/2 ~= 0.6657721932768628 */ \
    u_ = (s2*2727 + 2048) >> 12; \
    /* 3903/4096 ~= (Sin[23*Pi/64] - Cos[23*Pi/64])*2 ~= 0.9528683993863225 */ \
    s2 = (sd*3903 + 2048) >> 12; \
    /* 7005/8192 ~= Cos[23*Pi/64]*2 ~= 0.8551101868605642 */ \
    t_ = (t_*7005 + 4096) >> 13; \
    s2 += t_; \
    sd = u_ - OD_RSHIFT1(t_); \
    t_ = sc - s3h; \
    /* 10473/16384 ~= (Sin[25*Pi/64] + Cos[25*Pi/64])/2 ~=
        0.6392169592876205 */ \
    u_ = (s3*10473 + 8192) >> 14; \
    /* 39627/32768 ~= (Sin[25*Pi/64] - Cos[25*Pi/64])*2 ~=
        1.2093084235816014 */ \
    s3 = (sc*39627 + 16384) >> 15; \
    /* 11039/16384 ~= Cos[25*Pi/64]*2 ~= 0.6737797067844401 */ \
    t_ = (t_*11039 + 8192) >> 14; \
    s3 += t_; \
    sc = u_ + OD_RSHIFT1(t_); \
    t_ = s4h + sb; \
    /* 9937/16384 ~= (Sin[27*Pi/64] + Cos[27*Pi/64])/2 ~= 0.606505716548904 */ \
    u_ = (s4*9937 + 8192) >> 14; \
    /* 1489/1024 ~= (Sin[27*Pi/64] - Cos[27*Pi/64])*2 ~= 1.45410214658256 */ \
    s4 = (sb*1489 + 512) >> 10; \
    /* 3981/8192 ~= Cos[27*Pi/64]*2 ~= 0.48596035980652774 */ \
    t_ = (t_*3981 + 4096) >> 13; \
    s4 += t_; \
    sb = u_ - OD_RSHIFT1(t_); \
    t_ = s8 - s7h; \
    /* 18611/32768 ~= (Sin[29*Pi/64] + Cos[29*Pi/64])/2 ~=
        0.5679534922100714 */ \
    u_ = (s7*18611 + 16384) >> 15; \
    /* 55211/32768 ~= (Sin[29*Pi/64] - Cos[29*Pi/64])*2 ~=
        1.6848920710188384 */ \
    s7 = (s8*55211 + 16384) >> 15; \
    /* 601/2048 ~= Cos[29*Pi/64]*2 ~= 0.2934609489107235 */ \
    t_ = (t_*601 + 1024) >> 11; \
    s7 += t_; \
    s8 = u_ + OD_RSHIFT1(t_); \
    t_ = s0h + sf; \
    /* 1073/2048 ~= (Sin[31*Pi/64] + Cos[31*Pi/64])/2 ~= 0.5239315652662953 */ \
    u_ = (s0*1073 + 1024) >> 11; \
    /* 62241/32768 ~= (Sin[31*Pi/64] - Cos[31*Pi/64])*2 ~=
        1.8994555637555088 */ \
    s0 = (sf*62241 + 16384) >> 15; \
    /* 201/2048 ~= Cos[31*Pi/64]*2 ~= 0.09813534865483603 */ \
    t_ = (t_*201 + 1024) >> 11; \
    s0 += t_; \
    sf = u_ - OD_RSHIFT1(t_); \
  } \
  while (0)

#define OD_FDCT_32_PR(t0, tg, t8, to, t4, tk, tc, ts, t2, ti, ta, tq, t6, tm, \
  te, tu, t1, th, t9, tp, t5, tl, td, tt, t3, tj, tb, tr, t7, tn, tf, tv) \
  /* Embedded 32-point orthonormal Type-II fDCT. */ \
  do { \
    int tgh; \
    int thh; \
    int tih; \
    int tkh; \
    int tmh; \
    int tnh; \
    int toh; \
    int tqh; \
    int tsh; \
    int tuh; \
    int tvh; \
    tv = t0 - tv; \
    tvh = OD_RSHIFT1(tv); \
    t0 -= tvh; \
    tu += t1; \
    tuh = OD_RSHIFT1(tu); \
    t1 = tuh - t1; \
    tt = t2 - tt; \
    t2 -= OD_RSHIFT1(tt); \
    ts += t3; \
    tsh = OD_RSHIFT1(ts); \
    t3 = tsh - t3; \
    tr = t4 - tr; \
    t4 -= OD_RSHIFT1(tr); \
    tq += t5; \
    tqh = OD_RSHIFT1(tq); \
    t5 = tqh - t5; \
    tp = t6 - tp; \
    t6 -= OD_RSHIFT1(tp); \
    to += t7; \
    toh = OD_RSHIFT1(to); \
    t7 = toh - t7; \
    tn = t8 - tn; \
    tnh = OD_RSHIFT1(tn); \
    t8 -= tnh; \
    tm += t9; \
    tmh = OD_RSHIFT1(tm); \
    t9 = tmh - t9; \
    tl = ta - tl; \
    ta -= OD_RSHIFT1(tl); \
    tk += tb; \
    tkh = OD_RSHIFT1(tk); \
    tb = tkh - tb; \
    tj = tc - tj; \
    tc -= OD_RSHIFT1(tj); \
    ti += td; \
    tih = OD_RSHIFT1(ti); \
    td = tih - td; \
    th = te - th; \
    thh = OD_RSHIFT1(th); \
    te -= thh; \
    tg += tf; \
    tgh = OD_RSHIFT1(tg); \
    tf = tgh - tf; \
    OD_FDCT_16_ASYM_PR(t0, tg, tgh, t8, to, toh, t4, tk, tkh, tc, ts, tsh, \
     t2, ti, tih, ta, tq, tqh, t6, tm, tmh, te, tu, tuh); \
    OD_FDST_16_ASYM_PR(tv, tvh, tf, tn, tnh, t7, tr, tb, tj, t3, \
     tt, td, tl, t5, tp, t9, th, thh, t1); \
  } \
  while (0)

#define OD_IDCT_32_PR(t0, tg, t8, to, t4, tk, tc, ts, t2, ti, ta, tq, t6, tm, \
  te, tu, t1, th, t9, tp, t5, tl, td, tt, t3, tj, tb, tr, t7, tn, tf, tv) \
  /* Embedded 32-point orthonormal Type-II iDCT. */ \
  do { \
    int t1h; \
    int t3h; \
    int t5h; \
    int t7h; \
    int t9h; \
    int tbh; \
    int tdh; \
    int tfh; \
    int thh; \
    int tth; \
    int tvh; \
    OD_IDST_16_ASYM_PR(tv, tvh, tn, tr, tj, tt, tth, tl, tp, th, thh, \
     tu, tm, tq, ti, ts, tk, to, tg); \
    OD_IDCT_16_ASYM_PR(t0, t8, t4, tc, t2, ta, t6, te, \
     t1, t1h, t9, t9h, t5, t5h, td, tdh, t3, t3h, tb, tbh, t7, t7h, tf, tfh); \
    tu = t1h - tu; \
    t1 -= tu; \
    te += thh; \
    th = te - th; \
    tm = t9h - tm; \
    t9 -= tm; \
    t6 += OD_RSHIFT1(tp); \
    tp = t6 - tp; \
    tq = t5h - tq; \
    t5 -= tq; \
    ta += OD_RSHIFT1(tl); \
    tl = ta - tl; \
    ti = tdh - ti; \
    td -= ti; \
    t2 += tth; \
    tt = t2 - tt; \
    ts = t3h - ts; \
    t3 -= ts; \
    tc += OD_RSHIFT1(tj); \
    tj = tc - tj; \
    tk = tbh - tk; \
    tb -= tk; \
    t4 += OD_RSHIFT1(tr); \
    tr = t4 - tr; \
    to = t7h - to; \
    t7 -= to; \
    t8 += OD_RSHIFT1(tn); \
    tn = t8 - tn; \
    tg = tfh - tg; \
    tf -= tg; \
    t0 += tvh; \
    tv = t0 - tv; \
  } \
  while (0)

#define OD_FDCT_32_FLAT(t0, tg, t8, to, t4, tk, tc, ts, t2, ti, ta, tq, t6, \
  tm, te, tu, t1, th, t9, tp, t5, tl, td, tt, t3, tj, tb, tr, t7, tn, tf, tv) \
  /* Embedded 32-point orthonormal Type-II fDCT with flattened rotations. */ \
  do { \
    od_coeff tgh; \
    od_coeff thh; \
    od_coeff tih; \
    od_coeff tjh; \
    od_coeff tkh; \
    od_coeff tlh; \
    od_coeff tmh; \
    od_coeff tnh; \
    od_coeff toh; \
    od_coeff tph; \
    od_coeff tqh; \
    od_coeff trh; \
    od_coeff tsh; \
    od_coeff tth; \
    od_coeff tuh; \
    od_coeff tvh; \
    tv = t0 - tv; \
    tvh = OD_RSHIFT1(tv); \
    t0 -= tvh; \
    tu += t1; \
    tuh = OD_RSHIFT1(tu); \
    t1 -= tuh; \
    tt = t2 - tt; \
    tth = OD_RSHIFT1(tt); \
    t2 -= tth; \
    ts += t3; \
    tsh = OD_RSHIFT1(ts); \
    t3 -= tsh; \
    tr = t4 - tr; \
    trh = OD_RSHIFT1(tr); \
    t4 -= trh; \
    tq += t5; \
    tqh = OD_RSHIFT1(tq); \
    t5 -= tqh; \
    tp = t6 - tp; \
    tph = OD_RSHIFT1(tp); \
    t6 -= tph; \
    to += t7; \
    toh = OD_RSHIFT1(to); \
    t7 -= toh; \
    tn = t8 - tn; \
    tnh = OD_RSHIFT1(tn); \
    t8 -= tnh; \
    tm += t9; \
    tmh = OD_RSHIFT1(tm); \
    t9 -= tmh; \
    tl = ta - tl; \
    tlh = OD_RSHIFT1(tl); \
    ta -= tlh; \
    tk += tb; \
    tkh = OD_RSHIFT1(tk); \
    tb -= tkh; \
    tj = tc - tj; \
    tjh = OD_RSHIFT1(tj); \
    tc -= tjh; \
    ti += td; \
    tih = OD_RSHIFT1(ti); \
    td -= tih; \
    th = te - th; \
    thh = OD_RSHIFT1(th); \
    te -= thh; \
    tg += tf; \
    tgh = OD_RSHIFT1(tg); \
    tf -= tgh; \
    OD_FDCT_16_ASYM_FLAT(t0, tg, tgh, t8, to, toh, t4, tk, tkh, tc, ts, tsh, \
     t2, ti, tih, ta, tq, tqh, t6, tm, tmh, te, tu, tuh); \
    OD_FDST_16_ASYM_FLAT(tv, tvh, tf, tn, tnh, t7, tr, trh, tb, tj, tjh, t3, \
     tt, tth, td, tl, tlh, t5, tp, tph, t9, th, thh, t1); \
  } \
  while (0)

#define OD_IDCT_32_FLAT(t0, tg, t8, to, t4, tk, tc, ts, t2, ti, ta, tq, t6, \
  tm, te, tu, t1, th, t9, tp, t5, tl, td, tt, t3, tj, tb, tr, t7, tn, tf, tv) \
  /* Embedded 32-point orthonormal Type-II iDCT with flattened rotations. */ \
  do { \
    od_coeff t1h; \
    od_coeff t3h; \
    od_coeff t5h; \
    od_coeff t7h; \
    od_coeff t9h; \
    od_coeff tbh; \
    od_coeff tdh; \
    od_coeff tfh; \
    od_coeff thh; \
    od_coeff tth; \
    od_coeff tvh; \
    OD_IDST_16_ASYM_FLAT(tv, tn, tr, tj, tt, tl, tp, th, \
     tu, tm, tq, ti, ts, tk, to, tg); \
    OD_IDCT_16_ASYM_FLAT(t0, t8, t4, tc, t2, ta, t6, te, \
     t1, t1h, t9, t9h, t5, t5h, td, tdh, t3, t3h, tb, tbh, t7, t7h, tf, tfh); \
    tu += t1h; \
    t1 -= tu; \
    thh = OD_RSHIFT1(th); \
    te += thh; \
    th = te - th; \
    tm += t9h; \
    t9 -= tm; \
    t6 += OD_RSHIFT1(tp); \
    tp = t6 - tp; \
    tq += t5h; \
    t5 -= tq; \
    ta += OD_RSHIFT1(tl); \
    tl = ta - tl; \
    ti += tdh; \
    td -= ti; \
    tth = OD_RSHIFT1(tt); \
    t2 += tth; \
    tt = t2 - tt; \
    ts += t3h; \
    t3 -= ts; \
    tc += OD_RSHIFT1(tj); \
    tj = tc - tj; \
    tk += tbh; \
    tb -= tk; \
    t4 += OD_RSHIFT1(tr); \
    tr = t4 - tr; \
    to += t7h; \
    t7 -= to; \
    t8 += OD_RSHIFT1(tn); \
    tn = t8 - tn; \
    tg += tfh; \
    tf -= tg; \
    tvh = OD_RSHIFT1(tv); \
    t0 += tvh; \
    tv = t0 - tv; \
  } \
  while (0)

/* Embedded 32-point orthonormal Type-IV fDST. */
#define OD_FDST_32_PR(t0, t1, t2, t3, t4, t5, t6, t7, t8, t9, ta, tb, tc, td, \
  te, tf, tg, th, ti, tj, tk, tl, tm, tn, to, tp, tq, tr, ts, tt, tu, tv) \
  /* 117 "muls", 117 + 128 = 245 adds, 36 shifts */ \
  do { \
    od_coeff t0h; \
    od_coeff t1h; \
    od_coeff t2h; \
    od_coeff t3h; \
    od_coeff t4h; \
    od_coeff t6h; \
    od_coeff t8h; \
    od_coeff t9h; \
    od_coeff tah; \
    od_coeff tbh; \
    od_coeff tch; \
    od_coeff tdh; \
    od_coeff teh; \
    od_coeff tfh; \
    od_coeff tgh; \
    od_coeff thh; \
    od_coeff tih; \
    od_coeff tjh; \
    od_coeff tkh; \
    od_coeff tlh; \
    od_coeff tmh; \
    od_coeff tnh; \
    od_coeff tph; \
    od_coeff trh; \
    od_coeff tsh; \
    od_coeff tth; \
    od_coeff tuh; \
    od_coeff tvh; \
    /* Stage 0 */ \
    tp += (t6*659 + 2048) >> 12; \
    t6 -= (tp*10279 + 16384) >> 15; \
    tp += (t6*659 + 2048) >> 12; \
    th += (te*3045 + 4096) >> 13; \
    te -= (th*21403 + 16384) >> 15; \
    th += (te*3045 + 4096) >> 13; \
    t9 += (tm*20191 + 16384) >> 15; \
    tm -= (t9*29269 + 16384) >> 15; \
    t9 += (tm*20191 + 16384) >> 15; \
    tu += (t1*1207 + 16384) >> 15; \
    t1 -= (tu*2411 + 16384) >> 15; \
    tu += (t1*1207 + 16384) >> 15; \
    t4 += (tr*13113 + 8192) >> 14; \
    tr -= (t4*7993 + 4096) >> 13; \
    t4 += (tr*13113 + 8192) >> 14; \
    tj += (tc*10381 + 16384) >> 15; \
    tc -= (tj*4717 + 4096) >> 13; \
    tj += (tc*10381 + 16384) >> 15; \
    tb += (tk*18035 + 16384) >> 15; \
    tk -= (tb*6921 + 4096) >> 13; \
    tb += (tk*18035 + 16384) >> 15; \
    ts += (t3*1411 + 8192) >> 14; \
    t3 -= (ts*2801 + 8192) >> 14; \
    ts += (t3*1411 + 8192) >> 14; \
    tq += (t5*2225 + 8192) >> 14; \
    t5 -= (tq*2185 + 4096) >> 13; \
    tq += (t5*2225 + 8192) >> 14; \
    ti += (td*11273 + 16384) >> 15; \
    td -= (ti*315 + 256) >> 9; \
    ti += (td*11273 + 16384) >> 15; \
    tl += (ta*8637 + 16384) >> 15; \
    ta -= (tl*16151 + 16384) >> 15; \
    tl += (ta*8637 + 16384) >> 15; \
    tt += (t2*2013 + 16384) >> 15; \
    t2 -= (tt*4011 + 16384) >> 15; \
    tt += (t2*2013 + 16384) >> 15; \
    to += (t7*6101 + 16384) >> 15; \
    t7 -= (to*11793 + 16384) >> 15; \
    to += (t7*6101 + 16384) >> 15; \
    t8 += (tn*10659 + 8192) >> 14; \
    tn -= (t8*29957 + 16384) >> 15; \
    t8 += (tn*10659 + 8192) >> 14; \
    tg += (tf*819 + 1024) >> 11; \
    tf -= (tg*22595 + 16384) >> 15; \
    tg += (tf*819 + 1024) >> 11; \
    t0 += (tv*31973 + 16384) >> 15; \
    tv -= (t0*16379 + 8192) >> 14; \
    t0 += (tv*31973 + 16384) >> 15; \
    /* Stage 1 */ \
    tj -= ts; \
    tjh = OD_RSHIFT1(tj); \
    ts += tjh; \
    tr = tk - tr; \
    trh = OD_RSHIFT1(tr); \
    tk = trh - tk; \
    tc += t3; \
    tch = OD_RSHIFT1(tc); \
    t3 -= tch; \
    t4 += tb; \
    t4h = OD_RSHIFT1(t4); \
    tb -= t4h; \
    tv += tf; \
    tvh = OD_RSHIFT1(tv); \
    tf -= tvh; \
    t8 -= to; \
    t8h = OD_RSHIFT1(t8); \
    to += t8h; \
    t0 += tg; \
    t0h = OD_RSHIFT1(t0); \
    tg -= t0h; \
    tn = t7 - tn; \
    tnh = OD_RSHIFT1(tn); \
    t7 -= tnh; \
    th -= tu; \
    thh = OD_RSHIFT1(th); \
    tu += thh; \
    t6 += tm; \
    t6h = OD_RSHIFT1(t6); \
    tm = t6h - tm; \
    te += t1; \
    teh = OD_RSHIFT1(te); \
    t1 -= teh; \
    tp += t9; \
    tph = OD_RSHIFT1(tp); \
    t9 -= tph; \
    t2 -= td; \
    t2h = OD_RSHIFT1(t2); \
    td += t2h; \
    tl = tq - tl; \
    tlh = OD_RSHIFT1(tl); \
    tq -= tlh; \
    tt += ti; \
    tth = OD_RSHIFT1(tt); \
    ti -= tth; \
    ta += t5; \
    tah = OD_RSHIFT1(ta); \
    t5 -= tah; \
    /* Stage 2 */ \
    tm -= thh; \
    th += tm; \
    t9 = teh - t9; \
    te -= t9; \
    td = tlh - td; \
    tl -= td; \
    ti += tah; \
    ta -= ti; \
    tk = tjh - tk; \
    tj -= tk; \
    tb -= tch; \
    tc += tb; \
    tg += tnh; \
    tn = tg - tn; \
    tf += t8h; \
    t8 = tf - t8; \
    t3 -= trh; \
    tr += t3; \
    ts += t4h; \
    t4 -= ts; \
    to -= t0h; \
    t0 += to; \
    t7 = tvh - t7; \
    tv = t7 - tv; \
    t1 -= t6h; \
    t6 += t1; \
    tu += tph; \
    tp -= tu; \
    tq -= tth; \
    tt += tq; \
    t5 += t2h; \
    t2 -= t5; \
    /* Stage 3 */ \
    tj += (tc*11725 + 16384) >> 15; \
    tc -= (tj*5197 + 4096) >> 13; \
    tj += (tc*11725 + 16384) >> 15; \
    td += (ti*513 + 1024) >> 11; \
    ti -= (td*15447 + 16384) >> 15; \
    td += (ti*513 + 1024) >> 11; \
    th += (te*4861 + 16384) >> 15; \
    te -= (th*1189 + 2048) >> 12; \
    th += (te*4861 + 16384) >> 15; \
    tg += (tf*805 + 8192) >> 14; \
    tf -= (tg*803 + 4096) >> 13; \
    tg += (tf*805 + 8192) >> 14; \
    tb += (tk*7749 + 8192) >> 14; \
    tk -= (tb*12665 + 8192) >> 14; \
    tb += (tk*7749 + 8192) >> 14; \
    tl += (ta*2455 + 2048) >> 12; \
    ta -= (tl*28899 + 16384) >> 15; \
    tl += (ta*2455 + 2048) >> 12; \
    t9 += (tm*12151 + 8192) >> 14; \
    tm -= (t9*31357 + 16384) >> 15; \
    t9 += (tm*12151 + 8192) >> 14; \
    tn += (t8*29699 + 16384) >> 15; \
    t8 -= (tn*16305 + 8192) >> 14; \
    tn += (t8*29699 + 16384) >> 15; \
    /* Stage 4 */ \
    tf -= tc; \
    tfh = OD_RSHIFT1(tf); \
    tc += tfh; \
    ti = th - ti; \
    tih = OD_RSHIFT1(ti); \
    th -= tih; \
    tg += tj; \
    tgh = OD_RSHIFT1(tg); \
    tj = tgh - tj; \
    td -= te; \
    tdh = OD_RSHIFT1(td); \
    te += tdh; \
    tm = ta - tm; \
    tmh = OD_RSHIFT1(tm); \
    ta = tmh - ta; \
    t9 += tl; \
    t9h = OD_RSHIFT1(t9); \
    tl -= t9h; \
    tb += t8; \
    tbh = OD_RSHIFT1(tb); \
    t8 -= tbh; \
    tk += tn; \
    tkh = OD_RSHIFT1(tk); \
    tn -= tkh; \
    t1 -= t2; \
    t1h = OD_RSHIFT1(t1); \
    t2 += t1h; \
    t3 += tv; \
    t3h = OD_RSHIFT1(t3); \
    tv -= t3h; \
    tu += tt; \
    tuh = OD_RSHIFT1(tu); \
    tt -= tuh; \
    ts -= t0; \
    tsh = OD_RSHIFT1(ts); \
    t0 += tsh; \
    tq = t6 - tq; \
    t6 -= OD_RSHIFT1(tq); \
    to += tr; \
    tr = OD_RSHIFT1(to) - tr; \
    t7 = t4 - t7; \
    t4 -= OD_RSHIFT1(t7); \
    t5 -= tp; \
    tp += OD_RSHIFT1(t5); \
    /* Stage 5 */ \
    tp += (t6*2485 + 4096) >> 13; \
    t6 -= (tp*18205 + 16384) >> 15; \
    tp += (t6*2485 + 4096) >> 13; \
    to += (t7*3227 + 16384) >> 15; \
    t7 -= (to*6393 + 16384) >> 15; \
    to += (t7*3227 + 16384) >> 15; \
    tq += (t5*17515 + 16384) >> 15; \
    t5 -= (tq*13623 + 8192) >> 14; \
    tq += (t5*17515 + 16384) >> 15; \
    t4 += (tr*6723 + 4096) >> 13; \
    tr -= (t4*16069 + 8192) >> 14; \
    t4 += (tr*6723 + 4096) >> 13; \
    /* Stage 6 */ \
    tj += tdh; \
    td -= tj; \
    tc -= tih; \
    ti += tc; \
    th = tgh - th; \
    tg -= th; \
    te += tfh; \
    tf -= te; \
    tl = tkh - tl; \
    tk -= tl; \
    ta += tbh; \
    tb -= ta; \
    tn -= tmh; \
    tm += tn; \
    t8 += t9h; \
    t9 = t8 - t9; \
    tt = t3h - tt; \
    t3 -= tt; \
    t2 -= tsh; \
    ts += t2; \
    tv -= t1h; \
    t1 += tv; \
    t0 += tuh; \
    tu -= t0; \
    tp = OD_RSHIFT1(to) - tp; \
    to -= tp; \
    t6 += OD_RSHIFT1(t7); \
    t7 -= t6; \
    t4 = OD_RSHIFT1(tq) - t4; \
    tq -= t4; \
    tr += OD_RSHIFT1(t5); \
    t5 = tr - t5; \
    /* Stage 7 */ \
    td += (ti*21895 + 16384) >> 15; \
    ti -= (td*15137 + 8192) >> 14; \
    td += (ti*21895 + 16384) >> 15; \
    tj += (tc*21895 + 16384) >> 15; \
    tc -= (tj*15137 + 8192) >> 14; \
    tj += (tc*21895 + 16384) >> 15; \
    th += (te*13573 + 16384) >> 15; \
    te -= (th*11585 + 8192) >> 14; \
    th += (te*13573 + 16384) >> 15; \
    tb += (tk*21895 + 16384) >> 15; \
    tk -= (tb*15137 + 8192) >> 14; \
    tb += (tk*21895 + 16384) >> 15; \
    ta += (tl*3259 + 8192) >> 14; \
    tl -= (ta*3135 + 4096) >> 13; \
    ta += (tl*3259 + 8192) >> 14; \
    t9 += (tm*13573 + 16384) >> 15; \
    tm -= (t9*11585 + 8192) >> 14; \
    t9 += (tm*13573 + 16384) >> 15; \
    ts += (t3*3259 + 8192) >> 14; \
    t3 -= (ts*3135 + 4096) >> 13; \
    ts += (t3*3259 + 8192) >> 14; \
    t2 += (tt*3259 + 8192) >> 14; \
    tt -= (t2*3135 + 4096) >> 13; \
    t2 += (tt*3259 + 8192) >> 14; \
    tu += (t1*13573 + 16384) >> 15; \
    t1 -= (tu*11585 + 8192) >> 14; \
    tu += (t1*13573 + 16384) >> 15; \
    tp += (t6*13573 + 16384) >> 15; \
    t6 -= (tp*11585 + 8192) >> 14; \
    tp += (t6*13573 + 16384) >> 15; \
    tq += (t5*13573 + 16384) >> 15; \
    t5 -= (tq*11585 + 8192) >> 14; \
    tq += (t5*13573 + 16384) >> 15; \
  } \
  while (0)

/* Embedded 32-point orthonormal Type-IV iDST. */
#define OD_IDST_32_PR(t0, tg, t8, to, t4, tk, tc, ts, t2, ti, ta, tq, t6, tm, \
  te, tu, t1, th, t9, tp, t5, tl, td, tt, t3, tj, tb, tr, t7, tn, tf, tv) \
  /* 117 "muls", 117 + 128 = 245 adds, 36 shifts */ \
  do { \
    od_coeff t0h; \
    od_coeff t1h; \
    od_coeff t2h; \
    od_coeff t3h; \
    od_coeff t4h; \
    od_coeff t6h; \
    od_coeff t8h; \
    od_coeff t9h; \
    od_coeff tah; \
    od_coeff tbh; \
    od_coeff tch; \
    od_coeff tdh; \
    od_coeff teh; \
    od_coeff tfh; \
    od_coeff tgh; \
    od_coeff thh; \
    od_coeff tih; \
    od_coeff tjh; \
    od_coeff tkh; \
    od_coeff tlh; \
    od_coeff tmh; \
    od_coeff tnh; \
    od_coeff tph; \
    od_coeff trh; \
    od_coeff tsh; \
    od_coeff tth; \
    od_coeff tuh; \
    od_coeff tvh; \
    /* Stage 0 */ \
    tq -= (t5*13573 + 16384) >> 15; \
    t5 += (tq*11585 + 8192) >> 14; \
    tq -= (t5*13573 + 16384) >> 15; \
    tp -= (t6*13573 + 16384) >> 15; \
    t6 += (tp*11585 + 8192) >> 14; \
    tp -= (t6*13573 + 16384) >> 15; \
    tu -= (t1*13573 + 16384) >> 15; \
    t1 += (tu*11585 + 8192) >> 14; \
    tu -= (t1*13573 + 16384) >> 15; \
    t2 -= (tt*3259 + 8192) >> 14; \
    tt += (t2*3135 + 4096) >> 13; \
    t2 -= (tt*3259 + 8192) >> 14; \
    ts -= (t3*3259 + 8192) >> 14; \
    t3 += (ts*3135 + 4096) >> 13; \
    ts -= (t3*3259 + 8192) >> 14; \
    t9 -= (tm*13573 + 16384) >> 15; \
    tm += (t9*11585 + 8192) >> 14; \
    t9 -= (tm*13573 + 16384) >> 15; \
    ta -= (tl*3259 + 8192) >> 14; \
    tl += (ta*3135 + 4096) >> 13; \
    ta -= (tl*3259 + 8192) >> 14; \
    tb -= (tk*21895 + 16384) >> 15; \
    tk += (tb*15137 + 8192) >> 14; \
    tb -= (tk*21895 + 16384) >> 15; \
    th -= (te*13573 + 16384) >> 15; \
    te += (th*11585 + 8192) >> 14; \
    th -= (te*13573 + 16384) >> 15; \
    tj -= (tc*21895 + 16384) >> 15; \
    tc += (tj*15137 + 8192) >> 14; \
    tj -= (tc*21895 + 16384) >> 15; \
    td -= (ti*21895 + 16384) >> 15; \
    ti += (td*15137 + 8192) >> 14; \
    td -= (ti*21895 + 16384) >> 15; \
    /* Stage 1 */ \
    t5 = tr - t5; \
    tr -= OD_RSHIFT1(t5); \
    tq += t4; \
    t4 = OD_RSHIFT1(tq) - t4; \
    t7 += t6; \
    t6 -= OD_RSHIFT1(t7); \
    to += tp; \
    tp = OD_RSHIFT1(to) - tp; \
    tu += t0; \
    tuh = OD_RSHIFT1(tu); \
    t0 -= tuh; \
    t1 -= tv; \
    t1h = OD_RSHIFT1(t1); \
    tv += t1h; \
    ts -= t2; \
    tsh = OD_RSHIFT1(ts); \
    t2 += tsh; \
    t3 += tt; \
    t3h = OD_RSHIFT1(t3); \
    tt = t3h - tt; \
    t9 = t8 - t9; \
    t9h = OD_RSHIFT1(t9); \
    t8 -= t9h; \
    tm -= tn; \
    tmh = OD_RSHIFT1(tm); \
    tn += tmh; \
    tb += ta; \
    tbh = OD_RSHIFT1(tb); \
    ta -= tbh; \
    tk += tl; \
    tkh = OD_RSHIFT1(tk); \
    tl = tkh - tl; \
    tf += te; \
    tfh = OD_RSHIFT1(tf); \
    te -= tfh; \
    tg += th; \
    tgh = OD_RSHIFT1(tg); \
    th = tgh - th; \
    ti -= tc; \
    tih = OD_RSHIFT1(ti); \
    tc += tih; \
    td += tj; \
    tdh = OD_RSHIFT1(td); \
    tj -= tdh; \
    /* Stage 2 */ \
    t4 -= (tr*6723 + 4096) >> 13; \
    tr += (t4*16069 + 8192) >> 14; \
    t4 -= (tr*6723 + 4096) >> 13; \
    tq -= (t5*17515 + 16384) >> 15; \
    t5 += (tq*13623 + 8192) >> 14; \
    tq -= (t5*17515 + 16384) >> 15; \
    to -= (t7*3227 + 16384) >> 15; \
    t7 += (to*6393 + 16384) >> 15; \
    to -= (t7*3227 + 16384) >> 15; \
    tp -= (t6*2485 + 4096) >> 13; \
    t6 += (tp*18205 + 16384) >> 15; \
    tp -= (t6*2485 + 4096) >> 13; \
    /* Stage 3 */ \
    tp -= OD_RSHIFT1(t5); \
    t5 += tp; \
    t4 += OD_RSHIFT1(t7); \
    t7 = t4 - t7; \
    tr = OD_RSHIFT1(to) - tr; \
    to -= tr; \
    t6 += OD_RSHIFT1(tq); \
    tq = t6 - tq; \
    t0 -= tsh; \
    ts += t0; \
    tt += tuh; \
    tu -= tt; \
    tv += t3h; \
    t3 -= tv; \
    t2 -= t1h; \
    t1 += t2; \
    tn += tkh; \
    tk -= tn; \
    t8 += tbh; \
    tb -= t8; \
    tl += t9h; \
    t9 -= tl; \
    ta = tmh - ta; \
    tm = ta - tm; \
    te -= tdh; \
    td += te; \
    tj = tgh - tj; \
    tg -= tj; \
    th += tih; \
    ti = th - ti; \
    tc -= tfh; \
    tf += tc; \
    /* Stage 4 */ \
    tn -= (t8*29699 + 16384) >> 15; \
    t8 += (tn*16305 + 8192) >> 14; \
    tn -= (t8*29699 + 16384) >> 15; \
    t9 -= (tm*12151 + 8192) >> 14; \
    tm += (t9*31357 + 16384) >> 15; \
    t9 -= (tm*12151 + 8192) >> 14; \
    tl -= (ta*2455 + 2048) >> 12; \
    ta += (tl*28899 + 16384) >> 15; \
    tl -= (ta*2455 + 2048) >> 12; \
    tb -= (tk*7749 + 8192) >> 14; \
    tk += (tb*12665 + 8192) >> 14; \
    tb -= (tk*7749 + 8192) >> 14; \
    tg -= (tf*805 + 8192) >> 14; \
    tf += (tg*803 + 4096) >> 13; \
    tg -= (tf*805 + 8192) >> 14; \
    th -= (te*4861 + 16384) >> 15; \
    te += (th*1189 + 2048) >> 12; \
    th -= (te*4861 + 16384) >> 15; \
    td -= (ti*513 + 1024) >> 11; \
    ti += (td*15447 + 16384) >> 15; \
    td -= (ti*513 + 1024) >> 11; \
    tj -= (tc*11725 + 16384) >> 15; \
    tc += (tj*5197 + 4096) >> 13; \
    tj -= (tc*11725 + 16384) >> 15; \
    /* Stage 5 */ \
    t2 += t5; \
    t2h = OD_RSHIFT1(t2); \
    t5 -= t2h; \
    tt -= tq; \
    tth = OD_RSHIFT1(tt); \
    tq += tth; \
    tp += tu; \
    tph = OD_RSHIFT1(tp); \
    tu -= tph; \
    t6 -= t1; \
    t6h = OD_RSHIFT1(t6); \
    t1 += t6h; \
    tv = t7 - tv; \
    tvh = OD_RSHIFT1(tv); \
    t7 = tvh - t7; \
    t0 -= to; \
    t0h = OD_RSHIFT1(t0); \
    to += t0h; \
    t4 += ts; \
    t4h = OD_RSHIFT1(t4); \
    ts -= t4h; \
    tr -= t3; \
    trh = OD_RSHIFT1(tr); \
    t3 += trh; \
    t8 = tf - t8; \
    t8h = OD_RSHIFT1(t8); \
    tf -= t8h; \
    tn = tg - tn; \
    tnh = OD_RSHIFT1(tn); \
    tg -= tnh; \
    tc -= tb; \
    tch = OD_RSHIFT1(tc); \
    tb += tch; \
    tj += tk; \
    tjh = OD_RSHIFT1(tj); \
    tk = tjh - tk; \
    ta += ti; \
    tah = OD_RSHIFT1(ta); \
    ti -= tah; \
    tl += td; \
    tlh = OD_RSHIFT1(tl); \
    td = tlh - td; \
    te += t9; \
    teh = OD_RSHIFT1(te); \
    t9 = teh - t9; \
    th -= tm; \
    thh = OD_RSHIFT1(th); \
    tm += thh; \
    /* Stage 6 */ \
    t5 += tah; \
    ta -= t5; \
    ti += tth; \
    tt -= ti; \
    tq += tlh; \
    tl = tq - tl; \
    td -= t2h; \
    t2 += td; \
    t9 += tph; \
    tp -= t9; \
    t1 += teh; \
    te -= t1; \
    tm = t6h - tm; \
    t6 -= tm; \
    tu -= thh; \
    th += tu; \
    t7 += tnh; \
    tn = t7 - tn; \
    tg += t0h; \
    t0 -= tg; \
    to -= t8h; \
    t8 += to; \
    tf += tvh; \
    tv -= tf; \
    tb += t4h; \
    t4 -= tb; \
    t3 += tch; \
    tc -= t3; \
    tk = trh - tk; \
    tr = tk - tr; \
    ts -= tjh; \
    tj += ts; \
    /* Stage 7 */ \
    t0 -= (tv*31973 + 16384) >> 15; \
    tv += (t0*16379 + 8192) >> 14; \
    t0 -= (tv*31973 + 16384) >> 15; \
    tg -= (tf*819 + 1024) >> 11; \
    tf += (tg*22595 + 16384) >> 15; \
    tg -= (tf*819 + 1024) >> 11; \
    t8 -= (tn*10659 + 8192) >> 14; \
    tn += (t8*29957 + 16384) >> 15; \
    t8 -= (tn*10659 + 8192) >> 14; \
    to -= (t7*6101 + 16384) >> 15; \
    t7 += (to*11793 + 16384) >> 15; \
    to -= (t7*6101 + 16384) >> 15; \
    tt -= (t2*2013 + 16384) >> 15; \
    t2 += (tt*4011 + 16384) >> 15; \
    tt -= (t2*2013 + 16384) >> 15; \
    tl -= (ta*8637 + 16384) >> 15; \
    ta += (tl*16151 + 16384) >> 15; \
    tl -= (ta*8637 + 16384) >> 15; \
    ti -= (td*11273 + 16384) >> 15; \
    td += (ti*315 + 256) >> 9; \
    ti -= (td*11273 + 16384) >> 15; \
    tq -= (t5*2225 + 8192) >> 14; \
    t5 += (tq*2185 + 4096) >> 13; \
    tq -= (t5*2225 + 8192) >> 14; \
    ts -= (t3*1411 + 8192) >> 14; \
    t3 += (ts*2801 + 8192) >> 14; \
    ts -= (t3*1411 + 8192) >> 14; \
    tb -= (tk*18035 + 16384) >> 15; \
    tk += (tb*6921 + 4096) >> 13; \
    tb -= (tk*18035 + 16384) >> 15; \
    tj -= (tc*10381 + 16384) >> 15; \
    tc += (tj*4717 + 4096) >> 13; \
    tj -= (tc*10381 + 16384) >> 15; \
    t4 -= (tr*13113 + 8192) >> 14; \
    tr += (t4*7993 + 4096) >> 13; \
    t4 -= (tr*13113 + 8192) >> 14; \
    tu -= (t1*1207 + 16384) >> 15; \
    t1 += (tu*2411 + 16384) >> 15; \
    tu -= (t1*1207 + 16384) >> 15; \
    t9 -= (tm*20191 + 16384) >> 15; \
    tm += (t9*29269 + 16384) >> 15; \
    t9 -= (tm*20191 + 16384) >> 15; \
    th -= (te*3045 + 4096) >> 13; \
    te += (th*21403 + 16384) >> 15; \
    th -= (te*3045 + 4096) >> 13; \
    tp -= (t6*659 + 2048) >> 12; \
    t6 += (tp*10279 + 16384) >> 15; \
    tp -= (t6*659 + 2048) >> 12; \
  } \
  while (0)

#if CONFIG_TX64X64
#define OD_FDCT_32_ASYM_PR(t0, tg, tgh, t8, to, toh, t4, tk, tkh, tc, ts, tsh, \
  t2, ti, tih, ta, tq, tqh, t6, tm, tmh, te, tu, tuh, t1, th, thh, \
  t9, tp, tph, t5, tl, tlh, td, tt, tth, t3, tj, tjh, tb, tr, trh, \
  t7, tn, tnh, tf, tv, tvh) \
  /* Embedded 32-point asymmetric Type-II fDCT. */ \
  do { \
    t0 += tvh; \
    tv = t0 - tv; \
    t1 = tuh - t1; \
    tu -= t1; \
    t2 += tth; \
    tt = t2 - tt; \
    t3 = tsh - t3; \
    ts -= t3; \
    t4 += trh; \
    tr = t4 - tr; \
    t5 = tqh - t5; \
    tq -= t5; \
    t6 += tph; \
    tp = t6 - tp; \
    t7 = toh - t7; \
    to -= t7; \
    t8 += tnh; \
    tn = t8 - tn; \
    t9 = tmh - t9; \
    tm -= t9; \
    ta += tlh; \
    tl = ta - tl; \
    tb = tkh - tb; \
    tk -= tb; \
    tc += tjh; \
    tj = tc - tj; \
    td = tih - td; \
    ti -= td; \
    te += thh; \
    th = te - th; \
    tf = tgh - tf; \
    tg -= tf; \
    OD_FDCT_16_PR(t0, tg, t8, to, t4, tk, tc, ts, \
     t2, ti, ta, tq, t6, tm, te, tu); \
    OD_FDST_16_PR(tv, tf, tn, t7, tr, tb, tj, t3, \
     tt, td, tl, t5, tp, t9, th, t1); \
  } \
  while (0)

#define OD_IDCT_32_ASYM_PR(t0, tg, t8, to, t4, tk, tc, ts, t2, ti, ta, tq, \
  t6, tm, te, tu, t1, t1h, th, thh, t9, t9h, tp, tph, t5, t5h, tl, tlh, \
  td, tdh, tt, tth, t3, t3h, tj, tjh, tb, tbh, tr, trh, t7, t7h, tn, tnh, \
  tf, tfh, tv, tvh) \
  /* Embedded 32-point asymmetric Type-II iDCT. */ \
  do { \
    OD_IDST_16_PR(tv, tn, tr, tj, tt, tl, tp, th, \
     tu, tm, tq, ti, ts, tk, to, tg); \
    OD_IDCT_16_PR(t0, t8, t4, tc, t2, ta, t6, te, \
     t1, t9, t5, td, t3, tb, t7, tf); \
    tv = t0 - tv; \
    tvh = OD_RSHIFT1(tv); \
    t0 -= tvh; \
    t1 += tu; \
    t1h = OD_RSHIFT1(t1); \
    tu = t1h - tu; \
    tt = t2 - tt; \
    tth = OD_RSHIFT1(tt); \
    t2 -= tth; \
    t3 += ts; \
    t3h = OD_RSHIFT1(t3); \
    ts = t3h - ts; \
    tr = t4 - tr; \
    trh = OD_RSHIFT1(tr); \
    t4 -= trh; \
    t5 += tq; \
    t5h = OD_RSHIFT1(t5); \
    tq = t5h - tq; \
    tp = t6 - tp; \
    tph = OD_RSHIFT1(tp); \
    t6 -= tph; \
    t7 += to; \
    t7h = OD_RSHIFT1(t7); \
    to = t7h - to; \
    tn = t8 - tn; \
    tnh = OD_RSHIFT1(tn); \
    t8 -= tnh; \
    t9 += tm; \
    t9h = OD_RSHIFT1(t9); \
    tm = t9h - tm; \
    tl = ta - tl; \
    tlh = OD_RSHIFT1(tl); \
    ta -= tlh; \
    tb += tk; \
    tbh = OD_RSHIFT1(tb); \
    tk = tbh - tk; \
    tj = tc - tj; \
    tjh = OD_RSHIFT1(tj); \
    tc -= tjh; \
    td += ti; \
    tdh = OD_RSHIFT1(td); \
    ti = tdh - ti; \
    th = te - th; \
    thh = OD_RSHIFT1(th); \
    te -= thh; \
    tf += tg; \
    tfh = OD_RSHIFT1(tf); \
    tg = tfh - tg; \
  } \
  while (0)

#define OD_FDST_32_ASYM_PR(t0, tg, t8, to, t4, tk, tc, ts, t2, ti, ta, tq, t6, \
  tm, te, tu, t1, th, t9, tp, t5, tl, td, tt, t3, tj, tb, tr, t7, tn, tf, tv) \
  /* Embedded 32-point asymmetric Type-IV fDST. */ \
  do { \
    int t0h; \
    int t1h; \
    int t4h; \
    int t5h; \
    int tqh; \
    int trh; \
    int tuh; \
    int tvh; \
    \
    tu = -tu; \
    \
    /* 13573/16384 ~= 2*Tan[Pi/8] ~= 0.828427124746190 */ \
    OD_DCT_OVERFLOW_CHECK(tq, 13573, 8192, 271); \
    t5 -= (tq*13573 + 8192) >> 14; \
    /* 11585/32768 ~= Sin[Pi/4]/2 ~= 0.353553390593274 */ \
    OD_DCT_OVERFLOW_CHECK(t5, 11585, 16384, 272); \
    tq += (t5*11585 + 16384) >> 15; \
    /* 13573/16384 ~= 2*Tan[Pi/8] ~= 0.828427124746190 */ \
    OD_DCT_OVERFLOW_CHECK(tq, 13573, 8192, 273); \
    t5 -= (tq*13573 + 8192) >> 14; \
    /* 29957/32768 ~= Tan[Pi/8] + Tan[Pi/4]/2 ~= 0.914213562373095 */ \
    OD_DCT_OVERFLOW_CHECK(t6, 29957, 16384, 274); \
    tp += (t6*29957 + 16384) >> 15; \
    /* 11585/16384 ~= Sin[Pi/4] ~= 0.707106781186548 */ \
    OD_DCT_OVERFLOW_CHECK(tp, 11585, 8192, 275); \
    t6 -= (tp*11585 + 8192) >> 14; \
    /* -19195/32768 ~= Tan[Pi/8] - Tan[Pi/4] ~= -0.585786437626905 */ \
    OD_DCT_OVERFLOW_CHECK(t6, 19195, 16384, 276); \
    tp -= (t6*19195 + 16384) >> 15; \
    /* 29957/32768 ~= Tan[Pi/8] + Tan[Pi/4]/2 ~= 0.914213562373095 */ \
    OD_DCT_OVERFLOW_CHECK(t1, 29957, 16384, 277); \
    tu += (t1*29957 + 16384) >> 15; \
    /* 11585/16384 ~= Sin[Pi/4] ~= 0.707106781186548 */ \
    OD_DCT_OVERFLOW_CHECK(tu, 11585, 8192, 278); \
    t1 -= (tu*11585 + 8192) >> 14; \
    /* -19195/32768 ~= Tan[Pi/8] - Tan[Pi/4] ~= -0.585786437626905 */ \
    OD_DCT_OVERFLOW_CHECK(t1, 19195, 16384, 279); \
    tu -= (t1*19195 + 16384) >> 15; \
    /* 28681/32768 ~= Tan[3*Pi/16] + Tan[Pi/8]/2 ~= 0.875285419105846 */ \
    OD_DCT_OVERFLOW_CHECK(t2, 28681, 16384, 280); \
    tt += (t2*28681 + 16384) >> 15; \
    /* 15137/16384 ~= Sin[3*Pi/8] ~= 0.923879532511287 */ \
    OD_DCT_OVERFLOW_CHECK(tt, 15137, 8192, 281); \
    t2 -= (tt*15137 + 8192) >> 14; \
    /* 4161/16384 ~= Tan[3*Pi/16] - Tan[Pi/8] ~= 0.253965075546204 */ \
    OD_DCT_OVERFLOW_CHECK(t2, 4161, 8192, 282); \
    tt += (t2*4161 + 8192) >> 14; \
    /* 4161/16384 ~= Tan[3*Pi/16] - Tan[Pi/8] ~= 0.253965075546204 */ \
    OD_DCT_OVERFLOW_CHECK(ts, 4161, 8192, 283); \
    t3 += (ts*4161 + 8192) >> 14; \
    /* 15137/16384 ~= Sin[3*Pi/8] ~= 0.923879532511287 */ \
    OD_DCT_OVERFLOW_CHECK(t3, 15137, 8192, 284); \
    ts -= (t3*15137 + 8192) >> 14; \
    /* 14341/16384 ~= Tan[3*Pi/16] + Tan[Pi/8]/2 ~= 0.875285419105846 */ \
    OD_DCT_OVERFLOW_CHECK(ts, 14341, 8192, 285); \
    t3 += (ts*14341 + 8192) >> 14; \
    /* -19195/32768 ~= Tan[Pi/8] - Tan[Pi/4] ~= -0.585786437626905 */ \
    OD_DCT_OVERFLOW_CHECK(tm, 19195, 16384, 286); \
    t9 -= (tm*19195 + 16384) >> 15; \
    /* 11585/16384 ~= Sin[Pi/4] ~= 0.707106781186548 */ \
    OD_DCT_OVERFLOW_CHECK(t9, 11585, 8192, 287); \
    tm -= (t9*11585 + 8192) >> 14; \
    /* 7489/8192 ~= Tan[Pi/8] + Tan[Pi/4]/2 ~= 0.914213562373095 */ \
    OD_DCT_OVERFLOW_CHECK(tm, 7489, 4096, 288); \
    t9 += (tm*7489 + 4096) >> 13; \
    /* 3259/8192 ~= 2*Tan[Pi/16] ~= 0.397824734759316 */ \
    OD_DCT_OVERFLOW_CHECK(tl, 3259, 4096, 289); \
    ta += (tl*3259 + 4096) >> 13; \
    /* 3135/16384 ~= Sin[Pi/8]/2 ~= 0.1913417161825449 */ \
    OD_DCT_OVERFLOW_CHECK(ta, 3135, 8192, 290); \
    tl -= (ta*3135 + 8192) >> 14; \
    /* 3259/8192 ~= 2*Tan[Pi/16] ~= 0.397824734759316 */ \
    OD_DCT_OVERFLOW_CHECK(tl, 3259, 4096, 291); \
    ta += (tl*3259 + 4096) >> 13; \
    /* 4161/16384 ~= Tan[3*Pi/16] - Tan[Pi/8] ~= 0.253965075546204 */ \
    OD_DCT_OVERFLOW_CHECK(tk, 4161, 8192, 292); \
    tb += (tk*4161 + 8192) >> 14; \
    /* 15137/16384 ~= Sin[3*Pi/8] ~= 0.923879532511287 */ \
    OD_DCT_OVERFLOW_CHECK(tb, 15137, 8192, 293); \
    tk -= (tb*15137 + 8192) >> 14; \
    /* 14341/16384 ~= Tan[3*Pi/16] + Tan[Pi/8]/2 ~= 0.875285419105846 */ \
    OD_DCT_OVERFLOW_CHECK(tk, 14341, 8192, 294); \
    tb += (tk*14341 + 8192) >> 14; \
    /* 29957/32768 ~= Tan[Pi/8] + Tan[Pi/4]/2 ~= 0.914213562373095 */ \
    OD_DCT_OVERFLOW_CHECK(te, 29957, 16384, 295); \
    th += (te*29957 + 16384) >> 15; \
    /* 11585/16384 ~= Sin[Pi/4] ~= 0.707106781186548 */ \
    OD_DCT_OVERFLOW_CHECK(th, 11585, 8192, 296); \
    te -= (th*11585 + 8192) >> 14; \
    /* -19195/32768 ~= Tan[Pi/8] - Tan[Pi/4] ~= -0.585786437626905 */ \
    OD_DCT_OVERFLOW_CHECK(te, 19195, 16384, 297); \
    th -= (te*19195 + 16384) >> 15; \
    /* 28681/32768 ~= Tan[3*Pi/16] + Tan[Pi/8]/2 ~= 0.875285419105846 */ \
    OD_DCT_OVERFLOW_CHECK(tc, 28681, 16384, 298); \
    tj += (tc*28681 + 16384) >> 15; \
    /* 15137/16384 ~= Sin[3*Pi/8] ~= 0.923879532511287 */ \
    OD_DCT_OVERFLOW_CHECK(tj, 15137, 8192, 299); \
    tc -= (tj*15137 + 8192) >> 14; \
    /* 4161/16384 ~= Tan[3*Pi/16] - Tan[Pi/8] ~= 0.253965075546204 */ \
    OD_DCT_OVERFLOW_CHECK(tc, 4161, 8192, 300); \
    tj += (tc*4161 + 8192) >> 14; \
    /* 4161/16384 ~= Tan[3*Pi/16] - Tan[Pi/8] ~= 0.253965075546204 */ \
    OD_DCT_OVERFLOW_CHECK(ti, 4161, 8192, 301); \
    td += (ti*4161 + 8192) >> 14; \
    /* 15137/16384 ~= Sin[3*Pi/8] ~= 0.923879532511287 */ \
    OD_DCT_OVERFLOW_CHECK(td, 15137, 8192, 302); \
    ti -= (td*15137 + 8192) >> 14; \
    /* 14341/16384 ~= Tan[3*Pi/16] + Tan[Pi/8]/2 ~= 0.875285419105846 */ \
    OD_DCT_OVERFLOW_CHECK(ti, 14341, 8192, 303); \
    td += (ti*14341 + 8192) >> 14; \
    \
    t1 = -t1; \
    t2 = -t2; \
    t3 = -t3; \
    td = -td; \
    tg = -tg; \
    to = -to; \
    ts = -ts; \
    \
    tr -= OD_RSHIFT1(t5); \
    t5 += tr; \
    tq -= OD_RSHIFT1(t4); /* pass */ \
    t4 += tq; \
    t6 -= OD_RSHIFT1(t7); \
    t7 += t6; \
    to -= OD_RSHIFT1(tp); /* pass */ \
    tp += to; \
    t1 += OD_RSHIFT1(t0); /* pass */ \
    t0 -= t1; \
    tv -= OD_RSHIFT1(tu); \
    tu += tv; \
    t3 -= OD_RSHIFT1(tt); \
    tt += t3; \
    t2 += OD_RSHIFT1(ts); \
    ts -= t2; \
    t9 -= OD_RSHIFT1(t8); /* pass */ \
    t8 += t9; \
    tn += OD_RSHIFT1(tm); \
    tm -= tn; \
    tb += OD_RSHIFT1(ta); \
    ta -= tb; \
    tl -= OD_RSHIFT1(tk); \
    tk += tl; \
    te -= OD_RSHIFT1(tf); /* pass */ \
    tf += te; \
    tg -= OD_RSHIFT1(th); \
    th += tg; \
    tc -= OD_RSHIFT1(ti); \
    ti += tc; \
    td += OD_RSHIFT1(tj); \
    tj -= td; \
    \
    t4 = -t4; \
    \
    /* 6723/8192 ~= Tan[7*Pi/32] ~= 0.8206787908286602 */ \
    OD_DCT_OVERFLOW_CHECK(tr, 6723, 4096, 304); \
    t4 += (tr*6723 + 4096) >> 13; \
    /* 16069/16384 ~= Sin[7*Pi/16] ~= 0.9807852804032304 */ \
    OD_DCT_OVERFLOW_CHECK(t4, 16069, 8192, 305); \
    tr -= (t4*16069 + 8192) >> 14; \
    /* 6723/8192 ~= Tan[7*Pi/32] ~= 0.8206787908286602 */ \
    OD_DCT_OVERFLOW_CHECK(tr, 6723, 4096, 306); \
    t4 += (tr*6723 + 4096) >> 13; \
    /* 17515/32768 ~= Tan[5*Pi/32] ~= 0.5345111359507916 */ \
    OD_DCT_OVERFLOW_CHECK(tq, 17515, 16384, 307); \
    t5 += (tq*17515 + 16384) >> 15; \
    /* 13623/16384 ~= Sin[5*Pi/16] ~= 0.8314696123025452 */ \
    OD_DCT_OVERFLOW_CHECK(t5, 13623, 8192, 308); \
    tq -= (t5*13623 + 8192) >> 14; \
    /* 17515/32768 ~= Tan[5*Pi/32] ~= 0.5345111359507916 */ \
    OD_DCT_OVERFLOW_CHECK(tq, 17515, 16384, 309); \
    t5 += (tq*17515 + 16384) >> 15; \
    /* 3227/32768 ~= Tan[Pi/32] ~= 0.09849140335716425 */ \
    OD_DCT_OVERFLOW_CHECK(to, 3227, 16384, 310); \
    t7 += (to*3227 + 16384) >> 15; \
    /* 6393/32768 ~= Sin[Pi/16] ~= 0.19509032201612825 */ \
    OD_DCT_OVERFLOW_CHECK(t7, 6393, 16384, 311); \
    to -= (t7*6393 + 16384) >> 15; \
    /* 3227/32768 ~= Tan[Pi/32] ~= 0.09849140335716425 */ \
    OD_DCT_OVERFLOW_CHECK(to, 3227, 16384, 312); \
    t7 += (to*3227 + 16384) >> 15; \
    /* 2485/8192 ~= Tan[3*Pi/32] ~= 0.303346683607342 */ \
    OD_DCT_OVERFLOW_CHECK(tp, 2485, 4096, 313); \
    t6 += (tp*2485 + 4096) >> 13; \
    /* 18205/32768 ~= Sin[3*Pi/16] ~= 0.555570233019602 */ \
    OD_DCT_OVERFLOW_CHECK(t6, 18205, 16384, 314); \
    tp -= (t6*18205 + 16384) >> 15; \
    /* 2485/8192 ~= Tan[3*Pi/32] ~= 0.303346683607342 */ \
    OD_DCT_OVERFLOW_CHECK(tp, 2485, 4096, 315); \
    t6 += (tp*2485 + 4096) >> 13; \
    \
    t5 = -t5; \
    \
    tr += to; \
    trh = OD_RSHIFT1(tr); \
    to -= trh; \
    t4 += t7; \
    t4h = OD_RSHIFT1(t4); \
    t7 -= t4h; \
    t5 += tp; \
    t5h = OD_RSHIFT1(t5); \
    tp -= t5h; \
    tq += t6; \
    tqh = OD_RSHIFT1(tq); \
    t6 -= tqh; \
    t0 -= t3; \
    t0h = OD_RSHIFT1(t0); \
    t3 += t0h; \
    tv -= ts; \
    tvh = OD_RSHIFT1(tv); \
    ts += tvh; \
    tu += tt; \
    tuh = OD_RSHIFT1(tu); \
    tt -= tuh; \
    t1 -= t2; \
    t1h = OD_RSHIFT1(t1); \
    t2 += t1h; \
    t8 += tb; \
    tb -= OD_RSHIFT1(t8); \
    tn += tk; \
    tk -= OD_RSHIFT1(tn); \
    t9 += tl; \
    tl -= OD_RSHIFT1(t9); \
    tm -= ta; \
    ta += OD_RSHIFT1(tm); \
    tc -= tf; \
    tf += OD_RSHIFT1(tc); \
    tj += tg; \
    tg -= OD_RSHIFT1(tj); \
    td -= te; \
    te += OD_RSHIFT1(td); \
    ti += th; \
    th -= OD_RSHIFT1(ti); \
    \
    t9 = -t9; \
    tl = -tl; \
    \
    /* 805/16384 ~= Tan[Pi/64] ~= 0.04912684976946793 */ \
    OD_DCT_OVERFLOW_CHECK(tn, 805, 8192, 316); \
    t8 += (tn*805 + 8192) >> 14; \
    /* 803/8192 ~= Sin[Pi/32] ~= 0.0980171403295606 */ \
    OD_DCT_OVERFLOW_CHECK(t8, 803, 4096, 317); \
    tn -= (t8*803 + 4096) >> 13; \
    /* 805/16384 ~= Tan[Pi/64] ~= 0.04912684976946793 */ \
    OD_DCT_OVERFLOW_CHECK(tn, 805, 8192, 318); \
    t8 += (tn*805 + 8192) >> 14; \
    /* 11725/32768 ~= Tan[7*Pi/64] ~= 0.3578057213145241 */ \
    OD_DCT_OVERFLOW_CHECK(tb, 11725, 16384, 319); \
    tk += (tb*11725 + 16384) >> 15; \
    /* 5197/8192 ~= Sin[7*Pi/32] ~= 0.6343932841636455 */ \
    OD_DCT_OVERFLOW_CHECK(tk, 5197, 4096, 320); \
    tb -= (tk*5197 + 4096) >> 13; \
    /* 11725/32768 ~= Tan[7*Pi/64] ~= 0.3578057213145241 */ \
    OD_DCT_OVERFLOW_CHECK(tb, 11725, 16384, 321); \
    tk += (tb*11725 + 16384) >> 15; \
    /* 2455/4096 ~= Tan[11*Pi/64] ~= 0.5993769336819237 */ \
    OD_DCT_OVERFLOW_CHECK(tl, 2455, 2048, 322); \
    ta += (tl*2455 + 2048) >> 12; \
    /* 14449/16384 ~= Sin[11*Pi/32] ~= 0.881921264348355 */ \
    OD_DCT_OVERFLOW_CHECK(ta, 14449, 8192, 323); \
    tl -= (ta*14449 + 8192) >> 14; \
    /* 2455/4096 ~= Tan[11*Pi/64] ~= 0.5993769336819237 */ \
    OD_DCT_OVERFLOW_CHECK(tl, 2455, 2048, 324); \
    ta += (tl*2455 + 2048) >> 12; \
    /* 4861/32768 ~= Tan[3*Pi/64] ~= 0.14833598753834742 */ \
    OD_DCT_OVERFLOW_CHECK(tm, 4861, 16384, 325); \
    t9 += (tm*4861 + 16384) >> 15; \
    /* 1189/4096 ~= Sin[3*Pi/32] ~= 0.29028467725446233 */ \
    OD_DCT_OVERFLOW_CHECK(t9, 1189, 2048, 326); \
    tm -= (t9*1189 + 2048) >> 12; \
    /* 4861/32768 ~= Tan[3*Pi/64] ~= 0.14833598753834742 */ \
    OD_DCT_OVERFLOW_CHECK(tm, 4861, 16384, 327); \
    t9 += (tm*4861 + 16384) >> 15; \
    /* 805/16384 ~= Tan[Pi/64] ~= 0.04912684976946793 */ \
    OD_DCT_OVERFLOW_CHECK(tg, 805, 8192, 328); \
    tf += (tg*805 + 8192) >> 14; \
    /* 803/8192 ~= Sin[Pi/32] ~= 0.0980171403295606 */ \
    OD_DCT_OVERFLOW_CHECK(tf, 803, 4096, 329); \
    tg -= (tf*803 + 4096) >> 13; \
    /* 805/16384 ~= Tan[Pi/64] ~= 0.04912684976946793 */ \
    OD_DCT_OVERFLOW_CHECK(tg, 805, 8192, 330); \
    tf += (tg*805 + 8192) >> 14; \
    /* 2931/8192 ~= Tan[7*Pi/64] ~= 0.3578057213145241 */ \
    OD_DCT_OVERFLOW_CHECK(tj, 2931, 4096, 331); \
    tc += (tj*2931 + 4096) >> 13; \
    /* 5197/8192 ~= Sin[7*Pi/32] ~= 0.6343932841636455 */ \
    OD_DCT_OVERFLOW_CHECK(tc, 5197, 4096, 332); \
    tj -= (tc*5197 + 4096) >> 13; \
    /* 2931/8192 ~= Tan[7*Pi/64] ~= 0.3578057213145241 */ \
    OD_DCT_OVERFLOW_CHECK(tj, 2931, 4096, 333); \
    tc += (tj*2931 + 4096) >> 13; \
    /* 513/2048 ~= Tan[5*Pi/64] ~= 0.25048696019130545 */ \
    OD_DCT_OVERFLOW_CHECK(ti, 513, 1024, 334); \
    td += (ti*513 + 1024) >> 11; \
    /* 7723/16384 ~= Sin[5*Pi/32] ~= 0.47139673682599764 */ \
    OD_DCT_OVERFLOW_CHECK(td, 7723, 8192, 335); \
    ti -= (td*7723 + 8192) >> 14; \
    /* 513/2048 ~= Tan[5*Pi/64] ~= 0.25048696019130545 */ \
    OD_DCT_OVERFLOW_CHECK(ti, 513, 1024, 336); \
    td += (ti*513 + 1024) >> 11; \
    /* 4861/32768 ~= Tan[3*Pi/64] ~= 0.14833598753834742 */ \
    OD_DCT_OVERFLOW_CHECK(th, 4861, 16384, 337); \
    te += (th*4861 + 16384) >> 15; \
    /* 1189/4096 ~= Sin[3*Pi/32] ~= 0.29028467725446233 */ \
    OD_DCT_OVERFLOW_CHECK(te, 1189, 2048, 338); \
    th -= (te*1189 + 2048) >> 12; \
    /* 4861/32768 ~= Tan[3*Pi/64] ~= 0.14833598753834742 */ \
    OD_DCT_OVERFLOW_CHECK(th, 4861, 16384, 339); \
    te += (th*4861 + 16384) >> 15; \
    \
    ta = -ta; \
    tb = -tb; \
    \
    tt += t5h; \
    t5 -= tt; \
    t2 -= tqh; \
    tq += t2; \
    tp += t1h; \
    t1 -= tp; \
    t6 -= tuh; \
    tu += t6; \
    t7 += tvh; \
    tv -= t7; \
    to += t0h; \
    t0 -= to; \
    t3 -= t4h; \
    t4 += t3; \
    ts += trh; \
    tr -= ts; \
    tf -= OD_RSHIFT1(tn); \
    tn += tf; \
    tg -= OD_RSHIFT1(t8); \
    t8 += tg; \
    tk += OD_RSHIFT1(tc); \
    tc -= tk; \
    tb += OD_RSHIFT1(tj); \
    tj -= tb; \
    ta += OD_RSHIFT1(ti); \
    ti -= ta; \
    tl += OD_RSHIFT1(td); \
    td -= tl; \
    te -= OD_RSHIFT1(tm); \
    tm += te; \
    th -= OD_RSHIFT1(t9); \
    t9 += th; \
    ta -= t5; \
    t5 += OD_RSHIFT1(ta); \
    tq -= tl; \
    tl += OD_RSHIFT1(tq); \
    t2 -= ti; \
    ti += OD_RSHIFT1(t2); \
    td -= tt; \
    tt += OD_RSHIFT1(td); \
    tm += tp; \
    tp -= OD_RSHIFT1(tm); \
    t6 += t9; \
    t9 -= OD_RSHIFT1(t6); \
    te -= tu; \
    tu += OD_RSHIFT1(te); \
    t1 -= th; \
    th += OD_RSHIFT1(t1); \
    t0 -= tg; \
    tg += OD_RSHIFT1(t0); \
    tf += tv; \
    tv -= OD_RSHIFT1(tf); \
    t8 -= t7; \
    t7 += OD_RSHIFT1(t8); \
    to -= tn; \
    tn += OD_RSHIFT1(to); \
    t4 -= tk; \
    tk += OD_RSHIFT1(t4); \
    tb -= tr; \
    tr += OD_RSHIFT1(tb); \
    t3 -= tj; \
    tj += OD_RSHIFT1(t3); \
    tc -= ts; \
    ts += OD_RSHIFT1(tc); \
    \
    tr = -tr; \
    ts = -ts; \
    tt = -tt; \
    tu = -tu; \
    \
    /* 2847/4096 ~= (1/Sqrt[2] - Cos[63*Pi/128]/2)/Sin[63*Pi/128] */ \
    OD_DCT_OVERFLOW_CHECK(t0, 2847, 2048, 340); \
    tv += (t0*2847 + 2048) >> 12; \
    /* 5791/4096 ~= Sqrt[2]*Sin[63*Pi/128] */  \
    OD_DCT_OVERFLOW_CHECK(tv, 5791, 2048, 341); \
    t0 -= (tv*5791 + 2048) >> 12; \
    /* 5593/8192 ~= (1/Sqrt[2] - Cos[63*Pi/128])/Sin[63*Pi/128] */ \
    OD_DCT_OVERFLOW_CHECK(t0, 5593, 4096, 342); \
    tv += (t0*5593 + 4096) >> 13; \
    /* 4099/8192 ~= (1/Sqrt[2] - Cos[31*Pi/128]/2)/Sin[31*Pi/128] */ \
    OD_DCT_OVERFLOW_CHECK(tf, 4099, 4096, 343); \
    tg -= (tf*4099 + 4096) >> 13; \
    /* 1997/2048 ~= Sqrt[2]*Sin[31*Pi/128] */ \
    OD_DCT_OVERFLOW_CHECK(tg, 1997, 1024, 344); \
    tf += (tg*1997 + 1024) >> 11; \
    /* -815/32768 ~= (1/Sqrt[2] - Cos[31*Pi/128])/Sin[31*Pi/128] */ \
    OD_DCT_OVERFLOW_CHECK(tf, 815, 16384, 345); \
    tg += (tf*815 + 16384) >> 15; \
    /* 2527/4096 ~= (1/Sqrt[2] - Cos[17*Pi/128]/2)/Sin[17*Pi/128] */ \
    OD_DCT_OVERFLOW_CHECK(t8, 2527, 2048, 346); \
    tn -= (t8*2527 + 2048) >> 12; \
    /* 4695/8192 ~= Sqrt[2]*Sin[17*Pi/128] */ \
    OD_DCT_OVERFLOW_CHECK(tn, 4695, 4096, 347); \
    t8 += (tn*4695 + 4096) >> 13; \
    /* -4187/8192 ~= (1/Sqrt[2] - Cos[17*Pi/128])/Sin[17*Pi/128] */ \
    OD_DCT_OVERFLOW_CHECK(t8, 4187, 4096, 348); \
    tn += (t8*4187 + 4096) >> 13; \
    /* 5477/8192 ~= (1/Sqrt[2] - Cos[15*Pi/128]/2)/Sin[15*Pi/128] */ \
    OD_DCT_OVERFLOW_CHECK(to, 5477, 4096, 349); \
    t7 += (to*5477 + 4096) >> 13; \
    /* 4169/8192 ~= Sqrt[2]*Sin[15*Pi/128] */ \
    OD_DCT_OVERFLOW_CHECK(t7, 4169, 4096, 350); \
    to -= (t7*4169 + 4096) >> 13; \
    /* -2571/4096 ~= (1/Sqrt[2] - Cos[15*Pi/128])/Sin[15*Pi/128] */ \
    OD_DCT_OVERFLOW_CHECK(to, 2571, 2048, 351); \
    t7 -= (to*2571 + 2048) >> 12; \
    /* 5331/8192 ~= (1/Sqrt[2] - Cos[59*Pi/128]/2)/Sin[59*Pi/128] */ \
    OD_DCT_OVERFLOW_CHECK(t2, 5331, 4096, 352); \
    tt += (t2*5331 + 4096) >> 13; \
    /* 5749/4096 ~= Sqrt[2]*Sin[59*Pi/128] */ \
    OD_DCT_OVERFLOW_CHECK(tt, 5749, 2048, 353); \
    t2 -= (tt*5749 + 2048) >> 12; \
    /* 2413/4096 ~= (1/Sqrt[2] - Cos[59*Pi/128])/Sin[59*Pi/128] */ \
    OD_DCT_OVERFLOW_CHECK(t2, 2413, 2048, 354); \
    tt += (t2*2413 + 2048) >> 12; \
    /* 4167/8192 ~= (1/Sqrt[2] - Cos[27*Pi/128]/2)/Sin[27*Pi/128] */ \
    OD_DCT_OVERFLOW_CHECK(td, 4167, 4096, 355); \
    ti -= (td*4167 + 4096) >> 13; \
    /* 891/1024 ~= Sqrt[2]*Sin[27*Pi/128] */ \
    OD_DCT_OVERFLOW_CHECK(ti, 891, 512, 356); \
    td += (ti*891 + 512) >> 10; \
    /* -4327/32768 ~= (1/Sqrt[2] - Cos[27*Pi/128])/Sin[27*Pi/128] */ \
    OD_DCT_OVERFLOW_CHECK(td, 4327, 16384, 357); \
    ti += (td*4327 + 16384) >> 15; \
    /* 2261/4096 ~= (1/Sqrt[2] - Cos[21*Pi/128]/2)/Sin[21*Pi/128] */ \
    OD_DCT_OVERFLOW_CHECK(ta, 2261, 2048, 358); \
    tl -= (ta*2261 + 2048) >> 12; \
    /* 2855/4096 ~= Sqrt[2]*Sin[21*Pi/128] */ \
    OD_DCT_OVERFLOW_CHECK(tl, 2855, 2048, 359); \
    ta += (tl*2855 + 2048) >> 12; \
    /* -5417/16384 ~= (1/Sqrt[2] - Cos[21*Pi/128])/Sin[21*Pi/128] */ \
    OD_DCT_OVERFLOW_CHECK(ta, 5417, 8192, 360); \
    tl += (ta*5417 + 8192) >> 14; \
    /* 3459/4096 ~= (1/Sqrt[2] - Cos[11*Pi/128]/2)/Sin[11*Pi/128] */ \
    OD_DCT_OVERFLOW_CHECK(tq, 3459, 2048, 361); \
    t5 += (tq*3459 + 2048) >> 12; \
    /* 1545/4096 ~= Sqrt[2]*Sin[11*Pi/128] */ \
    OD_DCT_OVERFLOW_CHECK(t5, 1545, 2048, 362); \
    tq -= (t5*1545 + 2048) >> 12; \
    /* -1971/2048 ~= (1/Sqrt[2] - Cos[11*Pi/128])/Sin[11*Pi/128] */ \
    OD_DCT_OVERFLOW_CHECK(tq, 1971, 1024, 363); \
    t5 -= (tq*1971 + 1024) >> 11; \
    /* 323/512 ~= (1/Sqrt[2] - Cos[57*Pi/128]/2)/Sin[57*Pi/128] */ \
    OD_DCT_OVERFLOW_CHECK(t3, 323, 256, 364); \
    ts += (t3*323 + 256) >> 9; \
    /* 5707/4096 ~= Sqrt[2]*Sin[57*Pi/128] */ \
    OD_DCT_OVERFLOW_CHECK(ts, 5707, 2048, 365); \
    t3 -= (ts*5707 + 2048) >> 12; \
    /* 2229/4096 ~= (1/Sqrt[2] - Cos[57*Pi/128])/Sin[57*Pi/128] */ \
    OD_DCT_OVERFLOW_CHECK(t3, 2229, 2048, 366); \
    ts += (t3*2229 + 2048) >> 12; \
    /* 1061/2048 ~= (1/Sqrt[2] - Cos[25*Pi/128]/2)/Sin[25*Pi/128] */ \
    OD_DCT_OVERFLOW_CHECK(tc, 1061, 1024, 367); \
    tj -= (tc*1061 + 1024) >> 11; \
    /* 6671/8192 ~= Sqrt[2]*Sin[25*Pi/128] */ \
    OD_DCT_OVERFLOW_CHECK(tj, 6671, 4096, 368); \
    tc += (tj*6671 + 4096) >> 13; \
    /* -6287/32768 ~= (1/Sqrt[2] - Cos[25*Pi/128])/Sin[25*Pi/128] */ \
    OD_DCT_OVERFLOW_CHECK(tc, 6287, 16384, 369); \
    tj += (tc*6287 + 16384) >> 15; \
    /* 4359/8192 ~= (1/Sqrt[2] - Cos[23*Pi/128]/2)/Sin[23*Pi/128] */ \
    OD_DCT_OVERFLOW_CHECK(tb, 4359, 4096, 370); \
    tk -= (tb*4359 + 4096) >> 13; \
    /* 3099/4096 ~= Sqrt[2]*Sin[23*Pi/128] */ \
    OD_DCT_OVERFLOW_CHECK(tk, 3099, 2048, 371); \
    tb += (tk*3099 + 2048) >> 12; \
    /* -2109/8192 ~= (1/Sqrt[2] - Cos[23*Pi/128])/Sin[23*Pi/128] */ \
    OD_DCT_OVERFLOW_CHECK(tb, 2109, 4096, 372); \
    tk += (tb*2109 + 4096) >> 13; \
    /* 5017/8192 ~= (1/Sqrt[2] - Cos[55*Pi/128]/2)/Sin[55*Pi/128] */ \
    OD_DCT_OVERFLOW_CHECK(t4, 5017, 4096, 373); \
    tr += (t4*5017 + 4096) >> 13; \
    /* 1413/1024 ~= Sqrt[2]*Sin[55*Pi/128] */ \
    OD_DCT_OVERFLOW_CHECK(tr, 1413, 512, 374); \
    t4 -= (tr*1413 + 512) >> 10; \
    /* 8195/16384 ~= (1/Sqrt[2] - Cos[55*Pi/128])/Sin[55*Pi/128] */ \
    OD_DCT_OVERFLOW_CHECK(t4, 8195, 8192, 375); \
    tr += (t4*8195 + 8192) >> 14; \
    /* 2373/4096 ~= (1/Sqrt[2] - Cos[19*Pi/128]/2)/Sin[19*Pi/128] */ \
    OD_DCT_OVERFLOW_CHECK(tm, 2373, 2048, 376); \
    t9 += (tm*2373 + 2048) >> 12; \
    /* 5209/8192 ~= Sqrt[2]*Sin[19*Pi/128] */ \
    OD_DCT_OVERFLOW_CHECK(t9, 5209, 4096, 377); \
    tm -= (t9*5209 + 4096) >> 13; \
    /* -3391/8192 ~= (1/Sqrt[2] - Cos[19*Pi/128])/Sin[19*Pi/128] */ \
    OD_DCT_OVERFLOW_CHECK(tm, 3391, 4096, 378); \
    t9 -= (tm*3391 + 4096) >> 13; \
    /* 1517/2048 ~= (1/Sqrt[2] - Cos[13*Pi/128]/2)/Sin[13*Pi/128] */ \
    OD_DCT_OVERFLOW_CHECK(t6, 1517, 1024, 379); \
    tp -= (t6*1517 + 1024) >> 11; \
    /* 1817/4096 ~= Sqrt[2]*Sin[13*Pi/128] */ \
    OD_DCT_OVERFLOW_CHECK(tp, 1817, 2048, 380); \
    t6 += (tp*1817 + 2048) >> 12; \
    /* -6331/8192 ~= (1/Sqrt[2] - Cos[13*Pi/128])/Sin[13*Pi/128] */ \
    OD_DCT_OVERFLOW_CHECK(t6, 6331, 4096, 381); \
    tp += (t6*6331 + 4096) >> 13; \
    /* 515/1024 ~= (1/Sqrt[2] - Cos[29*Pi/128]/2)/Sin[29*Pi/128] */ \
    OD_DCT_OVERFLOW_CHECK(te, 515, 512, 382); \
    th -= (te*515 + 512) >> 10; \
    /* 7567/8192 ~= Sqrt[2]*Sin[29*Pi/128] */ \
    OD_DCT_OVERFLOW_CHECK(th, 7567, 4096, 383); \
    te += (th*7567 + 4096) >> 13; \
    /* -2513/32768 ~= (1/Sqrt[2] - Cos[29*Pi/128])/Sin[29*Pi/128] */ \
    OD_DCT_OVERFLOW_CHECK(te, 2513, 16384, 384); \
    th += (te*2513 + 16384) >> 15; \
    /* 2753/4096 ~= (1/Sqrt[2] - Cos[61*Pi/128]/2)/Sin[61*Pi/128] */ \
    OD_DCT_OVERFLOW_CHECK(t1, 2753, 2048, 385); \
    tu += (t1*2753 + 2048) >> 12; \
    /* 5777/4096 ~= Sqrt[2]*Sin[61*Pi/128] */ \
    OD_DCT_OVERFLOW_CHECK(tu, 5777, 2048, 386); \
    t1 -= (tu*5777 + 2048) >> 12; \
    /* 1301/2048 ~= (1/Sqrt[2] - Cos[61*Pi/128])/Sin[61*Pi/128] */ \
    OD_DCT_OVERFLOW_CHECK(t1, 1301, 1024, 387); \
    tu += (t1*1301 + 1024) >> 11; \
  } \
  while (0)

#define OD_IDST_32_ASYM_PR(t0, tg, t8, to, t4, tk, tc, ts, t2, ti, ta, tq, t6, \
  tm, te, tu, t1, th, t9, tp, t5, tl, td, tt, t3, tj, tb, tr, t7, tn, tf, tv) \
  /* Embedded 32-point asymmetric Type-IV iDST. */ \
  do { \
    int t0h; \
    int t4h; \
    int tbh; \
    int tfh; \
    int tgh; \
    int tkh; \
    int trh; \
    int tvh; \
    /* 1301/2048 ~= (1/Sqrt[2] - Cos[61*Pi/128])/Sin[61*Pi/128] */ \
    tf -= (tg*1301 + 1024) >> 11; \
    /* 5777/4096 ~= Sqrt[2]*Sin[61*Pi/128] */ \
    tg += (tf*5777 + 2048) >> 12; \
    /* 2753/4096 ~= (1/Sqrt[2] - Cos[61*Pi/128]/2)/Sin[61*Pi/128] */ \
    tf -= (tg*2753 + 2048) >> 12; \
    /* -2513/32768 ~= (1/Sqrt[2] - Cos[29*Pi/128])/Sin[29*Pi/128] */ \
    th -= (te*2513 + 16384) >> 15; \
    /* 7567/8192 ~= Sqrt[2]*Sin[29*Pi/128] */ \
    te -= (th*7567 + 4096) >> 13; \
    /* 515/1024 ~= (1/Sqrt[2] - Cos[29*Pi/128]/2)/Sin[29*Pi/128] */ \
    th += (te*515 + 512) >> 10; \
    /* -6331/8192 ~= (1/Sqrt[2] - Cos[13*Pi/128])/Sin[13*Pi/128] */ \
    tj -= (tc*6331 + 4096) >> 13; \
    /* 1817/4096 ~= Sqrt[2]*Sin[13*Pi/128] */ \
    tc -= (tj*1817 + 2048) >> 12; \
    /* 1517/2048 ~= (1/Sqrt[2] - Cos[13*Pi/128]/2)/Sin[13*Pi/128] */ \
    tj += (tc*1517 + 1024) >> 11; \
    /* -3391/8192 ~= (1/Sqrt[2] - Cos[19*Pi/128])/Sin[19*Pi/128] */ \
    ti += (td*3391 + 4096) >> 13; \
    /* 5209/8192 ~= Sqrt[2]*Sin[19*Pi/128] */ \
    td += (ti*5209 + 4096) >> 13; \
    /* 2373/4096 ~= (1/Sqrt[2] - Cos[19*Pi/128]/2)/Sin[19*Pi/128] */ \
    ti -= (td*2373 + 2048) >> 12; \
    /* 8195/16384 ~= (1/Sqrt[2] - Cos[55*Pi/128])/Sin[55*Pi/128] */ \
    tr -= (t4*8195 + 8192) >> 14; \
    /* 1413/1024 ~= Sqrt[2]*Sin[55*Pi/128] */ \
    t4 += (tr*1413 + 512) >> 10; \
    /* 5017/8192 ~= (1/Sqrt[2] - Cos[55*Pi/128]/2)/Sin[55*Pi/128] */ \
    tr -= (t4*5017 + 4096) >> 13; \
    /* -2109/8192 ~= (1/Sqrt[2] - Cos[23*Pi/128])/Sin[23*Pi/128] */ \
    t5 -= (tq*2109 + 4096) >> 13; \
    /* 3099/4096 ~= Sqrt[2]*Sin[23*Pi/128] */ \
    tq -= (t5*3099 + 2048) >> 12; \
    /* 4359/8192 ~= (1/Sqrt[2] - Cos[23*Pi/128]/2)/Sin[23*Pi/128] */ \
    t5 += (tq*4359 + 4096) >> 13; \
    /* -6287/32768 ~= (1/Sqrt[2] - Cos[25*Pi/128])/Sin[25*Pi/128] */ \
    tp -= (t6*6287 + 16384) >> 15; \
    /* 6671/8192 ~= Sqrt[2]*Sin[25*Pi/128] */ \
    t6 -= (tp*6671 + 4096) >> 13; \
    /* 1061/2048 ~= (1/Sqrt[2] - Cos[25*Pi/128]/2)/Sin[25*Pi/128] */ \
    tp += (t6*1061 + 1024) >> 11; \
    /* 2229/4096 ~= (1/Sqrt[2] - Cos[57*Pi/128])/Sin[57*Pi/128] */ \
    t7 -= (to*2229 + 2048) >> 12; \
    /* 5707/4096 ~= Sqrt[2]*Sin[57*Pi/128] */ \
    to += (t7*5707 + 2048) >> 12; \
    /* 323/512 ~= (1/Sqrt[2] - Cos[57*Pi/128]/2)/Sin[57*Pi/128] */ \
    t7 -= (to*323 + 256) >> 9; \
    /* -1971/2048 ~= (1/Sqrt[2] - Cos[11*Pi/128])/Sin[11*Pi/128] */ \
    tk += (tb*1971 + 1024) >> 11; \
    /* 1545/4096 ~= Sqrt[2]*Sin[11*Pi/128] */ \
    tb += (tk*1545 + 2048) >> 12; \
    /* 3459/4096 ~= (1/Sqrt[2] - Cos[11*Pi/128]/2)/Sin[11*Pi/128] */ \
    tk -= (tb*3459 + 2048) >> 12; \
    /* -5417/16384 ~= (1/Sqrt[2] - Cos[21*Pi/128])/Sin[21*Pi/128] */ \
    tl -= (ta*5417 + 8192) >> 14; \
    /* 2855/4096 ~= Sqrt[2]*Sin[21*Pi/128] */ \
    ta -= (tl*2855 + 2048) >> 12; \
    /* 2261/4096 ~= (1/Sqrt[2] - Cos[21*Pi/128]/2)/Sin[21*Pi/128] */ \
    tl += (ta*2261 + 2048) >> 12; \
    /* -4327/32768 ~= (1/Sqrt[2] - Cos[27*Pi/128])/Sin[27*Pi/128] */ \
    t9 -= (tm*4327 + 16384) >> 15; \
    /* 891/1024 ~= Sqrt[2]*Sin[27*Pi/128] */ \
    tm -= (t9*891 + 512) >> 10; \
    /* 4167/8192 ~= (1/Sqrt[2] - Cos[27*Pi/128]/2)/Sin[27*Pi/128] */ \
    t9 += (tm*4167 + 4096) >> 13; \
    /* 2413/4096 ~= (1/Sqrt[2] - Cos[59*Pi/128])/Sin[59*Pi/128] */ \
    tn -= (t8*2413 + 2048) >> 12; \
    /* 5749/4096 ~= Sqrt[2]*Sin[59*Pi/128] */ \
    t8 += (tn*5749 + 2048) >> 12; \
    /* 5331/8192 ~= (1/Sqrt[2] - Cos[59*Pi/128]/2)/Sin[59*Pi/128] */ \
    tn -= (t8*5331 + 4096) >> 13; \
    /* -2571/4096 ~= (1/Sqrt[2] - Cos[15*Pi/128])/Sin[15*Pi/128] */ \
    ts += (t3*2571 + 2048) >> 12; \
    /* 4169/8192 ~= Sqrt[2]*Sin[15*Pi/128] */ \
    t3 += (ts*4169 + 4096) >> 13; \
    /* 5477/8192 ~= (1/Sqrt[2] - Cos[15*Pi/128]/2)/Sin[15*Pi/128] */ \
    ts -= (t3*5477 + 4096) >> 13; \
    /* -4187/8192 ~= (1/Sqrt[2] - Cos[17*Pi/128])/Sin[17*Pi/128] */ \
    tt -= (t2*4187 + 4096) >> 13; \
    /* 4695/8192 ~= Sqrt[2]*Sin[17*Pi/128] */ \
    t2 -= (tt*4695 + 4096) >> 13; \
    /* 2527/4096 ~= (1/Sqrt[2] - Cos[17*Pi/128]/2)/Sin[17*Pi/128] */ \
    tt += (t2*2527 + 2048) >> 12; \
    /* -815/32768 ~= (1/Sqrt[2] - Cos[31*Pi/128])/Sin[31*Pi/128] */ \
    t1 -= (tu*815 + 16384) >> 15; \
    /* 1997/2048 ~= Sqrt[2]*Sin[31*Pi/128] */ \
    tu -= (t1*1997 + 1024) >> 11; \
    /* 4099/8192 ~= (1/Sqrt[2] - Cos[31*Pi/128]/2)/Sin[31*Pi/128] */ \
    t1 += (tu*4099 + 4096) >> 13; \
    /* 5593/8192 ~= (1/Sqrt[2] - Cos[63*Pi/128])/Sin[63*Pi/128] */ \
    tv -= (t0*5593 + 4096) >> 13; \
    /* 5791/4096 ~= Sqrt[2]*Sin[63*Pi/128] */ \
    t0 += (tv*5791 + 2048) >> 12; \
    /* 2847/4096 ~= (1/Sqrt[2] - Cos[63*Pi/128]/2)/Sin[63*Pi/128] */ \
    tv -= (t0*2847 + 2048) >> 12; \
    \
    t7 = -t7; \
    tf = -tf; \
    tn = -tn; \
    tr = -tr; \
    \
    t7 -= OD_RSHIFT1(t6); \
    t6 += t7; \
    tp -= OD_RSHIFT1(to); \
    to += tp; \
    tr -= OD_RSHIFT1(tq); \
    tq += tr; \
    t5 -= OD_RSHIFT1(t4); \
    t4 += t5; \
    tt -= OD_RSHIFT1(t3); \
    t3 += tt; \
    ts -= OD_RSHIFT1(t2); \
    t2 += ts; \
    tv += OD_RSHIFT1(tu); \
    tu -= tv; \
    t1 -= OD_RSHIFT1(t0); \
    t0 += t1; \
    th -= OD_RSHIFT1(tg); \
    tg += th; \
    tf -= OD_RSHIFT1(te); \
    te += tf; \
    ti += OD_RSHIFT1(tc); \
    tc -= ti; \
    tj += OD_RSHIFT1(td); \
    td -= tj; \
    tn -= OD_RSHIFT1(tm); \
    tm += tn; \
    t9 -= OD_RSHIFT1(t8); \
    t8 += t9; \
    tl -= OD_RSHIFT1(tb); \
    tb += tl; \
    tk -= OD_RSHIFT1(ta); \
    ta += tk; \
    \
    ti -= th; \
    th += OD_RSHIFT1(ti); \
    td -= te; \
    te += OD_RSHIFT1(td); \
    tm += tl; \
    tl -= OD_RSHIFT1(tm); \
    t9 += ta; \
    ta -= OD_RSHIFT1(t9); \
    tp += tq; \
    tq -= OD_RSHIFT1(tp); \
    t6 += t5; \
    t5 -= OD_RSHIFT1(t6); \
    t2 -= t1; \
    t1 += OD_RSHIFT1(t2); \
    tt -= tu; \
    tu += OD_RSHIFT1(tt); \
    tr += t7; \
    trh = OD_RSHIFT1(tr); \
    t7 -= trh; \
    t4 -= to; \
    t4h = OD_RSHIFT1(t4); \
    to += t4h; \
    t0 += t3; \
    t0h = OD_RSHIFT1(t0); \
    t3 -= t0h; \
    tv += ts; \
    tvh = OD_RSHIFT1(tv); \
    ts -= tvh; \
    tf -= tc; \
    tfh = OD_RSHIFT1(tf); \
    tc += tfh; \
    tg += tj; \
    tgh = OD_RSHIFT1(tg); \
    tj -= tgh; \
    tb -= t8; \
    tbh = OD_RSHIFT1(tb); \
    t8 += tbh; \
    tk += tn; \
    tkh = OD_RSHIFT1(tk); \
    tn -= tkh; \
    \
    ta = -ta; \
    tq = -tq; \
    \
    /* 4861/32768 ~= Tan[3*Pi/64] ~= 0.14833598753834742 */ \
    te -= (th*4861 + 16384) >> 15; \
    /* 1189/4096 ~= Sin[3*Pi/32] ~= 0.29028467725446233 */ \
    th += (te*1189 + 2048) >> 12; \
    /* 4861/32768 ~= Tan[3*Pi/64] ~= 0.14833598753834742 */ \
    te -= (th*4861 + 16384) >> 15; \
    /* 513/2048 ~= Tan[5*Pi/64] ~= 0.25048696019130545 */ \
    tm -= (t9*513 + 1024) >> 11; \
    /* 7723/16384 ~= Sin[5*Pi/32] ~= 0.47139673682599764 */ \
    t9 += (tm*7723 + 8192) >> 14; \
    /* 513/2048 ~= Tan[5*Pi/64] ~= 0.25048696019130545 */ \
    tm -= (t9*513 + 1024) >> 11; \
    /* 2931/8192 ~= Tan[7*Pi/64] ~= 0.3578057213145241 */ \
    t6 -= (tp*2931 + 4096) >> 13; \
    /* 5197/8192 ~= Sin[7*Pi/32] ~= 0.6343932841636455 */ \
    tp += (t6*5197 + 4096) >> 13; \
    /* 2931/8192 ~= Tan[7*Pi/64] ~= 0.3578057213145241 */ \
    t6 -= (tp*2931 + 4096) >> 13; \
    /* 805/16384 ~= Tan[Pi/64] ~= 0.04912684976946793 */ \
    tu -= (t1*805 + 8192) >> 14; \
    /* 803/8192 ~= Sin[Pi/32] ~= 0.0980171403295606 */ \
    t1 += (tu*803 + 4096) >> 13; \
    /* 805/16384 ~= Tan[Pi/64] ~= 0.04912684976946793 */ \
    tu -= (t1*805 + 8192) >> 14; \
    /* 4861/32768 ~= Tan[3*Pi/64] ~= 0.14833598753834742 */ \
    ti -= (td*4861 + 16384) >> 15; \
    /* 1189/4096 ~= Sin[3*Pi/32] ~= 0.29028467725446233 */ \
    td += (ti*1189 + 2048) >> 12; \
    /* 4861/32768 ~= Tan[3*Pi/64] ~= 0.14833598753834742 */ \
    ti -= (td*4861 + 16384) >> 15; \
    /* 2455/4096 ~= Tan[11*Pi/64] ~= 0.5993769336819237 */ \
    ta -= (tl*2455 + 2048) >> 12; \
    /* 14449/16384 ~= Sin[11*Pi/32] ~= 0.881921264348355 */ \
    tl += (ta*14449 + 8192) >> 14; \
    /* 2455/4096 ~= Tan[11*Pi/64] ~= 0.5993769336819237 */ \
    ta -= (tl*2455 + 2048) >> 12; \
    /* 11725/32768 ~= Tan[7*Pi/64] ~= 0.3578057213145241 */ \
    t5 -= (tq*11725 + 16384) >> 15; \
    /* 5197/8192 ~= Sin[7*Pi/32] ~= 0.6343932841636455 */ \
    tq += (t5*5197 + 4096) >> 13; \
    /* 11725/32768 ~= Tan[7*Pi/64] ~= 0.3578057213145241 */ \
    t5 -= (tq*11725 + 16384) >> 15; \
    /* 805/16384 ~= Tan[Pi/64] ~= 0.04912684976946793 */ \
    t2 -= (tt*805 + 8192) >> 14; \
    /* 803/8192 ~= Sin[Pi/32] ~= 0.0980171403295606 */ \
    tt += (t2*803 + 4096) >> 13; \
    /* 805/16384 ~= Tan[Pi/64] ~= 0.04912684976946793 */ \
    t2 -= (tt*805 + 8192) >> 14; \
    \
    tl = -tl; \
    ti = -ti; \
    \
    th += OD_RSHIFT1(t9); \
    t9 -= th; \
    te -= OD_RSHIFT1(tm); \
    tm += te; \
    t1 += OD_RSHIFT1(tp); \
    tp -= t1; \
    tu -= OD_RSHIFT1(t6); \
    t6 += tu; \
    ta -= OD_RSHIFT1(td); \
    td += ta; \
    tl += OD_RSHIFT1(ti); \
    ti -= tl; \
    t5 += OD_RSHIFT1(tt); \
    tt -= t5; \
    tq += OD_RSHIFT1(t2); \
    t2 -= tq; \
    \
    t8 -= tgh; \
    tg += t8; \
    tn += tfh; \
    tf -= tn; \
    t7 -= tvh; \
    tv += t7; \
    to -= t0h; \
    t0 += to; \
    tc += tbh; \
    tb -= tc; \
    tj += tkh; \
    tk -= tj; \
    ts += t4h; \
    t4 -= ts; \
    t3 += trh; \
    tr -= t3; \
    \
    tk = -tk; \
    \
    /* 2485/8192 ~= Tan[3*Pi/32] ~= 0.303346683607342 */ \
    tc -= (tj*2485 + 4096) >> 13; \
    /* 18205/32768 ~= Sin[3*Pi/16] ~= 0.555570233019602 */ \
    tj += (tc*18205 + 16384) >> 15; \
    /* 2485/8192 ~= Tan[3*Pi/32] ~= 0.303346683607342 */ \
    tc -= (tj*2485 + 4096) >> 13; \
    /* 3227/32768 ~= Tan[Pi/32] ~= 0.09849140335716425 */ \
    ts -= (t3*3227 + 16384) >> 15; \
    /* 6393/32768 ~= Sin[Pi/16] ~= 0.19509032201612825 */ \
    t3 += (ts*6393 + 16384) >> 15; \
    /* 3227/32768 ~= Tan[Pi/32] ~= 0.09849140335716425 */ \
    ts -= (t3*3227 + 16384) >> 15; \
    /* 17515/32768 ~= Tan[5*Pi/32] ~= 0.5345111359507916 */ \
    tk -= (tb*17515 + 16384) >> 15; \
    /* 13623/16384 ~= Sin[5*Pi/16] ~= 0.8314696123025452 */ \
    tb += (tk*13623 + 8192) >> 14; \
    /* 17515/32768 ~= Tan[5*Pi/32] ~= 0.5345111359507916 */ \
    tk -= (tb*17515 + 16384) >> 15; \
    /* 6723/8192 ~= Tan[7*Pi/32] ~= 0.8206787908286602 */ \
    t4 -= (tr*6723 + 4096) >> 13; \
    /* 16069/16384 ~= Sin[7*Pi/16] ~= 0.9807852804032304 */ \
    tr += (t4*16069 + 8192) >> 14; \
    /* 6723/8192 ~= Tan[7*Pi/32] ~= 0.8206787908286602 */ \
    t4 -= (tr*6723 + 4096) >> 13; \
    \
    t4 = -t4; \
    \
    tp += tm; \
    tm -= OD_RSHIFT1(tp); \
    t9 -= t6; \
    t6 += OD_RSHIFT1(t9); \
    th -= t1; \
    t1 += OD_RSHIFT1(th); \
    tu -= te; \
    te += OD_RSHIFT1(tu); /* pass */ \
    t5 -= tl; \
    tl += OD_RSHIFT1(t5); \
    ta += tq; \
    tq -= OD_RSHIFT1(ta); \
    td += tt; \
    tt -= OD_RSHIFT1(td); \
    t2 -= ti; \
    ti += OD_RSHIFT1(t2); /* pass */ \
    t7 += t8; \
    t8 -= OD_RSHIFT1(t7); \
    tn -= to; \
    to += OD_RSHIFT1(tn); \
    tf -= tv; \
    tv += OD_RSHIFT1(tf); \
    t0 += tg; \
    tg -= OD_RSHIFT1(t0); /* pass */ \
    tj -= t3; \
    t3 += OD_RSHIFT1(tj); /* pass */ \
    ts -= tc; \
    tc += OD_RSHIFT1(ts); \
    t4 -= tb; \
    tb += OD_RSHIFT1(t4); /* pass */ \
    tk -= tr; \
    tr += OD_RSHIFT1(tk); \
    \
    t1 = -t1; \
    t3 = -t3; \
    t7 = -t7; \
    t8 = -t8; \
    tg = -tg; \
    tm = -tm; \
    to = -to; \
    \
    /* 14341/16384 ~= Tan[3*Pi/16] + Tan[Pi/8]/2 ~= 0.875285419105846 */ \
    tm -= (t9*14341 + 8192) >> 14; \
    /* 15137/16384 ~= Sin[3*Pi/8] ~= 0.923879532511287 */ \
    t9 += (tm*15137 + 8192) >> 14; \
    /* 4161/16384 ~= Tan[3*Pi/16] - Tan[Pi/8] ~= 0.253965075546204 */ \
    tm -= (t9*4161 + 8192) >> 14; \
    /* 4161/16384 ~= Tan[3*Pi/16] - Tan[Pi/8] ~= 0.253965075546204 */ \
    tp -= (t6*4161 + 8192) >> 14; \
    /* 15137/16384 ~= Sin[3*Pi/8] ~= 0.923879532511287 */ \
    t6 += (tp*15137 + 8192) >> 14; \
    /* 28681/32768 ~= Tan[3*Pi/16] + Tan[Pi/8]/2 ~= 0.875285419105846 */ \
    tp -= (t6*28681 + 16384) >> 15; \
    /* -19195/32768 ~= Tan[Pi/8] - Tan[Pi/4] ~= -0.585786437626905 */ \
    th += (te*19195 + 16384) >> 15; \
    /* 11585/16384 ~= Sin[Pi/4] ~= 0.707106781186548 */ \
    te += (th*11585 + 8192) >> 14; \
    /* 29957/32768 ~= Tan[Pi/8] + Tan[Pi/4]/2 ~= 0.914213562373095 */ \
    th -= (te*29957 + 16384) >> 15; \
    /* 14341/16384 ~= Tan[3*Pi/16] + Tan[Pi/8]/2 ~= 0.875285419105846 */ \
    tq -= (t5*14341 + 8192) >> 14; \
    /* 15137/16384 ~= Sin[3*Pi/8] ~= 0.923879532511287 */ \
    t5 += (tq*15137 + 8192) >> 14; \
    /* 4161/16384 ~= Tan[3*Pi/16] - Tan[Pi/8] ~= 0.253965075546204 */ \
    tq -= (t5*4161 + 8192) >> 14; \
    /* 3259/8192 ~= 2*Tan[Pi/16] ~= 0.397824734759316 */ \
    ta -= (tl*3259 + 4096) >> 13; \
    /* 3135/16384 ~= Sin[Pi/8]/2 ~= 0.1913417161825449 */ \
    tl += (ta*3135 + 8192) >> 14; \
    /* 3259/8192 ~= 2*Tan[Pi/16] ~= 0.397824734759316 */ \
    ta -= (tl*3259 + 4096) >> 13; \
    /* 7489/8192 ~= Tan[Pi/8] + Tan[Pi/4]/2 ~= 0.914213562373095 */ \
    ti -= (td*7489 + 4096) >> 13; \
    /* 11585/16384 ~= Sin[Pi/4] ~= 0.707106781186548 */ \
    td += (ti*11585 + 8192) >> 14; \
    /* -19195/32768 ~= Tan[Pi/8] - Tan[Pi/4] ~= -0.585786437626905 */ \
    ti += (td*19195 + 16384) >> 15; \
    /* 14341/16384 ~= Tan[3*Pi/16] + Tan[Pi/8]/2 ~= 0.875285419105846 */ \
    to -= (t7*14341 + 8192) >> 14; \
    /* 15137/16384 ~= Sin[3*Pi/8] ~= 0.923879532511287 */ \
    t7 += (to*15137 + 8192) >> 14; \
    /* 4161/16384 ~= Tan[3*Pi/16] - Tan[Pi/8] ~= 0.253965075546204 */ \
    to -= (t7*4161 + 8192) >> 14; \
    /* 4161/16384 ~= Tan[3*Pi/16] - Tan[Pi/8] ~= 0.253965075546204 */ \
    tn -= (t8*4161 + 8192) >> 14; \
    /* 15137/16384 ~= Sin[3*Pi/8] ~= 0.923879532511287 */ \
    t8 += (tn*15137 + 8192) >> 14; \
    /* 28681/32768 ~= Tan[3*Pi/16] + Tan[Pi/8]/2 ~= 0.875285419105846 */ \
    tn -= (t8*28681 + 16384) >> 15; \
    /* -19195/32768 ~= Tan[Pi/8] - Tan[Pi/4] ~= -0.585786437626905 */ \
    tf += (tg*19195 + 16384) >> 15; \
    /* 11585/16384 ~= Sin[Pi/4] ~= 0.707106781186548 */ \
    tg += (tf*11585 + 8192) >> 14; \
    /* 29957/32768 ~= Tan[Pi/8] + Tan[Pi/4]/2 ~= 0.914213562373095 */ \
    tf -= (tg*29957 + 16384) >> 15; \
    /* -19195/32768 ~= Tan[Pi/8] - Tan[Pi/4] ~= -0.585786437626905 */ \
    tj += (tc*19195 + 16384) >> 15; \
    /* 11585/16384 ~= Sin[Pi/4] ~= 0.707106781186548 */ \
    tc += (tj*11585 + 8192) >> 14; \
    /* 29957/32768 ~= Tan[Pi/8] + Tan[Pi/4]/2 ~= 0.914213562373095 */ \
    tj -= (tc*29957 + 16384) >> 15; \
    /* 13573/16384 ~= 2*Tan[Pi/8] ~= 0.828427124746190 */ \
    tk += (tb*13573 + 8192) >> 14; \
    /* 11585/32768 ~= Sin[Pi/4]/2 ~= 0.353553390593274 */ \
    tb -= (tk*11585 + 16384) >> 15; \
    /* 13573/16384 ~= 2*Tan[Pi/8] ~= 0.828427124746190 */ \
    tk += (tb*13573 + 8192) >> 14; \
    \
    tf = -tf; \
    \
  } \
  while (0)

#define OD_FDCT_64_PR(u0, uw, ug, uM, u8, uE, uo, uU, u4, uA, uk, uQ, uc, uI, \
  us, uY, u2, uy, ui, uO, ua, uG, uq, uW, u6, uC, um, uS, ue, uK, uu, u_, u1, \
  ux, uh, uN, u9, uF, up, uV, u5, uB, ul, uR, ud, uJ, ut, uZ, u3, uz, uj, uP, \
  ub, uH, ur, uX, u7, uD, un, uT, uf, uL, uv, u) \
  /* Embedded 64-point orthonormal Type-II fDCT. */ \
  do { \
    int uwh; \
    int uxh; \
    int uyh; \
    int uzh; \
    int uAh; \
    int uBh; \
    int uCh; \
    int uDh; \
    int uEh; \
    int uFh; \
    int uGh; \
    int uHh; \
    int uIh; \
    int uJh; \
    int uKh; \
    int uLh; \
    int uMh; \
    int uNh; \
    int uOh; \
    int uPh; \
    int uQh; \
    int uRh; \
    int uSh; \
    int uTh; \
    int uUh; \
    int uVh; \
    int uWh; \
    int uXh; \
    int uYh; \
    int uZh; \
    int u_h; \
    int uh_; \
    u = u0 - u; \
    uh_ = OD_RSHIFT1(u); \
    u0 -= uh_; \
    u_ += u1; \
    u_h = OD_RSHIFT1(u_); \
    u1 = u_h - u1; \
    uZ = u2 - uZ; \
    uZh = OD_RSHIFT1(uZ); \
    u2 -= uZh; \
    uY += u3; \
    uYh = OD_RSHIFT1(uY); \
    u3 = uYh - u3; \
    uX = u4 - uX; \
    uXh = OD_RSHIFT1(uX); \
    u4 -= uXh; \
    uW += u5; \
    uWh = OD_RSHIFT1(uW); \
    u5 = uWh - u5; \
    uV = u6 - uV; \
    uVh = OD_RSHIFT1(uV); \
    u6 -= uVh; \
    uU += u7; \
    uUh = OD_RSHIFT1(uU); \
    u7 = uUh - u7; \
    uT = u8 - uT; \
    uTh = OD_RSHIFT1(uT); \
    u8 -= uTh; \
    uS += u9; \
    uSh = OD_RSHIFT1(uS); \
    u9 = uSh - u9; \
    uR = ua - uR; \
    uRh = OD_RSHIFT1(uR); \
    ua -= uRh; \
    uQ += ub; \
    uQh = OD_RSHIFT1(uQ); \
    ub = uQh - ub; \
    uP = uc - uP; \
    uPh = OD_RSHIFT1(uP); \
    uc -= uPh; \
    uO += ud; \
    uOh = OD_RSHIFT1(uO); \
    ud = uOh - ud; \
    uN = ue - uN; \
    uNh = OD_RSHIFT1(uN); \
    ue -= uNh; \
    uM += uf; \
    uMh = OD_RSHIFT1(uM); \
    uf = uMh - uf; \
    uL = ug - uL; \
    uLh = OD_RSHIFT1(uL); \
    ug -= uLh; \
    uK += uh; \
    uKh = OD_RSHIFT1(uK); \
    uh = uKh - uh; \
    uJ = ui - uJ; \
    uJh = OD_RSHIFT1(uJ); \
    ui -= uJh; \
    uI += uj; \
    uIh = OD_RSHIFT1(uI); \
    uj = uIh - uj; \
    uH = uk - uH; \
    uHh = OD_RSHIFT1(uH); \
    uk -= uHh; \
    uG += ul; \
    uGh = OD_RSHIFT1(uG); \
    ul = uGh - ul; \
    uF = um - uF; \
    uFh = OD_RSHIFT1(uF); \
    um -= uFh; \
    uE += un; \
    uEh = OD_RSHIFT1(uE); \
    un = uEh - un; \
    uD = uo - uD; \
    uDh = OD_RSHIFT1(uD); \
    uo -= uDh; \
    uC += up; \
    uCh = OD_RSHIFT1(uC); \
    up = uCh - up; \
    uB = uq - uB; \
    uBh = OD_RSHIFT1(uB); \
    uq -= uBh; \
    uA += ur; \
    uAh = OD_RSHIFT1(uA); \
    ur = uAh - ur; \
    uz = us - uz; \
    uzh = OD_RSHIFT1(uz); \
    us -= uzh; \
    uy += ut; \
    uyh = OD_RSHIFT1(uy); \
    ut = uyh - ut; \
    ux = uu - ux; \
    uxh = OD_RSHIFT1(ux); \
    uu -= uxh; \
    uw += uv; \
    uwh = OD_RSHIFT1(uw); \
    uv = uwh - uv; \
    OD_FDCT_32_ASYM_PR(u0, uw, uwh, ug, uM, uMh, u8, uE, uEh, uo, uU, uUh, \
      u4, uA, uAh, uk, uQ, uQh, uc, uI, uIh, us, uY, uYh, u2, uy, uyh, \
      ui, uO, uOh, ua, uG, uGh, uq, uW, uWh, u6, uC, uCh, um, uS, uSh, \
      ue, uK, uKh, uu, u_, u_h); \
    OD_FDST_32_ASYM_PR(u, uv, uL, uf, uT, un, uD, u7, uX, ur, uH, ub, uP, uj, \
      uz, u3, uZ, ut, uJ, ud, uR, ul, uB, u5, uV, up, uF, u9, uN, uh, ux, u1); \
  } \
  while (0)

#define OD_IDCT_64_PR(u0, uw, ug, uM, u8, uE, uo, uU, u4, uA, uk, uQ, uc, uI, \
  us, uY, u2, uy, ui, uO, ua, uG, uq, uW, u6, uC, um, uS, ue, uK, uu, u_, u1, \
  ux, uh, uN, u9, uF, up, uV, u5, uB, ul, uR, ud, uJ, ut, uZ, u3, uz, uj, uP, \
  ub, uH, ur, uX, u7, uD, un, uT, uf, uL, uv, u) \
  /* Embedded 64-point orthonormal Type-II fDCT. */ \
  do { \
    int u1h; \
    int u3h; \
    int u5h; \
    int u7h; \
    int u9h; \
    int ubh; \
    int udh; \
    int ufh; \
    int uhh; \
    int ujh; \
    int ulh; \
    int unh; \
    int uph; \
    int urh; \
    int uth; \
    int uvh; \
    int uxh; \
    int uzh; \
    int uBh; \
    int uDh; \
    int uFh; \
    int uHh; \
    int uJh; \
    int uLh; \
    int uNh; \
    int uPh; \
    int uRh; \
    int uTh; \
    int uVh; \
    int uXh; \
    int uZh; \
    int uh_; \
    OD_IDST_32_ASYM_PR(u, uL, uT, uD, uX, uH, uP, uz, uZ, uJ, uR, uB, uV, uF, \
      uN, ux, u_, uK, uS, uC, uW, uG, uO, uy, uY, uI, uQ, uA, uU, uE, uM, uw); \
    OD_IDCT_32_ASYM_PR(u0, ug, u8, uo, u4, uk, uc, us, u2, ui, ua, uq, u6, um, \
      ue, uu, u1, u1h, uh, uhh, u9, u9h, up, uph, u5, u5h, ul, ulh, ud, udh, \
      ut, uth, u3, u3h, uj, ujh, ub, ubh, ur, urh, u7, u7h, un, unh, uf, ufh, \
      uv, uvh); \
    uh_ = OD_RSHIFT1(u); \
    u0 += uh_; \
    u = u0 - u; \
    u_ = u1h - u_; \
    u1 -= u_; \
    uZh = OD_RSHIFT1(uZ); \
    u2 += uZh; \
    uZ = u2 - uZ; \
    uY = u3h - uY; \
    u3 -= uY; \
    uXh = OD_RSHIFT1(uX); \
    u4 += uXh; \
    uX = u4 - uX; \
    uW = u5h - uW; \
    u5 -= uW; \
    uVh = OD_RSHIFT1(uV); \
    u6 += uVh; \
    uV = u6 - uV; \
    uU = u7h - uU; \
    u7 -= uU; \
    uTh = OD_RSHIFT1(uT); \
    u8 += uTh; \
    uT = u8 - uT; \
    uS = u9h - uS; \
    u9 -= uS; \
    uRh = OD_RSHIFT1(uR); \
    ua += uRh; \
    uR = ua - uR; \
    uQ = ubh - uQ; \
    ub -= uQ; \
    uPh = OD_RSHIFT1(uP); \
    uc += uPh; \
    uP = uc - uP; \
    uO = udh - uO; \
    ud -= uO; \
    uNh = OD_RSHIFT1(uN); \
    ue += uNh; \
    uN = ue - uN; \
    uM = ufh - uM; \
    uf -= uM; \
    uLh = OD_RSHIFT1(uL); \
    ug += uLh; \
    uL = ug - uL; \
    uK = uhh - uK; \
    uh -= uK; \
    uJh = OD_RSHIFT1(uJ); \
    ui += uJh; \
    uJ = ui - uJ; \
    uI = ujh - uI; \
    uj -= uI; \
    uHh = OD_RSHIFT1(uH); \
    uk += uHh; \
    uH = uk - uH; \
    uG = ulh - uG; \
    ul -= uG; \
    uFh = OD_RSHIFT1(uF); \
    um += uFh; \
    uF = um - uF; \
    uE = unh - uE; \
    un -= uE; \
    uDh = OD_RSHIFT1(uD); \
    uo += uDh; \
    uD = uo - uD; \
    uC = uph - uC; \
    up -= uC; \
    uBh = OD_RSHIFT1(uB); \
    uq += uBh; \
    uB = uq - uB; \
    uA = urh - uA; \
    ur -= uA; \
    uzh = OD_RSHIFT1(uz); \
    us += uzh; \
    uz = us - uz; \
    uy = uth - uy; \
    ut -= uy; \
    uxh = OD_RSHIFT1(ux); \
    uu += uxh; \
    ux = uu - ux; \
    uw = uvh - uw; \
    uv -= uw; \
  } while (0)
#endif

/* 4-point orthonormal Type-II fDCT. */
void od_bin_fdct4(od_coeff y[4], const od_coeff *x, int xstride) {
  int q0;
  int q1;
  int q2;
  int q3;
  q0 = x[0*xstride];
  q1 = x[1*xstride];
  q2 = x[2*xstride];
  q3 = x[3*xstride];
  OD_FDCT_4_FLAT(q0, q1, q2, q3);
  y[0] = (od_coeff)q0;
  y[1] = (od_coeff)q2;
  y[2] = (od_coeff)q1;
  y[3] = (od_coeff)q3;
}

/* 4-point orthonormal Type-II iDCT. */
void od_bin_idct4(od_coeff *x, int xstride, const od_coeff y[4]) {
  int q0;
  int q1;
  int q2;
  int q3;
  q0 = y[0];
  q2 = y[1];
  q1 = y[2];
  q3 = y[3];
  OD_IDCT_4_FLAT(q0, q2, q1, q3);
  x[0*xstride] = (od_coeff)q0;
  x[1*xstride] = (od_coeff)q1;
  x[2*xstride] = (od_coeff)q2;
  x[3*xstride] = (od_coeff)q3;
}

/* 4-point orthonormal Type-VII fDST. */
void od_bin_fdst4(od_coeff y[4], const od_coeff *x, int xstride) {
  /* 11 adds, 5 "muls", 2 shifts.*/
  int q0;
  int q1;
  int q2;
  int q3;
  int t0;
  int t1;
  int t2;
  int t3;
  int t3h;
  int t4;
  int u4;
  q0 = x[0*xstride];
  q1 = x[1*xstride];
  q2 = x[2*xstride];
  q3 = x[3*xstride];
  t0 = q1 + q3;
  /*When used in a 4x16 transform, the following line could overflow 16 bits
    in SIMD unless implemented using PAVGW or VRHSUB.S16.*/
  t1 = q1 + OD_PAVG(q0, -t0);
  t2 = q0 - q1;
  t3 = q2;
  t4 = q0 + q3;
  /* 7021/16384 ~= 2*Sin[2*Pi/9]/3 ~= 0.428525073124360 */
  t0 = (t0*7021 + 8192) >> 14;
  /* 37837/32768 ~= 4*Sin[3*Pi/9]/3 ~= 1.154700538379252 */
  t1 = (t1*37837 + 16384) >> 15;
  /* 21513/32768 ~= 2*Sin[4*Pi/9]/3 ~= 0.656538502008139 */
  t2 = (t2*21513 + 16384) >> 15;
  /* 37837/32768 ~= 4*Sin[3*Pi/9]/3 ~= 1.154700538379252 */
  t3 = (t3*37837 + 16384) >> 15;
  /* 467/2048 ~= 2*Sin[1*Pi/9]/3 ~= 0.228013428883779 */
  t4 = (t4*467 + 1024) >> 11;
  t3h = OD_RSHIFT1(t3);
  u4 = t4 + t3h;
  q0 = t0 + u4;
  q1 = t1;
  q2 = t0 + t2 - t3h;
  q3 = t2 + t3 - u4;
  y[0] = (od_coeff)q0;
  y[1] = (od_coeff)q1;
  y[2] = (od_coeff)q2;
  y[3] = (od_coeff)q3;
}

/* 4-point orthonormal Type-VII iDST. */
void od_bin_idst4(od_coeff *x, int xstride, const od_coeff y[4]) {
  /* 11 adds, 5 "muls".*/
  int q0;
  int q1;
  int q2;
  int q3;
  int t0;
  int t1;
  int t2;
  int t3;
  int t3h;
  int t4;
  int u4;
  q0 = y[0];
  q1 = y[1];
  q2 = y[2];
  q3 = y[3];
  t0 = q0 - q3;
  t1 = q0 + q2;
  t2 = q3 + OD_PAVG(t0, -q2);
  t3 = q1;
  t4 = q2 + q3;
  /* 467/2048 ~= 2*Sin[1*Pi/9]/3 ~= 0.228013428883779 */
  t0 = (t0*467 + 1024) >> 11;
  /* 7021/16384 ~= 2*Sin[2*Pi/9]/3 ~= 0.428525073124360 */
  t1 = (t1*7021 + 8192) >> 14;
  /* 37837/32768 ~= 4*Sin[3*Pi/9]/3 ~= 1.154700538379252 */
  t2 = (t2*37837 + 16384) >> 15;
  /* 37837/32768 ~= 4*Sin[3*Pi/9]/3 ~= 1.154700538379252 */
  t3 = (t3*37837 + 16384) >> 15;
  /* 21513/32768 ~= 2*Sin[4*Pi/9]/3 ~= 0.656538502008139 */
  t4 = (t4*21513 + 16384) >> 15;
  t3h = OD_RSHIFT1(t3);
  u4 = t4 + t3h;
  q0 = t0 + u4;
  q1 = t1 + t3 - u4;
  q2 = t2;
  q3 = t0 + t1 - t3h;
  x[0*xstride] = q0;
  x[1*xstride] = q1;
  x[2*xstride] = q2;
  x[3*xstride] = q3;
}

void od_bin_fdct8(od_coeff y[8], const od_coeff *x, int xstride) {
  int r0;
  int r1;
  int r2;
  int r3;
  int r4;
  int r5;
  int r6;
  int r7;
  r0 = x[0*xstride];
  r1 = x[1*xstride];
  r2 = x[2*xstride];
  r3 = x[3*xstride];
  r4 = x[4*xstride];
  r5 = x[5*xstride];
  r6 = x[6*xstride];
  r7 = x[7*xstride];
  OD_FDCT_8_FLAT(r0, r1, r2, r3, r4, r5, r6, r7);
  y[0] = (od_coeff)r0;
  y[1] = (od_coeff)r4;
  y[2] = (od_coeff)r2;
  y[3] = (od_coeff)r6;
  y[4] = (od_coeff)r1;
  y[5] = (od_coeff)r5;
  y[6] = (od_coeff)r3;
  y[7] = (od_coeff)r7;
}

void od_bin_idct8(od_coeff *x, int xstride, const od_coeff y[8]) {
  int r0;
  int r1;
  int r2;
  int r3;
  int r4;
  int r5;
  int r6;
  int r7;
  r0 = y[0];
  r4 = y[1];
  r2 = y[2];
  r6 = y[3];
  r1 = y[4];
  r5 = y[5];
  r3 = y[6];
  r7 = y[7];
  OD_IDCT_8_FLAT(r0, r4, r2, r6, r1, r5, r3, r7);
  x[0*xstride] = (od_coeff)r0;
  x[1*xstride] = (od_coeff)r1;
  x[2*xstride] = (od_coeff)r2;
  x[3*xstride] = (od_coeff)r3;
  x[4*xstride] = (od_coeff)r4;
  x[5*xstride] = (od_coeff)r5;
  x[6*xstride] = (od_coeff)r6;
  x[7*xstride] = (od_coeff)r7;
}

#if !CONFIG_DAALA_TX_DST8
void od_bin_fdst8(od_coeff y[8], const od_coeff *x, int xstride) {
  int r0;
  int r1;
  int r2;
  int r3;
  int r4;
  int r5;
  int r6;
  int r7;
  r0 = x[0*xstride];
  r1 = x[1*xstride];
  r2 = x[2*xstride];
  r3 = x[3*xstride];
  r4 = x[4*xstride];
  r5 = x[5*xstride];
  r6 = x[6*xstride];
  r7 = x[7*xstride];
  OD_FDST_8_FLAT(r0, r1, r2, r3, r4, r5, r6, r7);
  y[0] = (od_coeff)r0;
  y[1] = (od_coeff)r4;
  y[2] = (od_coeff)r2;
  y[3] = (od_coeff)r6;
  y[4] = (od_coeff)r1;
  y[5] = (od_coeff)r5;
  y[6] = (od_coeff)r3;
  y[7] = (od_coeff)r7;
}

void od_bin_idst8(od_coeff *x, int xstride, const od_coeff y[8]) {
  int r0;
  int r1;
  int r2;
  int r3;
  int r4;
  int r5;
  int r6;
  int r7;
  r0 = y[0];
  r4 = y[1];
  r2 = y[2];
  r6 = y[3];
  r1 = y[4];
  r5 = y[5];
  r3 = y[6];
  r7 = y[7];
  OD_IDST_8_FLAT(r0, r4, r2, r6, r1, r5, r3, r7);
  x[0*xstride] = (od_coeff)r0;
  x[1*xstride] = (od_coeff)r1;
  x[2*xstride] = (od_coeff)r2;
  x[3*xstride] = (od_coeff)r3;
  x[4*xstride] = (od_coeff)r4;
  x[5*xstride] = (od_coeff)r5;
  x[6*xstride] = (od_coeff)r6;
  x[7*xstride] = (od_coeff)r7;
}
#else
const int OD_DST_8_PERM[8] = { 0, 7, 1, 6, 2, 5, 3, 4 };

/* Computes the Polynomial Product Y(z) ≡ X(z)*H(z) modulo (z^8 + 1) using
    Nussbaumer's "short" algorithm [1].
   The monomial coefficients in Y(z) are exactly the values of an acyclic
    convolution of the monomial coefficients of X(z) and H(z).
   Since H(z) is fixed, the multiplication terms are constant and precomputed.

   [1] Nussbaumer, Henri J. "Fast Fourier Transform and Convolution Algorithms"
        Springer-Verlag: Berlin, Heidelberg, New York (1981) pages 76-78. */
static void od_poly_prod_8(od_coeff y[8], const od_coeff x[8]) {
  /* 21 "muls", 76 adds, 21 shifts */
  od_coeff q0;
  od_coeff q1;
  od_coeff q2;
  od_coeff q3;
  od_coeff q4;
  od_coeff q5;
  od_coeff q6;
  od_coeff q7;
  od_coeff q8;
  od_coeff q9;
  od_coeff q10;
  od_coeff q11;
  od_coeff q12;
  od_coeff q13;
  od_coeff q14;
  od_coeff q15;
  od_coeff q16;
  od_coeff q17;
  od_coeff q18;
  od_coeff q19;
  od_coeff q20;
  od_coeff r0;
  od_coeff r1;
  od_coeff r2;
  od_coeff r3;
  od_coeff r4;
  od_coeff r5;
  od_coeff r6;
  od_coeff r7;
  od_coeff t0;
  od_coeff t1;
  od_coeff t2;
  od_coeff t3;
  od_coeff t4;
  od_coeff t5;
  od_coeff t6;
  od_coeff t7;
  od_coeff u0;
  od_coeff u1;
  od_coeff u1h;
  od_coeff u2;
  od_coeff u2h;
  od_coeff u3;
  od_coeff u4;
  od_coeff u4h;
  od_coeff u5;
  od_coeff u6;
  od_coeff u7;
  od_coeff u7h;
  od_coeff u8;
  od_coeff u9;
  od_coeff u10;
  od_coeff u11;
  od_coeff u12;
  od_coeff u13;
  od_coeff u14;
  od_coeff u15;
  od_coeff u16;
  od_coeff u17;
  od_coeff u18;
  od_coeff u19;
  od_coeff u20;
  od_coeff u21;
  od_coeff u22;
  od_coeff u23;
  od_coeff u24;
  od_coeff u25;
  od_coeff u26;
  od_coeff u27;
  t0 = x[0];
  t1 = x[1];
  t2 = x[2];
  t3 = x[3];
  t4 = x[4];
  t5 = x[5];
  t6 = x[6];
  t7 = x[7];
  /* Stage 0 Butterfly */
  u7 = t0 - t7;
  u7h = OD_RSHIFT1(u7);
  u0 = t0 - u7h;
  u2 = t2 - t6;
  u2h = OD_RSHIFT1(u2);
  u6 = t2 - u2h;
  u4 = t4 + t5;
  u4h = OD_RSHIFT1(u4);
  u5 = t4 - u4h;
  u1 = t3 - t1;
  u1h = OD_RSHIFT1(u1);
  u3 = t3 - u1h;
  /* Stage 1 Butterfly */
  q0 = u0 + u2h;
  q1 = q0 - u2;
  q4 = u3 + u4h;
  q5 = q4 - u4;
  q2 = u7h + u5;
  q7 = u7 - q2;
  q6 = u1h + u6;
  q3 = u1 - q6;
  /* Stage 2 Half-Butterfly */
  /*The intermediate sums can overflow 16 bits, but all SIMD instruction sets
     should be able to compute them without issue (i.e., using PAVGW or
     V{R}HADD.S16).*/
  q8 = (q0 + q4 + 1) >> 1;
  q9 = (q1 + q5) >> 1;
  q10 = (q2 + q3 + 1) >> 1;
  q11 = (q7 + q6) >> 1;
  /* Stage 3 */
  q12 = t0 + t3;
  q13 = t0;
  q14 = t3;
  q15 = t5 - t6;
  q16 = t6;
  q17 = t5;
  r0 = t2 + t4;
  r1 = t2 - OD_RSHIFT1(r0);
  r2 = (r1 - q15 + 1) >> 1;
  r3 = OD_RSHIFT1(t0);
  r4 = (r3 - t1 + 1) >> 1;
  /* q18 = (q6 - q4)/2 + (t0 - q15)/4
         = (t0 + t2 - t4)/4 - (t1 + t5 - t6)/2 */
  q18 = r2 + r4;
  r5 = t5 - (q15 >> 1);
  r6 = (r0 + t3 + 1) >> 1;
  r7 = (t7 + r6 + 1) >> 1;
  /* q19 = (q7 - q0)/2 + (t5 + t6 - t3)/4
         = (t5 + t6 - t7)/2 - (t2 + t3 + t4)/4 */
  q19 = r5 - r7;
  q20 = (q18 - q19) >> 1;
  /* Stage 4 */
  q0 = (-5995*q0 + 8192) >> 14;
  q1 = (-1373*q1 + 4096) >> 13;
  q2 = (22891*q2 + 16384) >> 15;
  q3 = (-217*q3 + 512) >> 10;
  q4 = (13427*q4 + 16384) >> 15;
  q5 = (-11013*q5 + 8192) >> 14;
  q6 = (1373*q6 + 1024) >> 11;
  q7 = (-14077*q7 + 16384) >> 15;
  q8 = (-1437*q8 + 16384) >> 15;
  q9 = (27519*q9 + 16384) >> 15;
  q10 = (-15947*q10 + 16384) >> 15;
  q11 = (-7891*q11 + 16384) >> 15;
  q12 = (4897*q12 + 16384) >> 15;
  q13 = (-5079*q13 + 8192) >> 14;
  q14 = (365*q14 + 16384) >> 15;
  q15 = (3325*q15 + 8192) >> 14;
  q16 = (-5225*q16 + 8192) >> 14;
  q17 = (-1425*q17 + 8192) >> 14;
  q18 = (3453*q18 + 16384) >> 15;
  q19 = (-8421*q19 + 8192) >> 14;
  q20 = (-20295*q20 + 16384) >> 15;
  /* Stage 5 */
  u0 = q0 + q8;
  u1 = q1 + q9;
  u2 = q2 + q10;
  u3 = q3 + q10;
  u4 = q4 + q8;
  u5 = q5 + q9;
  u6 = q6 + q11;
  u7 = q7 + q11;
  /* Stage 6 */
  u10 = u0 + u1;
  u11 = u0 - u1;
  u12 = u2 + u7;
  u13 = u2 - u7;
  u14 = u3 + u6;
  u15 = u3 - u6;
  u16 = u5 + u4;
  u17 = u5 - u4;
  /* Stage 7 */
  u8 = q19 + q20;
  u9 = q19 - q18;
  u18 = q12 + u8;
  u19 = u18 + q13;
  u20 = u18 + q14;
  u21 = 2*u9;
  u22 = q15 + u21;
  u23 = q16 - u22;
  u24 = u22 + q17;
  u25 = 2*u8;
  u26 = 2*u25;
  u27 = u25 - u9;
  /* Stage 8 */
  y[0] = u14 + u16 + u20;
  y[1] = u12 - u10 - u25;
  y[2] = u9 + u13 - u17;
  y[3] = u9 - u10 - u12 - u19;
  y[4] = u15 - u11 - u27;
  y[5] = u23 - u11 - u15;
  y[6] = u13 + u17 - u24 + u26;
  y[7] = u16 - u14 + u21 - u25;
}

void od_bin_fdst8(od_coeff y[8], const od_coeff *x, int xstride) {
  int i;
  od_coeff xp[8];
  od_coeff yp[8];
  for (i = 0; i < 8; i++) xp[i] = x[i*xstride];
  od_poly_prod_8(yp, xp);
  for (i = 0; i < 8; i++) y[OD_DST_8_PERM[i]] = yp[i];
}

void od_bin_idst8(od_coeff *x, int xstride, const od_coeff y[8]) {
  int i;
  od_coeff xp[8];
  od_coeff yp[8];
  for (i = 0; i < 8; i++) yp[i] = y[OD_DST_8_PERM[i]];
  od_poly_prod_8(xp, yp);
  for (i = 0; i < 8; i++) x[i*xstride] = xp[i];
}
#endif
void od_bin_fdct16(od_coeff y[16], const od_coeff *x, int xstride) {
  int s0;
  int s1;
  int s2;
  int s3;
  int s4;
  int s5;
  int s6;
  int s7;
  int s8;
  int s9;
  int sa;
  int sb;
  int sc;
  int sd;
  int se;
  int sf;
  s0 = x[0*xstride];
  s1 = x[1*xstride];
  s2 = x[2*xstride];
  s3 = x[3*xstride];
  s4 = x[4*xstride];
  s5 = x[5*xstride];
  s6 = x[6*xstride];
  s7 = x[7*xstride];
  s8 = x[8*xstride];
  s9 = x[9*xstride];
  sa = x[10*xstride];
  sb = x[11*xstride];
  sc = x[12*xstride];
  sd = x[13*xstride];
  se = x[14*xstride];
  sf = x[15*xstride];
  OD_FDCT_16_FLAT(s0, s1, s2, s3, s4, s5, s6, s7,
    s8, s9, sa, sb, sc, sd, se, sf);
  y[0] = (od_coeff)s0;
  y[1] = (od_coeff)s8;
  y[2] = (od_coeff)s4;
  y[3] = (od_coeff)sc;
  y[4] = (od_coeff)s2;
  y[5] = (od_coeff)sa;
  y[6] = (od_coeff)s6;
  y[7] = (od_coeff)se;
  y[8] = (od_coeff)s1;
  y[9] = (od_coeff)s9;
  y[10] = (od_coeff)s5;
  y[11] = (od_coeff)sd;
  y[12] = (od_coeff)s3;
  y[13] = (od_coeff)sb;
  y[14] = (od_coeff)s7;
  y[15] = (od_coeff)sf;
}

void od_bin_idct16(od_coeff *x, int xstride, const od_coeff y[16]) {
  int s0;
  int s1;
  int s2;
  int s3;
  int s4;
  int s5;
  int s6;
  int s7;
  int s8;
  int s9;
  int sa;
  int sb;
  int sc;
  int sd;
  int se;
  int sf;
  s0 = y[0];
  s8 = y[1];
  s4 = y[2];
  sc = y[3];
  s2 = y[4];
  sa = y[5];
  s6 = y[6];
  se = y[7];
  s1 = y[8];
  s9 = y[9];
  s5 = y[10];
  sd = y[11];
  s3 = y[12];
  sb = y[13];
  s7 = y[14];
  sf = y[15];
  OD_IDCT_16_FLAT(s0, s8, s4, sc, s2, sa, s6, se,
    s1, s9, s5, sd, s3, sb, s7, sf);
  x[0*xstride] = (od_coeff)s0;
  x[1*xstride] = (od_coeff)s1;
  x[2*xstride] = (od_coeff)s2;
  x[3*xstride] = (od_coeff)s3;
  x[4*xstride] = (od_coeff)s4;
  x[5*xstride] = (od_coeff)s5;
  x[6*xstride] = (od_coeff)s6;
  x[7*xstride] = (od_coeff)s7;
  x[8*xstride] = (od_coeff)s8;
  x[9*xstride] = (od_coeff)s9;
  x[10*xstride] = (od_coeff)sa;
  x[11*xstride] = (od_coeff)sb;
  x[12*xstride] = (od_coeff)sc;
  x[13*xstride] = (od_coeff)sd;
  x[14*xstride] = (od_coeff)se;
  x[15*xstride] = (od_coeff)sf;
}

void od_bin_fdst16(od_coeff y[16], const od_coeff *x, int xstride) {
  int s0;
  int s1;
  int s2;
  int s3;
  int s4;
  int s5;
  int s6;
  int s7;
  int s8;
  int s9;
  int sa;
  int sb;
  int sc;
  int sd;
  int se;
  int sf;
  s0 = x[0*xstride];
  s8 = x[1*xstride];
  s4 = x[2*xstride];
  sc = x[3*xstride];
  s2 = x[4*xstride];
  sa = x[5*xstride];
  s6 = x[6*xstride];
  se = x[7*xstride];
  s1 = x[8*xstride];
  s9 = x[9*xstride];
  s5 = x[10*xstride];
  sd = x[11*xstride];
  s3 = x[12*xstride];
  sb = x[13*xstride];
  s7 = x[14*xstride];
  sf = x[15*xstride];
  OD_FDST_16_FLAT(s0, s8, s4, sc, s2, sa, s6, se,
   s1, s9, s5, sd, s3, sb, s7, sf);
  y[0] = (od_coeff)s0;
  y[1] = (od_coeff)s1;
  y[2] = (od_coeff)s2;
  y[3] = (od_coeff)s3;
  y[4] = (od_coeff)s4;
  y[5] = (od_coeff)s5;
  y[6] = (od_coeff)s6;
  y[7] = (od_coeff)s7;
  y[8] = (od_coeff)s8;
  y[9] = (od_coeff)s9;
  y[10] = (od_coeff)sa;
  y[11] = (od_coeff)sb;
  y[12] = (od_coeff)sc;
  y[13] = (od_coeff)sd;
  y[14] = (od_coeff)se;
  y[15] = (od_coeff)sf;
}

void od_bin_idst16(od_coeff *x, int xstride, const od_coeff y[16]) {
  int s0;
  int s1;
  int s2;
  int s3;
  int s4;
  int s5;
  int s6;
  int s7;
  int s8;
  int s9;
  int sa;
  int sb;
  int sc;
  int sd;
  int se;
  int sf;
  s0 = y[0];
  s8 = y[1];
  s4 = y[2];
  sc = y[3];
  s2 = y[4];
  sa = y[5];
  s6 = y[6];
  se = y[7];
  s1 = y[8];
  s9 = y[9];
  s5 = y[10];
  sd = y[11];
  s3 = y[12];
  sb = y[13];
  s7 = y[14];
  sf = y[15];
  OD_IDST_16_FLAT(s0, s8, s4, sc, s2, sa, s6, se,
   s1, s9, s5, sd, s3, sb, s7, sf);
  x[0*xstride] = (od_coeff)s0;
  x[1*xstride] = (od_coeff)s1;
  x[2*xstride] = (od_coeff)s2;
  x[3*xstride] = (od_coeff)s3;
  x[4*xstride] = (od_coeff)s4;
  x[5*xstride] = (od_coeff)s5;
  x[6*xstride] = (od_coeff)s6;
  x[7*xstride] = (od_coeff)s7;
  x[8*xstride] = (od_coeff)s8;
  x[9*xstride] = (od_coeff)s9;
  x[10*xstride] = (od_coeff)sa;
  x[11*xstride] = (od_coeff)sb;
  x[12*xstride] = (od_coeff)sc;
  x[13*xstride] = (od_coeff)sd;
  x[14*xstride] = (od_coeff)se;
  x[15*xstride] = (od_coeff)sf;
}

void od_bin_fdct32(od_coeff y[32], const od_coeff *x, int xstride) {
  /*215 adds, 38 shifts, 87 "muls".*/
  int t0;
  int t1;
  int t2;
  int t3;
  int t4;
  int t5;
  int t6;
  int t7;
  int t8;
  int t9;
  int ta;
  int tb;
  int tc;
  int td;
  int te;
  int tf;
  int tg;
  int th;
  int ti;
  int tj;
  int tk;
  int tl;
  int tm;
  int tn;
  int to;
  int tp;
  int tq;
  int tr;
  int ts;
  int tt;
  int tu;
  int tv;
  t0 = x[0*xstride];
  tg = x[1*xstride];
  t8 = x[2*xstride];
  to = x[3*xstride];
  t4 = x[4*xstride];
  tk = x[5*xstride];
  tc = x[6*xstride];
  ts = x[7*xstride];
  t2 = x[8*xstride];
  ti = x[9*xstride];
  ta = x[10*xstride];
  tq = x[11*xstride];
  t6 = x[12*xstride];
  tm = x[13*xstride];
  te = x[14*xstride];
  tu = x[15*xstride];
  t1 = x[16*xstride];
  th = x[17*xstride];
  t9 = x[18*xstride];
  tp = x[19*xstride];
  t5 = x[20*xstride];
  tl = x[21*xstride];
  td = x[22*xstride];
  tt = x[23*xstride];
  t3 = x[24*xstride];
  tj = x[25*xstride];
  tb = x[26*xstride];
  tr = x[27*xstride];
  t7 = x[28*xstride];
  tn = x[29*xstride];
  tf = x[30*xstride];
  tv = x[31*xstride];
  OD_FDCT_32_FLAT(
    t0, tg, t8, to, t4, tk, tc, ts, t2, ti, ta, tq, t6, tm, te, tu,
    t1, th, t9, tp, t5, tl, td, tt, t3, tj, tb, tr, t7, tn, tf, tv);
  y[0] = (od_coeff)t0;
  y[1] = (od_coeff)t1;
  y[2] = (od_coeff)t2;
  y[3] = (od_coeff)t3;
  y[4] = (od_coeff)t4;
  y[5] = (od_coeff)t5;
  y[6] = (od_coeff)t6;
  y[7] = (od_coeff)t7;
  y[8] = (od_coeff)t8;
  y[9] = (od_coeff)t9;
  y[10] = (od_coeff)ta;
  y[11] = (od_coeff)tb;
  y[12] = (od_coeff)tc;
  y[13] = (od_coeff)td;
  y[14] = (od_coeff)te;
  y[15] = (od_coeff)tf;
  y[16] = (od_coeff)tg;
  y[17] = (od_coeff)th;
  y[18] = (od_coeff)ti;
  y[19] = (od_coeff)tj;
  y[20] = (od_coeff)tk;
  y[21] = (od_coeff)tl;
  y[22] = (od_coeff)tm;
  y[23] = (od_coeff)tn;
  y[24] = (od_coeff)to;
  y[25] = (od_coeff)tp;
  y[26] = (od_coeff)tq;
  y[27] = (od_coeff)tr;
  y[28] = (od_coeff)ts;
  y[29] = (od_coeff)tt;
  y[30] = (od_coeff)tu;
  y[31] = (od_coeff)tv;
}

void od_bin_idct32(od_coeff *x, int xstride, const od_coeff y[32]) {
  int t0;
  int t1;
  int t2;
  int t3;
  int t4;
  int t5;
  int t6;
  int t7;
  int t8;
  int t9;
  int ta;
  int tb;
  int tc;
  int td;
  int te;
  int tf;
  int tg;
  int th;
  int ti;
  int tj;
  int tk;
  int tl;
  int tm;
  int tn;
  int to;
  int tp;
  int tq;
  int tr;
  int ts;
  int tt;
  int tu;
  int tv;
  t0 = y[0];
  tg = y[1];
  t8 = y[2];
  to = y[3];
  t4 = y[4];
  tk = y[5];
  tc = y[6];
  ts = y[7];
  t2 = y[8];
  ti = y[9];
  ta = y[10];
  tq = y[11];
  t6 = y[12];
  tm = y[13];
  te = y[14];
  tu = y[15];
  t1 = y[16];
  th = y[17];
  t9 = y[18];
  tp = y[19];
  t5 = y[20];
  tl = y[21];
  td = y[22];
  tt = y[23];
  t3 = y[24];
  tj = y[25];
  tb = y[26];
  tr = y[27];
  t7 = y[28];
  tn = y[29];
  tf = y[30];
  tv = y[31];
  OD_IDCT_32_FLAT(
    t0, tg, t8, to, t4, tk, tc, ts, t2, ti, ta, tq, t6, tm, te, tu,
    t1, th, t9, tp, t5, tl, td, tt, t3, tj, tb, tr, t7, tn, tf, tv);
  x[0*xstride] = (od_coeff)t0;
  x[1*xstride] = (od_coeff)t1;
  x[2*xstride] = (od_coeff)t2;
  x[3*xstride] = (od_coeff)t3;
  x[4*xstride] = (od_coeff)t4;
  x[5*xstride] = (od_coeff)t5;
  x[6*xstride] = (od_coeff)t6;
  x[7*xstride] = (od_coeff)t7;
  x[8*xstride] = (od_coeff)t8;
  x[9*xstride] = (od_coeff)t9;
  x[10*xstride] = (od_coeff)ta;
  x[11*xstride] = (od_coeff)tb;
  x[12*xstride] = (od_coeff)tc;
  x[13*xstride] = (od_coeff)td;
  x[14*xstride] = (od_coeff)te;
  x[15*xstride] = (od_coeff)tf;
  x[16*xstride] = (od_coeff)tg;
  x[17*xstride] = (od_coeff)th;
  x[18*xstride] = (od_coeff)ti;
  x[19*xstride] = (od_coeff)tj;
  x[20*xstride] = (od_coeff)tk;
  x[21*xstride] = (od_coeff)tl;
  x[22*xstride] = (od_coeff)tm;
  x[23*xstride] = (od_coeff)tn;
  x[24*xstride] = (od_coeff)to;
  x[25*xstride] = (od_coeff)tp;
  x[26*xstride] = (od_coeff)tq;
  x[27*xstride] = (od_coeff)tr;
  x[28*xstride] = (od_coeff)ts;
  x[29*xstride] = (od_coeff)tt;
  x[30*xstride] = (od_coeff)tu;
  x[31*xstride] = (od_coeff)tv;
}

void od_bin_fdst32(od_coeff y[32], const od_coeff *x, int xstride) {
  od_coeff t0;
  od_coeff t1;
  od_coeff t2;
  od_coeff t3;
  od_coeff t4;
  od_coeff t5;
  od_coeff t6;
  od_coeff t7;
  od_coeff t8;
  od_coeff t9;
  od_coeff ta;
  od_coeff tb;
  od_coeff tc;
  od_coeff td;
  od_coeff te;
  od_coeff tf;
  od_coeff tg;
  od_coeff th;
  od_coeff ti;
  od_coeff tj;
  od_coeff tk;
  od_coeff tl;
  od_coeff tm;
  od_coeff tn;
  od_coeff to;
  od_coeff tp;
  od_coeff tq;
  od_coeff tr;
  od_coeff ts;
  od_coeff tt;
  od_coeff tu;
  od_coeff tv;
  #if !CONFIG_DAALA_TX_DST32
    assert(0 && "od_bin_fdst32() called when !CONFIG_DAALA_TX_DST32");
  #endif
  t0 = x[0*xstride];
  t1 = x[1*xstride];
  t2 = x[2*xstride];
  t3 = x[3*xstride];
  t4 = x[4*xstride];
  t5 = x[5*xstride];
  t6 = x[6*xstride];
  t7 = x[7*xstride];
  t8 = x[8*xstride];
  t9 = x[9*xstride];
  ta = x[10*xstride];
  tb = x[11*xstride];
  tc = x[12*xstride];
  td = x[13*xstride];
  te = x[14*xstride];
  tf = x[15*xstride];
  tg = x[16*xstride];
  th = x[17*xstride];
  ti = x[18*xstride];
  tj = x[19*xstride];
  tk = x[20*xstride];
  tl = x[21*xstride];
  tm = x[22*xstride];
  tn = x[23*xstride];
  to = x[24*xstride];
  tp = x[25*xstride];
  tq = x[26*xstride];
  tr = x[27*xstride];
  ts = x[28*xstride];
  tt = x[29*xstride];
  tu = x[30*xstride];
  tv = x[31*xstride];
  OD_FDST_32_PR(t0, t1, t2, t3, t4, t5, t6, t7, t8, t9, ta, tb, tc, td, te, tf,
    tg, th, ti, tj, tk, tl, tm, tn, to, tp, tq, tr, ts, tt, tu, tv);
  y[0] = t0;
  y[1] = tg;
  y[2] = t8;
  y[3] = to;
  y[4] = t4;
  y[5] = tk;
  y[6] = tc;
  y[7] = ts;
  y[8] = t2;
  y[9] = ti;
  y[10] = ta;
  y[11] = tq;
  y[12] = t6;
  y[13] = tm;
  y[14] = te;
  y[15] = tu;
  y[16] = t1;
  y[17] = th;
  y[18] = t9;
  y[19] = tp;
  y[20] = t5;
  y[21] = tl;
  y[22] = td;
  y[23] = tt;
  y[24] = t3;
  y[25] = tj;
  y[26] = tb;
  y[27] = tr;
  y[28] = t7;
  y[29] = tn;
  y[30] = tf;
  y[31] = tv;
}

void od_bin_idst32(od_coeff *x, int xstride, const od_coeff y[32]) {
  od_coeff t0;
  od_coeff t1;
  od_coeff t2;
  od_coeff t3;
  od_coeff t4;
  od_coeff t5;
  od_coeff t6;
  od_coeff t7;
  od_coeff t8;
  od_coeff t9;
  od_coeff ta;
  od_coeff tb;
  od_coeff tc;
  od_coeff td;
  od_coeff te;
  od_coeff tf;
  od_coeff tg;
  od_coeff th;
  od_coeff ti;
  od_coeff tj;
  od_coeff tk;
  od_coeff tl;
  od_coeff tm;
  od_coeff tn;
  od_coeff to;
  od_coeff tp;
  od_coeff tq;
  od_coeff tr;
  od_coeff ts;
  od_coeff tt;
  od_coeff tu;
  od_coeff tv;
  #if !CONFIG_DAALA_TX_DST32
    assert(0 && "od_bin_idst32() called when !CONFIG_DAALA_TX_DST32");
  #endif
  t0 = y[0];
  tg = y[1];
  t8 = y[2];
  to = y[3];
  t4 = y[4];
  tk = y[5];
  tc = y[6];
  ts = y[7];
  t2 = y[8];
  ti = y[9];
  ta = y[10];
  tq = y[11];
  t6 = y[12];
  tm = y[13];
  te = y[14];
  tu = y[15];
  t1 = y[16];
  th = y[17];
  t9 = y[18];
  tp = y[19];
  t5 = y[20];
  tl = y[21];
  td = y[22];
  tt = y[23];
  t3 = y[24];
  tj = y[25];
  tb = y[26];
  tr = y[27];
  t7 = y[28];
  tn = y[29];
  tf = y[30];
  tv = y[31];
  OD_IDST_32_PR(t0, tg, t8, to, t4, tk, tc, ts, t2, ti, ta, tq, t6, tm, te, tu,
    t1, th, t9, tp, t5, tl, td, tt, t3, tj, tb, tr, t7, tn, tf, tv);
  x[0*xstride] = t0;
  x[1*xstride] = t1;
  x[2*xstride] = t2;
  x[3*xstride] = t3;
  x[4*xstride] = t4;
  x[5*xstride] = t5;
  x[6*xstride] = t6;
  x[7*xstride] = t7;
  x[8*xstride] = t8;
  x[9*xstride] = t9;
  x[10*xstride] = ta;
  x[11*xstride] = tb;
  x[12*xstride] = tc;
  x[13*xstride] = td;
  x[14*xstride] = te;
  x[15*xstride] = tf;
  x[16*xstride] = tg;
  x[17*xstride] = th;
  x[18*xstride] = ti;
  x[19*xstride] = tj;
  x[20*xstride] = tk;
  x[21*xstride] = tl;
  x[22*xstride] = tm;
  x[23*xstride] = tn;
  x[24*xstride] = to;
  x[25*xstride] = tp;
  x[26*xstride] = tq;
  x[27*xstride] = tr;
  x[28*xstride] = ts;
  x[29*xstride] = tt;
  x[30*xstride] = tu;
  x[31*xstride] = tv;
}

#if CONFIG_TX64X64
void od_bin_fdct64(od_coeff y[64], const od_coeff *x, int xstride) {
  int t0;
  int t1;
  int t2;
  int t3;
  int t4;
  int t5;
  int t6;
  int t7;
  int t8;
  int t9;
  int ta;
  int tb;
  int tc;
  int td;
  int te;
  int tf;
  int tg;
  int th;
  int ti;
  int tj;
  int tk;
  int tl;
  int tm;
  int tn;
  int to;
  int tp;
  int tq;
  int tr;
  int ts;
  int tt;
  int tu;
  int tv;
  int tw;
  int tx;
  int ty;
  int tz;
  int tA;
  int tB;
  int tC;
  int tD;
  int tE;
  int tF;
  int tG;
  int tH;
  int tI;
  int tJ;
  int tK;
  int tL;
  int tM;
  int tN;
  int tO;
  int tP;
  int tQ;
  int tR;
  int tS;
  int tT;
  int tU;
  int tV;
  int tW;
  int tX;
  int tY;
  int tZ;
  int t_;
  int t;
  t0 = x[0*xstride];
  tw = x[1*xstride];
  tg = x[2*xstride];
  tM = x[3*xstride];
  t8 = x[4*xstride];
  tE = x[5*xstride];
  to = x[6*xstride];
  tU = x[7*xstride];
  t4 = x[8*xstride];
  tA = x[9*xstride];
  tk = x[10*xstride];
  tQ = x[11*xstride];
  tc = x[12*xstride];
  tI = x[13*xstride];
  ts = x[14*xstride];
  tY = x[15*xstride];
  t2 = x[16*xstride];
  ty = x[17*xstride];
  ti = x[18*xstride];
  tO = x[19*xstride];
  ta = x[20*xstride];
  tG = x[21*xstride];
  tq = x[22*xstride];
  tW = x[23*xstride];
  t6 = x[24*xstride];
  tC = x[25*xstride];
  tm = x[26*xstride];
  tS = x[27*xstride];
  te = x[28*xstride];
  tK = x[29*xstride];
  tu = x[30*xstride];
  t_ = x[31*xstride];
  t1 = x[32*xstride];
  tx = x[33*xstride];
  th = x[34*xstride];
  tN = x[35*xstride];
  t9 = x[36*xstride];
  tF = x[37*xstride];
  tp = x[38*xstride];
  tV = x[39*xstride];
  t5 = x[40*xstride];
  tB = x[41*xstride];
  tl = x[42*xstride];
  tR = x[43*xstride];
  td = x[44*xstride];
  tJ = x[45*xstride];
  tt = x[46*xstride];
  tZ = x[47*xstride];
  t3 = x[48*xstride];
  tz = x[49*xstride];
  tj = x[50*xstride];
  tP = x[51*xstride];
  tb = x[52*xstride];
  tH = x[53*xstride];
  tr = x[54*xstride];
  tX = x[55*xstride];
  t7 = x[56*xstride];
  tD = x[57*xstride];
  tn = x[58*xstride];
  tT = x[59*xstride];
  tf = x[60*xstride];
  tL = x[61*xstride];
  tv = x[62*xstride];
  t = x[63*xstride];
  OD_FDCT_64_PR(t0, tw, tg, tM, t8, tE, to, tU, t4, tA, tk, tQ, tc, tI, ts, tY,
    t2, ty, ti, tO, ta, tG, tq, tW, t6, tC, tm, tS, te, tK, tu, t_, t1, tx, th,
    tN, t9, tF, tp, tV, t5, tB, tl, tR, td, tJ, tt, tZ, t3, tz, tj, tP, tb, tH,
    tr, tX, t7, tD, tn, tT, tf, tL, tv, t);
  y[0] = (od_coeff)t0;
  y[1] = (od_coeff)t1;
  y[2] = (od_coeff)t2;
  y[3] = (od_coeff)t3;
  y[4] = (od_coeff)t4;
  y[5] = (od_coeff)t5;
  y[6] = (od_coeff)t6;
  y[7] = (od_coeff)t7;
  y[8] = (od_coeff)t8;
  y[9] = (od_coeff)t9;
  y[10] = (od_coeff)ta;
  y[11] = (od_coeff)tb;
  y[12] = (od_coeff)tc;
  y[13] = (od_coeff)td;
  y[14] = (od_coeff)te;
  y[15] = (od_coeff)tf;
  y[16] = (od_coeff)tg;
  y[17] = (od_coeff)th;
  y[18] = (od_coeff)ti;
  y[19] = (od_coeff)tj;
  y[20] = (od_coeff)tk;
  y[21] = (od_coeff)tl;
  y[22] = (od_coeff)tm;
  y[23] = (od_coeff)tn;
  y[24] = (od_coeff)to;
  y[25] = (od_coeff)tp;
  y[26] = (od_coeff)tq;
  y[27] = (od_coeff)tr;
  y[28] = (od_coeff)ts;
  y[29] = (od_coeff)tt;
  y[30] = (od_coeff)tu;
  y[31] = (od_coeff)tv;
  y[32] = (od_coeff)tw;
  y[33] = (od_coeff)tx;
  y[34] = (od_coeff)ty;
  y[35] = (od_coeff)tz;
  y[36] = (od_coeff)tA;
  y[37] = (od_coeff)tB;
  y[38] = (od_coeff)tC;
  y[39] = (od_coeff)tD;
  y[40] = (od_coeff)tE;
  y[41] = (od_coeff)tF;
  y[41] = (od_coeff)tF;
  y[42] = (od_coeff)tG;
  y[43] = (od_coeff)tH;
  y[44] = (od_coeff)tI;
  y[45] = (od_coeff)tJ;
  y[46] = (od_coeff)tK;
  y[47] = (od_coeff)tL;
  y[48] = (od_coeff)tM;
  y[49] = (od_coeff)tN;
  y[50] = (od_coeff)tO;
  y[51] = (od_coeff)tP;
  y[52] = (od_coeff)tQ;
  y[53] = (od_coeff)tR;
  y[54] = (od_coeff)tS;
  y[55] = (od_coeff)tT;
  y[56] = (od_coeff)tU;
  y[57] = (od_coeff)tV;
  y[58] = (od_coeff)tW;
  y[59] = (od_coeff)tX;
  y[60] = (od_coeff)tY;
  y[61] = (od_coeff)tZ;
  y[62] = (od_coeff)t_;
  y[63] = (od_coeff)t;
}

void od_bin_idct64(od_coeff *x, int xstride, const od_coeff y[64]) {
  int t0;
  int t1;
  int t2;
  int t3;
  int t4;
  int t5;
  int t6;
  int t7;
  int t8;
  int t9;
  int ta;
  int tb;
  int tc;
  int td;
  int te;
  int tf;
  int tg;
  int th;
  int ti;
  int tj;
  int tk;
  int tl;
  int tm;
  int tn;
  int to;
  int tp;
  int tq;
  int tr;
  int ts;
  int tt;
  int tu;
  int tv;
  int tw;
  int tx;
  int ty;
  int tz;
  int tA;
  int tB;
  int tC;
  int tD;
  int tE;
  int tF;
  int tG;
  int tH;
  int tI;
  int tJ;
  int tK;
  int tL;
  int tM;
  int tN;
  int tO;
  int tP;
  int tQ;
  int tR;
  int tS;
  int tT;
  int tU;
  int tV;
  int tW;
  int tX;
  int tY;
  int tZ;
  int t_;
  int t;
  t0 = y[0];
  tw = y[1];
  tg = y[2];
  tM = y[3];
  t8 = y[4];
  tE = y[5];
  to = y[6];
  tU = y[7];
  t4 = y[8];
  tA = y[9];
  tk = y[10];
  tQ = y[11];
  tc = y[12];
  tI = y[13];
  ts = y[14];
  tY = y[15];
  t2 = y[16];
  ty = y[17];
  ti = y[18];
  tO = y[19];
  ta = y[20];
  tG = y[21];
  tq = y[22];
  tW = y[23];
  t6 = y[24];
  tC = y[25];
  tm = y[26];
  tS = y[27];
  te = y[28];
  tK = y[29];
  tu = y[30];
  t_ = y[31];
  t1 = y[32];
  tx = y[33];
  th = y[34];
  tN = y[35];
  t9 = y[36];
  tF = y[37];
  tp = y[38];
  tV = y[39];
  t5 = y[40];
  tB = y[41];
  tl = y[42];
  tR = y[43];
  td = y[44];
  tJ = y[45];
  tt = y[46];
  tZ = y[47];
  t3 = y[48];
  tz = y[49];
  tj = y[50];
  tP = y[51];
  tb = y[52];
  tH = y[53];
  tr = y[54];
  tX = y[55];
  t7 = y[56];
  tD = y[57];
  tn = y[58];
  tT = y[59];
  tf = y[60];
  tL = y[61];
  tv = y[62];
  t = y[63];
  OD_IDCT_64_PR(t0, tw, tg, tM, t8, tE, to, tU, t4, tA, tk, tQ, tc, tI, ts, tY,
    t2, ty, ti, tO, ta, tG, tq, tW, t6, tC, tm, tS, te, tK, tu, t_, t1, tx, th,
    tN, t9, tF, tp, tV, t5, tB, tl, tR, td, tJ, tt, tZ, t3, tz, tj, tP, tb, tH,
    tr, tX, t7, tD, tn, tT, tf, tL, tv, t);
  x[0*xstride] = (od_coeff)t0;
  x[1*xstride] = (od_coeff)t1;
  x[2*xstride] = (od_coeff)t2;
  x[3*xstride] = (od_coeff)t3;
  x[4*xstride] = (od_coeff)t4;
  x[5*xstride] = (od_coeff)t5;
  x[6*xstride] = (od_coeff)t6;
  x[7*xstride] = (od_coeff)t7;
  x[8*xstride] = (od_coeff)t8;
  x[9*xstride] = (od_coeff)t9;
  x[10*xstride] = (od_coeff)ta;
  x[11*xstride] = (od_coeff)tb;
  x[12*xstride] = (od_coeff)tc;
  x[13*xstride] = (od_coeff)td;
  x[14*xstride] = (od_coeff)te;
  x[15*xstride] = (od_coeff)tf;
  x[16*xstride] = (od_coeff)tg;
  x[17*xstride] = (od_coeff)th;
  x[18*xstride] = (od_coeff)ti;
  x[19*xstride] = (od_coeff)tj;
  x[20*xstride] = (od_coeff)tk;
  x[21*xstride] = (od_coeff)tl;
  x[22*xstride] = (od_coeff)tm;
  x[23*xstride] = (od_coeff)tn;
  x[24*xstride] = (od_coeff)to;
  x[25*xstride] = (od_coeff)tp;
  x[26*xstride] = (od_coeff)tq;
  x[27*xstride] = (od_coeff)tr;
  x[28*xstride] = (od_coeff)ts;
  x[29*xstride] = (od_coeff)tt;
  x[30*xstride] = (od_coeff)tu;
  x[31*xstride] = (od_coeff)tv;
  x[32*xstride] = (od_coeff)tw;
  x[33*xstride] = (od_coeff)tx;
  x[34*xstride] = (od_coeff)ty;
  x[35*xstride] = (od_coeff)tz;
  x[36*xstride] = (od_coeff)tA;
  x[37*xstride] = (od_coeff)tB;
  x[38*xstride] = (od_coeff)tC;
  x[39*xstride] = (od_coeff)tD;
  x[40*xstride] = (od_coeff)tE;
  x[41*xstride] = (od_coeff)tF;
  x[41*xstride] = (od_coeff)tF;
  x[42*xstride] = (od_coeff)tG;
  x[43*xstride] = (od_coeff)tH;
  x[44*xstride] = (od_coeff)tI;
  x[45*xstride] = (od_coeff)tJ;
  x[46*xstride] = (od_coeff)tK;
  x[47*xstride] = (od_coeff)tL;
  x[48*xstride] = (od_coeff)tM;
  x[49*xstride] = (od_coeff)tN;
  x[50*xstride] = (od_coeff)tO;
  x[51*xstride] = (od_coeff)tP;
  x[52*xstride] = (od_coeff)tQ;
  x[53*xstride] = (od_coeff)tR;
  x[54*xstride] = (od_coeff)tS;
  x[55*xstride] = (od_coeff)tT;
  x[56*xstride] = (od_coeff)tU;
  x[57*xstride] = (od_coeff)tV;
  x[58*xstride] = (od_coeff)tW;
  x[59*xstride] = (od_coeff)tX;
  x[60*xstride] = (od_coeff)tY;
  x[61*xstride] = (od_coeff)tZ;
  x[62*xstride] = (od_coeff)t_;
  x[63*xstride] = (od_coeff)t;
}
#endif

void od_bin_fidtx4(od_coeff y[4], const od_coeff *x, int xstride) {
  int i;
  for (i = 0; i < 4; i++)
    y[i] = x[i*xstride];
}

void od_bin_fidtx8(od_coeff y[8], const od_coeff *x, int xstride) {
  int i;
  for (i = 0; i < 8; i++)
    y[i] = x[i*xstride];
}

void od_bin_fidtx16(od_coeff y[16], const od_coeff *x, int xstride) {
  int i;
  for (i = 0; i < 16; i++)
    y[i] = x[i*xstride];
}

void od_bin_fidtx32(od_coeff y[32], const od_coeff *x, int xstride) {
  int i;
  for (i = 0; i < 32; i++)
    y[i] = x[i*xstride];
}

#if CONFIG_TX64X64
void od_bin_fidtx64(od_coeff y[64], const od_coeff *x, int xstride) {
  int i;
  for (i = 0; i < 64; i++)
    y[i] = x[i*xstride];
}
#endif

void od_bin_iidtx4(od_coeff *x, int xstride, const od_coeff y[4]) {
  int i;
  for (i = 0; i < 4; i++)
    x[i*xstride] = y[i];
}

void od_bin_iidtx8(od_coeff *x, int xstride, const od_coeff y[8]) {
  int i;
  for (i = 0; i < 8; i++)
    x[i*xstride] = y[i];
}

void od_bin_iidtx16(od_coeff *x, int xstride, const od_coeff y[16]) {
  int i;
  for (i = 0; i < 16; i++)
    x[i*xstride] = y[i];
}

void od_bin_iidtx32(od_coeff *x, int xstride, const od_coeff y[32]) {
  int i;
  for (i = 0; i < 32; i++)
    x[i*xstride] = y[i];
}

#if CONFIG_TX64X64
void od_bin_iidtx64(od_coeff *x, int xstride, const od_coeff y[64]) {
  int i;
  for (i = 0; i < 64; i++)
    x[i*xstride] = y[i];
}
#endif

// Below are intermediate wrappers that handle the case when
// tran_low_t is a smaller type than od_coeff
void daala_fdct4(const tran_low_t *input, tran_low_t *output) {
  int i;
  od_coeff x[4];
  od_coeff y[4];
  for (i = 0; i < 4; i++) x[i] = (od_coeff)input[i];
  od_bin_fdct4(y, x, 1);
  for (i = 0; i < 4; i++) output[i] = (tran_low_t)y[i];
}

void daala_idct4(const tran_low_t *input, tran_low_t *output) {
  int i;
  od_coeff x[4];
  od_coeff y[4];
  for (i = 0; i < 4; i++) y[i] = input[i];
  od_bin_idct4(x, 1, y);
  for (i = 0; i < 4; i++) output[i] = (tran_low_t)x[i];
}

void daala_fdst4(const tran_low_t *input, tran_low_t *output) {
  int i;
  od_coeff x[4];
  od_coeff y[4];
  for (i = 0; i < 4; i++) x[i] = (od_coeff)input[i];
  od_bin_fdst4(y, x, 1);
  for (i = 0; i < 4; i++) output[i] = (tran_low_t)y[i];
}

void daala_idst4(const tran_low_t *input, tran_low_t *output) {
  int i;
  od_coeff x[4];
  od_coeff y[4];
  for (i = 0; i < 4; i++) y[i] = input[i];
  od_bin_idst4(x, 1, y);
  for (i = 0; i < 4; i++) output[i] = (tran_low_t)x[i];
}

void daala_idtx4(const tran_low_t *input, tran_low_t *output) {
  int i;
  for (i = 0; i < 4; i++) output[i] = input[i];
}

void daala_fdct8(const tran_low_t *input, tran_low_t *output) {
  int i;
  od_coeff x[8];
  od_coeff y[8];
  for (i = 0; i < 8; i++) x[i] = (od_coeff)input[i];
  od_bin_fdct8(y, x, 1);
  for (i = 0; i < 8; i++) output[i] = (tran_low_t)y[i];
}

void daala_idct8(const tran_low_t *input, tran_low_t *output) {
  int i;
  od_coeff x[8];
  od_coeff y[8];
  for (i = 0; i < 8; i++) y[i] = (od_coeff)input[i];
  od_bin_idct8(x, 1, y);
  for (i = 0; i < 8; i++) output[i] = (tran_low_t)x[i];
}

void daala_fdst8(const tran_low_t *input, tran_low_t *output) {
  int i;
  od_coeff x[8];
  od_coeff y[8];
  for (i = 0; i < 8; i++) x[i] = (od_coeff)input[i];
  od_bin_fdst8(y, x, 1);
  for (i = 0; i < 8; i++) output[i] = (tran_low_t)y[i];
}

void daala_idst8(const tran_low_t *input, tran_low_t *output) {
  int i;
  od_coeff x[8];
  od_coeff y[8];
  for (i = 0; i < 8; i++) y[i] = (od_coeff)input[i];
  od_bin_idst8(x, 1, y);
  for (i = 0; i < 8; i++) output[i] = (tran_low_t)x[i];
}

void daala_idtx8(const tran_low_t *input, tran_low_t *output) {
  int i;
  for (i = 0; i < 8; i++) output[i] = input[i];
}

void daala_fdct16(const tran_low_t *input, tran_low_t *output) {
  int i;
  od_coeff x[16];
  od_coeff y[16];
  for (i = 0; i < 16; i++) x[i] = (od_coeff)input[i];
  od_bin_fdct16(y, x, 1);
  for (i = 0; i < 16; i++) output[i] = (tran_low_t)y[i];
}

void daala_idct16(const tran_low_t *input, tran_low_t *output) {
  int i;
  od_coeff x[16];
  od_coeff y[16];
  for (i = 0; i < 16; i++) y[i] = (od_coeff)input[i];
  od_bin_idct16(x, 1, y);
  for (i = 0; i < 16; i++) output[i] = (tran_low_t)x[i];
}

void daala_fdst16(const tran_low_t *input, tran_low_t *output) {
  int i;
  od_coeff x[16];
  od_coeff y[16];
  for (i = 0; i < 16; i++) x[i] = (od_coeff)input[i];
  od_bin_fdst16(y, x, 1);
  for (i = 0; i < 16; i++) output[i] = (tran_low_t)y[i];
}

void daala_idst16(const tran_low_t *input, tran_low_t *output) {
  int i;
  od_coeff x[16];
  od_coeff y[16];
  for (i = 0; i < 16; i++) y[i] = (od_coeff)input[i];
  od_bin_idst16(x, 1, y);
  for (i = 0; i < 16; i++) output[i] = (tran_low_t)x[i];
}

void daala_idtx16(const tran_low_t *input, tran_low_t *output) {
  int i;
  for (i = 0; i < 16; i++) output[i] = input[i];
}

void daala_fdct32(const tran_low_t *input, tran_low_t *output) {
  int i;
  od_coeff x[32];
  od_coeff y[32];
  for (i = 0; i < 32; i++) x[i] = (od_coeff)input[i];
  od_bin_fdct32(y, x, 1);
  for (i = 0; i < 32; i++) output[i] = (tran_low_t)y[i];
}

void daala_idct32(const tran_low_t *input, tran_low_t *output) {
  int i;
  od_coeff x[32];
  od_coeff y[32];
  for (i = 0; i < 32; i++) y[i] = (od_coeff)input[i];
  od_bin_idct32(x, 1, y);
  for (i = 0; i < 32; i++) output[i] = (tran_low_t)x[i];
}

void daala_fdst32(const tran_low_t *input, tran_low_t *output) {
  int i;
  od_coeff x[32];
  od_coeff y[32];
  for (i = 0; i < 32; i++) x[i] = (od_coeff)input[i];
  od_bin_fdst32(y, x, 1);
  for (i = 0; i < 32; i++) output[i] = (tran_low_t)y[i];
}

void daala_idst32(const tran_low_t *input, tran_low_t *output) {
  int i;
  od_coeff x[32];
  od_coeff y[32];
  for (i = 0; i < 32; i++) y[i] = input[i];
  od_bin_idst32(x, 1, y);
  for (i = 0; i < 32; i++) output[i] = (tran_low_t)x[i];
}

void daala_idtx32(const tran_low_t *input, tran_low_t *output) {
  int i;
  for (i = 0; i < 32; i++) output[i] = input[i];
}

#if CONFIG_TX64X64
void daala_fdct64(const tran_low_t *input, tran_low_t *output) {
  int i;
  od_coeff x[64];
  od_coeff y[64];
  for (i = 0; i < 64; i++) x[i] = (od_coeff)input[i];
  od_bin_fdct64(y, x, 1);
  for (i = 0; i < 64; i++) output[i] = (tran_low_t)y[i];
}

void daala_idct64(const tran_low_t *input, tran_low_t *output) {
  int i;
  od_coeff x[64];
  od_coeff y[64];
  for (i = 0; i < 64; i++) y[i] = (od_coeff)input[i];
  od_bin_idct64(x, 1, y);
  for (i = 0; i < 64; i++) output[i] = (tran_low_t)x[i];
}

/* Preserve the "half-right" transform behavior. */
void daala_fdst64(const tran_low_t *input, tran_low_t *output) {
  int i;
  tran_low_t inputhalf[32];
  for (i = 0; i < 32; ++i) {
    output[32 + i] = input[i];
  }
  for (i = 0; i < 32; ++i) {
    inputhalf[i] = input[i + 32];
  }
  daala_fdct32(inputhalf, output);
}

/* Preserve the "half-right" transform behavior. */
void daala_idst64(const tran_low_t *input, tran_low_t *output) {
  int i;
  tran_low_t inputhalf[32];
  for (i = 0; i < 32; ++i) {
    inputhalf[i] = input[i];
  }
  for (i = 0; i < 32; ++i) {
    output[i] = input[32 + i];
  }
  daala_idct32(inputhalf, output + 32);
}

void daala_idtx64(const tran_low_t *input, tran_low_t *output) {
  int i;
  for (i = 0; i < 64; i++) output[i] = input[i];
}
#endif
