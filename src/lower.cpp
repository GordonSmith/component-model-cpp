#include "lower.hpp"
#include "util.hpp"
#include "store.hpp"

namespace cmcpp
{

    std::vector<WasmVal> lower_flat_list(const CallContext &cx, const Val &v)
    {
        auto [ptr, length] = store_list_into_range(cx, v.list());
        return {(int32_t)ptr, (int32_t)length};
    }

    std::vector<WasmVal> lower_flat_string(const CallContext &cx, const Val &v)
    {
        auto [ptr, packed_length] = store_string_into_range(cx, v);
        return {(int32_t)ptr, (int32_t)packed_length};
    }

    std::vector<WasmVal> lower_flat(const CallContext &cx, const Val &v)
    {
        switch (despecialize(v).kind())
        {
        case ValType::Bool:
            return {static_cast<int32_t>(v.b())};
        case ValType::U8:
            return {static_cast<int32_t>(v.u8())};
        case ValType::U16:
            return {static_cast<int32_t>(v.u16())};
        case ValType::U32:
            return {static_cast<int32_t>(v.u32())};
        case ValType::U64:
            return {static_cast<int64_t>(v.u64())};
        case ValType::S8:
            return {static_cast<int32_t>(v.s8())};
        case ValType::S16:
            return {static_cast<int32_t>(v.s16())};
        case ValType::S32:
            return {static_cast<int32_t>(v.s32())};
        case ValType::S64:
            return {static_cast<int64_t>(v.s64())};
        case ValType::Float32:
            return {static_cast<float32_t>(maybe_scramble_nan32(v.f32()))};
        case ValType::Float64:
            return {static_cast<float64_t>(maybe_scramble_nan64(v.f64()))};
        case ValType::Char:
            return {char_to_i32(v.c())};
        case ValType::String:
            return lower_flat_string(cx, v);
        case ValType::List:
            return lower_flat_list(cx, v);
        default:
            throw std::runtime_error("Invalid type");
        }
    }

    std::vector<WasmVal> lower_values(const CallContext &cx, const std::vector<Val> &vs, size_t max_flat, int *out_param)
    {
        if (vs.size() > max_flat)
        {
            TuplePtr tuple = std::make_shared<Tuple>();
            tuple->vs.insert(tuple->vs.end(), vs.begin(), vs.end());
            uint32_t ptr;
            if (out_param == nullptr)
            {
                ptr = cx.opts->realloc(0, 0, alignment(tuple), size(tuple));
            }
            else
            {
                //  TODO:  ptr = out_param.next('i32');
                std::abort();
            }
            if (ptr != align_to(ptr, alignment(ValType::Tuple)) || ptr + size(ValType::Tuple) > cx.opts->memory.size())
            {
                throw std::runtime_error("Out of bounds access");
            }
            store(cx, tuple, ptr);
            return {ptr};
        }
        else
        {
            std::vector<WasmVal> flat_vals;
            for (const Val &v : vs)
            {
                std::vector<WasmVal> temp = lower_flat(cx, v);
                flat_vals.insert(flat_vals.end(), temp.begin(), temp.end());
            }
            return flat_vals;
        }
    }
}