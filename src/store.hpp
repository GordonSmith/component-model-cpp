#ifndef STORE_HPP
#define STORE_HPP

#include "context.hpp"
#include "val.hpp"

void store(const CallContext &cx, const Val &v, uint32_t ptr);
std::tuple<uint32_t, uint32_t> store_string_into_range(const CallContext &cx, const Val &v, HostEncoding src_encoding = HostEncoding::Utf8);

#endif