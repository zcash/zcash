#ifndef BLAKE2_AVX2_BLAKE2B_LOAD_AVX2_SIMPLE_H
#define BLAKE2_AVX2_BLAKE2B_LOAD_AVX2_SIMPLE_H

#define BLAKE2B_LOAD_MSG_0_1(b0) b0 = _mm256_set_epi64x(m6, m4, m2, m0);
#define BLAKE2B_LOAD_MSG_0_2(b0) b0 = _mm256_set_epi64x(m7, m5, m3, m1);
#define BLAKE2B_LOAD_MSG_0_3(b0) b0 = _mm256_set_epi64x(m14, m12, m10, m8);
#define BLAKE2B_LOAD_MSG_0_4(b0) b0 = _mm256_set_epi64x(m15, m13, m11, m9);
#define BLAKE2B_LOAD_MSG_1_1(b0) b0 = _mm256_set_epi64x(m13, m9, m4, m14);
#define BLAKE2B_LOAD_MSG_1_2(b0) b0 = _mm256_set_epi64x(m6, m15, m8, m10);
#define BLAKE2B_LOAD_MSG_1_3(b0) b0 = _mm256_set_epi64x(m5, m11, m0, m1);
#define BLAKE2B_LOAD_MSG_1_4(b0) b0 = _mm256_set_epi64x(m3, m7, m2, m12);
#define BLAKE2B_LOAD_MSG_2_1(b0) b0 = _mm256_set_epi64x(m15, m5, m12, m11);
#define BLAKE2B_LOAD_MSG_2_2(b0) b0 = _mm256_set_epi64x(m13, m2, m0, m8);
#define BLAKE2B_LOAD_MSG_2_3(b0) b0 = _mm256_set_epi64x(m9, m7, m3, m10);
#define BLAKE2B_LOAD_MSG_2_4(b0) b0 = _mm256_set_epi64x(m4, m1, m6, m14);
#define BLAKE2B_LOAD_MSG_3_1(b0) b0 = _mm256_set_epi64x(m11, m13, m3, m7);
#define BLAKE2B_LOAD_MSG_3_2(b0) b0 = _mm256_set_epi64x(m14, m12, m1, m9);
#define BLAKE2B_LOAD_MSG_3_3(b0) b0 = _mm256_set_epi64x(m15, m4, m5, m2);
#define BLAKE2B_LOAD_MSG_3_4(b0) b0 = _mm256_set_epi64x(m8, m0, m10, m6);
#define BLAKE2B_LOAD_MSG_4_1(b0) b0 = _mm256_set_epi64x(m10, m2, m5, m9);
#define BLAKE2B_LOAD_MSG_4_2(b0) b0 = _mm256_set_epi64x(m15, m4, m7, m0);
#define BLAKE2B_LOAD_MSG_4_3(b0) b0 = _mm256_set_epi64x(m3, m6, m11, m14);
#define BLAKE2B_LOAD_MSG_4_4(b0) b0 = _mm256_set_epi64x(m13, m8, m12, m1);
#define BLAKE2B_LOAD_MSG_5_1(b0) b0 = _mm256_set_epi64x(m8, m0, m6, m2);
#define BLAKE2B_LOAD_MSG_5_2(b0) b0 = _mm256_set_epi64x(m3, m11, m10, m12);
#define BLAKE2B_LOAD_MSG_5_3(b0) b0 = _mm256_set_epi64x(m1, m15, m7, m4);
#define BLAKE2B_LOAD_MSG_5_4(b0) b0 = _mm256_set_epi64x(m9, m14, m5, m13);
#define BLAKE2B_LOAD_MSG_6_1(b0) b0 = _mm256_set_epi64x(m4, m14, m1, m12);
#define BLAKE2B_LOAD_MSG_6_2(b0) b0 = _mm256_set_epi64x(m10, m13, m15, m5);
#define BLAKE2B_LOAD_MSG_6_3(b0) b0 = _mm256_set_epi64x(m8, m9, m6, m0);
#define BLAKE2B_LOAD_MSG_6_4(b0) b0 = _mm256_set_epi64x(m11, m2, m3, m7);
#define BLAKE2B_LOAD_MSG_7_1(b0) b0 = _mm256_set_epi64x(m3, m12, m7, m13);
#define BLAKE2B_LOAD_MSG_7_2(b0) b0 = _mm256_set_epi64x(m9, m1, m14, m11);
#define BLAKE2B_LOAD_MSG_7_3(b0) b0 = _mm256_set_epi64x(m2, m8, m15, m5);
#define BLAKE2B_LOAD_MSG_7_4(b0) b0 = _mm256_set_epi64x(m10, m6, m4, m0);
#define BLAKE2B_LOAD_MSG_8_1(b0) b0 = _mm256_set_epi64x(m0, m11, m14, m6);
#define BLAKE2B_LOAD_MSG_8_2(b0) b0 = _mm256_set_epi64x(m8, m3, m9, m15);
#define BLAKE2B_LOAD_MSG_8_3(b0) b0 = _mm256_set_epi64x(m10, m1, m13, m12);
#define BLAKE2B_LOAD_MSG_8_4(b0) b0 = _mm256_set_epi64x(m5, m4, m7, m2);
#define BLAKE2B_LOAD_MSG_9_1(b0) b0 = _mm256_set_epi64x(m1, m7, m8, m10);
#define BLAKE2B_LOAD_MSG_9_2(b0) b0 = _mm256_set_epi64x(m5, m6, m4, m2);
#define BLAKE2B_LOAD_MSG_9_3(b0) b0 = _mm256_set_epi64x(m13, m3, m9, m15);
#define BLAKE2B_LOAD_MSG_9_4(b0) b0 = _mm256_set_epi64x(m0, m12, m14, m11);
#define BLAKE2B_LOAD_MSG_10_1(b0) b0 = _mm256_set_epi64x(m6, m4, m2, m0);
#define BLAKE2B_LOAD_MSG_10_2(b0) b0 = _mm256_set_epi64x(m7, m5, m3, m1);
#define BLAKE2B_LOAD_MSG_10_3(b0) b0 = _mm256_set_epi64x(m14, m12, m10, m8);
#define BLAKE2B_LOAD_MSG_10_4(b0) b0 = _mm256_set_epi64x(m15, m13, m11, m9);
#define BLAKE2B_LOAD_MSG_11_1(b0) b0 = _mm256_set_epi64x(m13, m9, m4, m14);
#define BLAKE2B_LOAD_MSG_11_2(b0) b0 = _mm256_set_epi64x(m6, m15, m8, m10);
#define BLAKE2B_LOAD_MSG_11_3(b0) b0 = _mm256_set_epi64x(m5, m11, m0, m1);
#define BLAKE2B_LOAD_MSG_11_4(b0) b0 = _mm256_set_epi64x(m3, m7, m2, m12);

#endif

