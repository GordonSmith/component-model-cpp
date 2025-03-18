#include "sample.h"

#include <cstdlib>

bool exports_example_sample_booleans_and(bool a, bool b)
{
    return a && b;
}

double exports_example_sample_floats_add(double a, double b)
{
    return a + b;
}

void exports_example_sample_strings_reverse(sample_string_t *a, sample_string_t *ret)
{
    ret->ptr = (uint8_t *)malloc(a->len);
    ret->len = a->len;
    for (size_t i = 0; i < a->len; ++i)
    {
        ret->ptr[i] = a->ptr[a->len - i - 1];
    }
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
