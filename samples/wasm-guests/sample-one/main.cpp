#include "sample.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

bool exports_example_sample_booleans_and(bool a, bool b)
{
    return example_sample_booleans_and(a, b);
}

double exports_example_sample_floats_add(double a, double b)
{
    return example_sample_floats_add(a, b);
}

void exports_example_sample_variants_variant_func(exports_example_sample_variants_v_t *a, exports_example_sample_variants_v_t *ret)
{
    ret->tag = a->tag;
    if (a->tag == EXPORTS_EXAMPLE_SAMPLE_VARIANTS_V_B)
    {
        ret->val.b = !a->val.b;
    }
    else if (a->tag == EXPORTS_EXAMPLE_SAMPLE_VARIANTS_V_U)
    {
        ret->val.u = a->val.u * 2;
    }
}

void exports_example_sample_strings_reverse(sample_string_t *a, sample_string_t *ret)
{
    sample_string_t buff;
    example_sample_strings_reverse(a, &buff);
    ret->ptr = (uint8_t *)malloc(buff.len);
    ret->len = buff.len;
    for (size_t i = 0; i < buff.len; ++i)
    {
        ret->ptr[i] = buff.ptr[buff.len - i - 1];
    }
}

uint32_t exports_example_sample_strings_lots(sample_string_t *p1, sample_string_t *p2, sample_string_t *p3, sample_string_t *p4, sample_string_t *p5, sample_string_t *p6, sample_string_t *p7, sample_string_t *p8, sample_string_t *p9, sample_string_t *p10, sample_string_t *p11, sample_string_t *p12, sample_string_t *p13, sample_string_t *p14, sample_string_t *p15, sample_string_t *p16, sample_string_t *p17)
{
    return example_sample_strings_lots(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17);
}

void exports_example_sample_tuples_reverse(sample_tuple2_bool_string_t *a, sample_tuple2_string_bool_t *ret)
{
    ret->f1 = !a->f0;
    exports_example_sample_strings_reverse(&a->f1, &ret->f0);
}

void exports_example_sample_lists_filter_bool(exports_example_sample_lists_list_v_t *a, sample_list_string_t *ret)
{
    ret->ptr = (sample_string_t *)malloc(a->len * sizeof(sample_string_t));
    ret->len = 0;
    for (size_t i = 0; i < a->len; ++i)
    {
        if (a->ptr[i].tag == EXPORTS_EXAMPLE_SAMPLE_LISTS_V_S)
        {
            ret->ptr[ret->len++] = a->ptr[i].val.s;
        }
    }
}

void exports_sample_void_func(void)
{
    sample_void_func();
}

bool exports_sample_ok_func(uint32_t a, uint32_t b, uint32_t *ret, sample_string_t *err)
{
    *ret = a + b;
    return true;
}

bool exports_sample_err_func(uint32_t a, uint32_t b, uint32_t *ret, sample_string_t *err)
{
    err->ptr = (uint8_t *)malloc(5);
    err->len = 5;
    std::memcpy(err->ptr, "error", 5);
    return false;
}

bool exports_sample_option_func(uint32_t *maybe_a, uint32_t *ret)
{
    if (maybe_a)
    {
        *ret = *maybe_a * 2;
        return true;
    }
    return false;
}

void exports_example_sample_variants_variant_func(exports_example_sample_variants_v_t *a)
{
    if (a->tag == EXPORTS_EXAMPLE_SAMPLE_VARIANTS_V_B)
    {
        a->val.b = !a->val.b;
    }
    else if (a->tag == EXPORTS_EXAMPLE_SAMPLE_VARIANTS_V_U)
    {
        a->val.u = a->val.u * 2;
    }
}

exports_example_sample_enums_e_t exports_example_sample_enums_enum_func(exports_example_sample_enums_e_t a)
{
    return a;
}