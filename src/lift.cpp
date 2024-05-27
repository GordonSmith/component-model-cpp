#include "lift.hpp"
#include "flatten.hpp"
#include "load.hpp"
#include "util.hpp"

#include <vector>
#include <variant>
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

    template <typename T>
    T lift_flat_unsigned(CoreValueIter &vi, int core_width, int t_width)
    {
        using SignedT = std::make_signed_t<T>;
        SignedT i = vi.next<SignedT>();
        assert(0 <= i < (1 << core_width));
        return i % (1 << t_width);
    }

    template <typename T>
    T lift_flat_signed(CoreValueIter &vi, int core_width, int t_width)
    {
        T i = vi.next<T>();
        assert(0 <= i && i < (1LL << 32));
        i %= (1LL << t_width);
        if (i >= (1LL << (t_width - 1)))
        {
            return i - (1LL << t_width);
        }
        return i;
    }

    StringPtr lift_flat_string(const CallContext &cx, CoreValueIter &vi)
    {
        auto ptr = vi.next<int32_t>();
        auto packed_length = vi.next<int32_t>();
        return load_string_from_range(cx, ptr, packed_length);
    }

    ListPtr lift_flat_list(const CallContext &cx, CoreValueIter &vi, ValType elem_type)
    {
        auto ptr = vi.next<int32_t>();
        auto length = vi.next<int32_t>();
        return load_list_from_range(cx, ptr, length, elem_type);
    }

    RecordPtr lift_flat_record(const CallContext &cx, CoreValueIter &vi, RecordPtr record)
    {
        for (Field &f : record->fields)
        {
            f.v = lift_flat(cx, vi, &f);
        }
        return record;
    }

    int32_t wrap_i64_to_i32(int64_t i)
    {
        assert(0 <= i && i < (1LL << 64));
        return i % (1LL << 32);
    }

    class CoerceValueIter : public CoreValueIter
    {
    public:
        std::vector<std::string>::const_iterator flat_types_iter;

        CoerceValueIter(CoreValueIter &vi, std::vector<std::string>::const_iterator flat_types_iter) : CoreValueIter(vi), flat_types_iter(flat_types_iter)
        {
        }

        template <typename T>
        T next()
        {
            const std::string have = *flat_types_iter;
            flat_types_iter++;

            if (have == "i32" && std::is_same<T, float32_t>())
            {
                return decode_i32_as_float(CoreValueIter::next<int32_t>());
            }
            else if (have == "i64" && std::is_same<T, int32_t>())
            {
                return wrap_i64_to_i32(CoreValueIter::next<int64_t>());
            }
            else if (have == "i64" && std::is_same<T, float32_t>())
            {
                return decode_i32_as_float(wrap_i64_to_i32(CoreValueIter::next<int64_t>()));
            }
            else if (have == "i64" && std::is_same<T, float64_t>())
            {
                return decode_i64_as_float(CoreValueIter::next<int64_t>());
            }
            else
            {
                return CoreValueIter::next<T>();
            }
        }
    };

    Val lift_flat_variant(const CallContext &cx, CoreValueIter &vi, const std::vector<Case> &cases)
    {
        auto flat_types_iter = flatten_variant(cases).begin();
        const std::string top = *flat_types_iter;
        flat_types_iter++;
        assert(top == "i32");
        int32_t case_index = vi.next<int32_t>();
        assert(case_index < cases.size());
        auto c = cases[case_index];
        Val v;
        if (c.v.has_value())
        {
            CoerceValueIter cvi(vi, flat_types_iter);
            v = lift_flat(cx, cvi, c.v.value());
        }
        for (; flat_types_iter != flatten_variant(cases).end(); flat_types_iter++)
        {
            vi.skip();
        }
        return v;
    }

    std::map<std::string, bool> lift_flat_flags(CoreValueIter &vi, const std::vector<std::string> &labels)
    {
        int32_t i = 0;
        int32_t shift = 0;
        for (size_t _ = 0; _ < num_i32_flags(labels); ++_)
        {
            i |= (vi.next<int32_t>() << shift);
            shift += 32;
        }
        return unpack_flags_from_int(i, labels);
    }

    Val lift_flat(const CallContext &cx, CoreValueIter &vi, const Val &v)
    {
        switch (despecialize(type(v)))
        {
        case ValType::Bool:
            return convert_int_to_bool(vi.next<int32_t>());
        case ValType::U8:
            return lift_flat_unsigned<uint32_t>(vi, 32, 8);
        case ValType::U16:
            return lift_flat_unsigned<uint32_t>(vi, 32, 16);
        case ValType::U32:
            return lift_flat_unsigned<uint32_t>(vi, 32, 32);
        case ValType::U64:
            return lift_flat_unsigned<uint64_t>(vi, 64, 64);
        case ValType::S8:
            return lift_flat_signed<int32_t>(vi, 32, 8);
        case ValType::S16:
            return lift_flat_signed<int32_t>(vi, 32, 16);
        case ValType::S32:
            return lift_flat_signed<int32_t>(vi, 32, 32);
        case ValType::S64:
            return lift_flat_signed<int64_t>(vi, 64, 64);
        case ValType::F32:
            return canonicalize_nan32(vi.next<float32_t>());
        case ValType::F64:
            return canonicalize_nan64(vi.next<float64_t>());
        case ValType::Char:
            return convert_i32_to_char(cx, vi.next<int32_t>());
        case ValType::String:
            return lift_flat_string(cx, vi);
        case ValType::List:
            return lift_flat_list(cx, vi, std::get<ListPtr>(v)->lt);
        case ValType::Record:
            return lift_flat_record(cx, vi, std::get<RecordPtr>(v));
        case ValType::Variant:
            return lift_flat_variant(cx, vi, std::get<VariantPtr>(v)->cases);
        // case ValType::Flags:
        //     return lift_flat_flags(cx, vi, std::get<FlagsPtr>(v)->labels);
        default:
            throw std::runtime_error("Invalid type");
        }
    }

    std::vector<Val> lift_values(const CallContext &cx, CoreValueIter &vi, const std::vector<Val> &ts)
    {
        auto flat_types = flatten_types(ts);
        if (flat_types.size() > MAX_FLAT_PARAMS)
        {
            auto ptr = vi.next<int32_t>();
            auto tuple_type = Tuple(ts);
            assert(ptr == align_to(ptr, alignment(tuple_type.t)));
            assert(ptr + elem_size(tuple_type.t) <= cx.opts->memory.size());
            return load_tuple(cx, ptr, ts)->vs;
        }
        else
        {
            std::vector<Val> retVal;
            for (auto t : ts)
            {
                retVal.push_back(lift_flat(cx, vi, t));
            }
            return retVal;
        }
    }
    std::vector<Val> lift_values(const CallContext &cx, const std::vector<WasmVal> &vs, const std::vector<Val> &ts)
    {
        CoreValueIter vi(vs);
        return lift_values(cx, vi, ts);
    }

    // std::vector<Val> lift_values(const CallContext &cx, const std::vector<WasmVal> &vs, const std::vector<std::pair<ValType, ValType>> &tps, int max_flat)
    // {
    //     std::vector<ValType> ts;
    //     for (auto tp : tps)
    //     {
    //         ts.push_back(tp.first);
    //     }
    //     auto flat_types = flatten_types(ts);
    //     std::vector<Val> retVal;
    //     if (flat_types.size() > max_flat)
    //     {
    //         // auto ptr = vi.next('i32');
    //         // auto tuple_type = Tuple(ts);
    //         // if (ptr != align_to(ptr, alignment(tuple_type)))
    //         // {
    //         //   throw std::runtime_error("Misaligned pointer");
    //         // }
    //         // if (ptr + size(tuple_type) > cx.opts.memory.size())
    //         // {
    //         //   throw std::runtime_error("Out of bounds access");
    //         // }
    //         // return load(cx, ptr, tuple_type).values();
    //     }
    //     else
    //     {
    //         for (size_t i = 0; i < tps.size(); ++i)
    //         {
    //             retVal.push_back(lift_flat(cx, vs[i], tps[i].first, tps[i].second));
    //         }
    //     }
    //     return retVal;
    // }
}