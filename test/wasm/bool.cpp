#include "util.hpp"

bool tests_bool_and(bool a, bool b)
{
    dbglog("tests_bool_and(" + std::to_string(a) + ", " + std::to_string(b) + ")");
    return a && b;
}

bool tests_bool_or(bool a, bool b)
{
    dbglog("tests_bool_and(" + std::to_string(a) + ", " + std::to_string(b) + ")");
    return a || b;
}

bool tests_bool_xor(bool a, bool b)
{
    dbglog("tests_bool_and(" + std::to_string(a) + ", " + std::to_string(b) + ")");
    return a ^ b;
}
