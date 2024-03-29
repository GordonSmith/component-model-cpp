#include "lift.hpp"
#include "flatten.hpp"
#include "load.hpp"
#include "util.hpp"

#include <cassert>

namespace cmcpp
{

    // uint32_t lift_own(const CallContext &_cx, int i, const Own &t)
    // {
    //     CallContext &cx = const_cast<CallContext &>(_cx);
    //     HandleElem h = cx.inst.handles.remove(t.rt, i);
    //     if (h.lend_count != 0)
    //     {
    //         throw std::runtime_error("Lend count is not zero");
    //     }
    //     if (!h.own)
    //     {
    //         throw std::runtime_error("Not owning");
    //     }
    //     return h.rep;
    // }

    // uint32_t lift_borrow(const CallContext &_cx, int i, const Borrow &t)
    // {
    //     CallContext &cx = const_cast<CallContext &>(_cx);
    //     HandleElem h = cx.inst.handles.get(t.rt, i);
    //     if (h.own)
    //     {
    //         cx.track_owning_lend(h);
    //     }
    //     return h.rep;
    // }

    uint32_t lift_flat_unsigned(int32_t v, int t_width)
    {
        uint32_t i = static_cast<uint32_t>(v);
        assert(0 <= i && i < (1UL << 32));
        return i % (1 << t_width);
    }

    uint64_t lift_flat_unsigned(int64_t v, int t_width)
    {
        uint64_t i = static_cast<uint64_t>(v);
        if (t_width < 64)
        {
            i %= (1ULL << t_width);
        }
        return i;
    }

    int32_t lift_flat_signed(int32_t i, int t_width)
    {
        assert(0 <= i && i < (1LL << 32));
        i %= (1LL << t_width);
        if (i >= (1LL << (t_width - 1)))
        {
            return i - (1LL << t_width);
        }
        return i;
    }

    int64_t lift_flat_signed(int64_t i, int t_width)
    {
        i %= (1LL << t_width);
        if (i >= (1LL << (t_width - 1)))
        {
            return i - (1LL << t_width);
        }
        return i;
    }

    Val lift_flat(const CallContext &cx, const WasmVal &v, const ValType &t, const ValType &lt = ValType::Unknown)
    {
        switch (despecialize(t))
        {
        case ValType::Bool:
            return convert_int_to_bool((int32_t)v);
        case ValType::U8:
            return lift_flat_unsigned((int32_t)v, 8);
        case ValType::U16:
            return lift_flat_unsigned((int32_t)v, 16);
        case ValType::U32:
            return lift_flat_unsigned((int32_t)v, 32);
        case ValType::U64:
            return lift_flat_unsigned((int64_t)v, 64);
        case ValType::S8:
            return lift_flat_signed((int32_t)v, 8);
        case ValType::S16:
            return lift_flat_signed((int32_t)v, 16);
        case ValType::S32:
            return lift_flat_signed((int32_t)v, 32);
        case ValType::S64:
            return lift_flat_signed((int64_t)v, 64);
        case ValType::Float32:
            return canonicalize_nan32(v);
        case ValType::Float64:
            return canonicalize_nan64(v);
        case ValType::Char:
            return convert_i32_to_char(v);
        case ValType::String:
            return load_string(cx, (int32_t)v);
        case ValType::List:
            return load_list(cx, (int32_t)v, lt);
        default:
            throw std::runtime_error("Invalid type");
        }
    }

    std::vector<Val> lift_values(const CallContext &cx, const std::vector<WasmVal> &vs, const std::vector<ValType> &ts, int max_flat)
    {
        auto flat_types = flatten_types(ts);
        std::vector<Val> retVal;
        if (flat_types.size() > max_flat)
        {
            // auto ptr = vi.next('i32');
            // auto tuple_type = Tuple(ts);
            // if (ptr != align_to(ptr, alignment(tuple_type)))
            // {
            //   throw std::runtime_error("Misaligned pointer");
            // }
            // if (ptr + size(tuple_type) > cx.opts.memory.size())
            // {
            //   throw std::runtime_error("Out of bounds access");
            // }
            // return load(cx, ptr, tuple_type).values();
        }
        else
        {
            for (size_t i = 0; i < ts.size(); ++i)
            {
                retVal.push_back(lift_flat(cx, vs[i], ts[i]));
            }
        }
        return retVal;
    }

    std::vector<Val> lift_values(const CallContext &cx, const std::vector<WasmVal> &vs, const std::vector<std::pair<ValType, ValType>> &tps, int max_flat)
    {
        std::vector<ValType> ts;
        for (auto tp : tps)
        {
            ts.push_back(tp.first);
        }
        auto flat_types = flatten_types(ts);
        std::vector<Val> retVal;
        if (flat_types.size() > max_flat)
        {
            // auto ptr = vi.next('i32');
            // auto tuple_type = Tuple(ts);
            // if (ptr != align_to(ptr, alignment(tuple_type)))
            // {
            //   throw std::runtime_error("Misaligned pointer");
            // }
            // if (ptr + size(tuple_type) > cx.opts.memory.size())
            // {
            //   throw std::runtime_error("Out of bounds access");
            // }
            // return load(cx, ptr, tuple_type).values();
        }
        else
        {
            for (size_t i = 0; i < tps.size(); ++i)
            {
                retVal.push_back(lift_flat(cx, vs[i], tps[i].first, tps[i].second));
            }
        }
        return retVal;
    }

}