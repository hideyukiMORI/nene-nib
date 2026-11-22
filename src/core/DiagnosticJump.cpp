#include "DiagnosticJump.hpp"

#include <algorithm>
#include <array>
#include <utility>

namespace nenenib::core
{
namespace
{
constexpr std::array<DiagnosticSeverity, 4> severities_heaviest_first{
    DiagnosticSeverity::error, DiagnosticSeverity::warning, DiagnosticSeverity::information,
    DiagnosticSeverity::hint};

// 出どころと番号の行（無ければ空）。
[[nodiscard]] std::string origin_line(const Diagnostic &diagnostic)
{
    if (diagnostic.source.empty())
    {
        return diagnostic.code;
    }
    if (diagnostic.code.empty())
    {
        return diagnostic.source;
    }
    return diagnostic.source + "(" + diagnostic.code + ")";
}

// 札の文に診断 1 つ（前の診断との間は空の行）。
void append_tip(std::string &text, const Diagnostic &diagnostic)
{
    if (!text.empty())
    {
        text += "\n\n";
    }
    text += diagnostic.message;
    const std::string origin = origin_line(diagnostic);
    if (!origin.empty())
    {
        text += "\n";
        text += origin;
    }
}

// covering（diagnostic_order の順）のうち重さ severity の診断を札の文に足す。
void append_severity(std::string &text, std::span<const Diagnostic> diagnostics,
                     const std::vector<std::size_t> &covering, DiagnosticSeverity severity)
{
    for (const std::size_t index : covering)
    {
        if (diagnostics[index].severity == severity)
        {
            append_tip(text, diagnostics[index]);
        }
    }
}

// 診断の始まりの位置を小さい順に、同じ位置を 1 つにした列。
[[nodiscard]] std::vector<std::size_t> landing_points(std::span<const Diagnostic> diagnostics)
{
    std::vector<std::size_t> points;
    points.reserve(diagnostics.size());
    for (const Diagnostic &diagnostic : diagnostics)
    {
        points.push_back(diagnostic.range.begin.value);
    }
    std::ranges::sort(points);
    const auto [first, last] = std::ranges::unique(points);
    points.erase(first, last);
    return points;
}
} // namespace

bool diagnostic_covers(const Diagnostic &diagnostic, Offset caret) noexcept
{
    const auto &range = diagnostic.range;
    if (range.begin.value == range.end.value)
    {
        return caret.value == range.begin.value;
    }
    return range.begin.value <= caret.value && caret.value < range.end.value;
}

std::vector<std::size_t> diagnostic_order(std::span<const Diagnostic> diagnostics)
{
    std::vector<std::size_t> order(diagnostics.size());
    for (std::size_t index = 0; index < order.size(); ++index)
    {
        order.at(index) = index;
    }
    std::ranges::sort(order,
                      [&diagnostics](std::size_t left, std::size_t right)
                      {
                          const Diagnostic &one = diagnostics[left];
                          const Diagnostic &other = diagnostics[right];
                          if (one.range.begin.value != other.range.begin.value)
                          {
                              return one.range.begin.value < other.range.begin.value;
                          }
                          if (one.severity != other.severity)
                          {
                              return is_heavier(one.severity, other.severity);
                          }
                          return left < right;
                      });
    return order;
}

std::optional<Offset> diagnostic_landing(std::span<const Diagnostic> diagnostics, Offset caret,
                                         bool forward, std::size_t count)
{
    const std::vector<std::size_t> points = landing_points(diagnostics);
    if (points.empty())
    {
        return std::nullopt;
    }
    const std::size_t size = points.size();
    // 1 回目の行き先から数えた残りの回数（列を回る分は余りだけ歩く）。
    const std::size_t rest = (count == 0 ? 0 : count - 1) % size;
    if (forward)
    {
        const auto after = std::ranges::upper_bound(points, caret.value);
        const auto first = after == points.end() ? std::size_t{0}
                                                 : static_cast<std::size_t>(after - points.begin());
        return Offset{points.at((first + rest) % size)};
    }
    const auto before = std::ranges::lower_bound(points, caret.value);
    const auto first =
        before == points.begin() ? size - 1 : static_cast<std::size_t>(before - points.begin()) - 1;
    return Offset{points.at((first + size - rest) % size)};
}

std::string diagnostic_tip_text(std::span<const Diagnostic> diagnostics, Offset caret)
{
    std::vector<std::size_t> covering;
    for (const std::size_t index : diagnostic_order(diagnostics))
    {
        if (diagnostic_covers(diagnostics[index], caret))
        {
            covering.push_back(index);
        }
    }
    std::string text;
    for (const DiagnosticSeverity severity : severities_heaviest_first)
    {
        append_severity(text, diagnostics, covering, severity);
    }
    return text;
}

std::string_view diagnostic_severity_mark(DiagnosticSeverity severity) noexcept
{
    switch (severity)
    {
    case DiagnosticSeverity::error:
        return "E";
    case DiagnosticSeverity::warning:
        return "W";
    case DiagnosticSeverity::information:
        return "I";
    case DiagnosticSeverity::hint:
        return "H";
    }
    std::unreachable();
}
} // namespace nenenib::core
