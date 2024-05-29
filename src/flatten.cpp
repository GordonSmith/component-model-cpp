#include "flatten.hpp"
#include "util.hpp"

// #include <any>
// #include <cassert>
// #include <cmath>
// #include <cstring>
// #include <map>
// #include <random>
// #include <sstream>

namespace cmcpp
{
    std::vector<std::string> flatten_type(ValType kind);

    // CoreFuncType flatten_functype(FuncTypePtr ft, std::string context)
    // {
    //     std::vector<std::string> flat_params = flatten_types(ft->param_types());
    //     if (flat_params.size() > MAX_FLAT_PARAMS)
    //     {
    //         flat_params = {"i32"};
    //     }

    //     std::vector<std::string> flat_results = flatten_types(ft->result_types());
    //     if (flat_results.size() > MAX_FLAT_RESULTS)
    //     {
    //         if (context == "lift")
    //         {
    //             flat_results = {"i32"};
    //         }
    //         else if (context == "lower")
    //         {
    //             flat_params.push_back("i32");
    //             flat_results = {};
    //         }
    //     }

    //     return CoreFuncType(flat_params, flat_results);
    // }

    std::vector<std::string> flatten_types(const std::vector<Val> &vs)
    {
        std::vector<std::string> result;
        for (auto v : vs)
        {
            std::vector<std::string> flattened = flatten_type(type(v));
            result.insert(result.end(), flattened.begin(), flattened.end());
        }
        return result;
    }

    std::vector<std::string> flatten_types(const std::vector<ValType> &ts)
    {
        std::vector<std::string> result;
        for (ValType t : ts)
        {
            std::vector<std::string> flattened = flatten_type(t);
            result.insert(result.end(), flattened.begin(), flattened.end());
        }
        return result;
    }

    std::vector<std::string> flatten_type(ValType kind)
    {
        if (kind == typeid(Bool))
            return {"i32"};
        else if (kind == typeid(U8) || kind == typeid(U16) || kind == typeid(U32))
            return {"i32"};
        else if (kind == typeid(U64))
            return {"i64"};
        else if (kind == typeid(S8) || kind == typeid(S16) || kind == typeid(S32))
            return {"i32"};
        else if (kind == typeid(S64))
            return {"i64"};
        else if (kind == typeid(F32))
            return {"f32"};
        else if (kind == typeid(F64))
            return {"f64"};
        else if (kind == typeid(Char))
            return {"i32"};
        else if (kind == typeid(String))
            return {"i32", "i32"};
        else if (kind == typeid(List))
            return {"i32", "i32"};
        // else if (kind == typeid(Field))
        // {
        //   Field f = static_cast<Field>(kind);
        //   return flatten_type(type(f.v));
        // }
        // else if (kind == typeid(Record))
        // {
        //   Record r = static_cast<Record>(kind);
        //   return flatten_record(r.fields);
        // }
        // else if (kind == typeid(Tuple))
        // {
        //   Tuple t = static_cast<Tuple>(kind);
        //   return flatten_types(t.ts);
        // }
        // else if (kind == typeid(Case))
        // {
        //   Case c = static_cast<Case>(kind);
        //   return flatten_type(type(c.v));
        // }
        // else if (kind == typeid(Variant))
        // {
        //   Variant v = static_cast<Variant>(kind);
        //   return flatten_variant(v.cases);
        // }
        // else if (kind == typeid(Enum))
        // {
        //   Enum e = static_cast<Enum>(kind);
        //   return flatten_variant(e.cases);
        // }
        // else if (kind == typeid(Option))
        // {
        //   Option o = static_cast<Option>(kind);
        //   return flatten_type(type(o.v));
        // }
        // else if (kind == typeid(Result))
        // {
        //   Result r = static_cast<Result>(kind);
        //   return flatten_types({type(r.ok), type(r.err)});
        // }
        // else if (kind == typeid(Flags))
        // {
        //   Flags f = static_cast<Flags>(kind);
        //   return std::vector<std::string>(num_i32_flags(f.labels), "i32");
        // }
        // else if (kind == typeid(Own) || kind == typeid(Borrow))
        //   return {"i32"};
        else
            throw std::runtime_error("Invalid type");
    }

    std::vector<std::string> flatten_record(const std::vector<Field> &fields)
    {
        std::vector<std::string> flat;
        for (const Field &f : fields)
        {
            auto flattened = flatten_type(f.v.t);
            flat.insert(flat.end(), flattened.begin(), flattened.end());
        }
        return flat;
    }

    std::vector<std::string> flatten_variant(const std::vector<Case> &cases)
    {
        std::vector<std::string> flat;
        for (const auto &c : cases)
        {
            if (c.v.has_value())
            {
                auto types = flatten_type(type(c.v.value()));
                for (size_t i = 0; i < types.size(); ++i)
                {
                    if (i < flat.size())
                    {
                        flat[i] = join(flat[i], types[i]);
                    }
                    else
                    {
                        flat.push_back(types[i]);
                    }
                }
            }
        }
        std::vector<std::string> discriminantFlattened = flatten_type(discriminant_type(cases));
        flat.insert(flat.begin(), discriminantFlattened.begin(), discriminantFlattened.end());
        return flat;
    }

    // std::string join(const std::string &a, const std::string &b);
    // std::vector<std::string> flatten_variant(const std::vector<Case> &cases)
    // {
    //     std::vector<std::string> flat;
    //     for (const auto &c : cases)
    //     {
    //         if (c.t.has_value())
    //         {
    //             auto flattened = flatten_type(c.t.value());
    //             for (size_t i = 0; i < flattened.size(); ++i)
    //             {
    //                 if (i < flat.size())
    //                 {
    //                     flat[i] = join(flat[i], flattened[i]);
    //                 }
    //                 else
    //                 {
    //                     flat.push_back(flattened[i]);
    //                 }
    //             }
    //         }
    //     }
    //     auto discriminantFlattened = flatten_type(discriminant_type(cases));
    //     flat.insert(flat.begin(), discriminantFlattened.begin(), discriminantFlattened.end());
    //     return flat;
    // }
}