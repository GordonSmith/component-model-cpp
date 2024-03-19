#include "tests.h"

#include <string>

void dbglog(const std::string str)
{
    tests_string_t msg;
    tests_string_dup(&msg, str.c_str());
    tests_dbglog(&msg);
}

bool tests_bool_test(bool a, bool b)
{
    dbglog("tests_bool_test(" + std::to_string(a) + ", " + std::to_string(b) + ")");
    return a && b;
}
float tests_float32_test(float a, float b)
{
    return a + b;
}
double tests_float64_test(double a, double b)
{
    return a + b;
}
uint8_t tests_u8_test(uint8_t a, uint8_t b)
{
    return a + b;
}
uint16_t tests_u16_test(uint16_t a, uint16_t b)
{
    return a + b;
}
uint32_t tests_u32_test(uint32_t a, uint32_t b)
{
    return a + b;
}
uint64_t tests_u64_test(uint64_t a, uint64_t b)
{
    return a + b;
}
int8_t tests_s8_test(int8_t a, int8_t b)
{
    return a + b;
}
int16_t tests_s16_test(int16_t a, int16_t b)
{
    return a + b;
}
int32_t tests_s32_test(int32_t a, int32_t b)
{
    return a + b;
}
int64_t tests_s64_test(int64_t a, int64_t b)
{
    return a + b;
}
uint32_t tests_char_test(uint32_t a, uint32_t b)
{
    return a + b;
}
static uint32_t tally = 0;
void tests_utf8_string_test(tests_string_t *a, tests_string_t *b, tests_string_t *ret)
{
    std::string s1((const char *)a->ptr, a->len);
    dbglog(std::string("s1:  ") + s1);
    tests_string_free(a);
    std::string s2((const char *)b->ptr, b->len);
    dbglog(std::string("s2:  ") + s2);
    tests_string_free(b);
    std::string r = s1 + s2;
    dbglog(std::to_string(++tally) + ":  " + r);
    tests_string_dup(ret, r.c_str());
}

void tests_utf8_string_test2(tests_string_t *a, tests_string_t *b, tests_string_t *ret0, tests_string_t *ret1)
{
    tests_utf8_string_test(a, b, ret0);
    tests_utf8_string_test(b, a, ret1);
}
