#include "lower.hpp"
#include "util.hpp"
#include "store.hpp"
#include "flatten.hpp"

#include <cassert>
#include <fmt/core.h>

namespace cmcpp
{

    std::vector<WasmVal> lower_flat_string(const CallContext &cx, const string_ptr &string)
    {
        auto [ptr, packed_length] = store_string_into_range(cx, string);
        return {(int32_t)ptr, (int32_t)packed_length};
    }

    std::vector<WasmVal> lower_flat_list(const CallContext &cx, const list_ptr &v, const Val &elem_type)
    {
        auto [ptr, length] = store_list_into_range(cx, v, elem_type);
        return {(int32_t)ptr, (int32_t)length};
    }

    std::vector<WasmVal> lower_flat_record(const CallContext &cx, const record_ptr &v, const std::vector<field_ptr> &fields)
    {
        std::vector<WasmVal> flat;
        for (auto f : fields)
        {
            auto flat_field = lower_flat(cx, v->find(f->label), f->v);
            flat.insert(flat.end(), flat_field.begin(), flat_field.end());
        }
        return flat;
    }

    uint32_t pack_flags_into_int(const flags_ptr &v, const std::vector<std::string> &labels)
    {
        uint32_t i = 0;
        uint32_t shift = 0;
        for (auto l : labels)
        {
            i |= (static_cast<uint32_t>(std::find(v->labels.begin(), v->labels.end(), l) != v->labels.end()) << shift);
            shift += 1;
        }
        return i;
    }

    std::vector<WasmVal> lower_flat_flags(const flags_ptr &v, const std::vector<std::string> &labels)
    {
        int32_t i = pack_flags_into_int(v, labels);
        std::vector<WasmVal> flat;
        for (size_t _ = 0; _ < num_i32_flags(labels); ++_)
        {
            flat.push_back((int32_t)(i & 0xffffffff));
            i >>= 32;
        }
        return flat;
    }

    /*
    def lower_flat_variant(cx, v, cases):
      case_index, case_value = match_case(v, cases)
      flat_types = flatten_variant(cases)
      assert(flat_types.pop(0) == 'i32')
      c = cases[case_index]
      if c.t is None:
        payload = []
      else:
        payload = lower_flat(cx, case_value, c.t)
        for i,(fv,have) in enumerate(zip(payload, flatten_type(c.t))):
          want = flat_types.pop(0)
          match (have, want):
            case ('f32', 'i32') : payload[i] = encode_float_as_i32(fv)
            case ('i32', 'i64') : payload[i] = fv
            case ('f32', 'i64') : payload[i] = encode_float_as_i32(fv)
            case ('f64', 'i64') : payload[i] = encode_float_as_i64(fv)
            case _              : assert(have == want)
      for _ in flat_types:
        payload.append(0)
      return [case_index] + payload
    */

    std::vector<WasmVal> lower_flat_variant(const CallContext &cx, const variant_ptr &v, const std::vector<case_ptr> &cases)
    {
        auto [case_index, case_value] = match_case(v, cases);
        std::vector<std::string> flat_types = flatten_variant(cases);
        assert(flat_types[0] == "i32");
        flat_types.erase(flat_types.begin());
        auto c = cases[case_index];
        std::vector<WasmVal> payload;
        if (c->v.has_value())
        {
            payload = lower_flat(cx, case_value.value(), c->v.value());
            auto vTypes = flatten_type(c->v.value());
            for (size_t i = 0; i < payload.size(); ++i)
            {
                auto fv = payload[i];
                auto have = vTypes[i];
                auto want = flat_types[0];
                flat_types.erase(flat_types.begin());
                if (have == "f32" && want == "i32")
                {
                    payload[i] = encode_float_as_i32(std::get<float32_t>(fv));
                }
                else if (have == "i32" && want == "i64")
                {
                    payload[i] = fv;
                }
                else if (have == "f32" && want == "i64")
                {
                    payload[i] = encode_float_as_i32(std::get<float32_t>(fv));
                }
                else if (have == "f64" && want == "i64")
                {
                    payload[i] = static_cast<int64_t>(encode_float_as_i64(std::get<float64_t>(fv)));
                }
                else
                {
                    assert(have == want);
                }
            }
        }
        for (auto _ : flat_types)
        {
            payload.push_back(0);
        }
        payload.insert(payload.begin(), case_index);
        return payload;
    }

    std::vector<WasmVal> lower_flat(const CallContext &cx, const Val &v, const Val &_t)
    {
        auto t = despecialize(_t);
        switch (valType(t))
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
            return lower_flat_list(cx, std::get<list_ptr>(v), std::get<list_ptr>(t)->lt);
        case ValType::Record:
            return lower_flat_record(cx, std::get<record_ptr>(v), std::get<record_ptr>(t)->fields);
        case ValType::Variant:
            return lower_flat_variant(cx, std::get<variant_ptr>(v), std::get<variant_ptr>(t)->cases);
        case ValType::Flags:
            return lower_flat_flags(std::get<flags_ptr>(v), std::get<flags_ptr>(t)->labels);
        // case ValType::Own:
        //     return lower_own(cx, std::get<OwnPtr>(v));
        // case ValType::Borrow:
        //     return lower_borrow(cx, std::get<BorrowPtr>(v));
        default:
            throw std::runtime_error(fmt::format("Invalid type:  {}", valTypeName(valType(t))));
        }
    }

}
