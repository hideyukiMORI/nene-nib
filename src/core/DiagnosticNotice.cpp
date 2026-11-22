#include "DiagnosticNotice.hpp"

#include "DiagnosticJump.hpp"
#include "DiagnosticSeverity.hpp"
#include "NoticeLine.hpp"

namespace nenenib::core
{
std::optional<DisplayText> diagnostic_notice(std::span<const Diagnostic> diagnostics, Offset caret)
{
    const Diagnostic *shown = nullptr;
    for (const Diagnostic &diagnostic : diagnostics)
    {
        if (diagnostic_covers(diagnostic, caret) &&
            (shown == nullptr || is_heavier(diagnostic.severity, shown->severity)))
        {
            shown = &diagnostic;
        }
    }
    if (shown == nullptr)
    {
        return std::nullopt;
    }
    return notice_line(shown->message);
}
} // namespace nenenib::core
