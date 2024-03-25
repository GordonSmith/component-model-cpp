#include "util.hpp"

void dbglog(const std::string str)
{
    tests_string_t msg;
    tests_string_dup(&msg, str.c_str());
    tests_dbglog(&msg);
}
