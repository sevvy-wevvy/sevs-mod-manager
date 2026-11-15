#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "webp.h"

#define MAX_SIDE 4096
#define BPS 32
#define FOURCC(a, b, c, d) ((uint32_t)(a) | (uint32_t)(b) << 8 | (uint32_t)(c) << 16 | (uint32_t)(d) << 24)

static uint32_t le32(const uint8_t *p)
{
    return p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

static inline uint8_t clip8(int v)
{
    return v < 0 ? 0 : v > 255 ? 255 : (uint8_t)v;
}

static const uint8_t kf_bmode_probs[900] = {
    231, 120, 48, 89, 115, 113, 120, 152, 112,
    152, 179, 64, 126, 170, 118, 46, 70, 95,
    175, 69, 143, 80, 85, 82, 72, 155, 103,
    56, 58, 10, 171, 218, 189, 17, 13, 152,
    144, 71, 10, 38, 171, 213, 144, 34, 26,
    114, 26, 17, 163, 44, 195, 21, 10, 173,
    121, 24, 80, 195, 26, 62, 44, 64, 85,
    170, 46, 55, 19, 136, 160, 33, 206, 71,
    63, 20, 8, 114, 114, 208, 12, 9, 226,
    81, 40, 11, 96, 182, 84, 29, 16, 36,
    134, 183, 89, 137, 98, 101, 106, 165, 148,
    72, 187, 100, 130, 157, 111, 32, 75, 80,
    66, 102, 167, 99, 74, 62, 40, 234, 128,
    41, 53, 9, 178, 241, 141, 26, 8, 107,
    104, 79, 12, 27, 217, 255, 87, 17, 7,
    74, 43, 26, 146, 73, 166, 49, 23, 157,
    65, 38, 105, 160, 51, 52, 31, 115, 128,
    87, 68, 71, 44, 114, 51, 15, 186, 23,
    47, 41, 14, 110, 182, 183, 21, 17, 194,
    66, 45, 25, 102, 197, 189, 23, 18, 22,
    88, 88, 147, 150, 42, 46, 45, 196, 205,
    43, 97, 183, 117, 85, 38, 35, 179, 61,
    39, 53, 200, 87, 26, 21, 43, 232, 171,
    56, 34, 51, 104, 114, 102, 29, 93, 77,
    107, 54, 32, 26, 51, 1, 81, 43, 31,
    39, 28, 85, 171, 58, 165, 90, 98, 64,
    34, 22, 116, 206, 23, 34, 43, 166, 73,
    68, 25, 106, 22, 64, 171, 36, 225, 114,
    34, 19, 21, 102, 132, 188, 16, 76, 124,
    62, 18, 78, 95, 85, 57, 50, 48, 51,
    193, 101, 35, 159, 215, 111, 89, 46, 111,
    60, 148, 31, 172, 219, 228, 21, 18, 111,
    112, 113, 77, 85, 179, 255, 38, 120, 114,
    40, 42, 1, 196, 245, 209, 10, 25, 109,
    100, 80, 8, 43, 154, 1, 51, 26, 71,
    88, 43, 29, 140, 166, 213, 37, 43, 154,
    61, 63, 30, 155, 67, 45, 68, 1, 209,
    142, 78, 78, 16, 255, 128, 34, 197, 171,
    41, 40, 5, 102, 211, 183, 4, 1, 221,
    51, 50, 17, 168, 209, 192, 23, 25, 82,
    125, 98, 42, 88, 104, 85, 117, 175, 82,
    95, 84, 53, 89, 128, 100, 113, 101, 45,
    75, 79, 123, 47, 51, 128, 81, 171, 1,
    57, 17, 5, 71, 102, 57, 53, 41, 49,
    115, 21, 2, 10, 102, 255, 166, 23, 6,
    38, 33, 13, 121, 57, 73, 26, 1, 85,
    41, 10, 67, 138, 77, 110, 90, 47, 114,
    101, 29, 16, 10, 85, 128, 101, 196, 26,
    57, 18, 10, 102, 102, 213, 34, 20, 43,
    117, 20, 15, 36, 163, 128, 68, 1, 26,
    138, 31, 36, 171, 27, 166, 38, 44, 229,
    67, 87, 58, 169, 82, 115, 26, 59, 179,
    63, 59, 90, 180, 59, 166, 93, 73, 154,
    40, 40, 21, 116, 143, 209, 34, 39, 175,
    57, 46, 22, 24, 128, 1, 54, 17, 37,
    47, 15, 16, 183, 34, 223, 49, 45, 183,
    46, 17, 33, 183, 6, 98, 15, 32, 183,
    65, 32, 73, 115, 28, 128, 23, 128, 205,
    40, 3, 9, 115, 51, 192, 18, 6, 223,
    87, 37, 9, 115, 59, 77, 64, 21, 47,
    104, 55, 44, 218, 9, 54, 53, 130, 226,
    64, 90, 70, 205, 40, 41, 23, 26, 57,
    54, 57, 112, 184, 5, 41, 38, 166, 213,
    30, 34, 26, 133, 152, 116, 10, 32, 134,
    75, 32, 12, 51, 192, 255, 160, 43, 51,
    39, 19, 53, 221, 26, 114, 32, 73, 255,
    31, 9, 65, 234, 2, 15, 1, 118, 73,
    88, 31, 35, 67, 102, 85, 55, 186, 85,
    56, 21, 23, 111, 59, 205, 45, 37, 192,
    55, 38, 70, 124, 73, 102, 1, 34, 98,
    102, 61, 71, 37, 34, 53, 31, 243, 192,
    69, 60, 71, 38, 73, 119, 28, 222, 37,
    68, 45, 128, 34, 1, 47, 11, 245, 171,
    62, 17, 19, 70, 146, 85, 55, 62, 70,
    75, 15, 9, 9, 64, 255, 184, 119, 16,
    37, 43, 37, 154, 100, 163, 85, 160, 1,
    63, 9, 92, 136, 28, 64, 32, 201, 85,
    86, 6, 28, 5, 64, 255, 25, 248, 1,
    56, 8, 17, 132, 137, 255, 55, 116, 128,
    58, 15, 20, 82, 135, 57, 26, 121, 40,
    164, 50, 31, 137, 154, 133, 25, 35, 218,
    51, 103, 44, 131, 131, 123, 31, 6, 158,
    86, 40, 64, 135, 148, 224, 45, 183, 128,
    22, 26, 17, 131, 240, 154, 14, 1, 209,
    83, 12, 13, 54, 192, 255, 68, 47, 28,
    45, 16, 21, 91, 64, 222, 7, 1, 197,
    56, 21, 39, 155, 60, 138, 23, 102, 213,
    85, 26, 85, 85, 128, 128, 32, 146, 171,
    18, 11, 7, 63, 144, 171, 4, 4, 246,
    35, 27, 10, 146, 174, 171, 12, 26, 128,
    190, 80, 35, 99, 180, 80, 126, 54, 45,
    85, 126, 47, 87, 176, 51, 41, 20, 32,
    101, 75, 128, 139, 118, 146, 116, 128, 85,
    56, 41, 15, 176, 236, 85, 37, 9, 62,
    146, 36, 19, 30, 171, 255, 97, 27, 20,
    71, 30, 17, 119, 118, 255, 17, 18, 138,
    101, 38, 60, 138, 55, 70, 43, 26, 142,
    138, 45, 61, 62, 219, 1, 81, 188, 64,
    32, 41, 20, 117, 151, 142, 20, 21, 163,
    112, 19, 12, 61, 195, 128, 48, 4, 24,
};

static const uint8_t coeff_update_probs[1056] = {
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    176, 246, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    223, 241, 252, 255, 255, 255, 255, 255, 255, 255, 255,
    249, 253, 253, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 244, 252, 255, 255, 255, 255, 255, 255, 255, 255,
    234, 254, 254, 255, 255, 255, 255, 255, 255, 255, 255,
    253, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 246, 254, 255, 255, 255, 255, 255, 255, 255, 255,
    239, 253, 254, 255, 255, 255, 255, 255, 255, 255, 255,
    254, 255, 254, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 248, 254, 255, 255, 255, 255, 255, 255, 255, 255,
    251, 255, 254, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 253, 254, 255, 255, 255, 255, 255, 255, 255, 255,
    251, 254, 254, 255, 255, 255, 255, 255, 255, 255, 255,
    254, 255, 254, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 254, 253, 255, 254, 255, 255, 255, 255, 255, 255,
    250, 255, 254, 255, 254, 255, 255, 255, 255, 255, 255,
    254, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    217, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    225, 252, 241, 253, 255, 255, 254, 255, 255, 255, 255,
    234, 250, 241, 250, 253, 255, 253, 254, 255, 255, 255,
    255, 254, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    223, 254, 254, 255, 255, 255, 255, 255, 255, 255, 255,
    238, 253, 254, 254, 255, 255, 255, 255, 255, 255, 255,
    255, 248, 254, 255, 255, 255, 255, 255, 255, 255, 255,
    249, 254, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 253, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    247, 254, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 253, 254, 255, 255, 255, 255, 255, 255, 255, 255,
    252, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 254, 254, 255, 255, 255, 255, 255, 255, 255, 255,
    253, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 254, 253, 255, 255, 255, 255, 255, 255, 255, 255,
    250, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    254, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    186, 251, 250, 255, 255, 255, 255, 255, 255, 255, 255,
    234, 251, 244, 254, 255, 255, 255, 255, 255, 255, 255,
    251, 251, 243, 253, 254, 255, 254, 255, 255, 255, 255,
    255, 253, 254, 255, 255, 255, 255, 255, 255, 255, 255,
    236, 253, 254, 255, 255, 255, 255, 255, 255, 255, 255,
    251, 253, 253, 254, 254, 255, 255, 255, 255, 255, 255,
    255, 254, 254, 255, 255, 255, 255, 255, 255, 255, 255,
    254, 254, 254, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 254, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    254, 254, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    254, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    254, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    248, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    250, 254, 252, 254, 255, 255, 255, 255, 255, 255, 255,
    248, 254, 249, 253, 255, 255, 255, 255, 255, 255, 255,
    255, 253, 253, 255, 255, 255, 255, 255, 255, 255, 255,
    246, 253, 253, 255, 255, 255, 255, 255, 255, 255, 255,
    252, 254, 251, 254, 254, 255, 255, 255, 255, 255, 255,
    255, 254, 252, 255, 255, 255, 255, 255, 255, 255, 255,
    248, 254, 253, 255, 255, 255, 255, 255, 255, 255, 255,
    253, 255, 254, 254, 255, 255, 255, 255, 255, 255, 255,
    255, 251, 254, 255, 255, 255, 255, 255, 255, 255, 255,
    245, 251, 254, 255, 255, 255, 255, 255, 255, 255, 255,
    253, 253, 254, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 251, 253, 255, 255, 255, 255, 255, 255, 255, 255,
    252, 253, 254, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 254, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 252, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    249, 255, 254, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 254, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 253, 255, 255, 255, 255, 255, 255, 255, 255,
    250, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    254, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
};

static const uint8_t coeff_default_probs[1056] = {
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    253, 136, 254, 255, 228, 219, 128, 128, 128, 128, 128,
    189, 129, 242, 255, 227, 213, 255, 219, 128, 128, 128,
    106, 126, 227, 252, 214, 209, 255, 255, 128, 128, 128,
    1, 98, 248, 255, 236, 226, 255, 255, 128, 128, 128,
    181, 133, 238, 254, 221, 234, 255, 154, 128, 128, 128,
    78, 134, 202, 247, 198, 180, 255, 219, 128, 128, 128,
    1, 185, 249, 255, 243, 255, 128, 128, 128, 128, 128,
    184, 150, 247, 255, 236, 224, 128, 128, 128, 128, 128,
    77, 110, 216, 255, 236, 230, 128, 128, 128, 128, 128,
    1, 101, 251, 255, 241, 255, 128, 128, 128, 128, 128,
    170, 139, 241, 252, 236, 209, 255, 255, 128, 128, 128,
    37, 116, 196, 243, 228, 255, 255, 255, 128, 128, 128,
    1, 204, 254, 255, 245, 255, 128, 128, 128, 128, 128,
    207, 160, 250, 255, 238, 128, 128, 128, 128, 128, 128,
    102, 103, 231, 255, 211, 171, 128, 128, 128, 128, 128,
    1, 152, 252, 255, 240, 255, 128, 128, 128, 128, 128,
    177, 135, 243, 255, 234, 225, 128, 128, 128, 128, 128,
    80, 129, 211, 255, 194, 224, 128, 128, 128, 128, 128,
    1, 1, 255, 128, 128, 128, 128, 128, 128, 128, 128,
    246, 1, 255, 128, 128, 128, 128, 128, 128, 128, 128,
    255, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    198, 35, 237, 223, 193, 187, 162, 160, 145, 155, 62,
    131, 45, 198, 221, 172, 176, 220, 157, 252, 221, 1,
    68, 47, 146, 208, 149, 167, 221, 162, 255, 223, 128,
    1, 149, 241, 255, 221, 224, 255, 255, 128, 128, 128,
    184, 141, 234, 253, 222, 220, 255, 199, 128, 128, 128,
    81, 99, 181, 242, 176, 190, 249, 202, 255, 255, 128,
    1, 129, 232, 253, 214, 197, 242, 196, 255, 255, 128,
    99, 121, 210, 250, 201, 198, 255, 202, 128, 128, 128,
    23, 91, 163, 242, 170, 187, 247, 210, 255, 255, 128,
    1, 200, 246, 255, 234, 255, 128, 128, 128, 128, 128,
    109, 178, 241, 255, 231, 245, 255, 255, 128, 128, 128,
    44, 130, 201, 253, 205, 192, 255, 255, 128, 128, 128,
    1, 132, 239, 251, 219, 209, 255, 165, 128, 128, 128,
    94, 136, 225, 251, 218, 190, 255, 255, 128, 128, 128,
    22, 100, 174, 245, 186, 161, 255, 199, 128, 128, 128,
    1, 182, 249, 255, 232, 235, 128, 128, 128, 128, 128,
    124, 143, 241, 255, 227, 234, 128, 128, 128, 128, 128,
    35, 77, 181, 251, 193, 211, 255, 205, 128, 128, 128,
    1, 157, 247, 255, 236, 231, 255, 255, 128, 128, 128,
    121, 141, 235, 255, 225, 227, 255, 255, 128, 128, 128,
    45, 99, 188, 251, 195, 217, 255, 224, 128, 128, 128,
    1, 1, 251, 255, 213, 255, 128, 128, 128, 128, 128,
    203, 1, 248, 255, 255, 128, 128, 128, 128, 128, 128,
    137, 1, 177, 255, 224, 255, 128, 128, 128, 128, 128,
    253, 9, 248, 251, 207, 208, 255, 192, 128, 128, 128,
    175, 13, 224, 243, 193, 185, 249, 198, 255, 255, 128,
    73, 17, 171, 221, 161, 179, 236, 167, 255, 234, 128,
    1, 95, 247, 253, 212, 183, 255, 255, 128, 128, 128,
    239, 90, 244, 250, 211, 209, 255, 255, 128, 128, 128,
    155, 77, 195, 248, 188, 195, 255, 255, 128, 128, 128,
    1, 24, 239, 251, 218, 219, 255, 205, 128, 128, 128,
    201, 51, 219, 255, 196, 186, 128, 128, 128, 128, 128,
    69, 46, 190, 239, 201, 218, 255, 228, 128, 128, 128,
    1, 191, 251, 255, 255, 128, 128, 128, 128, 128, 128,
    223, 165, 249, 255, 213, 255, 128, 128, 128, 128, 128,
    141, 124, 248, 255, 255, 128, 128, 128, 128, 128, 128,
    1, 16, 248, 255, 255, 128, 128, 128, 128, 128, 128,
    190, 36, 230, 255, 236, 255, 128, 128, 128, 128, 128,
    149, 1, 255, 128, 128, 128, 128, 128, 128, 128, 128,
    1, 226, 255, 128, 128, 128, 128, 128, 128, 128, 128,
    247, 192, 255, 128, 128, 128, 128, 128, 128, 128, 128,
    240, 128, 255, 128, 128, 128, 128, 128, 128, 128, 128,
    1, 134, 252, 255, 255, 128, 128, 128, 128, 128, 128,
    213, 62, 250, 255, 255, 128, 128, 128, 128, 128, 128,
    55, 93, 255, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    202, 24, 213, 235, 186, 191, 220, 160, 240, 175, 255,
    126, 38, 182, 232, 169, 184, 228, 174, 255, 187, 128,
    61, 46, 138, 219, 151, 178, 240, 170, 255, 216, 128,
    1, 112, 230, 250, 199, 191, 247, 159, 255, 255, 128,
    166, 109, 228, 252, 211, 215, 255, 174, 128, 128, 128,
    39, 77, 162, 232, 172, 180, 245, 178, 255, 255, 128,
    1, 52, 220, 246, 198, 199, 249, 220, 255, 255, 128,
    124, 74, 191, 243, 183, 193, 250, 221, 255, 255, 128,
    24, 71, 130, 219, 154, 170, 243, 182, 255, 255, 128,
    1, 182, 225, 249, 219, 240, 255, 224, 128, 128, 128,
    149, 150, 226, 252, 216, 205, 255, 171, 128, 128, 128,
    28, 108, 170, 242, 183, 194, 254, 223, 255, 255, 128,
    1, 81, 230, 252, 204, 203, 255, 192, 128, 128, 128,
    123, 102, 209, 247, 188, 196, 255, 233, 128, 128, 128,
    20, 95, 153, 243, 164, 173, 255, 203, 128, 128, 128,
    1, 222, 248, 255, 216, 213, 128, 128, 128, 128, 128,
    168, 175, 246, 252, 235, 205, 255, 255, 128, 128, 128,
    47, 116, 215, 255, 211, 212, 255, 255, 128, 128, 128,
    1, 121, 236, 253, 212, 214, 255, 255, 128, 128, 128,
    141, 84, 213, 252, 201, 202, 255, 219, 128, 128, 128,
    42, 80, 160, 240, 162, 185, 255, 205, 128, 128, 128,
    1, 1, 255, 128, 128, 128, 128, 128, 128, 128, 128,
    244, 1, 255, 128, 128, 128, 128, 128, 128, 128, 128,
    238, 1, 255, 128, 128, 128, 128, 128, 128, 128, 128,
};

static const uint16_t dc_table[128] = {
    4, 5, 6, 7, 8, 9, 10, 10, 11, 12, 13, 14, 15,
    16, 17, 17, 18, 19, 20, 20, 21, 21, 22, 22, 23, 23,
    24, 25, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35,
    36, 37, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 46,
    47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59,
    60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71, 72,
    73, 74, 75, 76, 76, 77, 78, 79, 80, 81, 82, 83, 84,
    85, 86, 87, 88, 89, 91, 93, 95, 96, 98, 100, 101, 102,
    104, 106, 108, 110, 112, 114, 116, 118, 122, 124, 126, 128, 130,
    132, 134, 136, 138, 140, 143, 145, 148, 151, 154, 157,
};

static const uint16_t ac_table[128] = {
    4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16,
    17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29,
    30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42,
    43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55,
    56, 57, 58, 60, 62, 64, 66, 68, 70, 72, 74, 76, 78,
    80, 82, 84, 86, 88, 90, 92, 94, 96, 98, 100, 102, 104,
    106, 108, 110, 112, 114, 116, 119, 122, 125, 128, 131, 134, 137,
    140, 143, 146, 149, 152, 155, 158, 161, 164, 167, 170, 173, 177,
    181, 185, 189, 193, 197, 201, 205, 209, 213, 217, 221, 225, 229,
    234, 239, 245, 249, 254, 259, 264, 269, 274, 279, 284,
};

static const int8_t distance_map[240] = {
    0, 1, 1, 0, 1, 1, -1, 1, 0, 2, 2, 0, 1, 2,
    -1, 2, 2, 1, -2, 1, 2, 2, -2, 2, 0, 3, 3, 0,
    1, 3, -1, 3, 3, 1, -3, 1, 2, 3, -2, 3, 3, 2,
    -3, 2, 0, 4, 4, 0, 1, 4, -1, 4, 4, 1, -4, 1,
    3, 3, -3, 3, 2, 4, -2, 4, 4, 2, -4, 2, 0, 5,
    3, 4, -3, 4, 4, 3, -4, 3, 5, 0, 1, 5, -1, 5,
    5, 1, -5, 1, 2, 5, -2, 5, 5, 2, -5, 2, 4, 4,
    -4, 4, 3, 5, -3, 5, 5, 3, -5, 3, 0, 6, 6, 0,
    1, 6, -1, 6, 6, 1, -6, 1, 2, 6, -2, 6, 6, 2,
    -6, 2, 4, 5, -4, 5, 5, 4, -5, 4, 3, 6, -3, 6,
    6, 3, -6, 3, 0, 7, 7, 0, 1, 7, -1, 7, 5, 5,
    -5, 5, 7, 1, -7, 1, 4, 6, -4, 6, 6, 4, -6, 4,
    2, 7, -2, 7, 7, 2, -7, 2, 3, 7, -3, 7, 7, 3,
    -7, 3, 5, 6, -5, 6, 6, 5, -6, 5, 8, 0, 4, 7,
    -4, 7, 7, 4, -7, 4, 8, 1, 8, 2, 6, 6, -6, 6,
    8, 3, 5, 7, -5, 7, 7, 5, -7, 5, 8, 4, 6, 7,
    -6, 7, 7, 6, -7, 6, 8, 5, 7, 7, -7, 7, 8, 6,
    8, 7,
};


static const uint8_t band_of[17] = {0, 1, 2, 3, 6, 4, 5, 6, 6, 6, 6, 6, 6, 6, 6, 7, 0};
static const uint8_t zigzag[16] = {0, 1, 4, 8, 5, 2, 3, 6, 9, 12, 13, 10, 7, 11, 14, 15};
static const uint8_t cat3[] = {173, 148, 140, 0};
static const uint8_t cat4[] = {176, 155, 140, 135, 0};
static const uint8_t cat5[] = {180, 157, 141, 134, 130, 0};
static const uint8_t cat6[] = {254, 254, 243, 230, 196, 177, 153, 140, 133, 130, 129, 0};
static const uint8_t *const cat_probs[4] = {cat3, cat4, cat5, cat6};

typedef struct {
    const uint8_t *p, *end;
    uint32_t value;
    uint32_t range;
    int count;
    int over;
} Bool;

static int bool_byte(Bool *b)
{
    if (b->p < b->end) return *b->p++;
    b->over++;
    return 0;
}

static void bool_init(Bool *b, const uint8_t *p, size_t n)
{
    b->p = p;
    b->end = p + n;
    b->range = 255;
    b->count = 0;
    b->over = 0;
    b->value = (uint32_t)bool_byte(b) << 8;
    b->value |= (uint32_t)bool_byte(b);
}

static int bool_get(Bool *b, int prob)
{
    uint32_t split = 1 + (((b->range - 1) * (uint32_t)prob) >> 8);
    uint32_t big = split << 8;
    int bit;
    if (b->value >= big) {
        bit = 1;
        b->range -= split;
        b->value -= big;
    } else {
        bit = 0;
        b->range = split;
    }
    while (b->range < 128) {
        b->value <<= 1;
        b->range <<= 1;
        if (++b->count == 8) {
            b->count = 0;
            b->value |= (uint32_t)bool_byte(b);
        }
    }
    return bit;
}

static int bool_bits(Bool *b, int n)
{
    int v = 0;
    while (n-- > 0) v = v << 1 | bool_get(b, 128);
    return v;
}

static int bool_signed(Bool *b, int n)
{
    int v = bool_bits(b, n);
    return bool_get(b, 128) ? -v : v;
}

static int bool_flagged(Bool *b, int n)
{
    return bool_get(b, 128) ? bool_signed(b, n) : 0;
}

typedef struct {
    uint8_t limit, ilevel, hev;
} Strength;

typedef struct {
    uint8_t limit, ilevel, hev, inner;
} MbFilter;

typedef struct {
    int w, h, mbw, mbh, ys, uvs;
    uint8_t *y, *u, *v;
    int seg_on, seg_map, seg_abs, seg_q[4], seg_lf[4];
    uint8_t seg_p[3];
    int simple, lf_level, sharp, delta_on, ref_delta[4], mode_delta[4];
    int nparts;
    int y1[4][2], y2[4][2], uv[4][2];
    uint8_t coef[4][8][3][11];
    int skip_on, skip_p;
    int filter_type;
    Strength strength[4][2];
    MbFilter *mf;
} Vp8;

static int vp8_info(const uint8_t *d, size_t n, int *w, int *h)
{
    if (n < 10) return 0;
    uint32_t tag = d[0] | (uint32_t)d[1] << 8 | (uint32_t)d[2] << 16;
    if (tag & 1) return 0;
    if (((tag >> 1) & 7) > 3) return 0;
    if (!((tag >> 4) & 1)) return 0;
    if (d[3] != 0x9d || d[4] != 0x01 || d[5] != 0x2a) return 0;
    *w = (d[6] | d[7] << 8) & 0x3fff;
    *h = (d[8] | d[9] << 8) & 0x3fff;
    return *w > 0 && *h > 0 && *w <= MAX_SIDE && *h <= MAX_SIDE;
}

static int clamp_int(int v, int lo, int hi)
{
    return v < lo ? lo : v > hi ? hi : v;
}

static void read_header(Vp8 *s, Bool *b, int *base_q)
{
    bool_bits(b, 2);
    s->seg_on = bool_get(b, 128);
    s->seg_map = 0;
    s->seg_abs = 0;
    memset(s->seg_q, 0, sizeof(s->seg_q));
    memset(s->seg_lf, 0, sizeof(s->seg_lf));
    s->seg_p[0] = s->seg_p[1] = s->seg_p[2] = 255;
    if (s->seg_on) {
        s->seg_map = bool_get(b, 128);
        int update_data = bool_get(b, 128);
        if (update_data) {
            s->seg_abs = bool_get(b, 128);
            for (int i = 0; i < 4; i++) s->seg_q[i] = bool_flagged(b, 7);
            for (int i = 0; i < 4; i++) s->seg_lf[i] = bool_flagged(b, 6);
        }
        if (s->seg_map)
            for (int i = 0; i < 3; i++) s->seg_p[i] = bool_get(b, 128) ? bool_bits(b, 8) : 255;
    }
    s->simple = bool_get(b, 128);
    s->lf_level = bool_bits(b, 6);
    s->sharp = bool_bits(b, 3);
    s->delta_on = bool_get(b, 128);
    memset(s->ref_delta, 0, sizeof(s->ref_delta));
    memset(s->mode_delta, 0, sizeof(s->mode_delta));
    if (s->delta_on && bool_get(b, 128)) {
        for (int i = 0; i < 4; i++)
            if (bool_get(b, 128)) s->ref_delta[i] = bool_signed(b, 6);
        for (int i = 0; i < 4; i++)
            if (bool_get(b, 128)) s->mode_delta[i] = bool_signed(b, 6);
    }
    s->filter_type = s->lf_level == 0 ? 0 : s->simple ? 1 : 2;
    s->nparts = 1 << bool_bits(b, 2);
    *base_q = bool_bits(b, 7);
    int dq_y1dc = bool_flagged(b, 4);
    int dq_y2dc = bool_flagged(b, 4);
    int dq_y2ac = bool_flagged(b, 4);
    int dq_uvdc = bool_flagged(b, 4);
    int dq_uvac = bool_flagged(b, 4);
    for (int i = 0; i < 4; i++) {
        int q = *base_q;
        if (s->seg_on) {
            q = s->seg_q[i];
            if (!s->seg_abs) q += *base_q;
        }
        s->y1[i][0] = dc_table[clamp_int(q + dq_y1dc, 0, 127)];
        s->y1[i][1] = ac_table[clamp_int(q, 0, 127)];
        s->y2[i][0] = dc_table[clamp_int(q + dq_y2dc, 0, 127)] * 2;
        s->y2[i][1] = ac_table[clamp_int(q + dq_y2ac, 0, 127)] * 101581 >> 16;
        if (s->y2[i][1] < 8) s->y2[i][1] = 8;
        s->uv[i][0] = dc_table[clamp_int(q + dq_uvdc, 0, 117)];
        s->uv[i][1] = ac_table[clamp_int(q + dq_uvac, 0, 127)];
    }
    bool_get(b, 128);
    for (int t = 0; t < 4; t++)
        for (int k = 0; k < 8; k++)
            for (int c = 0; c < 3; c++)
                for (int p = 0; p < 11; p++) {
                    int idx = ((t * 8 + k) * 3 + c) * 11 + p;
                    s->coef[t][k][c][p] = bool_get(b, coeff_update_probs[idx]) ? bool_bits(b, 8) : coeff_default_probs[idx];
                }
    s->skip_on = bool_get(b, 128);
    s->skip_p = s->skip_on ? bool_bits(b, 8) : 0;
    for (int i = 0; i < 4; i++) {
        int base = s->lf_level;
        if (s->seg_on) {
            base = s->seg_lf[i];
            if (!s->seg_abs) base += s->lf_level;
        }
        for (int i4 = 0; i4 < 2; i4++) {
            int level = base;
            if (s->delta_on) {
                level += s->ref_delta[0];
                if (i4) level += s->mode_delta[0];
            }
            level = clamp_int(level, 0, 63);
            Strength *st = &s->strength[i][i4];
            st->limit = 0;
            if (level > 0) {
                int il = level;
                if (s->sharp > 0) {
                    il >>= s->sharp > 4 ? 2 : 1;
                    if (il > 9 - s->sharp) il = 9 - s->sharp;
                }
                if (il < 1) il = 1;
                st->ilevel = il;
                st->limit = 2 * level + il;
                st->hev = level >= 40 ? 2 : level >= 15 ? 1 : 0;
            }
        }
    }
}

static int large_value(Bool *b, const uint8_t *p)
{
    int v;
    if (!bool_get(b, p[3])) {
        if (!bool_get(b, p[4])) v = 2;
        else v = 3 + bool_get(b, p[5]);
    } else if (!bool_get(b, p[6])) {
        if (!bool_get(b, p[7])) {
            v = 5 + bool_get(b, 159);
        } else {
            v = 7 + 2 * bool_get(b, 165);
            v += bool_get(b, 145);
        }
    } else {
        int hi = bool_get(b, p[8]);
        int lo = bool_get(b, p[9 + hi]);
        int cat = 2 * hi + lo;
        v = 0;
        for (const uint8_t *t = cat_probs[cat]; *t; t++) v += v + bool_get(b, *t);
        v += 3 + (8 << cat);
    }
    return v;
}

static int get_coeffs(Bool *b, const uint8_t (*bands)[3][11], int ctx, const int *dq, int first, int *out)
{
    const uint8_t *p = bands[band_of[first]][ctx];
    for (int n = first; n < 16; n++) {
        if (!bool_get(b, p[0])) return n;
        while (!bool_get(b, p[1])) {
            p = bands[band_of[++n]][0];
            if (n == 16) return 16;
        }
        int v;
        if (!bool_get(b, p[2])) {
            v = 1;
            p = bands[band_of[n + 1]][1];
        } else {
            v = large_value(b, p);
            p = bands[band_of[n + 1]][2];
        }
        out[zigzag[n]] = clamp_int((bool_get(b, 128) ? -v : v) * dq[n > 0], -32768, 32767);
    }
    return 16;
}

static void inverse_wht(const int *in, int *out)
{
    int tmp[16];
    for (int i = 0; i < 4; i++) {
        int a0 = in[i] + in[12 + i];
        int a1 = in[4 + i] + in[8 + i];
        int a2 = in[4 + i] - in[8 + i];
        int a3 = in[i] - in[12 + i];
        tmp[i] = a0 + a1;
        tmp[8 + i] = a0 - a1;
        tmp[4 + i] = a3 + a2;
        tmp[12 + i] = a3 - a2;
    }
    for (int i = 0; i < 4; i++) {
        int dc = tmp[i * 4] + 3;
        int a0 = dc + tmp[3 + i * 4];
        int a1 = tmp[1 + i * 4] + tmp[2 + i * 4];
        int a2 = tmp[1 + i * 4] - tmp[2 + i * 4];
        int a3 = dc - tmp[3 + i * 4];
        out[0] = (a0 + a1) >> 3;
        out[16] = (a3 + a2) >> 3;
        out[32] = (a0 - a1) >> 3;
        out[48] = (a3 - a2) >> 3;
        out += 64;
    }
}

static inline int mul_sin(int a)
{
    return (int)(((int64_t)a * 20091) >> 16) + a;
}

static inline int mul_cos(int a)
{
    return (int)(((int64_t)a * 35468) >> 16);
}

static void inverse_dct_add(const int *in, uint8_t *dst)
{
    int tmp[16];
    int *t = tmp;
    for (int i = 0; i < 4; i++) {
        int a = in[i] + in[8 + i];
        int b = in[i] - in[8 + i];
        int c = mul_cos(in[4 + i]) - mul_sin(in[12 + i]);
        int d = mul_sin(in[4 + i]) + mul_cos(in[12 + i]);
        t[0] = a + d;
        t[1] = b + c;
        t[2] = b - c;
        t[3] = a - d;
        t += 4;
    }
    t = tmp;
    for (int i = 0; i < 4; i++) {
        int dc = t[0] + 4;
        int a = dc + t[8];
        int b = dc - t[8];
        int c = mul_cos(t[4]) - mul_sin(t[12]);
        int d = mul_sin(t[4]) + mul_cos(t[12]);
        dst[0] = clip8(dst[0] + ((a + d) >> 3));
        dst[1] = clip8(dst[1] + ((b + c) >> 3));
        dst[2] = clip8(dst[2] + ((b - c) >> 3));
        dst[3] = clip8(dst[3] + ((a - d) >> 3));
        t++;
        dst += BPS;
    }
}

#define AVG3(a, b, c) ((uint8_t)(((a) + 2 * (b) + (c) + 2) >> 2))
#define AVG2(a, b) ((uint8_t)(((a) + (b) + 1) >> 1))
#define PX(x, y) dst[(x) + (y) * BPS]

enum { M_DC, M_TM, M_VE, M_HE, M_LD, M_RD, M_VR, M_VL, M_HD, M_HU };

static void pred_dc(uint8_t *dst, int size, int have_top, int have_left)
{
    int shift = size == 16 ? 4 : 3;
    int sum = 0, v = 128;
    if (have_top)
        for (int i = 0; i < size; i++) sum += dst[i - BPS];
    if (have_left)
        for (int i = 0; i < size; i++) sum += dst[i * BPS - 1];
    if (have_top && have_left) v = (sum + size) >> (shift + 1);
    else if (have_top || have_left) v = (sum + size / 2) >> shift;
    for (int j = 0; j < size; j++) memset(dst + j * BPS, v, size);
}

static void pred_tm(uint8_t *dst, int size)
{
    const uint8_t *top = dst - BPS;
    int tl = top[-1];
    for (int y = 0; y < size; y++) {
        int l = dst[y * BPS - 1];
        for (int x = 0; x < size; x++) dst[y * BPS + x] = clip8(l + top[x] - tl);
    }
}

static void pred_v(uint8_t *dst, int size)
{
    for (int y = 0; y < size; y++) memcpy(dst + y * BPS, dst - BPS, size);
}

static void pred_h(uint8_t *dst, int size)
{
    for (int y = 0; y < size; y++) memset(dst + y * BPS, dst[y * BPS - 1], size);
}

static void pred_big(uint8_t *dst, int size, int mode, int have_top, int have_left)
{
    switch (mode) {
    case M_DC: pred_dc(dst, size, have_top, have_left); break;
    case M_TM: pred_tm(dst, size); break;
    case M_VE: pred_v(dst, size); break;
    default: pred_h(dst, size); break;
    }
}

static void pred_sub(uint8_t *dst, int mode)
{
    const uint8_t *t = dst - BPS;
    int X = t[-1], A = t[0], B = t[1], C = t[2], D = t[3], E = t[4], F = t[5], G = t[6], H = t[7];
    int I = dst[-1], J = dst[BPS - 1], K = dst[2 * BPS - 1], L = dst[3 * BPS - 1];
    switch (mode) {
    case M_DC: {
        int v = (A + B + C + D + I + J + K + L + 4) >> 3;
        for (int y = 0; y < 4; y++) memset(dst + y * BPS, v, 4);
        break;
    }
    case M_TM: {
        int left[4] = {I, J, K, L};
        int top[4] = {A, B, C, D};
        for (int y = 0; y < 4; y++)
            for (int x = 0; x < 4; x++) PX(x, y) = clip8(left[y] + top[x] - X);
        break;
    }
    case M_VE: {
        uint8_t v[4] = {AVG3(X, A, B), AVG3(A, B, C), AVG3(B, C, D), AVG3(C, D, E)};
        for (int y = 0; y < 4; y++) memcpy(dst + y * BPS, v, 4);
        break;
    }
    case M_HE: {
        uint8_t v[4] = {AVG3(X, I, J), AVG3(I, J, K), AVG3(J, K, L), AVG3(K, L, L)};
        for (int y = 0; y < 4; y++) memset(dst + y * BPS, v[y], 4);
        break;
    }
    case M_LD:
        PX(0, 0) = AVG3(A, B, C);
        PX(1, 0) = PX(0, 1) = AVG3(B, C, D);
        PX(2, 0) = PX(1, 1) = PX(0, 2) = AVG3(C, D, E);
        PX(3, 0) = PX(2, 1) = PX(1, 2) = PX(0, 3) = AVG3(D, E, F);
        PX(3, 1) = PX(2, 2) = PX(1, 3) = AVG3(E, F, G);
        PX(3, 2) = PX(2, 3) = AVG3(F, G, H);
        PX(3, 3) = AVG3(G, H, H);
        break;
    case M_RD:
        PX(0, 3) = AVG3(J, K, L);
        PX(1, 3) = PX(0, 2) = AVG3(I, J, K);
        PX(2, 3) = PX(1, 2) = PX(0, 1) = AVG3(X, I, J);
        PX(3, 3) = PX(2, 2) = PX(1, 1) = PX(0, 0) = AVG3(A, X, I);
        PX(3, 2) = PX(2, 1) = PX(1, 0) = AVG3(B, A, X);
        PX(3, 1) = PX(2, 0) = AVG3(C, B, A);
        PX(3, 0) = AVG3(D, C, B);
        break;
    case M_VR:
        PX(0, 0) = PX(1, 2) = AVG2(X, A);
        PX(1, 0) = PX(2, 2) = AVG2(A, B);
        PX(2, 0) = PX(3, 2) = AVG2(B, C);
        PX(3, 0) = AVG2(C, D);
        PX(0, 3) = AVG3(K, J, I);
        PX(0, 2) = AVG3(J, I, X);
        PX(0, 1) = PX(1, 3) = AVG3(I, X, A);
        PX(1, 1) = PX(2, 3) = AVG3(X, A, B);
        PX(2, 1) = PX(3, 3) = AVG3(A, B, C);
        PX(3, 1) = AVG3(B, C, D);
        break;
    case M_VL:
        PX(0, 0) = AVG2(A, B);
        PX(1, 0) = PX(0, 2) = AVG2(B, C);
        PX(2, 0) = PX(1, 2) = AVG2(C, D);
        PX(3, 0) = PX(2, 2) = AVG2(D, E);
        PX(0, 1) = AVG3(A, B, C);
        PX(1, 1) = PX(0, 3) = AVG3(B, C, D);
        PX(2, 1) = PX(1, 3) = AVG3(C, D, E);
        PX(3, 1) = PX(2, 3) = AVG3(D, E, F);
        PX(3, 2) = AVG3(E, F, G);
        PX(3, 3) = AVG3(F, G, H);
        break;
    case M_HD:
        PX(0, 0) = PX(2, 1) = AVG2(I, X);
        PX(0, 1) = PX(2, 2) = AVG2(J, I);
        PX(0, 2) = PX(2, 3) = AVG2(K, J);
        PX(0, 3) = AVG2(L, K);
        PX(3, 0) = AVG3(A, B, C);
        PX(2, 0) = AVG3(X, A, B);
        PX(1, 0) = PX(3, 1) = AVG3(I, X, A);
        PX(1, 1) = PX(3, 2) = AVG3(J, I, X);
        PX(1, 2) = PX(3, 3) = AVG3(K, J, I);
        PX(1, 3) = AVG3(L, K, J);
        break;
    default:
        PX(0, 0) = AVG2(I, J);
        PX(2, 0) = PX(0, 1) = AVG2(J, K);
        PX(2, 1) = PX(0, 2) = AVG2(K, L);
        PX(1, 0) = AVG3(I, J, K);
        PX(3, 0) = PX(1, 1) = AVG3(J, K, L);
        PX(3, 1) = PX(1, 2) = AVG3(K, L, L);
        PX(3, 2) = PX(2, 2) = PX(0, 3) = PX(1, 3) = PX(2, 3) = PX(3, 3) = L;
        break;
    }
}

static const int8_t bmode_tree[18] = {0, 2, -1, 4, -2, 6, 8, 12, -3, 10, -5, -6, -4, 14, -7, 16, -8, -9};

static int read_sub_mode(Bool *b, const uint8_t *probs)
{
    int i = 0;
    do {
        i = bmode_tree[i + bool_get(b, probs[i >> 1])];
    } while (i > 0);
    return -i;
}

static inline int sclip1(int v)
{
    return v < -128 ? -128 : v > 127 ? 127 : v;
}

static inline int sclip2(int v)
{
    return v < -16 ? -16 : v > 15 ? 15 : v;
}

static inline int iabs(int v)
{
    return v < 0 ? -v : v;
}

static void filter_two(uint8_t *p, int step)
{
    int p1 = p[-2 * step], p0 = p[-step], q0 = p[0], q1 = p[step];
    int a = 3 * (q0 - p0) + sclip1(p1 - q1);
    int a1 = sclip2((a + 4) >> 3);
    int a2 = sclip2((a + 3) >> 3);
    p[-step] = clip8(p0 + a2);
    p[0] = clip8(q0 - a1);
}

static void filter_four(uint8_t *p, int step)
{
    int p1 = p[-2 * step], p0 = p[-step], q0 = p[0], q1 = p[step];
    int a = 3 * (q0 - p0);
    int a1 = sclip2((a + 4) >> 3);
    int a2 = sclip2((a + 3) >> 3);
    int a3 = (a1 + 1) >> 1;
    p[-2 * step] = clip8(p1 + a3);
    p[-step] = clip8(p0 + a2);
    p[0] = clip8(q0 - a1);
    p[step] = clip8(q1 - a3);
}

static void filter_six(uint8_t *p, int step)
{
    int p2 = p[-3 * step], p1 = p[-2 * step], p0 = p[-step];
    int q0 = p[0], q1 = p[step], q2 = p[2 * step];
    int a = sclip1(3 * (q0 - p0) + sclip1(p1 - q1));
    int a1 = (27 * a + 63) >> 7;
    int a2 = (18 * a + 63) >> 7;
    int a3 = (9 * a + 63) >> 7;
    p[-3 * step] = clip8(p2 + a3);
    p[-2 * step] = clip8(p1 + a2);
    p[-step] = clip8(p0 + a1);
    p[0] = clip8(q0 - a1);
    p[step] = clip8(q1 - a2);
    p[2 * step] = clip8(q2 - a3);
}

static int is_hev(const uint8_t *p, int step, int thresh)
{
    return iabs(p[-2 * step] - p[-step]) > thresh || iabs(p[step] - p[0]) > thresh;
}

static int needs_filter(const uint8_t *p, int step, int t, int it)
{
    int p3 = p[-4 * step], p2 = p[-3 * step], p1 = p[-2 * step], p0 = p[-step];
    int q0 = p[0], q1 = p[step], q2 = p[2 * step], q3 = p[3 * step];
    if (4 * iabs(p0 - q0) + iabs(p1 - q1) > t) return 0;
    return iabs(p3 - p2) <= it && iabs(p2 - p1) <= it && iabs(p1 - p0) <= it && iabs(q3 - q2) <= it &&
           iabs(q2 - q1) <= it && iabs(q1 - q0) <= it;
}

static void loop_simple(uint8_t *p, int across, int along, int count, int thresh)
{
    int t2 = 2 * thresh + 1;
    for (int i = 0; i < count; i++, p += along)
        if (4 * iabs(p[-across] - p[0]) + iabs(p[-2 * across] - p[across]) <= t2) filter_two(p, across);
}

static void loop_edge(uint8_t *p, int across, int along, int count, int thresh, int ithresh, int hev, int mb_edge)
{
    int t2 = 2 * thresh + 1;
    for (int i = 0; i < count; i++, p += along) {
        if (!needs_filter(p, across, t2, ithresh)) continue;
        if (is_hev(p, across, hev)) filter_two(p, across);
        else if (mb_edge) filter_six(p, across);
        else filter_four(p, across);
    }
}

static void filter_frame(Vp8 *s)
{
    int ys = s->ys, uvs = s->uvs;
    for (int my = 0; my < s->mbh; my++) {
        for (int mx = 0; mx < s->mbw; mx++) {
            const MbFilter *f = &s->mf[my * s->mbw + mx];
            if (!f->limit) continue;
            uint8_t *yp = s->y + (size_t)my * 16 * ys + mx * 16;
            uint8_t *up = s->u + (size_t)my * 8 * uvs + mx * 8;
            uint8_t *vp = s->v + (size_t)my * 8 * uvs + mx * 8;
            int limit = f->limit;
            if (s->filter_type == 1) {
                if (mx > 0) loop_simple(yp, 1, ys, 16, limit + 4);
                if (f->inner)
                    for (int k = 4; k < 16; k += 4) loop_simple(yp + k, 1, ys, 16, limit);
                if (my > 0) loop_simple(yp, ys, 1, 16, limit + 4);
                if (f->inner)
                    for (int k = 4; k < 16; k += 4) loop_simple(yp + k * ys, ys, 1, 16, limit);
                continue;
            }
            int il = f->ilevel, hev = f->hev;
            if (mx > 0) {
                loop_edge(yp, 1, ys, 16, limit + 4, il, hev, 1);
                loop_edge(up, 1, uvs, 8, limit + 4, il, hev, 1);
                loop_edge(vp, 1, uvs, 8, limit + 4, il, hev, 1);
            }
            if (f->inner) {
                for (int k = 4; k < 16; k += 4) loop_edge(yp + k, 1, ys, 16, limit, il, hev, 0);
                loop_edge(up + 4, 1, uvs, 8, limit, il, hev, 0);
                loop_edge(vp + 4, 1, uvs, 8, limit, il, hev, 0);
            }
            if (my > 0) {
                loop_edge(yp, ys, 1, 16, limit + 4, il, hev, 1);
                loop_edge(up, uvs, 1, 8, limit + 4, il, hev, 1);
                loop_edge(vp, uvs, 1, 8, limit + 4, il, hev, 1);
            }
            if (f->inner) {
                for (int k = 4; k < 16; k += 4) loop_edge(yp + k * ys, ys, 1, 16, limit, il, hev, 0);
                loop_edge(up + 4 * uvs, uvs, 1, 8, limit, il, hev, 0);
                loop_edge(vp + 4 * uvs, uvs, 1, 8, limit, il, hev, 0);
            }
        }
    }
}

static void load_edges(const Vp8 *s, int mx, int my, uint8_t *Y, uint8_t *U, uint8_t *V)
{
    int x0 = mx * 16, y0 = my * 16, cx = mx * 8, cy = my * 8;
    for (int j = 0; j < 16; j++) Y[j * BPS - 1] = mx ? s->y[(size_t)(y0 + j) * s->ys + x0 - 1] : 129;
    for (int j = 0; j < 8; j++) {
        U[j * BPS - 1] = mx ? s->u[(size_t)(cy + j) * s->uvs + cx - 1] : 129;
        V[j * BPS - 1] = mx ? s->v[(size_t)(cy + j) * s->uvs + cx - 1] : 129;
    }
    if (my == 0) {
        memset(Y - BPS - 1, 127, 21);
        memset(U - BPS - 1, 127, 9);
        memset(V - BPS - 1, 127, 9);
        return;
    }
    const uint8_t *ty = s->y + (size_t)(y0 - 1) * s->ys + x0;
    const uint8_t *tu = s->u + (size_t)(cy - 1) * s->uvs + cx;
    const uint8_t *tv = s->v + (size_t)(cy - 1) * s->uvs + cx;
    memcpy(Y - BPS, ty, 16);
    memcpy(U - BPS, tu, 8);
    memcpy(V - BPS, tv, 8);
    Y[-BPS - 1] = mx ? ty[-1] : 129;
    U[-BPS - 1] = mx ? tu[-1] : 129;
    V[-BPS - 1] = mx ? tv[-1] : 129;
    if (mx < s->mbw - 1) memcpy(Y - BPS + 16, ty + 16, 4);
    else memset(Y - BPS + 16, ty[15], 4);
}

static void store_block(uint8_t *plane, int stride, const uint8_t *src, int size)
{
    for (int j = 0; j < size; j++) memcpy(plane + (size_t)j * stride, src + j * BPS, size);
}

static int decode_frame(Vp8 *s, Bool *hb, Bool *parts)
{
    int mbw = s->mbw;
    uint8_t *top_nz = calloc((size_t)mbw, 9);
    uint8_t *top_modes = calloc((size_t)mbw, 4);
    if (!top_nz || !top_modes) {
        free(top_nz);
        free(top_modes);
        return 0;
    }
    uint8_t ybuf[BPS * 18], ubuf[BPS * 10], vbuf[BPS * 10];
    uint8_t *Y = ybuf + BPS + 8, *U = ubuf + BPS + 8, *V = vbuf + BPS + 8;
    int ok = 1;
    for (int my = 0; my < s->mbh && ok; my++) {
        Bool *tb = &parts[my & (s->nparts - 1)];
        uint8_t left_nz[9] = {0};
        uint8_t left_modes[4] = {0};
        for (int mx = 0; mx < mbw; mx++) {
            uint8_t *tn = top_nz + mx * 9;
            uint8_t *tm = top_modes + mx * 4;
            int seg = 0;
            if (s->seg_map) seg = bool_get(hb, s->seg_p[0]) ? 2 + bool_get(hb, s->seg_p[2]) : bool_get(hb, s->seg_p[1]);
            int skip = s->skip_on ? bool_get(hb, s->skip_p) : 0;
            int is4 = !bool_get(hb, 145);
            int ymode = M_DC;
            uint8_t sub[16];
            if (!is4) {
                if (bool_get(hb, 156)) ymode = bool_get(hb, 128) ? M_TM : M_HE;
                else ymode = bool_get(hb, 163) ? M_VE : M_DC;
                memset(tm, ymode, 4);
                memset(left_modes, ymode, 4);
            } else {
                for (int by = 0; by < 4; by++)
                    for (int bx = 0; bx < 4; bx++) {
                        int m = read_sub_mode(hb, kf_bmode_probs + (tm[bx] * 10 + left_modes[by]) * 9);
                        sub[by * 4 + bx] = m;
                        tm[bx] = left_modes[by] = m;
                    }
            }
            int uvmode = M_DC;
            if (bool_get(hb, 142)) {
                if (!bool_get(hb, 114)) uvmode = M_VE;
                else uvmode = bool_get(hb, 183) ? M_TM : M_HE;
            }

            int coef[24 * 16];
            uint8_t has[24];
            memset(coef, 0, sizeof(coef));
            memset(has, 0, sizeof(has));
            int any = 0;
            if (!skip) {
                int first = 0, type = 3;
                if (!is4) {
                    int dc[16] = {0};
                    int nz = get_coeffs(tb, s->coef[1], tn[8] + left_nz[8], s->y2[seg], 0, dc);
                    tn[8] = left_nz[8] = nz > 0;
                    if (nz > 1) {
                        inverse_wht(dc, coef);
                    } else {
                        int dc0 = (dc[0] + 3) >> 3;
                        for (int i = 0; i < 16; i++) coef[i * 16] = dc0;
                    }
                    first = 1;
                    type = 0;
                }
                for (int by = 0; by < 4; by++)
                    for (int bx = 0; bx < 4; bx++) {
                        int blk = by * 4 + bx;
                        int nz = get_coeffs(tb, s->coef[type], tn[bx] + left_nz[by], s->y1[seg], first, coef + blk * 16);
                        tn[bx] = left_nz[by] = nz > first;
                        has[blk] = nz > 1 || coef[blk * 16] != 0;
                        any |= has[blk];
                    }
                for (int pl = 0; pl < 2; pl++)
                    for (int by = 0; by < 2; by++)
                        for (int bx = 0; bx < 2; bx++) {
                            int blk = 16 + pl * 4 + by * 2 + bx;
                            uint8_t *t = &tn[4 + pl * 2 + bx], *l = &left_nz[4 + pl * 2 + by];
                            int nz = get_coeffs(tb, s->coef[2], *t + *l, s->uv[seg], 0, coef + blk * 16);
                            *t = *l = nz > 0;
                            has[blk] = nz > 1 || coef[blk * 16] != 0;
                            any |= has[blk];
                        }
            } else {
                memset(tn, 0, 8);
                memset(left_nz, 0, 8);
                if (!is4) tn[8] = left_nz[8] = 0;
            }

            load_edges(s, mx, my, Y, U, V);
            if (!is4) {
                pred_big(Y, 16, ymode, my > 0, mx > 0);
                for (int blk = 0; blk < 16; blk++)
                    if (has[blk]) inverse_dct_add(coef + blk * 16, Y + (blk & 3) * 4 + (blk >> 2) * 4 * BPS);
            } else {
                for (int k = 1; k < 4; k++) memcpy(Y + (4 * k - 1) * BPS + 16, Y - BPS + 16, 4);
                for (int blk = 0; blk < 16; blk++) {
                    uint8_t *dst = Y + (blk & 3) * 4 + (blk >> 2) * 4 * BPS;
                    pred_sub(dst, sub[blk]);
                    if (has[blk]) inverse_dct_add(coef + blk * 16, dst);
                }
            }
            pred_big(U, 8, uvmode, my > 0, mx > 0);
            pred_big(V, 8, uvmode, my > 0, mx > 0);
            for (int b = 0; b < 4; b++) {
                int off = (b & 1) * 4 + (b >> 1) * 4 * BPS;
                if (has[16 + b]) inverse_dct_add(coef + (16 + b) * 16, U + off);
                if (has[20 + b]) inverse_dct_add(coef + (20 + b) * 16, V + off);
            }
            store_block(s->y + (size_t)my * 16 * s->ys + mx * 16, s->ys, Y, 16);
            store_block(s->u + (size_t)my * 8 * s->uvs + mx * 8, s->uvs, U, 8);
            store_block(s->v + (size_t)my * 8 * s->uvs + mx * 8, s->uvs, V, 8);

            if (s->filter_type) {
                const Strength *st = &s->strength[seg][is4];
                MbFilter *f = &s->mf[my * mbw + mx];
                f->limit = st->limit;
                f->ilevel = st->ilevel;
                f->hev = st->hev;
                f->inner = is4 || any;
            }
        }
        if (hb->over > 8) ok = 0;
    }
    free(top_nz);
    free(top_modes);
    return ok;
}

static void yuv_pixel(uint32_t *out, int y, int u, int v, int alpha)
{
    int ym = (y * 19077) >> 8;
    int r = ym + ((v * 26149) >> 8) - 14234;
    int g = ym - ((u * 6419) >> 8) - ((v * 13320) >> 8) + 8708;
    int b = ym + ((u * 33050) >> 8) - 17685;
    r = (r & ~16383) == 0 ? r >> 6 : r < 0 ? 0 : 255;
    g = (g & ~16383) == 0 ? g >> 6 : g < 0 ? 0 : 255;
    b = (b & ~16383) == 0 ? b >> 6 : b < 0 ? 0 : 255;
    *out = (uint32_t)alpha << 24 | (uint32_t)r << 16 | (uint32_t)g << 8 | (uint32_t)b;
}

static void upsample_rows(const uint8_t *ty, const uint8_t *by, const uint8_t *tu, const uint8_t *tv, const uint8_t *cu,
                          const uint8_t *cv, uint32_t *top, uint32_t *bottom, int w, const uint8_t *ta, const uint8_t *ba)
{
    int tlu = tu[0], tlv = tv[0], lu = cu[0], lv = cv[0];
    yuv_pixel(top, ty[0], (3 * tlu + lu + 2) >> 2, (3 * tlv + lv + 2) >> 2, ta ? ta[0] : 255);
    if (by) yuv_pixel(bottom, by[0], (3 * lu + tlu + 2) >> 2, (3 * lv + tlv + 2) >> 2, ba ? ba[0] : 255);
    for (int x = 1; x <= (w - 1) >> 1; x++) {
        int tu1 = tu[x], tv1 = tv[x], cu1 = cu[x], cv1 = cv[x];
        int avg_u = tlu + tu1 + lu + cu1 + 8;
        int avg_v = tlv + tv1 + lv + cv1 + 8;
        int d12u = (avg_u + 2 * (tu1 + lu)) >> 3, d12v = (avg_v + 2 * (tv1 + lv)) >> 3;
        int d03u = (avg_u + 2 * (tlu + cu1)) >> 3, d03v = (avg_v + 2 * (tlv + cv1)) >> 3;
        int i = 2 * x - 1;
        yuv_pixel(top + i, ty[i], (d12u + tlu) >> 1, (d12v + tlv) >> 1, ta ? ta[i] : 255);
        yuv_pixel(top + i + 1, ty[i + 1], (d03u + tu1) >> 1, (d03v + tv1) >> 1, ta ? ta[i + 1] : 255);
        if (by) {
            yuv_pixel(bottom + i, by[i], (d03u + lu) >> 1, (d03v + lv) >> 1, ba ? ba[i] : 255);
            yuv_pixel(bottom + i + 1, by[i + 1], (d12u + cu1) >> 1, (d12v + cv1) >> 1, ba ? ba[i + 1] : 255);
        }
        tlu = tu1;
        tlv = tv1;
        lu = cu1;
        lv = cv1;
    }
    if (!(w & 1)) {
        int i = w - 1;
        yuv_pixel(top + i, ty[i], (3 * tlu + lu + 2) >> 2, (3 * tlv + lv + 2) >> 2, ta ? ta[i] : 255);
        if (by) yuv_pixel(bottom + i, by[i], (3 * lu + tlu + 2) >> 2, (3 * lv + tlv + 2) >> 2, ba ? ba[i] : 255);
    }
}

static uint32_t *convert_frame(const Vp8 *s, const uint8_t *alpha)
{
    int w = s->w, h = s->h;
    uint32_t *out = malloc((size_t)w * h * sizeof(uint32_t));
    if (!out) return NULL;
    #define ROW(plane, stride, r) ((plane) + (size_t)(r) * (stride))
    #define ALPHA_ROW(r) (alpha ? alpha + (size_t)(r) * w : NULL)
    upsample_rows(ROW(s->y, s->ys, 0), NULL, s->u, s->v, s->u, s->v, out, NULL, w, ALPHA_ROW(0), NULL);
    int y = 1;
    for (; y + 1 < h; y += 2) {
        int k = (y - 1) / 2;
        upsample_rows(ROW(s->y, s->ys, y), ROW(s->y, s->ys, y + 1), ROW(s->u, s->uvs, k), ROW(s->v, s->uvs, k),
                      ROW(s->u, s->uvs, k + 1), ROW(s->v, s->uvs, k + 1), out + (size_t)y * w, out + (size_t)(y + 1) * w, w,
                      ALPHA_ROW(y), ALPHA_ROW(y + 1));
    }
    if (y < h) {
        int k = (y - 1) / 2;
        upsample_rows(ROW(s->y, s->ys, y), NULL, ROW(s->u, s->uvs, k), ROW(s->v, s->uvs, k), ROW(s->u, s->uvs, k),
                      ROW(s->v, s->uvs, k), out + (size_t)y * w, NULL, w, ALPHA_ROW(y), NULL);
    }
    #undef ROW
    #undef ALPHA_ROW
    return out;
}

static uint32_t *vp8_decode(const uint8_t *d, size_t n, const uint8_t *alpha, int *outw, int *outh)
{
    int w, h;
    if (!vp8_info(d, n, &w, &h)) return NULL;
    uint32_t tag = d[0] | (uint32_t)d[1] << 8 | (uint32_t)d[2] << 16;
    size_t first = tag >> 5;
    if (first > n - 10) return NULL;
    Vp8 *s = calloc(1, sizeof(*s));
    if (!s) return NULL;
    s->w = w;
    s->h = h;
    s->mbw = (w + 15) >> 4;
    s->mbh = (h + 15) >> 4;
    s->ys = s->mbw * 16;
    s->uvs = s->mbw * 8;
    Bool hb;
    bool_init(&hb, d + 10, first);
    int base_q;
    read_header(s, &hb, &base_q);
    uint32_t *result = NULL;
    Bool parts[8];
    const uint8_t *p = d + 10 + first;
    size_t left = n - 10 - first;
    size_t table = 3 * (size_t)(s->nparts - 1);
    if (left < table || hb.over > 8) goto done;
    const uint8_t *data = p + table;
    left -= table;
    for (int i = 0; i < s->nparts - 1; i++) {
        size_t sz = p[3 * i] | (size_t)p[3 * i + 1] << 8 | (size_t)p[3 * i + 2] << 16;
        if (sz > left) sz = left;
        bool_init(&parts[i], data, sz);
        data += sz;
        left -= sz;
    }
    bool_init(&parts[s->nparts - 1], data, left);
    s->y = calloc((size_t)s->ys, (size_t)s->mbh * 16);
    s->u = calloc((size_t)s->uvs, (size_t)s->mbh * 8);
    s->v = calloc((size_t)s->uvs, (size_t)s->mbh * 8);
    s->mf = calloc((size_t)s->mbw * s->mbh, sizeof(MbFilter));
    if (!s->y || !s->u || !s->v || !s->mf) goto done;
    if (!decode_frame(s, &hb, parts)) goto done;
    if (s->filter_type) filter_frame(s);
    result = convert_frame(s, alpha);
    *outw = w;
    *outh = h;
done:
    free(s->y);
    free(s->u);
    free(s->v);
    free(s->mf);
    free(s);
    return result;
}

typedef struct Block {
    struct Block *next;
    uint64_t pad;
} Block;

typedef struct {
    Block *head;
} Arena;

static void *arena_alloc(Arena *a, size_t n, int zero)
{
    if (n > ((size_t)1 << 28)) return NULL;
    Block *b = malloc(sizeof(Block) + n);
    if (!b) return NULL;
    b->next = a->head;
    a->head = b;
    void *p = b + 1;
    if (zero) memset(p, 0, n);
    return p;
}

static void arena_free(Arena *a)
{
    while (a->head) {
        Block *b = a->head;
        a->head = b->next;
        free(b);
    }
}

typedef struct {
    const uint8_t *p, *end;
    uint64_t val;
    int bits;
    uint64_t used, total;
} LBits;

static void lb_init(LBits *b, const uint8_t *p, size_t n)
{
    b->p = p;
    b->end = p + n;
    b->val = 0;
    b->bits = 0;
    b->used = 0;
    b->total = (uint64_t)n * 8;
}

static void lb_fill(LBits *b)
{
    while (b->bits <= 56) {
        if (b->p < b->end) b->val |= (uint64_t)*b->p++ << b->bits;
        b->bits += 8;
    }
}

static uint32_t lb_get(LBits *b, int n)
{
    if (n == 0) return 0;
    if (b->bits < n) lb_fill(b);
    uint32_t v = (uint32_t)(b->val & (((uint64_t)1 << n) - 1));
    b->val >>= n;
    b->bits -= n;
    b->used += n;
    return v;
}

static int lb_overrun(const LBits *b)
{
    return b->used > b->total;
}

#define FAST_BITS 8

typedef struct {
    uint16_t fast[1 << FAST_BITS];
    uint16_t count[16];
    uint16_t *syms;
    int single;
    int sym0;
} Huff;

static int huff_build(Arena *a, Huff *h, const uint8_t *lens, int n)
{
    memset(h, 0, sizeof(*h));
    int used = 0, last = 0;
    for (int i = 0; i < n; i++)
        if (lens[i]) {
            h->count[lens[i]]++;
            used++;
            last = i;
        }
    if (used == 0) return 0;
    if (used == 1) {
        h->single = 1;
        h->sym0 = last;
        return 1;
    }
    int left = 1;
    for (int len = 1; len < 16; len++) {
        left <<= 1;
        left -= h->count[len];
        if (left < 0) return 0;
    }
    if (left != 0) return 0;
    h->syms = arena_alloc(a, sizeof(uint16_t) * (size_t)used, 0);
    if (!h->syms) return 0;
    uint16_t offs[17];
    offs[1] = 0;
    for (int len = 1; len < 16; len++) offs[len + 1] = offs[len] + h->count[len];
    uint32_t next_code[17];
    uint32_t code = 0;
    for (int len = 1; len < 16; len++) {
        next_code[len] = code;
        code = (code + h->count[len]) << 1;
    }
    for (int i = 0; i < n; i++) {
        int len = lens[i];
        if (!len) continue;
        h->syms[offs[len]++] = i;
        uint32_t c = next_code[len]++;
        if (len <= FAST_BITS) {
            uint32_t rev = 0;
            for (int k = 0; k < len; k++) rev |= ((c >> k) & 1) << (len - 1 - k);
            for (uint32_t idx = rev; idx < (1u << FAST_BITS); idx += 1u << len) h->fast[idx] = (uint16_t)(len << 12 | i);
        }
    }
    return 1;
}

static int huff_read(LBits *b, const Huff *h)
{
    if (h->single) return h->sym0;
    lb_fill(b);
    uint32_t e = h->fast[b->val & ((1u << FAST_BITS) - 1)];
    if (e) {
        int len = e >> 12;
        b->val >>= len;
        b->bits -= len;
        b->used += len;
        return e & 0xfff;
    }
    int code = 0, first = 0, index = 0;
    uint64_t v = b->val;
    for (int len = 1; len < 16; len++) {
        code |= (int)((v >> (len - 1)) & 1);
        int count = h->count[len];
        if (code - count < first) {
            b->val >>= len;
            b->bits -= len;
            b->used += len;
            return h->syms[index + (code - first)];
        }
        index += count;
        first += count;
        first <<= 1;
        code <<= 1;
    }
    return -1;
}

typedef struct {
    Huff h[5];
} Group;

static const uint8_t code_length_order[19] = {17, 18, 0, 1, 2, 3, 4, 5, 16, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};

static int read_code(LBits *b, Arena *a, Huff *h, int alpha_size)
{
    uint8_t lens[256 + 24 + 2048];
    memset(lens, 0, (size_t)alpha_size);
    if (lb_get(b, 1)) {
        int two = (int)lb_get(b, 1);
        int wide = (int)lb_get(b, 1);
        int s0 = (int)lb_get(b, 1 + 7 * wide);
        if (s0 >= alpha_size) return 0;
        lens[s0] = 1;
        if (two) {
            int s1 = (int)lb_get(b, 8);
            if (s1 >= alpha_size) return 0;
            lens[s1] = 1;
        }
    } else {
        int count = 4 + (int)lb_get(b, 4);
        uint8_t cl[19];
        memset(cl, 0, sizeof(cl));
        for (int i = 0; i < count; i++) cl[code_length_order[i]] = (uint8_t)lb_get(b, 3);
        Huff ch;
        if (!huff_build(a, &ch, cl, 19)) return 0;
        int max_symbol = alpha_size;
        if (lb_get(b, 1)) {
            int nbits = 2 + 2 * (int)lb_get(b, 3);
            max_symbol = 2 + (int)lb_get(b, nbits);
            if (max_symbol > alpha_size) return 0;
        }
        int sym = 0, prev = 8;
        while (sym < alpha_size) {
            if (max_symbol-- == 0) break;
            int c = huff_read(b, &ch);
            if (c < 0 || lb_overrun(b)) return 0;
            if (c < 16) {
                lens[sym++] = (uint8_t)c;
                if (c) prev = c;
            } else {
                int rep, val = 0;
                if (c == 16) {
                    rep = 3 + (int)lb_get(b, 2);
                    val = prev;
                } else if (c == 17) {
                    rep = 3 + (int)lb_get(b, 3);
                } else {
                    rep = 11 + (int)lb_get(b, 7);
                }
                if (sym + rep > alpha_size) return 0;
                while (rep-- > 0) lens[sym++] = (uint8_t)val;
            }
        }
    }
    if (lb_overrun(b)) return 0;
    return huff_build(a, h, lens, alpha_size);
}

static uint32_t add_px(uint32_t a, uint32_t b)
{
    return (((a & 0xff00ff00u) + (b & 0xff00ff00u)) & 0xff00ff00u) | (((a & 0x00ff00ffu) + (b & 0x00ff00ffu)) & 0x00ff00ffu);
}

static uint32_t avg2(uint32_t a, uint32_t b)
{
    return (((a ^ b) & 0xfefefefeu) >> 1) + (a & b);
}

static int chan_clip(int v)
{
    return v < 0 ? 0 : v > 255 ? 255 : v;
}

static uint32_t select_px(uint32_t l, uint32_t t, uint32_t tl)
{
    int pl = 0, pt = 0;
    for (int s = 0; s < 32; s += 8) {
        int cl = (l >> s) & 255, ct = (t >> s) & 255, ctl = (tl >> s) & 255;
        pl += iabs(ct - ctl);
        pt += iabs(cl - ctl);
    }
    return pl < pt ? l : t;
}

static uint32_t clamp_full(uint32_t a, uint32_t b, uint32_t c)
{
    uint32_t out = 0;
    for (int s = 0; s < 32; s += 8) out |= (uint32_t)chan_clip((int)((a >> s) & 255) + (int)((b >> s) & 255) - (int)((c >> s) & 255)) << s;
    return out;
}

static uint32_t clamp_half(uint32_t a, uint32_t b)
{
    uint32_t out = 0;
    for (int s = 0; s < 32; s += 8) {
        int ca = (a >> s) & 255, cb = (b >> s) & 255;
        out |= (uint32_t)chan_clip(ca + (ca - cb) / 2) << s;
    }
    return out;
}

static uint32_t predict_px(int mode, uint32_t l, uint32_t t, uint32_t tr, uint32_t tl)
{
    switch (mode) {
    case 1: return l;
    case 2: return t;
    case 3: return tr;
    case 4: return tl;
    case 5: return avg2(avg2(l, tr), t);
    case 6: return avg2(l, tl);
    case 7: return avg2(l, t);
    case 8: return avg2(tl, t);
    case 9: return avg2(t, tr);
    case 10: return avg2(avg2(l, tl), avg2(t, tr));
    case 11: return select_px(l, t, tl);
    case 12: return clamp_full(l, t, tl);
    case 13: return clamp_half(avg2(l, t), tl);
    default: return 0xff000000u;
    }
}

static void inverse_predictor(uint32_t *d, int w, int h, int bits, const uint32_t *sub)
{
    int bw = (w + (1 << bits) - 1) >> bits;
    d[0] = add_px(d[0], 0xff000000u);
    for (int x = 1; x < w; x++) d[x] = add_px(d[x], d[x - 1]);
    for (int y = 1; y < h; y++) {
        uint32_t *row = d + (size_t)y * w;
        row[0] = add_px(row[0], row[-w]);
        for (int x = 1; x < w; x++) {
            int mode = (sub[(size_t)(y >> bits) * bw + (x >> bits)] >> 8) & 15;
            row[x] = add_px(row[x], predict_px(mode, row[x - 1], row[x - w], row[x - w + 1], row[x - w - 1]));
        }
    }
}

static int color_delta(int8_t t, int8_t c)
{
    return ((int)t * (int)c) >> 5;
}

static void inverse_cross_color(uint32_t *d, int w, int h, int bits, const uint32_t *sub)
{
    int bw = (w + (1 << bits) - 1) >> bits;
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            uint32_t m = sub[(size_t)(y >> bits) * bw + (x >> bits)];
            int8_t g2r = (int8_t)(m & 255), g2b = (int8_t)((m >> 8) & 255), r2b = (int8_t)((m >> 16) & 255);
            uint32_t px = d[(size_t)y * w + x];
            int green = (int8_t)((px >> 8) & 255);
            int red = (px >> 16) & 255, blue = px & 255;
            red = (red + color_delta(g2r, (int8_t)green)) & 255;
            blue = (blue + color_delta(g2b, (int8_t)green) + color_delta(r2b, (int8_t)red)) & 255;
            d[(size_t)y * w + x] = (px & 0xff00ff00u) | (uint32_t)red << 16 | (uint32_t)blue;
        }
}

static void inverse_add_green(uint32_t *d, size_t count)
{
    for (size_t i = 0; i < count; i++) {
        uint32_t px = d[i];
        uint32_t g = (px >> 8) & 255;
        uint32_t r = (((px >> 16) & 255) + g) & 255;
        uint32_t b = ((px & 255) + g) & 255;
        d[i] = (px & 0xff00ff00u) | r << 16 | b;
    }
}

static void inverse_indexing(uint32_t *d, int w, int h, int bits, const uint32_t *table)
{
    int per = 1 << bits, bpp = 8 >> bits, mask = (1 << bpp) - 1;
    size_t pw = ((size_t)w + per - 1) >> bits;
    for (int y = h - 1; y >= 0; y--)
        for (int x = w - 1; x >= 0; x--) {
            uint32_t g = (d[(size_t)y * pw + (x >> bits)] >> 8) & 255;
            d[(size_t)y * w + x] = table[(g >> ((x & (per - 1)) * bpp)) & mask];
        }
}

static int prefix_value(LBits *b, int prefix)
{
    if (prefix < 4) return prefix + 1;
    int extra = (prefix - 2) >> 1;
    int offset = (2 + (prefix & 1)) << extra;
    return offset + (int)lb_get(b, extra) + 1;
}

static uint32_t cache_slot(uint32_t px, int bits)
{
    return (px * 0x1e35a7bdu) >> (32 - bits);
}

static int decode_pixels(LBits *b, Arena *a, uint32_t *out, int xs, int ys, int cache_bits, const Group *groups,
                         const uint16_t *map, int map_bits, int map_w)
{
    size_t total = (size_t)xs * ys, pos = 0;
    int x = 0, y = 0;
    uint32_t *cache = NULL;
    if (cache_bits) {
        cache = arena_alloc(a, sizeof(uint32_t) << cache_bits, 1);
        if (!cache) return 0;
    }
    const Group *g = groups;
    while (pos < total) {
        if (map) g = &groups[map[(size_t)(y >> map_bits) * map_w + (x >> map_bits)]];
        int code = huff_read(b, &g->h[0]);
        if (code < 0 || lb_overrun(b)) return 0;
        uint32_t px;
        if (code < 256) {
            int r = huff_read(b, &g->h[1]);
            int bl = huff_read(b, &g->h[2]);
            int al = huff_read(b, &g->h[3]);
            if (r < 0 || bl < 0 || al < 0) return 0;
            px = (uint32_t)al << 24 | (uint32_t)r << 16 | (uint32_t)code << 8 | (uint32_t)bl;
        } else if (code < 280) {
            int len = prefix_value(b, code - 256);
            int dsym = huff_read(b, &g->h[4]);
            if (dsym < 0 || dsym >= 40) return 0;
            int dcode = prefix_value(b, dsym);
            int dist;
            if (dcode > 120) {
                dist = dcode - 120;
            } else {
                dist = distance_map[(dcode - 1) * 2] + distance_map[(dcode - 1) * 2 + 1] * xs;
                if (dist < 1) dist = 1;
            }
            if ((size_t)dist > pos || (size_t)len > total - pos || lb_overrun(b)) return 0;
            for (int i = 0; i < len; i++) {
                px = out[pos + i - dist];
                out[pos + i] = px;
                if (cache) cache[cache_slot(px, cache_bits)] = px;
            }
            pos += len;
            y = (int)(pos / xs);
            x = (int)(pos % xs);
            continue;
        } else {
            if (!cache) return 0;
            px = cache[code - 280];
        }
        out[pos++] = px;
        if (cache) cache[cache_slot(px, cache_bits)] = px;
        if (++x == xs) {
            x = 0;
            y++;
        }
    }
    return !lb_overrun(b);
}

typedef struct {
    int type, bits, xs;
    uint32_t *data;
    uint32_t table[256];
} Transform;

static uint32_t *decode_stream(LBits *b, Arena *a, int xsize, int ysize, int level0);

static uint32_t *read_sub_image(LBits *b, Arena *a, int xs, int ys, int bits, int *bw)
{
    *bw = (xs + (1 << bits) - 1) >> bits;
    int bh = (ys + (1 << bits) - 1) >> bits;
    return decode_stream(b, a, *bw, bh, 0);
}

static uint32_t *decode_stream(LBits *b, Arena *a, int xsize, int ysize, int level0)
{
    Transform tr[4];
    int ntr = 0, seen = 0, xs = xsize;
    if (level0) {
        while (lb_get(b, 1)) {
            if (ntr >= 4) return NULL;
            Transform *t = &tr[ntr];
            memset(t, 0, sizeof(*t));
            t->type = (int)lb_get(b, 2);
            if (seen & (1 << t->type)) return NULL;
            seen |= 1 << t->type;
            t->xs = xs;
            if (t->type == 0 || t->type == 1) {
                int bw;
                t->bits = 2 + (int)lb_get(b, 3);
                t->data = read_sub_image(b, a, xs, ysize, t->bits, &bw);
                if (!t->data) return NULL;
            } else if (t->type == 3) {
                int count = 1 + (int)lb_get(b, 8);
                uint32_t *tab = decode_stream(b, a, count, 1, 0);
                if (!tab) return NULL;
                for (int i = 1; i < count; i++) tab[i] = add_px(tab[i], tab[i - 1]);
                memcpy(t->table, tab, sizeof(uint32_t) * (size_t)count);
                t->bits = count <= 2 ? 3 : count <= 4 ? 2 : count <= 16 ? 1 : 0;
                xs = (xs + (1 << t->bits) - 1) >> t->bits;
            }
            ntr++;
        }
    }
    int cache_bits = 0;
    if (lb_get(b, 1)) {
        cache_bits = (int)lb_get(b, 4);
        if (cache_bits < 1 || cache_bits > 11) return NULL;
    }
    uint16_t *map = NULL;
    int map_bits = 0, map_w = 0, ngroups = 1, nused = 1;
    uint16_t *compact = NULL;
    if (level0 && lb_get(b, 1)) {
        map_bits = 2 + (int)lb_get(b, 3);
        uint32_t *meta = read_sub_image(b, a, xs, ysize, map_bits, &map_w);
        if (!meta) return NULL;
        size_t cells = (size_t)map_w * ((ysize + (1 << map_bits) - 1) >> map_bits);
        for (size_t i = 0; i < cells; i++) {
            int gi = (int)((meta[i] >> 8) & 0xffff);
            if (gi + 1 > ngroups) ngroups = gi + 1;
        }
        compact = arena_alloc(a, sizeof(uint16_t) * (size_t)ngroups, 0);
        map = arena_alloc(a, sizeof(uint16_t) * cells, 0);
        if (!compact || !map) return NULL;
        for (int i = 0; i < ngroups; i++) compact[i] = 0xffff;
        nused = 0;
        for (size_t i = 0; i < cells; i++) {
            int gi = (int)((meta[i] >> 8) & 0xffff);
            if (compact[gi] == 0xffff) {
                if (nused >= 4096) return NULL;
                compact[gi] = (uint16_t)nused++;
            }
            map[i] = compact[gi];
        }
    }
    Group *groups = arena_alloc(a, sizeof(Group) * (size_t)nused, 1);
    if (!groups) return NULL;
    int cache_size = cache_bits ? 1 << cache_bits : 0;
    int sizes[5] = {256 + 24 + cache_size, 256, 256, 256, 40};
    for (int gi = 0; gi < ngroups; gi++) {
        int slot = compact ? compact[gi] : 0;
        Arena scratch = {0};
        Group tmp;
        Group *g = slot == 0xffff ? &tmp : &groups[slot];
        Arena *use = slot == 0xffff ? &scratch : a;
        for (int k = 0; k < 5; k++) {
            if (!read_code(b, use, &g->h[k], sizes[k])) {
                arena_free(&scratch);
                return NULL;
            }
        }
        arena_free(&scratch);
    }
    size_t bytes = (size_t)xsize * ysize * sizeof(uint32_t);
    uint32_t *pix = level0 ? malloc(bytes) : arena_alloc(a, bytes, 0);
    if (!pix) return NULL;
    if (!decode_pixels(b, a, pix, xs, ysize, cache_bits, groups, map, map_bits, map_w)) {
        if (level0) free(pix);
        return NULL;
    }
    for (int i = ntr - 1; i >= 0; i--) {
        Transform *t = &tr[i];
        switch (t->type) {
        case 0: inverse_predictor(pix, t->xs, ysize, t->bits, t->data); break;
        case 1: inverse_cross_color(pix, t->xs, ysize, t->bits, t->data); break;
        case 2: inverse_add_green(pix, (size_t)t->xs * ysize); break;
        default: inverse_indexing(pix, t->xs, ysize, t->bits, t->table); break;
        }
    }
    return pix;
}

static uint32_t *vp8l_decode(const uint8_t *d, size_t n, int *w, int *h)
{
    if (n < 5 || d[0] != 0x2f) return NULL;
    LBits b;
    lb_init(&b, d + 1, n - 1);
    int xs = 1 + (int)lb_get(&b, 14);
    int ys = 1 + (int)lb_get(&b, 14);
    lb_get(&b, 1);
    if (lb_get(&b, 3) != 0) return NULL;
    if (xs > MAX_SIDE || ys > MAX_SIDE) return NULL;
    Arena a = {0};
    uint32_t *pix = decode_stream(&b, &a, xs, ys, 1);
    arena_free(&a);
    if (pix) {
        *w = xs;
        *h = ys;
    }
    return pix;
}

static uint8_t *alpha_decode(const uint8_t *d, size_t n, int w, int h)
{
    if (n < 1) return NULL;
    int compression = d[0] & 3, filter = (d[0] >> 2) & 3;
    size_t count = (size_t)w * h;
    uint8_t *out = malloc(count);
    if (!out) return NULL;
    if (compression == 0) {
        if (n - 1 < count) {
            free(out);
            return NULL;
        }
        memcpy(out, d + 1, count);
    } else if (compression == 1) {
        LBits b;
        lb_init(&b, d + 1, n - 1);
        Arena a = {0};
        uint32_t *pix = decode_stream(&b, &a, w, h, 1);
        arena_free(&a);
        if (!pix) {
            free(out);
            return NULL;
        }
        for (size_t i = 0; i < count; i++) out[i] = (uint8_t)(pix[i] >> 8);
        free(pix);
    } else {
        free(out);
        return NULL;
    }
    for (int y = 0; y < h; y++) {
        uint8_t *row = out + (size_t)y * w;
        const uint8_t *prev = y ? row - w : NULL;
        if (filter == 0) break;
        if (filter == 1 || !prev) {
            int pred = prev ? prev[0] : 0;
            for (int x = 0; x < w; x++) {
                row[x] = (uint8_t)(pred + row[x]);
                pred = row[x];
            }
        } else if (filter == 2) {
            for (int x = 0; x < w; x++) row[x] = (uint8_t)(row[x] + prev[x]);
        } else {
            int top = prev[0], tl = top, left = top;
            for (int x = 0; x < w; x++) {
                top = prev[x];
                left = (row[x] + chan_clip(left + top - tl)) & 255;
                tl = top;
                row[x] = (uint8_t)left;
            }
        }
    }
    return out;
}

typedef struct {
    const uint8_t *alph, *img;
    size_t alph_n, img_n;
    int lossless, anim;
} Found;

static void scan_chunks(const uint8_t *d, size_t n, Found *f, int depth)
{
    size_t pos = 0;
    while (n - pos >= 8) {
        uint32_t tag = le32(d + pos);
        size_t sz = le32(d + pos + 4);
        pos += 8;
        if (sz > n - pos) sz = n - pos;
        const uint8_t *p = d + pos;
        if (tag == FOURCC('V', 'P', '8', 'X')) {
            if (sz >= 1 && (p[0] & 2)) f->anim = 1;
        } else if (tag == FOURCC('A', 'L', 'P', 'H')) {
            if (!f->alph) {
                f->alph = p;
                f->alph_n = sz;
            }
        } else if (tag == FOURCC('V', 'P', '8', ' ') || tag == FOURCC('V', 'P', '8', 'L')) {
            if (!f->img) {
                f->img = p;
                f->img_n = sz;
                f->lossless = tag == FOURCC('V', 'P', '8', 'L');
            }
            return;
        } else if (tag == FOURCC('A', 'N', 'M', 'F') && depth == 0 && sz > 16) {
            scan_chunks(p + 16, sz - 16, f, 1);
            if (f->img) return;
        }
        pos += sz;
        if ((sz & 1) && pos < n) pos++;
    }
}

int smm_webp_check(const void *data, size_t n)
{
    const uint8_t *d = data;
    return n >= 16 && memcmp(d, "RIFF", 4) == 0 && memcmp(d + 8, "WEBP", 4) == 0;
}

uint32_t *smm_webp_decode_argb(const void *data, size_t n, int *w, int *h)
{
    const uint8_t *d = data;
    if (!smm_webp_check(d, n)) return NULL;
    size_t riff = (size_t)le32(d + 4) + 8;
    if (riff >= 12 && riff < n) n = riff;
    Found f;
    memset(&f, 0, sizeof(f));
    scan_chunks(d + 12, n - 12, &f, 0);
    if (!f.img) return NULL;
    if (f.lossless) return vp8l_decode(f.img, f.img_n, w, h);
    int vw, vh;
    if (!vp8_info(f.img, f.img_n, &vw, &vh)) return NULL;
    uint8_t *alpha = f.alph ? alpha_decode(f.alph, f.alph_n, vw, vh) : NULL;
    uint32_t *px = vp8_decode(f.img, f.img_n, alpha, w, h);
    free(alpha);
    return px;
}

int smm_webp_decode(const void *data, size_t n, goome_bitmap *out)
{
    int w = 0, h = 0;
    uint32_t *px = smm_webp_decode_argb(data, n, &w, &h);
    if (!px) return GOOME_EARG;
    size_t count = (size_t)w * h;
    for (size_t i = 0; i < count; i++) {
        uint32_t c = px[i], a = c >> 24;
        if (a != 255) {
            uint32_t r = (((c >> 16) & 255) * a + 127) / 255;
            uint32_t g = (((c >> 8) & 255) * a + 127) / 255;
            uint32_t b = ((c & 255) * a + 127) / 255;
            px[i] = a << 24 | r << 16 | g << 8 | b;
        }
    }
    out->px = px;
    out->w = w;
    out->h = h;
    out->stride = w;
    out->owned = 1;
    return GOOME_OK;
}
