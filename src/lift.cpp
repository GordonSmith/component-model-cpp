#include "lift.hpp"

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

// std::vector<Val> lift_values(CallContext &cx, int max_flat, const std::vector<ValType> &ts)
// {
//     auto flat_types = flatten_types(ts);
//     if (flat_types.size() > max_flat)
//     {
//         auto ptr = vi.next('i32');
//         auto tuple_type = Tuple(ts);
//         if (ptr != align_to(ptr, alignment(tuple_type)))
//         {
//             throw std::runtime_error("Misaligned pointer");
//         }
//         if (ptr + size(tuple_type) > cx.opts.memory.size())
//         {
//             throw std::runtime_error("Out of bounds access");
//         }
//         return load(cx, ptr, tuple_type).values();
//     }
//     else
//     {
//         std::vector<Val> result;
//         for (const auto &t : ts)
//         {
//             result.push_back(lift_flat(cx, vi, t));
//         }
//         return result;
//     }
// }
