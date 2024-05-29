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
    for (Val v : vs)
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
      // case ValType::Record:
      //     return flatten_record(static_cast<const Record &>(t).fields);
      // case ValType::Variant:
      //     return flatten_variant(static_cast<VariantPtr >(t).cases);
      // case ValType::Flags:
      //     return std::vector<std::string>(num_i32_flags(static_cast<const Flags &>(t).labels),
      //     "i32");
      // case ValType::Own:
      // case ValType::Borrow:
      //     return {"i32"};
    default:
      throw std::runtime_error("Invalid type");
    }
  }

  std::vector<std::string> flatten_record(const std::vector<Field> &fields)
  {
    std::vector<std::string> flat;
    for (const Field &f : fields)
    {
      auto flattened = flatten_type(f.ft);
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