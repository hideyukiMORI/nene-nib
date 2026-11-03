#include "DiagnosticEdits.hpp"

#include "Column.hpp"
#include "DiagnosticSeverity.hpp"
#include "EditAdjust.hpp"
#include "OffsetRange.hpp"
#include "Utf8.hpp"

#include <algorithm>
#include <cstddef>
#include <optional>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

namespace nenenib::core
{
namespace
{
// 行の中の桁（0 始まり）の半開区間と重さ（行の写しの途中の値）。
using Piece = std::pair<std::pair<std::size_t, std::size_t>, DiagnosticSeverity>;

// 編集の範囲に掛かるか。消した範囲 [at, removed_end) と半開区間で重なるか、純粋な挿入が区間の
// 内側（両端を除く）に入るとき。2 つは「at < end かつ begin < removed_end」の 1 式に畳める。
[[nodiscard]] bool touched(const OffsetRange &range, const Edit &edit) noexcept
{
    const std::size_t at = edit.at.value;
    const std::size_t removed_end = at + edit.removed.size();
    return at < range.end.value && range.begin.value < removed_end;
}

// 行の内容の中のバイト位置 → 桁（code point の序数・0 始まり）。
[[nodiscard]] std::size_t column_index(std::string_view line, std::size_t byte) noexcept
{
    return code_point_count(line.substr(0, byte));
}

// 診断 1 件を行の内容 [0, line.size()] のバイトへ切り、桁の区間にする。行に掛からなければ値なし。
[[nodiscard]] std::optional<Piece> piece_of(std::string_view line, Offset line_start,
                                            const Diagnostic &diagnostic)
{
    const std::size_t start = line_start.value;
    const std::size_t finish = start + line.size();
    const std::size_t begin = diagnostic.range.begin.value;
    const std::size_t end = diagnostic.range.end.value;
    if (begin == end)
    {
        if (begin < start || begin > finish)
        {
            return std::nullopt;
        }
        const std::size_t column = column_index(line, begin - start);
        return Piece{{column, column + 1}, diagnostic.severity};
    }
    const std::size_t clipped_begin = std::max(begin, start);
    const std::size_t clipped_end = std::min(end, finish);
    if (clipped_begin >= clipped_end)
    {
        return std::nullopt;
    }
    return Piece{
        {column_index(line, clipped_begin - start), column_index(line, clipped_end - start)},
        diagnostic.severity};
}

// 桁 column を覆う区間の中で最も重い重さ。覆う区間が無ければ値なし。
[[nodiscard]] std::optional<DiagnosticSeverity> heaviest_at(std::span<const Piece> pieces,
                                                            std::size_t column) noexcept
{
    std::optional<DiagnosticSeverity> heaviest;
    for (const Piece &piece : pieces)
    {
        const bool covers = piece.first.first <= column && column < piece.first.second;
        if (covers && (!heaviest.has_value() || is_heavier(piece.second, heaviest.value())))
        {
            heaviest = piece.second;
        }
    }
    return heaviest;
}

// 区間の端の桁を昇順で重複なく。
[[nodiscard]] std::vector<std::size_t> edges_of(std::span<const Piece> pieces)
{
    std::vector<std::size_t> edges;
    edges.reserve(pieces.size() * 2);
    for (const Piece &piece : pieces)
    {
        edges.push_back(piece.first.first);
        edges.push_back(piece.first.second);
    }
    std::ranges::sort(edges);
    const auto repeated = std::ranges::unique(edges);
    edges.erase(repeated.begin(), repeated.end());
    return edges;
}

void append_span(std::vector<DiagnosticSpan> &spans, std::size_t begin, std::size_t end,
                 DiagnosticSeverity severity)
{
    const Column first{begin + 1};
    if (!spans.empty() && spans.back().end == first && spans.back().severity == severity)
    {
        spans.back().end = Column{end + 1};
        return;
    }
    spans.push_back(DiagnosticSpan{.begin = first, .end = Column{end + 1}, .severity = severity});
}
} // namespace

std::vector<Diagnostic> diagnostics_adjusted(std::vector<Diagnostic> diagnostics, const Edit &edit)
{
    std::vector<Diagnostic> kept;
    kept.reserve(diagnostics.size());
    for (Diagnostic &diagnostic : diagnostics)
    {
        if (touched(diagnostic.range, edit))
        {
            continue;
        }
        // 編集より前（end が at 以前で、at から始まる空の診断でない）はそのまま。
        const bool before =
            diagnostic.range.end.value <= edit.at.value && !(diagnostic.range.begin == edit.at);
        if (!before)
        {
            diagnostic.range = OffsetRange{.begin = offset_after_edit(diagnostic.range.begin, edit),
                                           .end = offset_after_edit(diagnostic.range.end, edit)};
        }
        kept.push_back(std::move(diagnostic));
    }
    return kept;
}

std::vector<DiagnosticSpan> diagnostic_spans(std::string_view line, Offset line_start,
                                             std::span<const Diagnostic> diagnostics)
{
    std::vector<Piece> pieces;
    for (const Diagnostic &diagnostic : diagnostics)
    {
        const std::optional<Piece> piece = piece_of(line, line_start, diagnostic);
        if (piece.has_value())
        {
            pieces.push_back(piece.value());
        }
    }
    const std::vector<std::size_t> edges = edges_of(pieces);
    std::vector<DiagnosticSpan> spans;
    for (std::size_t index = 0; index + 1 < edges.size(); ++index)
    {
        const std::optional<DiagnosticSeverity> severity = heaviest_at(pieces, edges[index]);
        if (severity.has_value())
        {
            append_span(spans, edges[index], edges[index + 1], severity.value());
        }
    }
    return spans;
}
} // namespace nenenib::core
