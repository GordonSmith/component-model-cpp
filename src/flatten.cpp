#include "flatten.hpp"
#include "util.hpp"
#include "val.hpp"

// #include <any>
// #include <cassert>
// #include <cmath>
// #include <cstring>
// #include <map>
// #include <random>
// #include <sstream>
#include <fmt/core.h>

namespace cmcpp
{

    std::vector<std::string> flatten_types(const std::vector<Val> &vs)
    {
        std::vector<std::string> result;
        for (auto v : vs)
        {
            std::vector<std::string> flattened = flatten_type(v);
            result.insert(result.end(), flattened.begin(), flattened.end());
        }
        return result;
    }

    // std::vector<std::string> flatten_types(const std::vector<ValType> &ts)
    // {
    //     std::vector<std::string> result;
    //     for (ValType t : ts)
    //     {
    //         std::vector<std::string> flattened = flatten_type(t);
    //         result.insert(result.end(), flattened.begin(), flattened.end());
    //     }
    //     return result;
    // }

    std::vector<std::string> flatten_record(const std::vector<field_ptr> &fields)
    {
        std::vector<std::string> flat;
        for (auto f : fields)
        {
            auto flattened = flatten_type(f->v);
            flat.insert(flat.end(), flattened.begin(), flattened.end());
        }
        return flat;
    }
    std::vector<std::string> flatten_variant(const std::vector<case_ptr> &cases)
    {
        std::vector<std::string> flat;
        for (const auto &c : cases)
        {
            if (c->v.has_value())
            {
                auto fts = flatten_type(c->v.value());
                for (size_t i = 0; i < fts.size(); ++i)
                {
                    auto ft = fts[i];
                    if (i < flat.size())
                    {
                        flat[i] = join(flat[i], ft);
                    }
                    else
                    {
                        flat.push_back(ft);
                    }
                }
            }
        }
        std::vector<std::string> discriminantFlattened = flatten_type(discriminant_type(cases));
        flat.insert(flat.begin(), discriminantFlattened.begin(), discriminantFlattened.end());
        return flat;
    }

    std::vector<std::string> flatten_type(ValType kind)
    {
        switch (kind)
        {
        case ValType::Bool:
            return {"i32"};
        case ValType::U8:
        case ValType::U16:
        case ValType::U32:
            return {"i32"};
        case ValType::U64:
            return {"i64"};
        case ValType::S8:
        case ValType::S16:
        case ValType::S32:
            return {"i32"};
        case ValType::S64:
            return {"i64"};
        case ValType::F32:
            return {"f32"};
        case ValType::F64:
            return {"f64"};
        case ValType::Char:
            return {"i32"};
        case ValType::String:
            return {"i32", "i32"};
        case ValType::List:
            return {"i32", "i32"};
        default:
            throw std::runtime_error(fmt::format("Invalid type:  {}", valTypeName(kind)));
        }
    }

    std::vector<std::string> flatten_type(const Val &v)
    {
        switch (valType(v))
        {
        case ValType::Record:
            return flatten_record(std::get<record_ptr>(v)->fields);
        case ValType::Variant:
            return flatten_variant(std::get<variant_ptr>(v)->cases);
        // case ValType::Flags:
        //   return std::vector<std::string>(num_i32_flags(reinterpret_cast<const Flags &>(v).labels), "i32");
        // case ValType::Own:
        // case ValType::Borrow:
        //     return {"i32"};
        default:
            return flatten_type(valType(v));
        }
    }

}
