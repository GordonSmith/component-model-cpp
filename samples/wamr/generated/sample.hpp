#pragma once

#ifndef GENERATED_SAMPLE_HPP_HPP
#define GENERATED_SAMPLE_HPP_HPP

#include <cmcpp.hpp>

// Generated host function declarations from WIT
// - 'host' namespace: Guest imports (host implements these)
// - 'guest' namespace: Guest exports (guest implements these, host calls them)

namespace host {

// Interface: booleans
// Package: example:sample
namespace booleans {

cmcpp::bool_t and_(cmcpp::bool_t a, cmcpp::bool_t b);

} // namespace booleans

// Interface: floats
// Package: example:sample
namespace floats {

cmcpp::float64_t add(cmcpp::float64_t a, cmcpp::float64_t b);

} // namespace floats

// Interface: strings
// Package: example:sample
namespace strings {

cmcpp::string_t reverse(cmcpp::string_t a);

uint32_t lots(cmcpp::string_t p1, cmcpp::string_t p2, cmcpp::string_t p3, cmcpp::string_t p4, cmcpp::string_t p5, cmcpp::string_t p6, cmcpp::string_t p7, cmcpp::string_t p8, cmcpp::string_t p9, cmcpp::string_t p10, cmcpp::string_t p11, cmcpp::string_t p12, cmcpp::string_t p13, cmcpp::string_t p14, cmcpp::string_t p15, cmcpp::string_t p16, cmcpp::string_t p17);

} // namespace strings

// Interface: lists
// Package: example:sample
namespace lists {

using v = cmcpp::variant_t<cmcpp::bool_t, cmcpp::string_t>;

cmcpp::list_t<cmcpp::string_t> filter_bool(cmcpp::list_t<v> a);

} // namespace lists

// Interface: logging
// Package: example:sample
namespace logging {

void log_bool(cmcpp::bool_t a, cmcpp::string_t s);

void log_u32(uint32_t a, cmcpp::string_t s);

void log_u64(uint64_t a, cmcpp::string_t s);

void log_f32(cmcpp::float32_t a, cmcpp::string_t s);

void log_f64(cmcpp::float64_t a, cmcpp::string_t s);

void log_str(cmcpp::string_t a, cmcpp::string_t s);

} // namespace logging

// Standalone function: void-func
// Package: example:sample
void void_func();


} // namespace host

namespace guest {

// Interface: booleans
// Package: example:sample
namespace booleans {

// Guest function signature for use with guest_function<and_t>()
using and_t = cmcpp::bool_t(cmcpp::bool_t, cmcpp::bool_t);

} // namespace booleans

// Interface: floats
// Package: example:sample
namespace floats {

// Guest function signature for use with guest_function<add_t>()
using add_t = cmcpp::float64_t(cmcpp::float64_t, cmcpp::float64_t);

} // namespace floats

// Interface: strings
// Package: example:sample
namespace strings {

// Guest function signature for use with guest_function<reverse_t>()
using reverse_t = cmcpp::string_t(cmcpp::string_t);

// Guest function signature for use with guest_function<lots_t>()
using lots_t = uint32_t(cmcpp::string_t, cmcpp::string_t, cmcpp::string_t, cmcpp::string_t, cmcpp::string_t, cmcpp::string_t, cmcpp::string_t, cmcpp::string_t, cmcpp::string_t, cmcpp::string_t, cmcpp::string_t, cmcpp::string_t, cmcpp::string_t, cmcpp::string_t, cmcpp::string_t, cmcpp::string_t, cmcpp::string_t);

} // namespace strings

// Interface: tuples
// Package: example:sample
namespace tuples {

// Guest function signature for use with guest_function<reverse_t>()
using reverse_t = cmcpp::tuple_t<cmcpp::string_t, cmcpp::bool_t>(cmcpp::tuple_t<cmcpp::bool_t, cmcpp::string_t>);

} // namespace tuples

// Interface: lists
// Package: example:sample
namespace lists {

using v = cmcpp::variant_t<cmcpp::bool_t, cmcpp::string_t>;

// Guest function signature for use with guest_function<filter_bool_t>()
using filter_bool_t = cmcpp::list_t<cmcpp::string_t>(cmcpp::list_t<v>);

} // namespace lists

// Interface: variants
// Package: example:sample
namespace variants {

using v = cmcpp::variant_t<cmcpp::bool_t, uint32_t>;

// Guest function signature for use with guest_function<variant_func_t>()
using variant_func_t = v(v);

} // namespace variants

// Interface: enums
// Package: example:sample
namespace enums {

enum class e {
    a,
    b,
    c
};

// Guest function signature for use with guest_function<enum_func_t>()
using enum_func_t = e(e);

} // namespace enums

// Standalone function: void-func
// Package: example:sample
// Guest function signature for use with guest_function<void_func_t>()
using void_func_t = void();


// Standalone function: ok-func
// Package: example:sample
// Guest function signature for use with guest_function<ok_func_t>()
using ok_func_t = cmcpp::result_t<uint32_t, cmcpp::string_t>(uint32_t, uint32_t);


// Standalone function: err-func
// Package: example:sample
// Guest function signature for use with guest_function<err_func_t>()
using err_func_t = cmcpp::result_t<uint32_t, cmcpp::string_t>(uint32_t, uint32_t);


// Standalone function: option-func
// Package: example:sample
// Guest function signature for use with guest_function<option_func_t>()
using option_func_t = cmcpp::option_t<uint32_t>(cmcpp::option_t<uint32_t>);


} // namespace guest

#endif // GENERATED_SAMPLE_HPP_HPP
