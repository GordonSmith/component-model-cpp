#include "lower.hpp"
#include "util.hpp"
#include "store.hpp"
#include "flatten.hpp"

namespace cmcpp
{

    std::vector<WasmVal> lower_flat_list(const CallContext &cx, ListPtr list)
    {
        auto [ptr, length] = store_list_into_range(cx, list);
        return {(int32_t)ptr, (int32_t)length};
    }

    std::vector<WasmVal> lower_flat_string(const CallContext &cx, StringPtr string)
    {
        auto [ptr, packed_length] = store_string_into_range(cx, string);
        return {(int32_t)ptr, (int32_t)packed_length};
    }

    std::vector<WasmVal> lower_flat(const CallContext &cx, const Val &v)
    {
        switch (despecialize(type(v)))
        {
        case ValType::Bool:
            return {static_cast<int32_t>(std::get<bool>(v))};
        case ValType::U8:
            return {static_cast<int32_t>(std::get<uint8_t>(v))};
        case ValType::U16:
            return {static_cast<int32_t>(std::get<uint16_t>(v))};
        case ValType::U32:
            return {static_cast<int32_t>(std::get<uint32_t>(v))};
        case ValType::U64:
            return {static_cast<int64_t>(std::get<uint64_t>(v))};
        case ValType::S8:
            return {static_cast<int32_t>(std::get<int8_t>(v))};
        case ValType::S16:
            return {static_cast<int32_t>(std::get<int16_t>(v))};
        case ValType::S32:
            return {static_cast<int32_t>(std::get<int32_t>(v))};
        case ValType::S64:
            return {static_cast<int64_t>(std::get<int64_t>(v))};
        case ValType::F32:
            return {static_cast<float32_t>(maybe_scramble_nan32(std::get<float32_t>(v)))};
        case ValType::F64:
            return {static_cast<float64_t>(maybe_scramble_nan64(std::get<float64_t>(v)))};
        case ValType::Char:
            return {char_to_i32(std::get<char>(v))};
        case ValType::String:
            return lower_flat_string(cx, std::get<StringPtr>(v));
        case ValType::List:
            return lower_flat_list(cx, std::get<ListPtr>(v));
        case ValType::Record:
            // return lower_flat_record(cx, std::get<RecordPtr>(v));
        case ValType::Variant:
            // return lower_flat_variant(cx, std::get<const Variant &>(v));
        case ValType::Flags:
            // return lower_flat_flags(cx, std::get<FlagsPtr>(v));
        case ValType::Own:
            // return lower_own(cx, std::get<OwnPtr>(v));
        case ValType::Borrow:
            // return lower_borrow(cx, std::get<BorrowPtr>(v));
        default:
            throw std::runtime_error("Invalid type");
        }
    }

    Val adapt(const Val &v)
    {
        if (auto str = std::get_if<std::string>(&v))
        {
            return std::make_shared<String>((const char8_t *)str->c_str(), str->size());
        }
        return v;
    }

    std::vector<WasmVal> lower_values(const CallContext &cx, const std::vector<Val> &vs, std::optional<CoreValueIter> out_param)
    {
        auto flat_types = flatten_types(vs);
        if (flat_types.size() > MAX_FLAT_PARAMS)
        {
            TuplePtr tuple = std::make_shared<Tuple>();
            for (Val v : vs)
            {
                tuple->vs.push_back(adapt(v));
            }
            uint32_t ptr;
            if (!out_param.has_value())
            {
                ptr = cx.opts->realloc(0, 0, alignment(tuple), elem_size(tuple->t));
            }
            else
            {
                ptr = out_param.value().next<int32_t>();
                std::abort();
            }
            if (ptr != align_to(ptr, alignment(ValType::Tuple)) || ptr + elem_size(ValType::Tuple) > cx.opts->memory.size())
            {
                throw std::runtime_error("Out of bounds access");
            }
            store(cx, tuple, ptr);
            return {ptr};
        }
        else
        {
            std::vector<WasmVal> flat_vals;
            for (Val v : vs)
            {
                std::vector<WasmVal> temp = lower_flat(cx, adapt(v));
                flat_vals.insert(flat_vals.end(), temp.begin(), temp.end());
            }
            return flat_vals;
        }
    }

    Val lower_hostVal(const CallContext &cx, std::string hostVal)
    {
        auto retVal = std::make_shared<String>(hostVal);
        return retVal;
    }

}
