#include "VimStepDetail.hpp"

#include "Utf8.hpp"
#include "VimPutSide.hpp"

#include <cstddef>
#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace nenenib::core
{
using detail::forward_characters;
using detail::last_code_point_size;
using detail::line_of;
using detail::macro_name_of;
using detail::put_characters;
using detail::put_lines;
using detail::refused_register;
using detail::register_selection_of;
using detail::repeated;
using detail::selected_register;
using detail::single_step;

namespace detail
{
// ---------------------------------------------------------------- p / P（決定 4）

[[nodiscard]] std::expected<std::string, VimRepeatFailure> repeated(std::string_view body,
                                                                    std::size_t count)
{
    std::string result;
    if (body.empty())
    {
        return result;
    }
    if (count > result.max_size() / body.size())
    {
        return std::unexpected(VimRepeatFailure::too_large);
    }
    result.reserve(body.size() * count);
    for (std::size_t step = 0; step < count; ++step)
    {
        result.append(body);
    }
    return result;
}

// 貼った本文の最後の code point の長さ。文字単位の put はその文字の上にキャレットが載る。
[[nodiscard]] std::size_t last_code_point_size(std::string_view body)
{
    return body.size() - previous_code_point(body, Offset{body.size()}).value;
}
} // namespace detail

namespace
{
// 貼った行の最初の非空白までのバイト数。行単位の put はそこへキャレットが載る。
[[nodiscard]] std::size_t first_non_blank_index(std::string_view body)
{
    std::size_t index = 0;
    while (index < body.size() && (body[index] == ' ' || body[index] == '\t'))
    {
        ++index;
    }
    return index;
}
} // namespace

namespace detail
{
// 文字単位の put。1 行に収まる本文は貼った全体の最後の文字へ、改行を含む本文は貼った本文の
// 先頭（回数つきは最初のコピーの先頭）へキャレットが載る（本物の Vim の実測・Issue #205）。
// 先頭が行末を越えるときの寄せは NORMAL のキャレットの寄せに任せる（ARC-001）。
[[nodiscard]] VimInsertAt put_characters(const TextBuffer &text, Offset caret, std::string body,
                                         VimPutSide side)
{
    const Offset at =
        side == VimPutSide::after ? forward_characters(text, caret, single_step) : caret;
    const Offset rest =
        body.contains('\n') ? at : Offset{at.value + body.size() - last_code_point_size(body)};
    return VimInsertAt{at, std::move(body), rest, EditBoundary::separate};
}

// 行単位の put。上の行へ入れるときと最終行でない行の下へ入れるときは行の先頭にそのまま入る。
// 最終行の下だけは、そこにもう改行が無いので本文の末尾に改行から書く。
[[nodiscard]] VimInsertAt put_lines(const TextBuffer &text, Offset caret, std::string body,
                                    VimPutSide side)
{
    const LineNumber line = line_of(text, caret);
    const std::size_t indent = first_non_blank_index(body);
    if (side == VimPutSide::before)
    {
        const Offset at = text.line_start(line);
        return VimInsertAt{at, std::move(body), Offset{at.value + indent}, EditBoundary::separate};
    }
    if (line.value < text.line_count())
    {
        const Offset at = text.line_terminator_end(line);
        return VimInsertAt{at, std::move(body), Offset{at.value + indent}, EditBoundary::separate};
    }
    const Offset at = text.line_end(line);
    std::string tail = "\n";
    tail.append(body, 0, body.size() - single_step);
    return VimInsertAt{at, std::move(tail), Offset{at.value + single_step + indent},
                       EditBoundary::separate};
}

// ---------------------------------------------------------------- レジスタの名前（ADR 0048）

// `"` の次の鍵が選ぶレジスタ（決定 2）。a〜z は名前つき、A〜Z は同じ名前への追記、`"` は無名、
// `_` はブラックホール、`0`〜`9` は数字、`-` は小削除（ADR 0050 の決定 2・回数の桁ではない）、
// `+` と `*` はクリップボード（ADR 0051 の決定 1・名前は打った鍵のまま）。
// ほかの鍵は名前を持たない。
[[nodiscard]] std::optional<VimRegisterSelection> register_selection_of(char32_t name)
{
    if (name == U'"')
    {
        return VimRegisterSelection{VimRegisterTarget::unnamed, '"', false};
    }
    if (name == U'_')
    {
        return VimRegisterSelection{VimRegisterTarget::black_hole, '_', false};
    }
    if (name == U'-')
    {
        return VimRegisterSelection{VimRegisterTarget::small_delete, '-', false};
    }
    if (name == U'+' || name == U'*')
    {
        return VimRegisterSelection{VimRegisterTarget::clipboard, static_cast<char>(name), false};
    }
    if (vim_numbered_index(name).has_value())
    {
        return VimRegisterSelection{VimRegisterTarget::numbered, static_cast<char>(name), false};
    }
    const auto index = vim_register_index(name);
    if (!index.has_value())
    {
        return std::nullopt;
    }
    return VimRegisterSelection{VimRegisterTarget::named, static_cast<char>(U'a' + index.value()),
                                name >= U'A' && name <= U'Z'};
}

// ---------------------------------------------------------------- `"`（ADR 0048）

// `"{name}`（決定 2）。選んだレジスタを置いて次の命令を待つ。回数は消さず（`3"ayy` と `"a3yy` は
// 同じ）、`"a"b` は後勝ち。名前にならない鍵は回数ごと捨ててビープする。
[[nodiscard]] VimStep selected_register(const VimState &state, const VimKey &key)
{
    const auto selection = register_selection_of(macro_name_of(key).value_or(char32_t{}));
    if (!selection.has_value())
    {
        return refused_register(state);
    }
    VimState next = state;
    next.input_wait = std::nullopt;
    next.selected_register = selection.value();
    return VimStep{std::move(next), VimNoEffect{}};
}
} // namespace detail
} // namespace nenenib::core
