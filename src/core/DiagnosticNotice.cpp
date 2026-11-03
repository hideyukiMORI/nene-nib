#include "DiagnosticNotice.hpp"

#include "DiagnosticSeverity.hpp"
#include "NoticeLine.hpp"

namespace nenenib::core
{
namespace
{
[[nodiscard]] bool covers(const Diagnostic &diagnostic, Offset caret) noexcept
{
    const auto &range = diagnostic.range;
    if (range.begin.value == range.end.value)
    {
        return caret.value == range.begin.value;
    }
    return range.begin.value <= caret.value && caret.value < range.end.value;
}
} // namespace

std::optional<DisplayText> diagnostic_notice(std::span<const Diagnostic> diagnostics, Offset caret)
{
    const Diagnostic *shown = nullptr;
    for (const Diagnostic &diagnostic : diagnostics)
    {
        if (covers(diagnostic, caret) &&
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
