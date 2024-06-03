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
        auto xxx = wasmValType(SignedT());
        T i = vi.next(SignedT());
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
        T i = vi.next(T());
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

    string_ptr lift_flat_string(const CallContext &cx, const CoreValueIter &vi)
    {
        auto ptr = vi.next(int32_t());
        auto packed_length = vi.next(int32_t());
        return load_string_from_range(cx, ptr, packed_length);
    }

    list_ptr lift_flat_list(const CallContext &cx, const CoreValueIter &vi, const Val &elem_type)
    {
        auto ptr = vi.next(int32_t());
        auto length = vi.next(int32_t());
        return load_list_from_range(cx, ptr, length, elem_type);
    }

    record_ptr lift_flat_record(const CallContext &cx, const CoreValueIter &vi, const std::vector<field_ptr> fields)
    {
        auto r = std::make_shared<record_t>();
        for (auto f : fields)
        {
            r->fields.push_back(std::make_shared<field_t>(f->label, lift_flat(cx, vi, f->v)));
        }
        return r;
    }

    flags_ptr unpack_flags_from_int(int i, const std::vector<std::string> &labels)
    {
        std::vector<std::string> flags;
        for (const auto &l : labels)
        {
            if (i & 1)
            {
                flags.push_back(l);
            }
            i >>= 1;
        }
        return std::make_shared<flags_t>(flags);
    }

    int32_t wrap_i64_to_i32(int64_t i)
    {
        return i % (1LL << 32);
    }

    class CoerceValueIter : public CoreValueIter
    {
    public:
        mutable std::vector<std::string>::const_iterator flat_types_iter;

        CoerceValueIter(const CoreValueIter &vi, std::vector<std::string>::const_iterator flat_types_iter) : CoreValueIter(vi), flat_types_iter(flat_types_iter)
        {
        }

        template <typename T>
        T _next() const
        {
            const std::string have = *flat_types_iter;
            ++flat_types_iter;
            if (have == "i32" && std::is_same<T, float32_t>())
            {
                return decode_i32_as_float(CoreValueIter::next(int32_t()));
            }
            else if (have == "i64" && std::is_same<T, int32_t>())
            {
                return wrap_i64_to_i32(CoreValueIter::next(int64_t()));
            }
            else if (have == "i64" && std::is_same<T, float32_t>())
            {
                return decode_i32_as_float(wrap_i64_to_i32(CoreValueIter::next(int64_t())));
            }
            else if (have == "i64" && std::is_same<T, float64_t>())
            {
                return decode_i64_as_float(CoreValueIter::next(int64_t()));
            }
            else
            {
                return CoreValueIter::next(T());
            }
        }

        virtual int32_t next(int32_t _) const
        {
            return _next<int32_t>();
        }

        virtual int64_t next(int64_t _) const
        {
            return _next<int64_t>();
        }

        virtual float32_t next(float32_t _) const
        {
            return _next<float32_t>();
        }

        virtual float64_t next(float64_t _) const
        {
            return _next<float64_t>();
        }
    };

    variant_ptr lift_flat_variant(const CallContext &cx, const CoreValueIter &vi, const std::vector<case_ptr> &cases)
    {
        std::vector<std::string> flat_types = flatten_variant(cases);
        auto flat_types_iter = flat_types.begin();
        const std::string top = *flat_types_iter;
        assert(top == "i32");
        ++flat_types_iter;
        int32_t case_index = vi.next(int32_t());
        assert(case_index < cases.size());
        auto c = cases[case_index];
        auto v = std::make_shared<variant_t>();
        if (c->v.has_value())
        {
            CoerceValueIter cvi(vi, flat_types_iter);
            auto val = lift_flat(cx, cvi, c->v.value());
            v->cases.push_back(std::make_shared<case_t>(c->label, val));
        }
        for (; flat_types_iter != flat_types.end(); flat_types_iter++)
        {
            vi.skip();
        }
        return v;
    }

    flags_ptr lift_flat_flags(const CoreValueIter &vi, const std::vector<std::string> &labels)
    {
        int32_t i = 0;
        int32_t shift = 0;
        for (size_t _ = 0; _ < num_i32_flags(labels); ++_)
        {
            i |= (vi.next(int32_t()) << shift);
            shift += 32;
        }
        return unpack_flags_from_int(i, labels);
    }

    Val lift_flat(const CallContext &cx, const CoreValueIter &vi, const Val &_t)
    {
        auto t = despecialize(_t);
        switch (valType(t))
        {
        case ValType::Bool:
            return convert_int_to_bool(vi.next(int32_t()));
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
            return canonicalize_nan32(vi.next(float32_t()));
        case ValType::F64:
            return canonicalize_nan64(vi.next(float64_t()));
        case ValType::Char:
            return convert_i32_to_char(cx, vi.next(int32_t()));
        case ValType::String:
            return lift_flat_string(cx, vi);
        case ValType::List:
            return lift_flat_list(cx, vi, std::get<list_ptr>(t)->lt);
        case ValType::Record:
            return lift_flat_record(cx, vi, std::get<record_ptr>(t)->fields);
        case ValType::Variant:
            return lift_flat_variant(cx, vi, std::get<variant_ptr>(t)->cases);
        case ValType::Flags:
            return lift_flat_flags(vi, std::get<flags_ptr>(t)->labels);
        default:
            throw std::runtime_error(fmt::format("Invalid type:  {}", valTypeName(valType(t))));
        }
    }

}