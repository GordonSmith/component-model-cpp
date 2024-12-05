#include <utility>
#include "context.hpp"

using namespace cmcpp;

std::pair<char8_t *, size_t> encodeTo(void *dest, const char *src, uint32_t byte_len, GuestEncoding encoding);
std::pair<char *, uint32_t> decodeFrom(const void *src, uint32_t byte_len, HostEncoding encoding);
