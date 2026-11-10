#include "VimLineColumns.hpp"

#include "DisplayWidth.hpp"
#include "Utf8.hpp"
#include "VimCharacterBoundary.hpp"
#include "VirtualColumnIndex.hpp"

#include <algorithm>
#include <cstddef>
#include <string>
#include <string_view>
#include <utility>

namespace nenenib::core
{
namespace
{
// Vim の 1 文字の終わりを探す窓の初めの大きさ（バイト）。行の全体は写さない（ADR 0053 の決定 5）。
constexpr std::size_t character_window_bytes = 64;
// UTF-8 の 1 code point の最長のバイト数。窓の終わりで切れた code point を読まないための余白。
constexpr std::size_t longest_code_point_bytes = 4;

// 行の写しの上で at から手前へ Vim の 1 文字ずつ count 回戻る（索引を持たない本文の道・今のまま）。
[[nodiscard]] std::size_t copied_characters_back(std::string_view line, std::size_t at,
                                                 std::size_t count)
{
    for (std::size_t step = 0; step < count && at > 0; ++step)
    {
        at = vim_character_start(line, Offset{at}).value;
    }
    return at;
}

// 行の写しの上で at から Vim の 1 文字ずつ count 回進む（索引を持たない本文の道・今のまま）。
[[nodiscard]] std::size_t copied_characters_forward(std::string_view line, std::size_t at,
                                                    std::size_t count)
{
    for (std::size_t step = 0; step < count && at < line.size(); ++step)
    {
        at = vim_character_end(line, Offset{at}).value;
    }
    return at;
}

// content の at から始まる 1 文字の終わり。at と次の 1 バイトがどちらも ASCII で、次のバイトが
// 幅 0 でなければその 1 バイトが 1 文字で、vim_character_end と同じ答えを code point を読まずに
// 返す（大きな回数の `l` の 1 歩・#1269）。ほかは vim_character_end に問う。
[[nodiscard]] std::size_t character_after(std::string_view content, std::size_t at) noexcept
{
    constexpr unsigned char ascii_end = 0x80U;
    const std::size_t next = at + 1;
    if (next < content.size() && static_cast<unsigned char>(content[at]) < ascii_end)
    {
        const auto after = static_cast<unsigned char>(content[next]);
        if (after < ascii_end && display_width(after) != DisplayWidth::zero)
        {
            return next;
        }
    }
    return vim_character_end(content, Offset{at}).value;
}

// 窓 content の頭から Vim の 1 文字ずつ最大 count 回進み、止まった位置と進んだ回数を返す。
// clipped（content の終わりが行の終わりではない窓）なら、終わりが窓の終わりから 1 code point
// ぶんの余白に入る 1 歩は（窓の外に幅 0 の code point が続くかもしれず）答えが確かでないので
// 踏まない。
[[nodiscard]] std::pair<std::size_t, std::size_t>
characters_forward(std::string_view content, bool clipped, std::size_t count)
{
    std::size_t at = 0;
    std::size_t walked = 0;
    while (walked < count && at < content.size())
    {
        const std::size_t found = character_after(content, at);
        if (clipped && found + longest_code_point_bytes > content.size())
        {
            break;
        }
        at = found;
        ++walked;
    }
    return {at, walked};
}

// 文字の頭 at（そこの code point は前の文字に続かない）の手前の 1 文字の頭。手前の 1 バイトが
// 幅 0 でない ASCII ならその 1 バイトが 1 文字で、vim_character_start と同じ答えを code point を
// 読まずに返す（大きな回数の `h` の 1 歩・#1258）。ほかは vim_character_start に問う。
[[nodiscard]] std::size_t character_before(std::string_view content, std::size_t at,
                                           bool at_start) noexcept
{
    constexpr unsigned char ascii_end = 0x80U;
    const auto before = static_cast<unsigned char>(content[at - 1]);
    if (at_start && before < ascii_end && display_width(before) != DisplayWidth::zero)
    {
        return at - 1;
    }
    return vim_character_start(content, Offset{at}).value;
}

// 窓 content の at から手前へ Vim の 1 文字ずつ最大 count 回戻り、止まった位置と戻った回数を
// 返す。clipped（content の頭が行頭ではない窓）なら、窓の頭まで戻る 1 歩は答えが確かでないので
// 踏まない。at が文字の頭か（そこの code point が前の文字に続かないか）は最初の 1 歩の前に 1 回
// だけ確かめ、1 歩戻った後は必ず頭。
[[nodiscard]] std::pair<std::size_t, std::size_t>
characters_back(std::string_view content, std::size_t at, bool clipped, std::size_t count)
{
    std::size_t walked = 0;
    while (walked < count && at > 0)
    {
        const std::size_t found = character_before(
            content, at, walked > 0 || !vim_character_continues(content, Offset{at}));
        if (found == 0 && clipped)
        {
            break;
        }
        at = found;
        ++walked;
    }
    return {at, walked};
}
} // namespace

VimLineColumns::VimLineColumns(const TextBuffer &text, LineNumber line)
    : text_(text), line_(line), start_(text.line_start(line)),
      size_(text.line_end(line).value - text.line_start(line).value),
      copied_(text.has_virtual_columns() ? std::nullopt
                                         : std::optional<std::string>{text.line_text(line)})
{
}

Offset VimLineColumns::start() const noexcept
{
    return start_;
}

std::size_t VimLineColumns::size() const noexcept
{
    return size_;
}

std::size_t VimLineColumns::width() const
{
    if (copied_.has_value())
    {
        return virtual_width(copied_.value());
    }
    const TextBuffer &text = text_.get();
    return text.virtual_columns().value().get().virtual_column_at(text,
                                                                  Offset{start_.value + size_});
}

std::size_t VimLineColumns::byte_at(VirtualColumn column) const
{
    if (copied_.has_value())
    {
        return byte_at_column(copied_.value(), column);
    }
    return offset_at_virtual_column(text_.get(), line_, column).value - start_.value;
}

VirtualColumn VimLineColumns::column_of(std::size_t byte) const
{
    if (copied_.has_value())
    {
        return core::column_of(copied_.value(), byte);
    }
    return virtual_column(text_.get(), Offset{start_.value + std::min(byte, size_)});
}

VirtualColumn VimLineColumns::column_end_of(std::size_t byte) const
{
    if (copied_.has_value())
    {
        return core::column_end_of(copied_.value(), byte);
    }
    return virtual_column_end(text_.get(), Offset{start_.value + std::min(byte, size_)});
}

// 索引の道は byte から窓を 1 回読み、その中で何歩も進む（大きな回数の `l` `<Space>` `x`・#1269）。
// 終わりが窓の終わりに近い 1 歩（Tab の後ろに長く続く結合文字の列）はそこで止め、止めた位置から
// 窓を倍にして読み直す（VimCaret の character_near と同じ形）。行の終わりまで届いた窓の答えは行を
// 写した答えと同じ。
std::size_t VimLineColumns::character_end(std::size_t byte, std::size_t count) const
{
    if (copied_.has_value())
    {
        return copied_characters_forward(copied_.value(), byte, count);
    }
    std::size_t at = byte;
    std::size_t left = count;
    std::size_t window = character_window_bytes;
    while (left > 0 && at < size_)
    {
        const std::size_t stop = std::min(at + window, size_);
        const std::string content =
            text_.get().text_range(Offset{start_.value + at}, Offset{start_.value + stop});
        const bool clipped = stop < size_;
        const std::string_view whole{content.data(),
                                     clipped ? whole_code_points(content) : content.size()};
        const auto [local, walked] = characters_forward(whole, clipped, left);
        at += local;
        left -= walked;
        window *= 2;
    }
    return at;
}

// 索引の道は byte の手前の窓（と byte の 1 code point）を 1 回読み、その中で何歩も戻る（大きな
// 回数の `h` `X` `dh`・#1258）。窓の頭まで戻ってしまったら（窓の頭は文字の頭とは限らない）そこで
// 止めた位置から窓を倍にして読み直す。行頭まで届いた窓の答えは行を写した答えと同じ。
std::size_t VimLineColumns::character_start(std::size_t byte, std::size_t count) const
{
    if (copied_.has_value())
    {
        return copied_characters_back(copied_.value(), byte, count);
    }
    std::size_t at = std::min(byte, size_);
    std::size_t left = count;
    std::size_t window = character_window_bytes;
    while (left > 0 && at > 0)
    {
        const std::size_t begin = at > window ? at - window : 0;
        const std::size_t stop = std::min(at + longest_code_point_bytes, size_);
        const std::string content =
            text_.get().text_range(Offset{start_.value + begin}, Offset{start_.value + stop});
        const auto [local, walked] = characters_back(content, at - begin, begin > 0, left);
        at = begin + local;
        left -= walked;
        window *= 2;
    }
    return at;
}

char32_t VimLineColumns::code_point_at(std::size_t byte) const
{
    if (copied_.has_value())
    {
        return core::code_point_at(copied_.value(), Offset{byte});
    }
    const std::size_t stop = std::min(byte + longest_code_point_bytes, size_);
    const std::string head =
        text_.get().text_range(Offset{start_.value + byte}, Offset{start_.value + stop});
    return core::code_point_at(head, Offset{0});
}

std::size_t VimLineColumns::next_code_point(std::size_t byte) const
{
    if (copied_.has_value())
    {
        return core::next_code_point(copied_.value(), Offset{byte}).value;
    }
    const std::size_t stop = std::min(byte + longest_code_point_bytes, size_);
    const std::string head =
        text_.get().text_range(Offset{start_.value + byte}, Offset{start_.value + stop});
    return byte + core::next_code_point(head, Offset{0}).value;
}
} // namespace nenenib::core
