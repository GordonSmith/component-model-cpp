#include <utility>
#include "context.hpp"

using namespace cmcpp;

void trap(const char *msg = "");

const char * encodingToICU(const Encoding encoding);

std::pair<void *, size_t> convert(void *dest, uint32_t dest_byte_len, const void *src, uint32_t src_byte_len, Encoding from_encoding, Encoding to_encoding);
 
