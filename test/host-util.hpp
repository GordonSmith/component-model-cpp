#include <utility>
#include "context.hpp"

using namespace cmcpp;

void trap(const char *msg = "");
std::pair<char8_t *, size_t> encodeTo(void *dest, const char *src, uint32_t byte_len, Encoding from_encoding, PythonEncoding to_encoding);
std::pair<char *, uint32_t> decodeFrom(const void *src, uint32_t byte_len, PythonEncoding from_encoding, Encoding to_encoding);
