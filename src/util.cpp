#include "util.hpp"

#include <cassert>
#include <cmath>
#include <cstring>
#include <random>
#include <sstream>
#include <limits>
#include <codecvt>

namespace cmcpp
{

    ValType despecialize(const ValType t)
    {
        switch (t)
        {
        case ValType::Tuple:
            return ValType::Record;
        case ValType::Enum:
            return ValType::Variant;
        case ValType::Option:
            return ValType::Variant;
        case ValType::Result:
            return ValType::Variant;
        }
        return t;
    }

    Val despecialize(const Val &v)
    {
        switch (type(v))
        {
        // case ValType::Tuple:
        // {
        //     auto r = std::make_shared<Record>();
        //     for (size_t i = 0; i < reinterpret_cast<const Tuple &>(v).vs.size(); ++i)
        //     {
        //         auto f = std::make_shared<Field>(fmt::format("{}", i), despecialize(v->t));
        //         f->v = reinterpret_cast<const Tuple &>(v).vs[i];
        //         r->fields.push_back(f);
        //     }
        //     return r;
        // }
        // case ValType::Enum:
        // {
        //     std::vector<std::shared_ptr<Case>> cases;
        //     for (const auto &label : reinterpret_cast<const Enum &>(v).labels)
        //     {
        //         auto c = std::make_shared<Case>(label);
        //         cases.push_back(c);
        //     }
        //     return std::make_shared<Variant>(cases);
        // }
        // case ValType::Option:
        //     return std::make_shared<Variant>(std::vector<std::shared_ptr<Case>>({std::make_shared<Case>("None"), std::make_shared<Case>("Some", std::dynamic_pointer_cast<Option>(v)->v)}));
        // case ValType::Result:
        //     return std::make_shared<Variant>(std::vector<std::shared_ptr<Case>>({std::make_shared<Case>("Ok", std::dynamic_pointer_cast<Result>(v)->ok), std::make_shared<Case>("Error", std::dynamic_pointer_cast<Result>(v)->error)}));
        default:
            return v;
        }
    }

    bool convert_int_to_bool(int32_t i)
    {
        return i != 0;
    }

    wchar_t convert_i32_to_char(const CallContext &cx, int32_t i)
    {
        // assert(i >= 0);
        // assert(i < 0x110000);
        // assert(!(0xD800 <= i && i <= 0xDFFF));
        return static_cast<uint32_t>(i);
    }

    uint32_t align_to(uint32_t ptr, uint32_t alignment)
    {
        return (ptr + alignment - 1) & ~(alignment - 1);
    }

    bool isAligned(uint32_t ptr, uint32_t alignment) { return (ptr & (alignment - 1)) == 0; }

    std::shared_ptr<Bool> convert_int_to_bool(uint8_t i)
    {
        return std::make_shared<Bool>(i > 0);
    }

    float32_t canonicalize_nan32(float32_t f)
    {
        if (std::isnan(f))
        {
            f = CANONICAL_FLOAT32_NAN;
            assert(std::isnan(f));
        }
        return f;
    }

    float64_t canonicalize_nan64(float64_t f)
    {
        if (std::isnan(f))
        {
            f = CANONICAL_FLOAT64_NAN;
            assert(std::isnan(f));
        }
        return f;
    }

    float32_t core_f32_reinterpret_i32(int32_t i)
    {
        float f;
        std::memcpy(&f, &i, sizeof f);
        return f;
    }

    float64_t core_f64_reinterpret_i64(int64_t i)
    {
        double d;
        std::memcpy(&d, &i, sizeof d);
        return d;
    }

    int32_t char_to_i32(wchar_t c) { return static_cast<uint32_t>(c); }

    template <typename T>
    T random_nan_bits(int bits, int quiet_bits)
    {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> distrib(1 << quiet_bits, (1 << bits) - 1);
        return distrib(gen);
    }

    float32_t maybe_scramble_nan32(float32_t f)
    {
        if (std::isnan(f))
        {
            if (DETERMINISTIC_PROFILE)
            {
                f = CANONICAL_FLOAT32_NAN;
            }
            else
            {
                f = core_f32_reinterpret_i32(random_nan_bits<int32_t>(32, 8));
            }
            assert(std::isnan(f));
        }
        return f;
    }

    float64_t maybe_scramble_nan64(float64_t f)
    {
        if (std::isnan(f))
        {
            if (DETERMINISTIC_PROFILE)
            {
                f = CANONICAL_FLOAT32_NAN;
            }
            else
            {
                f = core_f64_reinterpret_i64(random_nan_bits<int64_t>(64, 11));
            }
            assert(std::isnan(f));
        }
        return f;
    }

    std::pair<char8_t *, uint32_t> decode(void *src, uint32_t byte_len, HostEncoding encoding)
    {
        switch (encoding)
        {
        case HostEncoding::Utf8:
        case HostEncoding::Latin1:
            return {reinterpret_cast<char8_t *>(src), byte_len};
        case HostEncoding::Latin1_Utf16:
            assert(false);
            break;
        default:
            throw std::runtime_error("Invalid encoding");
        }
    }

    size_t encodeTo(void *dest, const char8_t *src, uint32_t byte_len, GuestEncoding encoding)
    {
        switch (encoding)
        {
        case GuestEncoding::Utf8:
        case GuestEncoding::Latin1:
            std::memcpy(dest, src, byte_len);
            return byte_len;
        case GuestEncoding::Utf16le:
            assert(false);
            break;
        default:
            throw std::runtime_error("Invalid encoding");
        }
    }

    CoreValueIter::CoreValueIter(const std::vector<WasmVal> &values) : values(values) {}

    void CoreValueIter::skip()
    {
        ++i;
    }
}
