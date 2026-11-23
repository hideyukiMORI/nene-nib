#include "LspFormatting.hpp"

#include "JsonNull.hpp"
#include "LspDiagnostics.hpp"
#include "LspJsonRead.hpp"

#include <algorithm>
#include <cstddef>
#include <string>
#include <utility>
#include <variant>

namespace nenenib::core
{
namespace
{
// 写した区間と、答えの列の中の位置（同じ始まりの並びを届いた順に保つ鍵）。
using PlacedEdit = std::pair<OffsetRange, std::size_t>;

[[nodiscard]] std::optional<LspTextEdit> text_edit_of(const JsonValue &value)
{
    const JsonObject *object = json_object_of(value);
    if (object == nullptr)
    {
        return std::nullopt;
    }
    const std::string *text = json_text_member(*object, "newText");
    const auto range = lsp_range_member(*object, "range");
    if (text == nullptr || !range.has_value())
    {
        return std::nullopt;
    }
    return LspTextEdit{.range = range.value(), .new_text = *text};
}

// 各編集を本文の位置へ写し、始まりの順（同じ始まりは届いた順）に並べる。写せない編集があれば空。
[[nodiscard]] std::vector<PlacedEdit> placed_edits(const TextBuffer &text,
                                                   std::span<const LspTextEdit> edits,
                                                   LspPositionEncoding encoding)
{
    std::vector<PlacedEdit> placed;
    placed.reserve(edits.size());
    for (std::size_t index = 0; index < edits.size(); ++index)
    {
        const auto range = lsp_range_to_offsets(text, edits[index].range, encoding);
        if (!range.has_value())
        {
            return {};
        }
        placed.emplace_back(range.value(), index);
    }
    std::ranges::sort(placed,
                      [](const PlacedEdit &left, const PlacedEdit &right)
                      {
                          return left.first.begin != right.first.begin
                                     ? left.first.begin < right.first.begin
                                     : left.second < right.second;
                      });
    return placed;
}

// 当てる前のキャレットを写す（決定 7）。手前で終わる編集は長さの差だけずらし、キャレットを中に
// 含む編集はその新しい本文の終わりへ置く。キャレットの位置への挿入（始まりがキャレット）は後ろに
// 入るので数えない。
[[nodiscard]] Offset mapped_caret(std::span<const PlacedEdit> placed,
                                  std::span<const LspTextEdit> edits, Offset caret)
{
    std::size_t moved = caret.value;
    std::size_t grown = 0;
    std::size_t shrunk = 0;
    for (const auto &[range, index] : placed)
    {
        if (range.begin.value >= caret.value)
        {
            break;
        }
        const std::size_t added = edits[index].new_text.size();
        const std::size_t removed = range.end.value - range.begin.value;
        if (range.end.value > caret.value)
        {
            moved = range.begin.value + added;
            return Offset{moved + grown - shrunk};
        }
        grown += added;
        shrunk += removed;
    }
    return Offset{moved + grown - shrunk};
}
} // namespace

std::expected<std::vector<LspTextEdit>, LspAnswerFailure> lsp_text_edits(const JsonValue &result)
{
    if (std::holds_alternative<JsonNull>(result.value))
    {
        return std::vector<LspTextEdit>{};
    }
    const auto *list = std::get_if<JsonArray>(&result.value);
    if (list == nullptr)
    {
        return std::unexpected{LspAnswerFailure::invalid_result};
    }
    std::vector<LspTextEdit> edits;
    edits.reserve(list->size());
    for (const JsonValue &entry : *list)
    {
        auto edit = text_edit_of(entry);
        if (!edit.has_value())
        {
            return std::unexpected{LspAnswerFailure::invalid_result};
        }
        edits.push_back(std::move(edit).value());
    }
    return edits;
}

std::optional<FormattedSpan> formatted_span(const TextBuffer &text,
                                            std::span<const LspTextEdit> edits,
                                            LspPositionEncoding encoding, Offset caret)
{
    const std::vector<PlacedEdit> placed = placed_edits(text, edits, encoding);
    if (placed.empty())
    {
        return std::nullopt;
    }
    const Offset start = placed.front().first.begin;
    Offset cursor = start;
    std::string replaced;
    for (const auto &[range, index] : placed)
    {
        if (range.begin < cursor)
        {
            return std::nullopt;
        }
        replaced += text.text_range(cursor, range.begin);
        replaced += edits[index].new_text;
        cursor = range.end;
    }
    return FormattedSpan{.range = OffsetRange{start, cursor},
                         .text = std::move(replaced),
                         .caret = mapped_caret(placed, edits, caret)};
}
} // namespace nenenib::core
