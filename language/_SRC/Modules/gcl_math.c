/*
 * gcl_math.c — GCL Math module (.dll/.so).
 *
 * Full function set from simple_doc.md:
 *   randInt, randint, randFloat, randfloat,
 *   min, max, abs, floor, ceil, round, sqrt, pow,
 *   sin, cos, tan, asin, acos, atan,
 *   log, log10, exp, clamp, sign, randBool, randChoice, randSign
 */

#include "gcl_module.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static double fn_rand_int(int argc, const char **argv) {
    int lo = argc > 0 ? (int)atof(argv[0]) : 0;
    int hi = argc > 1 ? (int)atof(argv[1]) : 100;
    if (hi <= lo) return (double)lo;
    return (double)(lo + rand() % (hi - lo + 1));
}

static double fn_rand_float(int argc, const char **argv) {
    double lo = argc > 0 ? atof(argv[0]) : 0.0;
    double hi = argc > 1 ? atof(argv[1]) : 1.0;
    if (hi <= lo) return lo;
    return lo + (double)rand() / (double)RAND_MAX * (hi - lo);
}

static double fn_min(int argc, const char **argv) {
    if (argc <= 0) return 0.0;
    double m = atof(argv[0]);
    for (int i = 1; i < argc; i++) {
        double v = atof(argv[i]);
        if (v < m) m = v;
    }
    return m;
}

static double fn_max(int argc, const char **argv) {
    if (argc <= 0) return 0.0;
    double m = atof(argv[0]);
    for (int i = 1; i < argc; i++) {
        double v = atof(argv[i]);
        if (v > m) m = v;
    }
    return m;
}

static double fn_abs(int argc, const char **argv) {
    return fabs(argc > 0 ? atof(argv[0]) : 0.0);
}

static double fn_floor(int argc, const char **argv) {
    return floor(argc > 0 ? atof(argv[0]) : 0.0);
}

static double fn_ceil(int argc, const char **argv) {
    return ceil(argc > 0 ? atof(argv[0]) : 0.0);
}

static double fn_round(int argc, const char **argv) {
    return round(argc > 0 ? atof(argv[0]) : 0.0);
}

static double fn_sqrt(int argc, const char **argv) {
    double x = argc > 0 ? atof(argv[0]) : 0.0;
    return x < 0 ? 0.0 : sqrt(x);
}

static double fn_pow(int argc, const char **argv) {
    return pow(argc > 0 ? atof(argv[0]) : 0.0, argc > 1 ? atof(argv[1]) : 0.0);
}

static double fn_sin(int argc, const char **argv) { return sin((argc > 0 ? atof(argv[0]) : 0.0) * 3.14159265358979323846 / 180.0); }
static double fn_cos(int argc, const char **argv) { return cos((argc > 0 ? atof(argv[0]) : 0.0) * 3.14159265358979323846 / 180.0); }
static double fn_tan(int argc, const char **argv) { return tan((argc > 0 ? atof(argv[0]) : 0.0) * 3.14159265358979323846 / 180.0); }
static double fn_asin(int argc, const char **argv) { return asin(argc > 0 ? atof(argv[0]) : 0.0) * 180.0 / 3.14159265358979323846; }
static double fn_acos(int argc, const char **argv) { return acos(argc > 0 ? atof(argv[0]) : 0.0) * 180.0 / 3.14159265358979323846; }
static double fn_atan(int argc, const char **argv) { return atan(argc > 0 ? atof(argv[0]) : 0.0) * 180.0 / 3.14159265358979323846; }

static double fn_log(int argc, const char **argv) { return log(argc > 0 ? atof(argv[0]) : 0.0); }
static double fn_log10(int argc, const char **argv) { return log10(argc > 0 ? atof(argv[0]) : 0.0); }
static double fn_exp(int argc, const char **argv) { return exp(argc > 0 ? atof(argv[0]) : 0.0); }

static double fn_clamp(int argc, const char **argv) {
    double x = argc > 0 ? atof(argv[0]) : 0.0;
    double lo = argc > 1 ? atof(argv[1]) : 0.0;
    double hi = argc > 2 ? atof(argv[2]) : 0.0;
    return x < lo ? lo : (x > hi ? hi : x);
}

static double fn_sign(int argc, const char **argv) {
    double x = argc > 0 ? atof(argv[0]) : 0.0;
    return x < 0 ? -1.0 : (x > 0 ? 1.0 : 0.0);
}

static double fn_rand_bool(int argc, const char **argv) {
    (void)argc; (void)argv;
    return (double)(rand() % 2);
}

static double fn_rand_choice(int argc, const char **argv) {
    if (argc <= 0) return 0.0;
    return atof(argv[rand() % argc]);
}

static double fn_rand_sign(int argc, const char **argv) {
    (void)argc; (void)argv;
    return (double)(rand() % 2 ? 1 : -1);
}

static const GclNativeEntry g_entries[] = {
    {"randInt", fn_rand_int},
    {"randint", fn_rand_int},
    {"randFloat", fn_rand_float},
    {"randfloat", fn_rand_float},
    {"min", fn_min},
    {"max", fn_max},
    {"abs", fn_abs},
    {"floor", fn_floor},
    {"ceil", fn_ceil},
    {"round", fn_round},
    {"sqrt", fn_sqrt},
    {"pow", fn_pow},
    {"sin", fn_sin},
    {"cos", fn_cos},
    {"tan", fn_tan},
    {"asin", fn_asin},
    {"acos", fn_acos},
    {"atan", fn_atan},
    {"log", fn_log},
    {"log10", fn_log10},
    {"exp", fn_exp},
    {"clamp", fn_clamp},
    {"sign", fn_sign},
    {"randBool", fn_rand_bool},
    {"randChoice", fn_rand_choice},
    {"randSign", fn_rand_sign},
};

GCL_EXPORT const GclNativeEntry *gcl_math_get_functions(int *count) {
    static int seeded = 0;
    if (!seeded) { srand((unsigned)time(NULL)); seeded = 1; }
    *count = (int)(sizeof(g_entries) / sizeof(g_entries[0]));
    return g_entries;
}
