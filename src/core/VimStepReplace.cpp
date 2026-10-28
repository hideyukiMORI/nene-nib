#include "VimStepDetail.hpp"

#include "Utf8.hpp"
#include "VimCharacterBoundary.hpp"
#include "VimVisualRange.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

namespace nenenib::core
{
using detail::carriage_return_character;
using detail::count_of;
using detail::failed;
using detail::finished_input_wait;
using detail::line_feed;
using detail::line_of;
using detail::replaced_block;
using detail::replacement_key;
using detail::started_prefix;
using detail::started_replacement;
using detail::visual_restore;

namespace
{
// ---------------------------------------------------------------- r（ADR 0029）

[[nodiscard]] std::optional<OffsetRange> normal_replacement_range(const TextBuffer &text,
                                                                  Offset caret, std::size_t count)
{
    const Offset end = text.line_end(line_of(text, caret));
    // 数えるのは Vim の文字（結合文字は付く文字ごと・ADR 0053 の決定 4）。
    const std::string available = text.text_range(caret, end);
    Offset at{0};
    for (std::size_t step = 0; step < count; ++step)
    {
        if (at.value >= available.size())
        {
            return std::nullopt;
        }
        at = vim_character_end(available, at);
    }
    return OffsetRange{caret, Offset{caret.value + at.value}};
}

[[nodiscard]] std::optional<OffsetRange> replacement_range(const VimState &state,
                                                           const VimEditorView &view)
{
    switch (state.mode)
    {
    case VimMode::normal:
        return normal_replacement_range(view.text, view.selection.caret, count_of(state.count));
    case VimMode::visual:
    case VimMode::visual_line:
        return vim_visual_range(view.text, view.selection, state.mode).range;
    case VimMode::insert:
    // 矩形は 1 つの範囲では表せない。replaced_character が先に行ごとの経路へ分かれる（決定 4）。
    case VimMode::visual_block:
        return std::nullopt;
    }
    std::unreachable();
}
} // namespace

namespace detail
{
// 回数が行に収まらなくても、Vim と同じく置き換える文字を 1 鍵読んでから失敗にする（nv_replace）。
// 失敗は replaced_character が返す。Esc は待ちを取り消すだけ。
[[nodiscard]] VimStep started_replacement(const VimState &state)
{
    return started_prefix(state, VimPrefix::r);
}
} // namespace detail

namespace
{
// 文字だけを置換する。source は文字の先頭から始まる。行境界は残し、効果の本文はLFへ揃える。
// '\n' の直前の '\r' を改行の一部と見るのは本文の改行の形が CRLF のときだけで、LF の本文の
// '\r' は置換される文字である（ADR 0036）。
[[nodiscard]] std::string replacement_text(std::string_view source, char32_t target,
                                           LineEnding ending)
{
    std::string result;
    result.reserve(source.size());
    Offset at{0};
    while (at.value < source.size())
    {
        const char32_t code = code_point_at(source, at);
        // 1 文字（付く結合文字ごと）を 1 つの target に置き換える（ADR 0053 の決定 4）。
        // 改行の後ろは行頭なので、改行はそれだけで 1 文字として進む。
        at = code == line_feed ? next_code_point(source, at) : vim_character_end(source, at);
        if (ending == LineEnding::crlf && code == carriage_return_character &&
            at.value < source.size() && source.at(at.value) == '\n')
        {
            continue;
        }
        append_utf8(result, code == line_feed ? line_feed : target);
    }
    return result;
}

// r が書けない文字（Tab を除く制御文字と DEL・ADR 0029）。改行は NORMAL が行を割り、文字単位と
// 行単位の VISUAL が選んだ各文字を literal CR に置き換える（ADR 0036 の決定 5）。矩形 VISUAL の
// <CR> は未測なので、これまでどおり受け取らない（ADR 0035 の範囲のまま）。
[[nodiscard]] bool unsupported_replacement(VimMode mode, char32_t target) noexcept
{
    const bool newline = target == line_feed || target == carriage_return_character;
    return (newline && mode == VimMode::visual_block) ||
           (!newline && target < U' ' && target != U'\t') || target == U'\x7f';
}

// 置き換えたあとの本文（LF 換算）。VISUAL の <CR> は各文字を literal CR にする（ADR 0036）。
[[nodiscard]] std::string replacement_body(const VimEditorView &view, OffsetRange selected,
                                           char32_t target, bool newline)
{
    return replacement_text(view.text.text_range(selected.begin, selected.end),
                            newline ? carriage_return_character : target, view.text.line_ending());
}

[[nodiscard]] VimStep replaced_character(const VimState &state, const VimEditorView &view,
                                         char32_t target)
{
    if (unsupported_replacement(state.mode, target))
    {
        return failed(VimStep{finished_input_wait(state), VimNoEffect{}},
                      VimRepeatFailure::refused);
    }
    if (state.mode == VimMode::visual_block)
    {
        return replaced_block(state, view, target);
    }
    const bool newline = target == line_feed || target == carriage_return_character;
    // NORMAL の r<CR> は行を割り、文字単位と行単位の VISUAL の r<CR> は選んだ各文字を
    // literal CR にする（ADR 0029 の決定 5 と ADR 0036 の決定 5・実測）。
    const bool splits_line = newline && state.mode == VimMode::normal;
    const auto range = replacement_range(state, view);
    if (!range.has_value())
    {
        return failed(VimStep{finished_input_wait(state), VimNoEffect{}},
                      VimRepeatFailure::refused);
    }
    const OffsetRange selected = range.value();
    std::string body =
        splits_line ? std::string("\n") : replacement_body(view, selected, target, newline);
    Offset caret = selected.begin;
    if (state.mode == VimMode::normal)
    {
        caret.value +=
            splits_line ? body.size() : previous_code_point(body, Offset{body.size()}).value;
    }
    VimStep step{vim_resting_from(state, state.unnamed_register),
                 VimReplaceRange{selected, std::move(body), caret}};
    // NORMAL の r は engine が書かない（ADR 0052 の決定 11）。VISUAL は決定 10。
    if (state.mode != VimMode::normal)
    {
        step.restore = visual_restore(view.text, view.selection, state.mode);
    }
    return step;
}
} // namespace

namespace detail
{
[[nodiscard]] VimStep replacement_key(const VimState &state, const VimEditorView &view, VimKey key)
{
    if (std::holds_alternative<VimCharacter>(key))
    {
        return replaced_character(state, view, std::get<VimCharacter>(key).code);
    }
    if (!std::holds_alternative<VimSpecialKey>(key))
    {
        return VimStep{finished_input_wait(state), VimNoEffect{}};
    }
    switch (std::get<VimSpecialKey>(key))
    {
    case VimSpecialKey::enter:
        return replaced_character(state, view, line_feed);
    case VimSpecialKey::escape:
    case VimSpecialKey::backspace:
    case VimSpecialKey::arrow_left:
    case VimSpecialKey::arrow_right:
    case VimSpecialKey::arrow_up:
    case VimSpecialKey::arrow_down:
    case VimSpecialKey::control_r:
    case VimSpecialKey::home:
    case VimSpecialKey::end:
    case VimSpecialKey::page_up:
    case VimSpecialKey::page_down:
    case VimSpecialKey::control_d:
    case VimSpecialKey::control_u:
    case VimSpecialKey::control_f:
    case VimSpecialKey::control_b:
    case VimSpecialKey::control_v:
    case VimSpecialKey::control_w:
        return VimStep{finished_input_wait(state), VimNoEffect{}};
    }
    std::unreachable();
}
} // namespace detail
} // namespace nenenib::core
