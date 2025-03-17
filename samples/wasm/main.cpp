#include "sample.h"

#include <cstdlib>

bool exports_example_sample_boolean_and(bool a, bool b)
{
    return a && b;
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