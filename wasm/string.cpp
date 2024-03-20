#include "util.hpp"

static uint32_t tally = 0;

void tests_list_char_append(tests_list_char32_t *a, tests_list_char32_t *b, tests_list_char32_t *ret)
{
    std::vector<uint32_t> v(a->ptr, a->ptr + a->len);
    std::vector<uint32_t> v2(b->ptr, b->ptr + b->len);
    v.insert(v.end(), v2.begin(), v2.end());
    ret->ptr = (uint32_t *)malloc(v.size() * sizeof(uint32_t));
    ret->len = v.size();
    std::memcpy(ret->ptr, v.data(), v.size() * sizeof(uint32_t));
}

void tests_string_append(tests_string_t *a, tests_string_t *b, tests_string_t *ret)
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
