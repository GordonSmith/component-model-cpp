#include "lower.hpp"
#include "util.hpp"
#include "store.hpp"

std::vector<WasmVal> lower_flat_string(const CallContext &cx, const Val &v)
{
  auto [ptr, packed_length] = store_string_into_range(cx, v);
  return {ptr, packed_length};
}

std::vector<WasmVal> lower_flat(const CallContext &cx, const Val &v)
{
  switch (despecialize(v).kind())
  {
  case ValType::Bool:
    return {WasmVal(static_cast<uint32_t>(v.b()))};
  case ValType::U8:
    return {WasmVal(static_cast<uint32_t>(v.u8()))};
  case ValType::U16:
    return {WasmVal(static_cast<uint32_t>(v.u16()))};
  case ValType::U32:
    return {WasmVal(static_cast<uint32_t>(v.u32()))};
  case ValType::U64:
    return {WasmVal(static_cast<uint64_t>(v.u64()))};
  case ValType::S8:
    return {WasmVal(static_cast<uint32_t>(v.s8()))};
  case ValType::S16:
    return {WasmVal(static_cast<uint32_t>(v.s16()))};
  case ValType::S32:
    return {WasmVal(static_cast<uint32_t>(v.s32()))};
  case ValType::S64:
    return {WasmVal(static_cast<uint64_t>(v.s64()))};
  case ValType::Float32:
    return {WasmVal(static_cast<float32_t>(maybe_scramble_nan32(v.f32())))};
  case ValType::Float64:
    return {WasmVal(static_cast<float64_t>(maybe_scramble_nan64(v.f64())))};
  case ValType::Char:
    return {WasmVal(char_to_i32(v.c()))};
  case ValType::String:
    return lower_flat_string(cx, v);
  default:
    throw std::runtime_error("Invalid type");
  }
}

std::vector<WasmVal> lower_values(const CallContext &cx, size_t max_flat, const std::vector<Val> &vs, int *out_param = nullptr)
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
    return {WasmVal(ptr)};
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
