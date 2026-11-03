#include "VimMatchingPair.hpp"

#include "LineNumber.hpp"
#include "VimPairScan.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <iterator>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>

namespace nenenib::core
{
namespace
{
// 始まりの括弧と探す相手の 1 組（first が始まり）。
using BracketPair = std::pair<char, char>;

constexpr char no_character = '\0';
constexpr unsigned char continuation_mask = 0xC0;
constexpr unsigned char continuation_bits = 0x80;
constexpr std::size_t one_byte = 1;
constexpr std::size_t literal_span = 2;
constexpr std::size_t escaped_literal_span = 3;

// 'matchpairs' の既定（開き, 閉じ）。この版は設定を持たない（ADR 0152 の限界）。
constexpr std::array<BracketPair, 3> match_pairs{{{'(', ')'}, {'[', ']'}, {'{', '}'}}};

// 括弧 → （始まり, 相手）。括弧でなければ nullopt。
[[nodiscard]] std::optional<BracketPair> bracket_of(char byte) noexcept
{
    for (const BracketPair pair : match_pairs)
    {
        if (pair.first == byte)
        {
            return pair;
        }
        if (pair.second == byte)
        {
            return BracketPair{pair.second, pair.first};
        }
    }
    return std::nullopt;
}

[[nodiscard]] bool opens(char byte) noexcept
{
    for (const BracketPair pair : match_pairs)
    {
        if (pair.first == byte)
        {
            return true;
        }
    }
    return false;
}

[[nodiscard]] bool continues(char byte) noexcept
{
    return (static_cast<unsigned char>(byte) & continuation_mask) == continuation_bits;
}

[[nodiscard]] char byte_at(std::string_view line, std::size_t index) noexcept
{
    return index < line.size() ? line[index] : no_character;
}

// index の前に続く `\` の数（Vim の check_prevcol の数え方）。
[[nodiscard]] std::size_t backslashes_before(std::string_view line, std::size_t index) noexcept
{
    std::size_t count = 0;
    while (index > count && line[index - count - 1] == '\\')
    {
        ++count;
    }
    return count;
}

[[nodiscard]] bool odd_backslashes(std::string_view line, std::size_t index) noexcept
{
    return backslashes_before(line, index) % 2 == 1;
}

// 行の `"` が偶数個か（Vim の do_quotes）。`'"'` と `\` の次の文字は数えない。0 個も偶数。
[[nodiscard]] bool even_quotes(std::string_view line) noexcept
{
    std::size_t quotes = 0;
    for (std::size_t index = 0; index < line.size(); ++index)
    {
        const bool quoted =
            index > 0 && line[index - 1] == '\'' && byte_at(line, index + one_byte) == '\'';
        quotes += static_cast<std::size_t>(line[index] == '"' && !quoted);
        index += static_cast<std::size_t>(line[index] == '\\' && index + one_byte < line.size());
    }
    return quotes % 2 == 0;
}

// 行の中の index から右の最初の括弧の桁。行末を越えた桁は最後の文字から探す。
[[nodiscard]] std::optional<std::size_t> first_bracket(std::string_view line, std::size_t index)
{
    std::size_t at = index >= line.size() && index > 0 ? line.size() - one_byte : index;
    for (; at < line.size(); ++at)
    {
        if (bracket_of(line[at]).has_value())
        {
            return at;
        }
    }
    return std::nullopt;
}

[[nodiscard]] VimPairScan scan_at(const TextBuffer &text, LineNumber line, bool backwards)
{
    std::string content = text.line_text(line);
    const std::size_t index = backwards ? content.size() : 0;
    const bool quotes = even_quotes(content);
    return VimPairScan{line, text.line_start(line).value, std::move(content), index, quotes, false,
                       0};
}

// 走査の今の位置の本文のバイト位置。
[[nodiscard]] std::size_t offset_of(const VimPairScan &scan) noexcept
{
    return scan.start + scan.index;
}

// 1 文字戻る。本文の先頭か窓の始まりなら false。前の行へ渡ったら行の内容の終わりに立つ。
[[nodiscard]] bool stepped_back(const TextBuffer &text, VimPairScan &scan, std::size_t begin)
{
    if (offset_of(scan) <= begin)
    {
        return false;
    }
    if (scan.index > 0)
    {
        --scan.index;
        while (scan.index > 0 && continues(scan.content[scan.index]))
        {
            --scan.index;
        }
        return true;
    }
    // 窓の始まりより前の行は写さない（巨大な行を読まない）。
    if (scan.line.value <= 1 || text.line_end(LineNumber{scan.line.value - 1}).value < begin)
    {
        return false;
    }
    const bool inside = scan.inside;
    const std::size_t depth = scan.depth;
    scan = scan_at(text, LineNumber{scan.line.value - 1}, true);
    scan.inside = inside;
    scan.depth = depth;
    return true;
}

// 1 文字進む。本文の末尾か窓の終わりなら false。行の内容の終わりから次の行の行頭へ渡る。
[[nodiscard]] bool stepped_forward(const TextBuffer &text, VimPairScan &scan, std::size_t end)
{
    if (scan.index < scan.content.size())
    {
        ++scan.index;
        while (scan.index < scan.content.size() && continues(scan.content[scan.index]))
        {
            ++scan.index;
        }
        return offset_of(scan) < end;
    }
    // 窓の終わりから後ろの行は写さない（巨大な行を読まない）。
    if (scan.line.value >= text.line_count() ||
        text.line_start(LineNumber{scan.line.value + 1}).value >= end)
    {
        return false;
    }
    const bool inside = scan.inside;
    const std::size_t depth = scan.depth;
    scan = scan_at(text, LineNumber{scan.line.value + 1}, false);
    scan.inside = inside;
    scan.depth = depth;
    return true;
}

// 後ろ向きの走査が `'` に着いたとき、`'x'` `'\x'` の文字を飛ばした桁。
[[nodiscard]] std::size_t skipped_literal_back(std::string_view line, std::size_t index) noexcept
{
    if (index >= literal_span && line[index - literal_span] == '\'')
    {
        return index - literal_span;
    }
    const bool escaped = index >= escaped_literal_span && line[index - literal_span] == '\\' &&
                         line[index - escaped_literal_span] == '\'';
    return escaped ? index - escaped_literal_span : index;
}

// `'x'` `'\x'` の文字を飛ばした桁（Vim の findmatchlimit の `'` の分岐）。
[[nodiscard]] std::size_t skipped_literal(std::string_view line, std::size_t index,
                                          bool backwards) noexcept
{
    if (backwards)
    {
        return skipped_literal_back(line, index);
    }
    if (byte_at(line, index + one_byte) == '\\' &&
        byte_at(line, index + literal_span) != no_character &&
        byte_at(line, index + escaped_literal_span) == '\'')
    {
        return index + escaped_literal_span;
    }
    const bool literal = byte_at(line, index + one_byte) != no_character &&
                         byte_at(line, index + literal_span) == '\'';
    return literal ? index + literal_span : index;
}

// 引用符と文字の印の扱い。括弧として数えない文字なら true（走査を続ける）。
[[nodiscard]] bool passed_quoting(VimPairScan &scan, bool backwards)
{
    const char byte = byte_at(scan.content, scan.index);
    if (scan.index >= scan.content.size())
    {
        // 行末に `\` が無ければ引用符の内側を閉じる。
        scan.inside = scan.inside && scan.index > 0 && scan.content[scan.index - 1] == '\\';
        return true;
    }
    if (byte == '"')
    {
        scan.inside = scan.inside != (scan.quotes && !odd_backslashes(scan.content, scan.index));
        return true;
    }
    if (byte == '\'')
    {
        scan.index = skipped_literal(scan.content, scan.index, backwards);
        return true;
    }
    return scan.inside;
}

// 本文の位置 at が除く区間のどれかの中か。区間は昇順で重ならない。
[[nodiscard]] bool skipped_at(std::span<const OffsetRange> skipped, std::size_t at)
{
    const auto after = std::ranges::upper_bound(skipped, at, {}, [](const OffsetRange &range)
                                                { return range.begin.value; });
    return after != skipped.begin() && at < std::prev(after)->end.value;
}

// 括弧を数える。相手が見つかれば true。除く区間の中の括弧は数えない（ADR 0164 の決定 3）。
[[nodiscard]] bool counted(VimPairScan &scan, const BracketPair &pair, bool escaped,
                           std::span<const OffsetRange> skipped)
{
    const char byte = scan.content[scan.index];
    if ((byte != pair.first && byte != pair.second) ||
        odd_backslashes(scan.content, scan.index) != escaped ||
        skipped_at(skipped, offset_of(scan)))
    {
        return false;
    }
    if (byte == pair.first)
    {
        ++scan.depth;
        return false;
    }
    if (scan.depth == 0)
    {
        return true;
    }
    --scan.depth;
    return false;
}
} // namespace

std::optional<Offset> vim_matching_pair(const TextBuffer &text, Offset caret)
{
    const LineNumber line = text.position_of(caret).line;
    const Offset start = text.line_start(line);
    const auto found = first_bracket(text.line_text(line), caret.value - start.value);
    if (!found.has_value())
    {
        return std::nullopt;
    }
    return matched_bracket(text, Offset{start.value + found.value()},
                           OffsetRange{Offset{0}, Offset{text.size_bytes()}}, {});
}

std::optional<Offset> matched_bracket(const TextBuffer &text, Offset bracket, OffsetRange window,
                                      std::span<const OffsetRange> skipped)
{
    if (bracket.value < window.begin.value || bracket.value >= window.end.value ||
        bracket.value >= text.size_bytes())
    {
        return std::nullopt;
    }
    const LineNumber line = text.position_of(bracket).line;
    VimPairScan scan = scan_at(text, line, false);
    scan.index = bracket.value - scan.start;
    const auto found = bracket_of(byte_at(scan.content, scan.index));
    if (!found.has_value())
    {
        return std::nullopt;
    }
    const BracketPair pair = found.value();
    const bool backwards = !opens(pair.first);
    const bool escaped = odd_backslashes(scan.content, scan.index);
    while (backwards ? stepped_back(text, scan, window.begin.value)
                     : stepped_forward(text, scan, window.end.value))
    {
        if (!passed_quoting(scan, backwards) && counted(scan, pair, escaped, skipped))
        {
            return Offset{offset_of(scan)};
        }
    }
    return std::nullopt;
}
} // namespace nenenib::core
