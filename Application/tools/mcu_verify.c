/**
 * @file    mcu_verify.c
 * @brief   One-shot MCU preprocessing self-test — see mcu_verify.h.
 *
 * ============================================================================
 *  TEST VECTOR PROVENANCE
 * ============================================================================
 *  The mcu_verify_raw[][][] array below is a verbatim copy of test_raw[]
 *  from tools/test_vectors.h (5 vectors: 2 smooth + 3 rough, seed=42).
 *
 *  If gen_test_vectors.py is re-run with a different RANDOM_SEED, regenerate
 *  this array. Column order per sample: { ax, ay, az, gx, gy, gz }.
 *
 * ============================================================================
 *  PYTHON-SIDE PARSER (reference, for offline comparison — multi-block)
 * ============================================================================
 *
 *  import re, numpy as np
 *
 *  def parse_uart_log(text):
 *      blocks = []
 *      cur    = None
 *      feat_buf, ts_buf = {}, {}
 *      for line in text.splitlines():
 *          m = re.match(r'===VERIFY_START===,VEC=(\d+),LABEL=(\d+)', line)
 *          if m:
 *              cur = {'vec': int(m.group(1)), 'label': int(m.group(2)),
 *                     'features': [], 'stat_int8': [], 'ts_int8': [],
 *                     'inference': None}
 *              feat_buf, ts_buf = {}, {}
 *              continue
 *          if line.startswith('===VERIFY_END==='):
 *              for k in sorted(feat_buf): cur['features'].extend(feat_buf[k])
 *              for k in sorted(ts_buf):   cur['ts_int8'].extend(ts_buf[k])
 *              blocks.append(cur); cur = None
 *              continue
 *          if cur is None: continue
 *          m = re.match(r'FEATURES_(\d+),(.*)', line)
 *          if m:
 *              feat_buf[int(m.group(1))] = [float(x) for x in m.group(2).split(',')]
 *              continue
 *          m = re.match(r'STAT_INT8,(.*)', line)
 *          if m:
 *              cur['stat_int8'] = [int(x) for x in m.group(1).split(',')]
 *              continue
 *          m = re.match(r'TS_INT8_(\d+),(.*)', line)
 *          if m:
 *              ts_buf[int(m.group(1))] = [int(x) for x in m.group(2).split(',')]
 *              continue
 *          m = re.match(r'INFERENCE,(\d+),(\d+)', line)
 *          if m:
 *              cur['inference'] = (int(m.group(1)), int(m.group(2)))
 *      return blocks
 *
 *  # Compare each block against the matching reference vector:
 *  for blk in parse_uart_log(open('uart.log').read()):
 *      i = blk['vec']
 *      np.testing.assert_allclose(blk['features'], expected_features[i],
 *                                 rtol=1e-3, atol=1e-5)
 *      assert blk['ts_int8'] == list(expected_ts_int8[i])
 *      # NOTE: expected_stat_int8 must be added to gen_test_vectors.py
 *      #       for full STAT_INT8 verification.
 *
 * ============================================================================
 */

#include <stdint.h>
#include <stdio.h>      /* snprintf */
#include <math.h>       /* isnan, isinf, nearbyint */

#include "STD_TYPES.h"
#include "norm_params.h"
#include "scale.h"
#include "features.h"
#include "quantize.h"
#include "inference.h"
#include "STD_BUFFER.h"
#include "UART_SERVICE.h"
#include "mcu_verify.h"

/*----------------------------------------------------------------------------*/
/*  Configuration                                                             */
/*----------------------------------------------------------------------------*/
#define MCU_VERIFY_N_VECTORS 5
#ifndef MCU_VERIFY_UART_ID
#define MCU_VERIFY_UART_ID  UART1_ID    /* override at build time if needed */
#endif

#define MCU_VERIFY_LINE_BUF_SIZE   768U   /* worst-case line: TS_INT8_x ~ 510B */
#define MCU_VERIFY_FLOAT_FRAC_DIG    8U   /* 8 fractional digits on FEATURES_x */
#define MCU_VERIFY_TX_TIMEOUT_TICKS  10000000UL  /* spin-loop safety bound */

/*----------------------------------------------------------------------------*/
/*  Hardcoded test vectors — test_raw[0..4] from tools/test_vectors.h         */
/*  (2 smooth + 3 rough, seed=42)                                             */
/*----------------------------------------------------------------------------*/

static const int16_t mcu_verify_raw[MCU_VERIFY_N_VECTORS][WINDOW_SIZE][N_FEATURES] =
{
    {   /* vec 0 — smooth */
        {   1623,    -56,   8707,     19,    -93,   -109 },
        {   1344,    237,   9342,    124,   -158,   -128 },
        {   1425,     58,   8886,    -28,    -35,   -107 },
        {   1572,    206,   9396,    103,   -102,   -134 },
        {   1512,     89,   9032,     -9,      3,   -113 },
        {   1322,    296,   9844,     98,   -152,   -130 },
        {   1492,     16,   9329,     44,     35,   -121 },
        {   1392,    193,   9199,     70,    -88,   -127 },
        {   1236,    190,   9541,     34,    -29,   -121 },
        {   1292,   -279,   8577,    113,     24,   -128 },
        {   1332,    548,   9638,     35,    -95,   -115 },
        {   1188,    -61,   8838,     10,     12,   -112 },
        {   1486,    305,   8856,    154,     48,   -139 },
        {   1600,    164,   8664,    -78,     -7,   -101 },
        {   1367,    -48,   9378,    215,    -39,   -155 },
        {   1524,    418,   8411,      4,     91,   -113 },
        {   1058,     -5,   9450,    128,   -169,   -136 },
        {   1548,    152,   7885,    117,    193,   -142 },
        {   1098,    228,   8947,      4,   -140,   -123 },
        {   1234,    198,   8455,    133,    123,   -143 },
        {   1238,    273,   8195,    -12,      7,   -118 },
        {   1768,   -155,   9149,    104,     -5,   -133 },
        {   1715,    318,   8625,    -39,     84,   -106 },
        {   1128,   -241,   9344,     39,    -59,   -123 },
        {   1542,    196,   7734,     13,    132,   -122 },
        {   1450,   -124,   9292,     24,   -205,   -117 },
        {   1450,   -124,   9292,     24,   -205,   -117 },
        {   1208,      7,   7523,     11,    167,   -118 },
        {   1175,    283,   8793,    -68,    -50,   -104 },
        {   1459,    136,   7946,      3,    130,   -124 },
        {   1699,    150,   8968,     43,   -103,   -130 },
        {   1181,     35,   8678,     29,    -19,   -121 },
        {   1329,    231,   8762,     -7,    -15,   -109 },
        {   1395,   -373,   8706,     54,     44,   -120 },
        {   1281,    707,   8465,    -53,     -8,   -102 },
        {   1081,   -696,   9038,     56,    -80,   -122 },
        {   1279,    579,   7811,     47,    109,   -121 },
        {   1530,   -579,   8802,   -105,   -128,    -93 },
        {   1601,    -98,   8194,    116,     84,   -133 },
        {   1391,    315,   8616,   -156,    -62,    -86 },
        {   1560,   -380,   8684,     80,    -41,   -128 },
        {   1387,    613,   8343,    -58,     32,   -103 },
        {   1138,   -358,   8776,     30,   -135,   -117 },
        {   1551,    590,   8315,     40,     90,   -114 },
        {   1327,   -113,   8382,    -66,    -50,    -96 },
        {   1489,    451,   8266,     58,    129,   -121 },
        {   1303,      3,   8551,    -70,    -84,    -92 },
        {   1559,    169,   8272,     86,     30,   -124 },
        {   1367,    154,   8495,     -9,     -8,   -106 },
        {   1364,    -33,   8546,     28,     14,   -116 }
    },
    {   /* vec 1 — smooth */
        {   2319,     35,   8243,    -58,    -43,    -84 },
        {   2069,   -204,   8656,     57,    -18,    -98 },
        {   1981,      8,   8538,    -43,      0,    -78 },
        {   1981,      8,   8538,    -43,      0,    -78 },
        {   2113,     -7,   8497,     -4,     58,    -87 },
        {   2009,   -197,   8341,    -23,     56,    -83 },
        {   2099,     65,   8479,     36,      0,    -87 },
        {   2180,    -13,   8435,     18,     27,    -85 },
        {   1986,   -146,   8634,     25,     69,    -82 },
        {   1903,    -74,   8508,    -14,     98,    -72 },
        {   1983,    179,   8551,     25,     61,    -76 },
        {   1957,   -234,   8616,     31,     61,    -77 },
        {   2167,    140,   8232,     23,    121,    -71 },
        {   2037,    -87,   8329,    -51,    101,    -62 },
        {   1997,   -143,   8553,     49,    114,    -73 },
        {   1891,    347,   8512,    -54,     72,    -52 },
        {   2082,   -385,   8300,     26,    121,    -67 },
        {   2072,    150,   8082,     -9,    146,    -58 },
        {   2044,    123,   8411,    -70,     81,    -53 },
        {   1958,   -135,   8402,     61,    159,    -76 },
        {   1915,    123,   8409,    -33,     97,    -54 },
        {   2101,    -63,   8137,     40,    126,    -66 },
        {   1950,     40,   8162,     18,    125,    -66 },
        {   1923,     63,   8211,      3,    121,    -62 },
        {   2154,     75,   8197,     21,    153,    -61 },
        {   2142,    -10,   7958,     16,    145,    -59 },
        {   1833,      6,   8245,     41,     86,    -67 },
        {   2163,      9,   8211,     29,    123,    -65 },
        {   2035,    -22,   8234,     44,    132,    -72 },
        {   1902,     19,   7985,     23,    156,    -66 },
        {   2065,    -56,   8449,     39,    127,    -73 },
        {   2095,    135,   8246,     38,    134,    -71 },
        {   2005,   -217,   8050,     59,    112,    -76 },
        {   2153,    -42,   8286,     59,    114,    -74 },
        {   2207,     10,   7961,    -26,    161,    -59 },
        {   2208,   -262,   7910,     60,    150,    -70 },
        {   2123,    155,   8568,    -17,     56,    -57 },
        {   1899,   -417,   8790,     48,     99,    -68 },
        {   2044,    163,   8020,    -16,    188,    -56 },
        {   2287,   -237,   8315,    -65,    126,    -45 },
        {   2189,   -132,   8284,     15,    163,    -56 },
        {   1885,    165,   8199,    -78,     45,    -38 },
        {   1908,   -144,   8615,     29,     88,    -53 },
        {   1937,     49,   8560,    -72,    156,    -33 },
        {   2004,    -71,   7739,    -35,    182,    -41 },
        {   1961,    120,   7992,     20,    109,    -46 },
        {   1888,    190,   8278,      9,     74,    -43 },
        {   2037,   -206,   7975,    107,    150,    -57 },
        {   2285,    256,   8174,     18,    152,    -41 },
        {   2208,    -30,   8525,     77,    179,    -47 }
    },
    {   /* vec 2 — rough */
        {   1538,    672,   8084,    191,     80,     39 },
        {   1460,     99,   8420,    132,   -103,     56 },
        {   1581,    326,   8499,    250,      1,     38 },
        {   1505,    561,   8555,     28,     36,     74 },
        {   1894,   -293,   8398,    152,     91,     48 },
        {   1758,    651,   8166,    144,     39,     53 },
        {   1593,   -291,   8172,     73,    -74,     62 },
        {   1454,    -82,   8124,    230,     -9,     40 },
        {   1662,    832,   8271,    -70,    -60,     89 },
        {   1301,   -647,   8477,    187,     46,     46 },
        {   1399,    796,   8138,     60,     50,     77 },
        {   1632,   -162,   8136,     36,    -59,     79 },
        {   1644,   -602,   8174,    271,     77,     42 },
        {   1487,    607,   8271,    -61,     -6,     98 },
        {   1473,   -653,   8273,    125,     51,     57 },
        {   1549,    190,   7799,    141,     90,     60 },
        {   1198,      1,   8554,     41,    -88,     78 },
        {   1406,   -251,   8237,    219,    133,     43 },
        {   1495,    153,   8100,     -5,     99,     84 },
        {   1695,   -572,   7914,    113,     93,     67 },
        {   1560,     82,   8238,    102,     91,     58 },
        {   1722,    -32,   8384,    -26,     66,     83 },
        {   1859,   -388,   8149,    132,    166,     48 },
        {   1457,    109,   8075,    -20,     65,     73 },
        {   1331,   -598,   8670,     48,    -19,     71 },
        {   1545,     90,   8443,     52,     78,     74 },
        {   1604,   -407,   8247,   -191,     84,    110 },
        {   1722,   -566,   8157,    -21,    145,     89 },
        {   1745,    360,   8108,   -256,     65,    128 },
        {   1661,   -426,   8399,   -104,     -3,     94 },
        {   1968,    299,   8479,   -122,     46,     93 },
        {   1968,    299,   8479,   -122,     46,     93 },
        {   1915,     12,   8636,   -251,     40,     99 },
        {   1972,    158,   8290,    -87,    134,     64 },
        {   1815,    410,   8620,   -202,    -35,     78 },
        {   1893,    104,   8716,     14,    -44,     29 },
        {   1857,    624,   8320,    -68,     24,     36 },
        {   1824,    -38,   8512,     -6,    -37,     27 },
        {   1664,    881,   8585,    102,    -35,      2 },
        {   1628,     95,   8548,     50,    -72,     12 },
        {   1592,    617,   8749,    212,    -78,    -12 },
        {   1902,    511,   8443,     89,    -48,     11 },
        {   1723,   -180,   8660,    243,    -73,     -8 },
        {   1577,    695,   8394,    133,    -82,     29 },
        {   1517,   -219,   8486,    202,   -170,     32 },
        {   1943,    152,   8372,    191,   -119,     44 },
        {   1591,     15,   8332,     -8,    -99,     95 },
        {   1553,     31,   8525,     80,   -137,     90 },
        {   1687,   -183,   8372,    -20,   -166,    119 },
        {   1865,     23,   8682,    -10,   -251,    126 }
    },
    {   /* vec 3 — rough */
        {   2240,    694,   8393,   -183,   -148,   -607 },
        {   1624,   -247,   8865,     45,   -178,   -655 },
        {   2399,    299,   8193,    -44,    -32,   -654 },
        {   2121,    354,   8852,   -135,   -112,   -660 },
        {   1998,    419,   8129,    -47,    127,   -694 },
        {   2593,     59,   7936,    -58,     53,   -690 },
        {   2049,   -122,   8007,    113,    -89,   -720 },
        {   1687,    -55,   7566,     26,   -112,   -706 },
        {   2079,   -623,   8661,     52,    -84,   -707 },
        {   2136,    176,   8245,    -81,    139,   -687 },
        {   2481,   -932,   7424,   -128,    211,   -679 },
        {   2578,   -145,   8194,    -22,     12,   -689 },
        {   1770,   -283,   8546,   -193,   -103,   -660 },
        {   1770,   -283,   8546,   -193,   -103,   -660 },
        {   1916,  -1025,   8413,    133,   -136,   -708 },
        {   2305,   -124,   8175,     -3,   -156,   -677 },
        {   1999,   -638,   8342,   -154,    -11,   -647 },
        {   2208,   -775,   8371,     29,    104,   -675 },
        {   2045,    -71,   8076,   -113,     32,   -655 },
        {   2168,  -1160,   8957,    116,   -124,   -696 },
        {   2087,   -611,   8135,     35,     55,   -690 },
        {   1963,   -557,   8168,   -237,     38,   -651 },
        {   2254,   -804,   8052,    -61,     93,   -679 },
        {   1736,   -665,   8585,    -79,    -93,   -683 },
        {   2314,   -482,   7933,   -142,    -71,   -689 },
        {   2053,  -1127,   8672,    -73,   -117,   -700 },
        {   2239,   -502,   9344,    -87,    -18,   -714 },
        {   2399,    -67,   8573,   -369,    184,   -701 },
        {   2785,   -969,   8407,    -25,    132,   -773 },
        {   2451,    152,   8511,     33,   -103,   -794 },
        {   2169,   -828,   8607,    -73,    -70,   -783 },
        {   2532,   -497,   8864,    164,     98,   -820 },
        {   2047,     69,   8416,   -168,    149,   -793 },
        {   2325,  -1278,   8926,     99,    148,   -854 },
        {   2614,    310,   8616,     53,     95,   -858 },
        {   2266,   -413,   8731,     45,    -48,   -862 },
        {   2441,   -722,   8449,    195,     94,   -873 },
        {   2664,  -1183,   8164,     17,     33,   -857 },
        {   2454,    228,   8712,     77,     23,   -883 },
        {   2117,   -287,   8632,   -108,     70,   -863 },
        {   1871,   -954,   8270,    165,     30,   -895 },
        {   2612,    -22,   8539,    -25,   -131,   -855 },
        {   2336,  -1414,   8955,     46,    -18,   -859 },
        {   2529,   -448,   8672,    -47,    140,   -848 },
        {   2622,   -305,   7625,   -171,     90,   -841 },
        {   1798,  -1621,   8829,    181,   -199,   -885 },
        {   1929,    403,   9079,     38,   -125,   -849 },
        {   2070,  -1420,   7935,   -167,     90,   -815 },
        {   2754,   -664,   7185,     92,    160,   -842 },
        {   1937,   -593,   7651,   -229,    -73,   -778 }
    },
    {   /* vec 4 — rough */
        {   2079,   -117,   8396,    -55,    133,   -123 },
        {   2320,    224,   8199,    -26,    180,   -123 },
        {   2135,    990,   8444,   -231,     64,    -96 },
        {   1967,   -551,   8089,     43,    170,   -146 },
        {   2120,   1016,   8392,    -63,     35,   -112 },
        {   1979,    223,   8317,   -188,     88,    -94 },
        {   1954,   -208,   8209,     87,    196,   -151 },
        {   2398,    784,   7918,   -245,    106,   -103 },
        {   2206,   -460,   8358,    -36,    104,   -145 },
        {   2135,    511,   7715,   -126,    146,   -126 },
        {   1958,    250,   8053,   -225,     44,   -101 },
        {   2103,    239,   8352,     -4,     62,   -141 },
        {   1942,     -5,   8679,   -112,     -5,   -133 },
        {   2407,    801,   8337,    -65,    106,   -156 },
        {   2199,     84,   8288,   -154,     99,   -145 },
        {   1935,    349,   8053,     14,     78,   -174 },
        {   1653,    389,   8439,    -61,   -116,   -157 },
        {   1989,    231,   8675,    128,    -87,   -190 },
        {   1932,    519,   8229,    -64,      4,   -160 },
        {   1876,   -228,   8369,    -60,     83,   -160 },
        {   1838,    959,   8064,    -68,     57,   -151 },
        {   2020,    -87,   7992,    -51,   -104,   -145 },
        {   1721,     11,   8326,    156,   -143,   -181 },
        {   1623,    659,   8416,   -207,    -71,   -119 },
        {   1704,   -231,   8434,    -37,     16,   -153 },
        {   1578,    483,   8199,    -46,    -23,   -142 },
        {   1721,    176,   8308,   -132,   -117,   -124 },
        {   1689,   -117,   8544,     43,    -27,   -152 },
        {   1381,    695,   8238,   -159,     -6,   -110 },
        {   1163,   -208,   8211,    -10,    -49,   -137 },
        {   1169,    730,   8275,     23,    -80,   -135 },
        {   1206,     97,   8573,    -54,   -157,   -117 },
        {   1396,    276,   8526,    111,      9,   -149 },
        {   1327,    727,   8251,   -168,     12,   -102 },
        {   1360,   -297,   8504,    112,    -41,   -143 },
        {   1017,   1104,   8423,      0,    -82,   -123 },
        {   1017,   1104,   8423,      0,    -82,   -123 },
        {   1053,   -215,   8615,    -25,   -139,   -112 },
        {   1491,    729,   8586,    166,     -1,   -159 },
        {   1708,    608,   8123,   -213,     55,   -109 },
        {   1468,     50,   8552,    148,     24,   -164 },
        {   1432,   1005,   8251,    -74,     -4,   -112 },
        {   1519,    -63,   8507,    113,   -136,   -142 },
        {   1881,    627,   8780,    165,   -101,   -153 },
        {   1751,    498,   8825,    -33,     16,   -126 },
        {   2010,    515,   8419,     75,    179,   -153 },
        {   1859,    427,   7993,     58,     28,   -144 },
        {   1525,    660,   8316,    228,   -133,   -170 },
        {   1319,    189,   8828,    219,    -99,   -172 },
        {   1650,    426,   8647,    166,     57,   -169 }
    },
};

static const uint8_t mcu_verify_labels[MCU_VERIFY_N_VECTORS] = { 0, 0, 1, 1, 1 };

/*----------------------------------------------------------------------------*/
/*  Working buffers (BSS — keeps stack flat)                                  */
/*----------------------------------------------------------------------------*/

static float32_t mcu_verify_scaled    [WINDOW_SIZE][N_FEATURES];
static float32_t mcu_verify_feats     [N_STAT_FEATURES];
static float32_t mcu_verify_feats_norm[N_STAT_FEATURES];
static int8_t    mcu_verify_stat_q    [N_STAT_FEATURES];
static int8_t    mcu_verify_ts_q      [WINDOW_SIZE * N_FEATURES];

/* Line assembly buffer. uint8_t[] satisfies Buffer_t.data type;
   we cast to (char *) for snprintf (MISRA Dir 4.6 / Rule 11.3 deviation —
   intentional aliasing for stdio API). */
static uint8_t   mcu_verify_line_storage[MCU_VERIFY_LINE_BUF_SIZE];
static Buffer_t  mcu_verify_line = BUFFER_INIT(mcu_verify_line_storage,
                                               MCU_VERIFY_LINE_BUF_SIZE);

static void mcu_verify_run_one(uint32_t vec_idx);

/*----------------------------------------------------------------------------*/
/*  Static helpers — UART layer                                               */
/*----------------------------------------------------------------------------*/

/**
 * @brief  Busy-wait for the UART TX path to become idle.
 * @note   Pre-scheduler context — busy-wait is acceptable.
 *         Guarded by a tick counter to avoid permanent spin if the
 *         peripheral or DMA wedges.
 */
static void mcu_verify_wait_tx_idle(void)
{
    uint8_t  idle  = 0U;
    uint32_t guard = 0U;

    do
    {
        UART_SVC_Error_t err = UART_SVC_IsTxIdle(MCU_VERIFY_UART_ID, &idle);
        if (err != UART_SVC_OK)
        {
            /* If the service can't even tell us, bail out — nothing
               sensible to do here in a self-test. */
            break;
        }
        guard++;
    } while ((idle == 0U) && (guard < MCU_VERIFY_TX_TIMEOUT_TICKS));
}

/**
 * @brief  Send the line buffer over UART via DMA and block until done.
 *         Buffer_t.length must already be set by the caller.
 */
static void mcu_verify_send_line(void)
{
    /* Pre-flight: previous DMA must be complete before we hand the
       same backing storage to a new transfer. */
    mcu_verify_wait_tx_idle();

    (void)UART_SVC_TransmitDMA(MCU_VERIFY_UART_ID, &mcu_verify_line);

    /* Small barrier so the DMA controller has time to assert the busy
       state before we poll for idle. Prevents a 1-tick race on fast MCUs. */
    for (volatile uint32_t guard_i = 0U; guard_i < 100U; ++guard_i)
    {
        __asm volatile ("nop");
    }

    /* Post-flight: caller is about to mutate the line buffer for the
       next line, so we must wait for the DMA to drain it first. */
    mcu_verify_wait_tx_idle();
}

/**
 * @brief  Send a fixed C-string as one UART line (with CRLF appended).
 */
static void mcu_verify_send_string(const char *s)
{
    char    *dst   = (char *)mcu_verify_line_storage;
    uint32_t i     = 0U;
    uint32_t cap   = (uint32_t)MCU_VERIFY_LINE_BUF_SIZE - 3U; /* CRLF + NUL */

    BUFFER_RESET(&mcu_verify_line);

    while ((s[i] != '\0') && (i < cap))
    {
        dst[i] = s[i];
        i++;
    }
    dst[i]      = '\r';
    dst[i + 1U] = '\n';

    mcu_verify_line.length = (uint16_t)(i + 2U);
    mcu_verify_send_line();
}

/*----------------------------------------------------------------------------*/
/*  Static helpers — formatting                                               */
/*----------------------------------------------------------------------------*/

/**
 * @brief  Append a deterministic fixed-8-decimal float to a char buffer.
 *
 * Format: optional '-', integer digits, '.', exactly 8 fractional digits.
 * Special values: NaN -> "nan", +Inf -> "inf", -Inf -> "-inf".
 *
 * Avoids reliance on newlib's `%f` (which is disabled in newlib-nano by
 * default on STM32 PlatformIO builds) so the output is bit-deterministic
 * across toolchains.
 *
 * @param  out   Destination buffer.
 * @param  cap   Capacity of out[].
 * @param  pos   Current write position in out[] (updated on return).
 * @param  v     Value to format.
 * @return New write position. If insufficient capacity, pos is unchanged
 *         (or "!OF" sentinel is appended if at least 3 bytes remain).
 */
static uint32_t mcu_verify_fmt_float(char *out,
                                     uint32_t cap,
                                     uint32_t pos,
                                     float32_t v)
{
    /* Reserve worst-case: sign + ~12 int digits + '.' + 8 frac + NUL = 23 */
    if ((cap - pos) < 24U)
    {
        /* Buffer overflow guard — emit "!OF" so the Python parser can
           detect truncation rather than silently losing data. */
        if ((cap - pos) >= 3U)
        {
            out[pos]      = '!';
            out[pos + 1U] = 'O';
            out[pos + 2U] = 'F';
            return pos + 3U;
        }
        return pos;
    }

    /* Special values */
    if (isnan(v) != 0)
    {
        out[pos] = 'n'; out[pos + 1U] = 'a'; out[pos + 2U] = 'n';
        return pos + 3U;
    }
    if (isinf(v) != 0)
    {
        uint32_t p = pos;
        if (v < 0.0f) { out[p] = '-'; p++; }
        out[p] = 'i'; out[p + 1U] = 'n'; out[p + 2U] = 'f';
        return p + 3U;
    }

    /* Sign */
    uint32_t  p   = pos;
    float32_t mag = v;
    if (v < 0.0f)
    {
        out[p] = '-';
        p++;
        mag = -v;
    }

    /* Split into integer and fractional parts.
       Scale factor 1e8 matches MCU_VERIFY_FLOAT_FRAC_DIG = 8.
       Use double internally to preserve 8 decimal digits cleanly. */
    double   d        = (double)mag;
    /* Match Python's %.8f rounding (round-half-to-even). nearbyint()
       respects the current rounding mode; default on bare-metal Cortex-M4
       is round-to-nearest-even, matching IEEE 754 default. */
    uint64_t scaled   = (uint64_t)nearbyint(d * 1e8);
    uint64_t int_part = scaled / 100000000ULL;
    uint64_t frac     = scaled % 100000000ULL;

    /* Integer digits — write reversed then flip */
    char     ibuf[24];
    uint32_t ilen = 0U;
    if (int_part == 0ULL)
    {
        ibuf[ilen] = '0';
        ilen++;
    }
    else
    {
        while ((int_part > 0ULL) && (ilen < (uint32_t)sizeof(ibuf)))
        {
            ibuf[ilen] = (char)('0' + (int)(int_part % 10ULL));
            int_part  /= 10ULL;
            ilen++;
        }
    }
    /* Reverse copy */
    for (uint32_t i = 0U; i < ilen; ++i)
    {
        out[p + i] = ibuf[ilen - 1U - i];
    }
    p += ilen;

    out[p] = '.';
    p++;

    /* Fractional digits — exactly 8, zero-padded on the left */
    char fbuf[8];
    for (int32_t i = 7; i >= 0; --i)
    {
        fbuf[i] = (char)('0' + (int)(frac % 10ULL));
        frac   /= 10ULL;
    }
    for (uint32_t i = 0U; i < 8U; ++i)
    {
        out[p + i] = fbuf[i];
    }
    p += 8U;

    return p;
}

/**
 * @brief  Append a signed int8 as decimal text.
 */
static uint32_t mcu_verify_fmt_int8(char *out,
                                    uint32_t cap,
                                    uint32_t pos,
                                    int8_t v)
{
    /* Worst-case "-128" + NUL = 5 */
    if ((cap - pos) < 5U)
    {
        return pos;
    }
    int n = snprintf(&out[pos], (size_t)(cap - pos), "%d", (int)v);
    if (n < 0)
    {
        return pos;
    }
    return pos + (uint32_t)n;
}

/**
 * @brief  Emit one line of the form "<prefix>,<v0>,<v1>,...".
 *
 * Used for FEATURES_x lines (slice of the float feature vector).
 */
static void mcu_verify_emit_float_slice(const char      *prefix,
                                        const float32_t *arr,
                                        uint32_t        start,
                                        uint32_t        count)
{
    char    *dst = (char *)mcu_verify_line_storage;
    uint32_t cap = (uint32_t)MCU_VERIFY_LINE_BUF_SIZE;
    uint32_t pos = 0U;
    int      n;

    BUFFER_RESET(&mcu_verify_line);

    n = snprintf(&dst[pos], (size_t)(cap - pos), "%s", prefix);
    if (n > 0) { pos += (uint32_t)n; }

    for (uint32_t i = 0U; i < count; ++i)
    {
        if ((cap - pos) < 2U) { break; }
        dst[pos] = ',';
        pos++;
        pos = mcu_verify_fmt_float(dst, cap, pos, arr[start + i]);
    }

    /* CRLF */
    if ((cap - pos) >= 2U)
    {
        dst[pos]      = '\r';
        dst[pos + 1U] = '\n';
        pos += 2U;
    }

    mcu_verify_line.length = (uint16_t)pos;
    mcu_verify_send_line();
}

/**
 * @brief  Emit one line of the form "<prefix>,<v0>,<v1>,...".
 *
 * Used for STAT_INT8 and TS_INT8_x lines.
 */
static void mcu_verify_emit_int8_slice(const char   *prefix,
                                       const int8_t *arr,
                                       uint32_t     start,
                                       uint32_t     count)
{
    char    *dst = (char *)mcu_verify_line_storage;
    uint32_t cap = (uint32_t)MCU_VERIFY_LINE_BUF_SIZE;
    uint32_t pos = 0U;
    int      n;

    BUFFER_RESET(&mcu_verify_line);

    n = snprintf(&dst[pos], (size_t)(cap - pos), "%s", prefix);
    if (n > 0) { pos += (uint32_t)n; }

    for (uint32_t i = 0U; i < count; ++i)
    {
        if ((cap - pos) < 2U) { break; }
        dst[pos] = ',';
        pos++;
        pos = mcu_verify_fmt_int8(dst, cap, pos, arr[start + i]);
    }

    if ((cap - pos) >= 2U)
    {
        dst[pos]      = '\r';
        dst[pos + 1U] = '\n';
        pos += 2U;
    }

    mcu_verify_line.length = (uint16_t)pos;
    mcu_verify_send_line();
}

/*----------------------------------------------------------------------------*/
/*  Public API                                                                */
/*----------------------------------------------------------------------------*/

void McuVerify_RunOnce(void)
{
    Features_Init();
    (void)Inference_Init();

    for (uint32_t i = 0U; i < (uint32_t)MCU_VERIFY_N_VECTORS; ++i)
    {
        mcu_verify_run_one(i);
    }
}

/**
 * @brief  Run the full preprocessing + inference pipeline on a single
 *         hardcoded test vector and dump all intermediate stages to UART.
 *
 * Output format (one block per call), framed by ===VERIFY_START=== /
 * ===VERIFY_END=== markers carrying the vector index and ground-truth
 * label. See the parser sketch at the top of this file for the exact
 * line grammar.
 *
 * @param  vec_idx  Index into mcu_verify_raw[] / mcu_verify_labels[].
 *                  Must be < MCU_VERIFY_N_VECTORS (caller-checked).
 *
 * @note   Reuses the file-scope working buffers (mcu_verify_scaled etc.),
 *         so this function is NOT re-entrant. Intended to be called
 *         strictly sequentially from McuVerify_RunOnce().
 */
static void mcu_verify_run_one(uint32_t vec_idx)
{
    Scale_RawWindow(mcu_verify_raw[vec_idx], mcu_verify_scaled);
    Features_Extract(mcu_verify_scaled, mcu_verify_feats);
    Quantize_TS(mcu_verify_scaled, mcu_verify_ts_q);

    for (uint32_t i = 0U; i < N_STAT_FEATURES; ++i)
    {
        mcu_verify_feats_norm[i] = mcu_verify_feats[i];
    }
    Quantize_NormalizeStat(mcu_verify_feats_norm);
    Quantize_Stat(mcu_verify_feats_norm, mcu_verify_stat_q);

    Inference_Result_t inf = { 0U, 0U };
    Inference_Run(mcu_verify_ts_q, mcu_verify_stat_q, &inf);

    /* Framing marker: ===VERIFY_START===,VEC=<idx>,LABEL=<gt> */
    {
        char buf[64];
        int  n = snprintf(buf, sizeof(buf),
                          "===VERIFY_START===,VEC=%u,LABEL=%u\r\n",
                          (unsigned)vec_idx,
                          (unsigned)mcu_verify_labels[vec_idx]);
        if ((n > 0) && (n < (int)sizeof(buf)))
        {
            uint8_t *saved_data = mcu_verify_line.data;
            uint16_t saved_size = mcu_verify_line.size;
            mcu_verify_line.data   = (uint8_t *)buf;
            mcu_verify_line.size   = (uint16_t)sizeof(buf);
            mcu_verify_line.length = (uint16_t)n;
            mcu_verify_line.index  = 0U;
            mcu_verify_send_line();
            mcu_verify_line.data = saved_data;
            mcu_verify_line.size = saved_size;
        }
    }

    mcu_verify_emit_float_slice("FEATURES_0", mcu_verify_feats,  0U, 25U);
    mcu_verify_emit_float_slice("FEATURES_1", mcu_verify_feats, 25U, 25U);
    mcu_verify_emit_int8_slice ("STAT_INT8",  mcu_verify_stat_q,  0U, 50U);
    mcu_verify_emit_int8_slice ("TS_INT8_0",  mcu_verify_ts_q,    0U, 100U);
    mcu_verify_emit_int8_slice ("TS_INT8_1",  mcu_verify_ts_q,  100U, 100U);
    mcu_verify_emit_int8_slice ("TS_INT8_2",  mcu_verify_ts_q,  200U, 100U);

    {
        char buf[64];
        int  n = snprintf(buf, sizeof(buf),
                          "INFERENCE,%u,%u\r\n",
                          (unsigned)inf.label, (unsigned)inf.confidence);
        if ((n > 0) && (n < (int)sizeof(buf)))
        {
            uint8_t *saved_data = mcu_verify_line.data;
            uint16_t saved_size = mcu_verify_line.size;
            mcu_verify_line.data   = (uint8_t *)buf;
            mcu_verify_line.size   = (uint16_t)sizeof(buf);
            mcu_verify_line.length = (uint16_t)n;
            mcu_verify_line.index  = 0U;
            mcu_verify_send_line();
            mcu_verify_line.data = saved_data;
            mcu_verify_line.size = saved_size;
        }
    }

    mcu_verify_send_string("===VERIFY_END===");
    mcu_verify_wait_tx_idle();
}
