#include "lift.hpp"
#include "flatten.hpp"
#include "load.hpp"
#include "util.hpp"

#include <vector>
#include <variant>
#include <cassert>
#include <cmath>
#include <functional>

#include <fmt/core.h>

namespace cmcpp
{

    template <typename T, typename R>
    R lift_flat_unsigned(const CoreValueIter &vi, int core_width, int t_width)
    {
        using SignedT = std::make_signed_t<T>;
        T i = vi.next<SignedT>();
        if (t_width == 64)
        {
            return (R)i;
        }
        assert(0 <= i && i < (1ULL << core_width));
        return R(i % (1ULL << t_width));
    }

    template <typename T, typename R>
    R lift_flat_signed(const CoreValueIter &vi, int core_width, int t_width)
    {
        using UnsignedT = std::make_unsigned_t<T>;
        T i = vi.next<T>();
        if (t_width == 64)
        {
            return (R)i;
        }
        assert(0 <= (UnsignedT)i && (UnsignedT)i < (1LL << 32));
        i %= (1LL << t_width);
        if (i >= (1LL << (t_width - 1)))
        {
            return i - (1LL << t_width);
        }
        return i;
    }

    std::shared_ptr<string_t> lift_flat_string(const CallContext &cx, const CoreValueIter &vi)
    {
        auto ptr = vi.next<int32_t>();
        auto packed_length = vi.next<int32_t>();
        return load_string_from_range(cx, ptr, packed_length);
    }

    Val lift_flat(const CallContext &cx, const CoreValueIter &vi, ValType t)
    {
        switch (t)
        {
        case ValType::Bool:
            return convert_int_to_bool(vi.next<int32_t>());
        case ValType::U8:
            return lift_flat_unsigned<uint32_t, uint8_t>(vi, 32, 8);
        case ValType::U16:
            return lift_flat_unsigned<uint32_t, uint16_t>(vi, 32, 16);
        case ValType::U32:
            return lift_flat_unsigned<uint32_t, uint32_t>(vi, 32, 32);
        case ValType::U64:
            return lift_flat_unsigned<uint64_t, uint64_t>(vi, 64, 64);
        case ValType::S8:
            return lift_flat_signed<int32_t, int8_t>(vi, 32, 8);
        case ValType::S16:
            return lift_flat_signed<int32_t, int16_t>(vi, 32, 16);
        case ValType::S32:
            return lift_flat_signed<int32_t, int32_t>(vi, 32, 32);
        case ValType::S64:
            return lift_flat_signed<int64_t, int64_t>(vi, 64, 64);
        case ValType::F32:
            return canonicalize_nan32(vi.next<float32_t>());
        case ValType::F64:
            return canonicalize_nan64(vi.next<float64_t>());
        case ValType::Char:
            return convert_i32_to_char(cx, vi.next<int32_t>());
        case ValType::String:
            return lift_flat_string(cx, vi);
        // case ValType::List:
        //     return lift_flat_list(cx, vi, std::dynamic_pointer_cast<List>(v)->lt);
        // case ValType::Record:
        //     return lift_flat_record(cx, vi, std::dynamic_pointer_cast<Record>(v)->fields);
        // case ValType::Variant:
        //     return lift_flat_variant(cx, vi, std::dynamic_pointer_cast<Variant>(v)->cases);
        // case ValType::Flags:
        //     return lift_flat_flags(cx, vi, std::get<Flags>(v)->labels);
        default:
            throw std::runtime_error(fmt::format("Invalid type:  {}", valTypeName(t)));
        }
    }

}