#include "DiagnosticColor.hpp"

#include <utility>

namespace nenenib::core
{
RgbColor diagnostic_color(const SyntaxPalette &body, const Palette &ui,
                          DiagnosticSeverity severity) noexcept
{
    switch (severity)
    {
    case DiagnosticSeverity::error:
        return body.error;
    case DiagnosticSeverity::warning:
        return body.warning;
    case DiagnosticSeverity::information:
    case DiagnosticSeverity::hint:
        return ui.muted;
    }
    std::unreachable();
}
} // namespace nenenib::core
