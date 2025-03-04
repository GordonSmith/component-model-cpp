#include "util.hpp"
#include "float.hpp"

namespace cmcpp
{

    void trap_if(const CallContext &cx, bool condition, const char *message)
    {
        if (condition)
        {
            cx.trap(message);
        }
    }

    ValType despecialize(const ValType t)
    {
        switch (t)
        {
        case ValType::Tuple:
            return ValType::Tuple;
        case ValType::Enum:
            return ValType::Variant;
        case ValType::Option:
            return ValType::Variant;
        case ValType::Result:
            return ValType::Variant;
        }
        return t;
    }

    bool convert_int_to_bool(uint8_t i)
    {
        return i > 0;
    }

    char_t convert_i32_to_char(const CallContext &cx, int32_t i)
    {
        trap_if(cx, i >= 0x110000);
        trap_if(cx, 0xD800 <= i && i <= 0xDFFF);
        return i;
    }

    int32_t char_to_i32(const CallContext &cx, const char_t &v)
    {
        uint32_t retVal = v;
        trap_if(cx, retVal >= 0x110000);
        trap_if(cx, 0xD800 <= retVal && retVal <= 0xDFFF);
        return retVal;
    }

    int32_t wrap_i64_to_i32(int64_t x)
    {
        if (x < std::numeric_limits<int32_t>::lowest() || x > std::numeric_limits<int32_t>::max())
        {
            return std::numeric_limits<int32_t>::lowest();
        }
        return static_cast<int32_t>(x);
    }

    CoreValueIter::CoreValueIter(const WasmValVector &v) : it(v.begin()), end(v.end())
    {
    }

    WasmVal CoreValueIter::next(const WasmValType &t) const
    {
        assert(it != end);
        return *it++;
    }

    CoerceValueIter::CoerceValueIter(const CoreValueIter &vi, WasmValTypeVector &flat_types) : CoreValueIter({}), vi(vi), flat_types(flat_types)
    {
    }

    WasmVal CoerceValueIter::next(const WasmValType &want) const
    {
        auto have = flat_types.front();
        flat_types.erase(flat_types.begin());
        auto x = vi.next(have);
        if (have == WasmValType::i32 && want == WasmValType::f32)
        {
            return float_::decode_i32_as_float(std::get<int32_t>(x));
        }
        else if (have == WasmValType::i64 && want == WasmValType::i32)
        {
            return wrap_i64_to_i32(std::get<int64_t>(x));
        }
        else if (have == WasmValType::i64 && want == WasmValType::f32)
        {
            return float_::decode_i32_as_float(wrap_i64_to_i32(std::get<int64_t>(x)));
        }
        else if (have == WasmValType::i64 && want == WasmValType::f64)
        {
            return float_::decode_i64_as_float(std::get<int64_t>(x));
        }
        else
        {
            assert(have == want);
            return x;
        }
    }
}
