#ifndef CMCPP_ERROR_CONTEXT_HPP
#define CMCPP_ERROR_CONTEXT_HPP

#include "context.hpp"
#include "string.hpp"
#include <utility>

namespace cmcpp
{
    class ErrorContext : public TableEntry
    {
    public:
        explicit ErrorContext(string_t message)
            : debug_message_(std::move(message))
        {
        }

        const string_t &debug_message() const
        {
            return debug_message_;
        }

    private:
        string_t debug_message_{};
    };

    inline uint32_t canon_error_context_new(Task &task,
                                            const LiftLowerContext *cx,
                                            uint32_t ptr,
                                            uint32_t tagged_code_units,
                                            const HostTrap &trap)
    {
        auto *inst = task.component_instance();
        auto trap_cx = make_trap_context(trap);
        trap_if(trap_cx, inst == nullptr, "task missing component instance");
        ensure_may_leave(*inst, trap);

        string_t message;
        if (cx != nullptr)
        {
            message = string::load_from_range<string_t>(*cx, ptr, tagged_code_units);
        }

        auto entry = std::make_shared<ErrorContext>(std::move(message));
        return inst->table.add(entry, trap);
    }

    inline void canon_error_context_debug_message(Task &task,
                                                  LiftLowerContext &cx,
                                                  uint32_t index,
                                                  uint32_t ptr,
                                                  const HostTrap &trap)
    {
        auto *inst = task.component_instance();
        auto trap_cx = make_trap_context(trap);
        trap_if(trap_cx, inst == nullptr, "task missing component instance");
        ensure_may_leave(*inst, trap);

        auto errctx = inst->table.get<ErrorContext>(index, trap);
        string::store(cx, errctx->debug_message(), ptr);
    }

    inline void canon_error_context_drop(Task &task, uint32_t index, const HostTrap &trap)
    {
        auto *inst = task.component_instance();
        auto trap_cx = make_trap_context(trap);
        trap_if(trap_cx, inst == nullptr, "task missing component instance");
        ensure_may_leave(*inst, trap);

        inst->table.remove<ErrorContext>(index, trap);
    }
}

#endif // CMCPP_ERROR_CONTEXT_HPP
