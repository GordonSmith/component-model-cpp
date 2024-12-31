#include "util.hpp"
#include <vector>
#include <cstdint>

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

void tests_list_list_string_append(tests_list_list_string_t *a, tests_list_list_string_t *b, tests_string_t *ret)
{
    dbglog(std::string("a len:  ") + std::to_string(a->len));
    dbglog(std::string("b len:  ") + std::to_string(b->len));

    std::vector<std::vector<std::string>> v;
    for (size_t i = 0; i < a->len; i++)
    {
        std::vector<std::string> v2;
        for (size_t j = 0; j < a->ptr[i].len; j++)
        {
            std::string s((const char *)a->ptr[i].ptr[j].ptr, a->ptr[i].ptr[j].len);
            dbglog(std::string("a-s:  ") + s);
            v2.push_back(s);
        }
        v.push_back(v2);
    }

    for (size_t i = 0; i < b->len; i++)
    {
        std::vector<std::string> v2;
        for (size_t j = 0; j < b->ptr[i].len; j++)
        {
            std::string s((const char *)b->ptr[i].ptr[j].ptr, b->ptr[i].ptr[j].len);
            dbglog(std::string("b-s:  ") + s);
            v2.push_back(s);
        }
        v.push_back(v2);
    }

    std::string r;
    for (size_t i = 0; i < v.size(); i++)
    {
        for (size_t j = 0; j < v[i].size(); j++)
        {
            r += v[i][j];
        }
    }

    dbglog(std::to_string(++tally) + ":  " + r);
    tests_string_dup(ret, r.c_str());
}

void tests_utf8_string(tests_string_t *a, tests_string_t *b, tests_string_t *ret)
{
    std::string s1((const char *)a->ptr, (size_t)a->len);
    tests_string_free(a);
    std::string s2((const char *)b->ptr, (size_t)b->len);
    tests_string_free(b);
    std::string r = s1 + s2;
    dbglog(std::to_string(++tally) + ":  " + r);
    tests_string_dup(ret, r.c_str());
}
