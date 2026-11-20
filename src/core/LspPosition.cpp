#include "LspPosition.hpp"

#include "LineNumber.hpp"
#include "LspPositionEncoding.hpp"
#include "Offset.hpp"
#include "TextBuffer.hpp"
#include "TextPosition.hpp"
#include "Utf16.hpp"
#include "Utf8.hpp"

#include <algorithm>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

namespace nenenib::core
{
namespace
{
// UTF-8 の 1 文字の最大のバイト数。窓の端の切れた文字を寄せるのに足りる長さ。
constexpr std::size_t widest_code_point = 4;
// UTF-16 の 1 単位が占める UTF-8 のバイトの最大（3 バイトの文字が 1 単位・4 バイトの文字は 2
// 単位）。
constexpr std::size_t bytes_per_utf16_unit = 3;

// 行の内容 [start, end) の中の at を後ろの境目へ寄せる。読むのは at からの最大 4 バイトの窓だけ。
[[nodiscard]] std::size_t snapped_forward(const TextBuffer &buffer, std::size_t at, std::size_t end)
{
    const std::size_t stop = std::min(end, at + widest_code_point);
    if (at >= stop)
    {
        return at;
    }
    const std::string window = buffer.text_range(Offset{at}, Offset{stop});
    return at + leading_continuations(window);
}

// 行の内容の先頭（窓 line の先頭）から UTF-16 の単位の数 character の位置。代理対の途中なら対の
// 後ろへ。
[[nodiscard]] std::size_t utf16_bytes(std::string_view line, std::size_t character)
{
    std::size_t at = 0;
    std::size_t units = 0;
    while (at < line.size() && units < character)
    {
        const std::size_t next = next_code_point(line, Offset{at}).value;
        units += utf16_length(line.substr(at, next - at)).value_or(1);
        at = next;
    }
    return at;
}
} // namespace

// 行の全体を写さない（1 編集ごとに呼ばれる・#1897）。utf-8 は桁の前後の 4 バイトの窓だけ、utf-16 は
// 行頭から桁に届くまでの窓（1 単位は最大 3 バイト）だけを読む。単位の数は行のバイト数を超えない
// ので、character が行の長さ以上なら行末。
std::optional<Offset> lsp_offset_of(const TextBuffer &buffer, LspPosition position,
                                    LspPositionEncoding encoding)
{
    if (position.line >= buffer.line_count())
    {
        return std::nullopt;
    }
    const LineNumber line{position.line + 1};
    const std::size_t start = buffer.line_start(line).value;
    const std::size_t end = buffer.line_end(line).value;
    const std::size_t length = end - start;
    if (position.character >= length)
    {
        return Offset{end};
    }
    switch (encoding)
    {
    case LspPositionEncoding::utf8:
        return Offset{snapped_forward(buffer, start + position.character, end)};
    case LspPositionEncoding::utf16:
    {
        const std::size_t reach =
            std::min(length, (position.character * bytes_per_utf16_unit) + widest_code_point);
        const std::string window = buffer.text_range(Offset{start}, Offset{start + reach});
        const std::string_view whole =
            std::string_view{window}.substr(0, whole_code_points(window));
        return Offset{start + utf16_bytes(whole, position.character)};
    }
    }
    return Offset{end};
}

// 行頭から at までだけを読む（utf-8 は at の後ろの 4 バイトの窓だけ・#1897）。
LspPosition lsp_position_of(const TextBuffer &buffer, Offset at, LspPositionEncoding encoding)
{
    const Offset bounded{std::min(at.value, buffer.size_bytes())};
    const LineNumber line = buffer.position_of(bounded).line;
    const std::size_t start = buffer.line_start(line).value;
    const std::size_t line_end = buffer.line_end(line).value;
    // コードポイントの途中の at は、lsp_offset_of と同じく後ろの境目へ寄せてから数える。
    const std::size_t end = snapped_forward(buffer, std::min(bounded.value, line_end), line_end);
    switch (encoding)
    {
    case LspPositionEncoding::utf8:
        return LspPosition{.line = line.value - 1, .character = end - start};
    case LspPositionEncoding::utf16:
    {
        const std::string before = buffer.text_range(Offset{start}, Offset{end});
        return LspPosition{.line = line.value - 1,
                           .character = utf16_length(before).value_or(before.size())};
    }
    }
    return LspPosition{.line = line.value - 1, .character = end - start};
}
} // namespace nenenib::core
