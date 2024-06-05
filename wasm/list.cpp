#include "util.hpp"
#include <vector>
#include <string>

void tests_list_bool(tests_list_bool_t *ret)
{
    ret->len = 8;
    ret->ptr = (bool *)malloc(ret->len * sizeof(bool));
    for (size_t i = 0; i < ret->len; ++i)
    {
        ret->ptr[i] = i % 2 == 0;
    }
}

void tests_list_bool_bool(tests_list_bool_t *a, tests_list_bool_t *ret)
{
    std::vector<bool> v1(a->ptr, a->ptr + a->len);
    tests_list_bool_free(a);
    ret->len = v1.size();
    ret->ptr = (bool *)malloc(ret->len * sizeof(bool));
    for (size_t i = 0; i < ret->len; ++i)
    {
        ret->ptr[ret->len - i - 1] = v1[i];
    }
}

void tests_list_u32(tests_list_u32_t *ret)
{
    ret->len = 4;
    ret->ptr = (uint32_t *)malloc(ret->len * sizeof(uint32_t));
    for (size_t i = 0; i < ret->len; ++i)
    {
        ret->ptr[i] = i;
    }
}

void tests_list_u32_u32(tests_list_u32_t *a, tests_list_u32_t *ret)
{
    std::vector<uint32_t> v1(a->ptr, a->ptr + a->len);
    tests_list_u32_free(a);
    ret->len = v1.size();
    ret->ptr = (uint32_t *)malloc(ret->len * sizeof(uint32_t));
    for (size_t i = 0; i < ret->len; ++i)
    {
        ret->ptr[ret->len - i - 1] = v1[i];
    }
}

void tests_list_f32(tests_list_f32_t *ret)
{
    ret->len = 4;
    ret->ptr = (float *)malloc(ret->len * sizeof(float));
    for (size_t i = 0; i < ret->len; ++i)
    {
        ret->ptr[i] = i + 0.33;
    }
}

void tests_list_f32_f32(tests_list_f32_t *a, tests_list_f32_t *ret)
{
    std::vector<float> v1(a->ptr, a->ptr + a->len);
    tests_list_f32_free(a);
    ret->len = v1.size();
    ret->ptr = (float *)malloc(ret->len * sizeof(float));
    for (size_t i = 0; i < ret->len; ++i)
    {
        ret->ptr[ret->len - i - 1] = v1[i];
    }
}

void tests_list_string(tests_list_string_t *ret)
{
    ret->len = 4;
    ret->ptr = (tests_string_t *)malloc(ret->len * sizeof(tests_string_t));
    for (size_t i = 0; i < ret->len; ++i)
    {
        std::string str = "test-";
        str += std::to_string(i);
        tests_string_dup(&ret->ptr[i], str.c_str());
    }
}

void tests_list_string_string(tests_list_string_t *a, tests_list_string_t *ret)
{
    std::vector<tests_string_t> v1(a->ptr, a->ptr + a->len);
    ret->len = v1.size();
    ret->ptr = (tests_string_t *)malloc(ret->len * sizeof(tests_string_t));
    for (size_t i = 0; i < ret->len; ++i)
    {
        std::string str((const char *)v1[i].ptr, (size_t)v1[i].len);
        tests_string_dup(&ret->ptr[ret->len - i - 1], str.c_str());
    }
    tests_list_string_free(a);
}

void tests_list_list(tests_list_list_string_t *ret)
{
    ret->len = 4;
    ret->ptr = (tests_list_string_t *)malloc(ret->len * sizeof(tests_list_string_t));
    for (size_t i = 0; i < ret->len; ++i)
    {
        ret->ptr->len = 4;
        ret->ptr->ptr = (tests_string_t *)malloc(ret->ptr->len * sizeof(tests_string_t));
        for (size_t i = 0; i < ret->ptr->len; ++i)
        {
            std::string str = "test-";
            str += std::to_string(i);
            tests_string_dup(&ret->ptr->ptr[i], str.c_str());
        }
    }
}
