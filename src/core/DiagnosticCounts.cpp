#include "DiagnosticCounts.hpp"

#include "DiagnosticSeverity.hpp"

namespace nenenib::core
{
DiagnosticCounts diagnostic_counts(std::span<const Diagnostic> diagnostics,
                                   DiagnosticsCount completeness) noexcept
{
    DiagnosticCounts counts{0, 0, completeness};
    for (const Diagnostic &diagnostic : diagnostics)
    {
        switch (diagnostic.severity)
        {
        case DiagnosticSeverity::error:
            ++counts.errors;
            break;
        case DiagnosticSeverity::warning:
            ++counts.warnings;
            break;
        case DiagnosticSeverity::information:
        case DiagnosticSeverity::hint:
            break;
        }
    }
    return counts;
}
} // namespace nenenib::core
