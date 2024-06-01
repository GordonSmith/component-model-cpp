#include "lower.hpp"
#include "util.hpp"
#include "store.hpp"
#include "flatten.hpp"

namespace cmcpp
{

    std::vector<WasmVal> lower_flat_list(const CallContext &cx, list_ptr list)
    {
        auto [ptr, length] = store_list_into_range(cx, list);
        return {(int32_t)ptr, (int32_t)length};
    }

    std::vector<WasmVal> lower_flat_string(const CallContext &cx, const string_ptr &string)
    {
        auto [ptr, packed_length] = store_string_into_range(cx, string);
        return {(int32_t)ptr, (int32_t)packed_length};
    }

    std::vector<WasmVal> lower_flat(const CallContext &cx, const Val &v)
    {
        switch (despecialize(valType(v)))
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
            return {char_to_i32(std::get<wchar_t>(v))};
        case ValType::String:
            return lower_flat_string(cx, std::get<string_ptr>(v));
        case ValType::List:
            return lower_flat_list(cx, std::get<list_ptr>(v));
        // case ValType::Record:
        //     return lower_flat_record(cx, reinterpret_cast<const Record &>(v)->fields);
        // case ValType::Variant:
        //     return lower_flat_variant(cx, reinterpret_cast<const Variant &>(v)->cases);
        // case ValType::Flags:
        //     return lower_flat_flags(cx, reinterpret_cast<const Flags &>(v)->labels);
        // case ValType::Own:
        //     return lower_own(cx, std::get<OwnPtr>(v));
        // case ValType::Borrow:
        //     return lower_borrow(cx, std::get<BorrowPtr>(v));
        default:
            throw std::runtime_error("Invalid type");
        }
    }

}
