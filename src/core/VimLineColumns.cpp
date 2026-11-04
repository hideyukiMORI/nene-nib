#include "VimLineColumns.hpp"

#include "Utf8.hpp"
#include "VimCharacterBoundary.hpp"
#include "VirtualColumnIndex.hpp"

#include <algorithm>
#include <cstddef>
#include <string>
#include <string_view>

namespace nenenib::core
{
namespace
{
// Vim の 1 文字の終わりを探す窓の初めの大きさ（バイト）。行の全体は写さない（ADR 0053 の決定 5）。
constexpr std::size_t character_window_bytes = 64;
// UTF-8 の 1 code point の最長のバイト数。窓の終わりで切れた code point を読まないための余白。
constexpr std::size_t longest_code_point_bytes = 4;
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

// 索引の道は byte から窓を読み、文字の終わりが窓の終わりに近ければ窓を倍にして読み直す（Tab の
// 後ろに長く続く結合文字の列でも行の終わりまで伸ばせる・VimCaret の character_near と同じ形）。
std::size_t VimLineColumns::character_end(std::size_t byte) const
{
    if (copied_.has_value())
    {
        return vim_character_end(copied_.value(), Offset{byte}).value;
    }
    if (byte >= size_)
    {
        return byte;
    }
    std::size_t window = character_window_bytes;
    for (;;)
    {
        const std::size_t length = std::min(window, size_ - byte);
        const std::string content = text_.get().text_range(Offset{start_.value + byte},
                                                           Offset{start_.value + byte + length});
        const std::size_t end = vim_character_end(content, Offset{0}).value;
        if (byte + length == size_ || end + longest_code_point_bytes <= content.size())
        {
            return byte + end;
        }
        window *= 2;
    }
}

// 索引の道は byte の手前の窓（と byte の 1 code point）を読み、窓の頭まで戻ってしまったら（窓の
// 頭は文字の頭とは限らない）窓を倍にして読み直す。行頭まで届いた窓の答えは行を写した答えと同じ。
std::size_t VimLineColumns::character_start(std::size_t byte) const
{
    if (copied_.has_value())
    {
        return vim_character_start(copied_.value(), Offset{byte}).value;
    }
    const std::size_t at = std::min(byte, size_);
    std::size_t window = character_window_bytes;
    for (;;)
    {
        const std::size_t begin = at > window ? at - window : 0;
        const std::size_t stop = std::min(at + longest_code_point_bytes, size_);
        const std::string content =
            text_.get().text_range(Offset{start_.value + begin}, Offset{start_.value + stop});
        const std::size_t found = vim_character_start(content, Offset{at - begin}).value;
        if (begin == 0 || found > 0)
        {
            return begin + found;
        }
        window *= 2;
    }
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
