#include "VimStepDetail.hpp"

#include "CaretMotion.hpp"
#include "CaretMove.hpp"
#include "LineEnding.hpp"
#include "Selection.hpp"
#include "Utf8.hpp"
#include "VimAction.hpp"
#include "VimBlockInsert.hpp"
#include "VimCharacterBoundary.hpp"
#include "VimInsertStart.hpp"
#include "VimScrollDirection.hpp"
#include "VimWordClass.hpp"
#include "VimWordMotion.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>

namespace nenenib::core
{
using detail::backward_characters;
using detail::block_insert_edit;
using detail::display_view;
using detail::erased_nothing;
using detail::finished_input_wait;
using detail::insert_character;
using detail::insert_special;
using detail::last_code_point_size;
using detail::line_feed;
using detail::line_of;
using detail::page_step;
using detail::register_read;
using detail::register_selection_of;
using detail::repeated;
using detail::requested_layout;
using detail::requested_viewport;
using detail::single_step;
using detail::started_prefix;

namespace
{
// INSERT の移動の鍵がキャレットを to へ動かす。挿入の反復はここで切れる（ADR 0028 の決定 3）。
[[nodiscard]] VimStep insert_moved_to(const VimState &state, Offset to)
{
    VimState next = state;
    next.insert_repeat = std::nullopt;
    return VimStep{std::move(next), VimMoveTo{to}};
}

[[nodiscard]] VimStep insert_moved(const VimState &state, const TextBuffer &text, Offset caret,
                                   CaretMotion motion)
{
    return insert_moved_to(
        state,
        moved_caret(text, caret, CaretMoveRequest{motion, single_step, CaretUnit::code_point}));
}

// INSERT の矢印が読む窓の初めの大きさ（行の全体は写さない・長い結合文字の列では倍に広げる）。
constexpr std::size_t arrow_window_bytes = 64;
constexpr std::size_t longest_code_point_bytes = 4;

// INSERT の `<Left>`（Vim の ins_left と oneleft・#1136）。既定の whichwrap=b,s に `[` が無いので
// 行頭では止まり、何も変えずに INSERT を続ける（回数もそのまま・ADR 0049 の追補）。ほかは Vim の
// 1 文字ぶん戻る（ADR 0053 の追補）。
[[nodiscard]] VimStep insert_left(const VimState &state, const TextBuffer &text, Offset caret)
{
    const Offset line_start = text.line_start(line_of(text, caret));
    if (caret.value <= line_start.value)
    {
        return VimStep{state, VimNoEffect{}};
    }
    std::size_t window = arrow_window_bytes;
    for (;;)
    {
        const Offset begin{caret.value - std::min(window, caret.value - line_start.value)};
        const std::string content = text.text_range(begin, caret);
        const Offset start = vim_character_start(content, Offset{content.size()});
        if (start.value > 0 || begin == line_start)
        {
            return insert_moved_to(state, Offset{begin.value + start.value});
        }
        window *= 2;
    }
}

// INSERT の `<Right>`（Vim の ins_right と mb_ptr2len・#1136）。`]` が無いので行の内容の終わり
// （改行の手前）では止まり、何も変えない。ほかは Vim の 1 文字ぶん進む。
[[nodiscard]] VimStep insert_right(const VimState &state, const TextBuffer &text, Offset caret)
{
    const Offset line_end = text.line_end(line_of(text, caret));
    if (caret.value >= line_end.value)
    {
        return VimStep{state, VimNoEffect{}};
    }
    std::size_t window = arrow_window_bytes;
    for (;;)
    {
        const Offset end{caret.value + std::min(window, line_end.value - caret.value)};
        const std::string content = text.text_range(caret, end);
        const Offset after = vim_character_end(content, Offset{0});
        if (end == line_end || after.value + longest_code_point_bytes <= content.size())
        {
            return insert_moved_to(state, Offset{caret.value + after.value});
        }
        window *= 2;
    }
}

// 回数の鍵の列に key を足す（#1093）。回数の無い INSERT では何もしない。
[[nodiscard]] VimState insert_recorded(const VimState &state, VimKey key)
{
    VimState next = state;
    if (next.insert_repeat.has_value())
    {
        VimInsertRepeat &repeat = next.insert_repeat.value();
        repeat.keys = repeat.keys.appended(key);
    }
    return next;
}

// 消す鍵（Backspace・Ctrl-w・Ctrl-u）を回数の鍵の列に足す（#1093）。何も消せずに止まった鍵
// （本文の先頭・Vim のビープ）は Vim の redo にも載らないので足さない（`3i<C-u>x` は xxx）。
// 判定は `.` の記録と同じ erased_nothing の 1 本。
[[nodiscard]] VimStep erase_recorded(VimStep step, VimSpecialKey key)
{
    const VimKey pressed{key};
    if (!erased_nothing(pressed, step.effect))
    {
        step.next = insert_recorded(step.next, pressed);
    }
    return step;
}

// 行頭で前の行とつなぐ（`backspace` の eol・#1040）。Backspace も `Ctrl-w` も `Ctrl-u` もここを
// 通り、1 鍵では改行だけを消す。INSERT を始めた行をつないだら、始めの位置はつないだ点へ移る
// （`jI<C-w>x<C-w>` は x だけを消す・oracle で実測）。
[[nodiscard]] VimStep joined_back(const VimState &state, const TextBuffer &text, Offset caret,
                                  LineNumber line)
{
    const LineNumber above{line.value - 1};
    const Offset end = text.line_end(above);
    VimState next = state;
    if (next.insert_start.has_value() && next.insert_start.value().line == line)
    {
        next.insert_start = VimInsertStart{above, end.value - text.line_start(above).value, false};
    }
    return VimStep{std::move(next), VimRemoveRange{OffsetRange{end, caret}}};
}

// INSERT の Backspace。挿入を始めた位置より前も消せる（oracle の backspace=indent,eol,start）。
[[nodiscard]] VimStep insert_erased(const VimState &state, const TextBuffer &text, Offset caret)
{
    if (caret.value == 0)
    {
        return VimStep{state, VimNoEffect{}};
    }
    const LineNumber line = line_of(text, caret);
    if (caret == text.line_start(line))
    {
        return joined_back(state, text, caret, line);
    }
    return VimStep{
        state, VimRemoveRange{OffsetRange{backward_characters(text, caret, single_step), caret}}};
}

// `Ctrl-w` `Ctrl-u` が戻れる左端（Vim の ins_bs の mincol と Insstart_orig・#1040）。キャレットが
// 始めの行で始めの桁より右にあればそこ、ほかは行頭（autoindent は無いので字下げで止まらない）。
[[nodiscard]] Offset erase_floor(const VimState &state, const TextBuffer &text, Offset caret,
                                 LineNumber line)
{
    const Offset line_start = text.line_start(line);
    if (!state.insert_start.has_value())
    {
        return line_start;
    }
    const VimInsertStart start = state.insert_start.value();
    if (start.line == line && start.byte_column < caret.value - line_start.value)
    {
        return Offset{line_start.value + start.byte_column};
    }
    return line_start;
}

// Vim の vim_isspace（Tab・改行の類・空白）。語の前の空白を先に消すかを決める。
[[nodiscard]] bool erased_blank(char32_t code) noexcept
{
    constexpr char32_t tab = U'\t';
    constexpr char32_t carriage_return = U'\r';
    return code == U' ' || (code >= tab && code <= carriage_return);
}

// INSERT の `Ctrl-w` の左端（#1040）。手前の空白を消してから、同じ種類の文字（`b` と同じ表
// vim_character_class）の連なりを消す。Vim と同じく少なくとも 1 文字は消し、floor で止まる。
[[nodiscard]] std::size_t word_erase_begin(std::string_view content, std::size_t floor)
{
    std::size_t index = content.size();
    std::optional<std::uint32_t> kind;
    do
    {
        const std::size_t previous = vim_character_start(content, Offset{index}).value;
        const char32_t code = code_point_at(content, Offset{previous});
        const std::uint32_t current = vim_character_class(code, VimWordClass::word);
        if (kind.has_value() && (erased_blank(code) || current != kind.value()))
        {
            break;
        }
        if (!kind.has_value() && !erased_blank(code))
        {
            kind = current;
        }
        index = previous;
    } while (index > floor);
    return index;
}

// INSERT の `Ctrl-w` と `Ctrl-u`（#1040）。行頭では前の行とつなぎ、ほかは左端まで（`Ctrl-w` は語の
// 境でも）消す。`.` の記録と回数の鍵の列には鍵のまま残り、再生も同じ消し方をする（Vim の redo
// と同じ・#1093）。
[[nodiscard]] VimStep insert_cut(const VimState &state, const TextBuffer &text, Offset caret,
                                 bool by_word)
{
    if (caret.value == 0)
    {
        return VimStep{state, VimNoEffect{}};
    }
    const LineNumber line = line_of(text, caret);
    const Offset line_start = text.line_start(line);
    if (caret == line_start)
    {
        return joined_back(state, text, caret, line);
    }
    const Offset floor = erase_floor(state, text, caret, line);
    Offset begin = floor;
    if (by_word)
    {
        // 消しうるのは floor から先だけなので、行頭からは写さない（1 行 16 MiB の行・#1040）。
        const std::string content = text.text_range(floor, caret);
        begin = Offset{floor.value + word_erase_begin(content, 0)};
    }
    return VimStep{state, VimRemoveRange{OffsetRange{begin, caret}}};
}

// 鍵の列が打った本文だけ（文字と改行）なら、その本文（#1093）。消す鍵を含む列は前の周の本文
// まで消すので、流し直さないと決まらない（空を返す）。
// 鍵 1 つが打った本文（#1093）。文字はその文字、改行の鍵は改行で、ほかの鍵は本文にならない。
// 鍵の種類が増えたら std::visit がここで落ちる（CPP-002）。
[[nodiscard]] std::optional<std::string> typed_piece(VimCharacter key)
{
    std::string text;
    append_utf8(text, key.code);
    return text;
}

[[nodiscard]] std::optional<std::string> typed_piece(VimSpecialKey key)
{
    if (key == VimSpecialKey::enter)
    {
        return std::string{"\n"};
    }
    return std::nullopt;
}

[[nodiscard]] std::optional<std::string> typed_piece(const VimSearchPattern &)
{
    return std::nullopt;
}

[[nodiscard]] std::optional<std::string> typed_text(const std::vector<VimKey> &keys)
{
    std::string text;
    for (const VimKey &key : keys)
    {
        auto piece = std::visit([](const auto &value) { return typed_piece(value); }, key);
        if (!piece.has_value())
        {
            return std::nullopt;
        }
        text += piece.value();
    }
    return text;
}

// 流し直す 1 鍵（#1093）。打った鍵と同じ関数を通る。鍵の種類が増えたら std::visit がここで
// 落ちる（CPP-002）。
[[nodiscard]] VimStep replayed_key(const VimState &state, const VimEditorView &, VimCharacter key)
{
    return insert_character(state, key);
}

[[nodiscard]] VimStep replayed_key(const VimState &state, const VimEditorView &view,
                                   VimSpecialKey key)
{
    return insert_special(state, view, key);
}

// 検索の型は INSERT の鍵の列に載らない（INSERT で受けても何もしない・searched_key と同じ）。
[[nodiscard]] VimStep replayed_key(const VimState &state, const VimEditorView &,
                                   const VimSearchPattern &)
{
    return VimStep{state, VimNoEffect{}};
}

// 流し直した 1 鍵の効果を本文の写しへ写す（#1093）。列の鍵の効果は挿入・改行・範囲の削除の
// どれか（何も消せない鍵は列に載らない）で、controller と同じに改行は文書の形で入れる。
[[nodiscard]] std::pair<TextBuffer, Offset> replayed_effect(const TextBuffer &text, Offset caret,
                                                            const VimEffect &effect)
{
    if (const auto *const typed = std::get_if<VimInsertString>(&effect))
    {
        return {text.insert(caret, typed->utf8), Offset{caret.value + typed->utf8.size()}};
    }
    if (std::holds_alternative<VimNewLine>(effect))
    {
        const std::string_view newline = newline_of(text.line_ending());
        return {text.insert(caret, newline), Offset{caret.value + newline.size()}};
    }
    if (const auto *const removed = std::get_if<VimRemoveRange>(&effect))
    {
        return {text.erase(removed->range.begin, removed->range.end), removed->range.begin};
    }
    return {text, caret};
}

// [begin, end) の本文を LF の改行で写す（#1093）。効果の本文は LF で数え、controller が文書の
// 改行へ直す（CRLF の文書でも行の中身だけを写す）。
[[nodiscard]] std::string lf_text(const TextBuffer &text, Offset begin, Offset end)
{
    const LineNumber last = line_of(text, end);
    std::string body;
    for (LineNumber line = line_of(text, begin);; line = LineNumber{line.value + 1})
    {
        const Offset from{std::max(begin.value, text.line_start(line).value)};
        if (line == last)
        {
            body += text.text_range(from, end);
            return body;
        }
        body += text.text_range(from, text.line_end(line));
        body.push_back('\n');
    }
}

// 鍵 1 つを種類ごとの replayed_key へ写す（#1093）。
[[nodiscard]] VimStep replayed_key(const VimState &state, const VimEditorView &view,
                                   const VimKey &key)
{
    return std::visit([&](const auto &value) { return replayed_key(state, view, value); }, key);
}

// 鍵の列を残りの周だけ本文の写しへ流し直した本文・キャレット・消した手前の端（#1093）。
[[nodiscard]] std::tuple<TextBuffer, Offset, Offset>
replayed_rounds(const VimState &state, const VimEditorView &view, std::size_t remaining,
                const std::vector<VimKey> &keys)
{
    VimState replaying = state;
    replaying.insert_repeat = std::nullopt;
    TextBuffer text = view.text;
    Offset caret = view.selection.caret;
    Offset low = caret;
    for (std::size_t round = 0; round < remaining; ++round)
    {
        for (const VimKey &key : keys)
        {
            const Selection here = collapsed_at(caret);
            const VimEditorView replay_view{text, here, view.viewport};
            VimStep keyed = replayed_key(replaying, replay_view, key);
            replaying = std::move(keyed.next);
            auto replayed = replayed_effect(text, caret, keyed.effect);
            text = std::move(replayed.first);
            caret = replayed.second;
            low = Offset{std::min(low.value, caret.value)};
        }
    }
    return {std::move(text), caret, low};
}

// 回数の残りの周で鍵の列を流し直す（#1093・Vim の start_redo_ins）。2 周目からの消す鍵は前の周
// が入れた本文も、行頭なら前の行との改行（backspace の eol）も消すので、本文の写しの上で 1 鍵
// ずつ打った鍵と同じ insert_character / insert_special を通す。始めの位置（insert_start）は周を
// またいで持ち越す（Vim の Insstart_orig も同じ INSERT のまま）。効果は、消した手前の端から Esc
// のキャレットまでを流し直した本文に置き換える 1 つで、undo は INSERT の 1 単位に吸収する。
[[nodiscard]] VimStep replayed_insert(const VimEditorView &view, VimStep step,
                                      const std::tuple<TextBuffer, Offset, Offset> &replayed)
{
    const Offset escaped = view.selection.caret;
    const auto &[text, caret, low] = replayed;
    // 流し直しが手前を消さず何も残さなければ、本文は Esc の前のまま（`2o<C-w>`）。
    if (low == escaped && caret == escaped)
    {
        return step;
    }
    // Esc は 1 文字左へ戻る。流し直しが何も残さずに止まった所では、戻り先の文字が消した手前の端
    // より前にあるので、置き換えの範囲をその文字の頭から取る（本文は前と同じ文字）。
    const std::size_t retreat = caret.value - backward_characters(text, caret, single_step).value;
    const Offset begin{std::min(low.value, caret.value - retreat)};
    std::string body = lf_text(text, begin, caret);
    const Offset rest{begin.value + body.size() - retreat};
    step.insert_left = retreat;
    step.effect =
        VimReplaceRange{OffsetRange{begin, escaped}, std::move(body), rest, EditBoundary::absorb};
    return step;
}

[[nodiscard]] VimStep plain_left_insert(const VimState &state, const VimEditorView &view)
{
    const Offset caret = view.selection.caret;
    const Offset back = backward_characters(view.text, caret, single_step);
    VimStep step{vim_resting_from(state, state.unnamed_register), VimMoveTo{back}};
    // `'^` は 1 つ左へ戻る前のキャレット（Vim の b_last_insert・#1039）。
    step.insert_left = caret.value - back.value;
    if (!state.insert_repeat.has_value())
    {
        return step;
    }
    const VimInsertRepeat &repeat = state.insert_repeat.value();
    const std::vector<VimKey> keys = repeat.keys.owned_keys();
    if (keys.empty())
    {
        return step;
    }
    auto typed = typed_text(keys);
    if (!typed.has_value())
    {
        return replayed_insert(view, std::move(step),
                               replayed_rounds(state, view, repeat.remaining, keys));
    }
    // 打った本文だけの列は、流し直しても本文を残りの回数だけ続けたものと同じなので 1 回で作る。
    auto result = repeated(typed.value(), repeat.remaining);
    if (!result.has_value())
    {
        return step;
    }
    std::string body = std::move(result).value();
    const std::size_t retreat = body.back() == '\n' ? 0 : last_code_point_size(body);
    const Offset rest{caret.value + body.size() - retreat};
    step.insert_left = retreat;
    step.effect = VimInsertAt{caret, std::move(body), rest, EditBoundary::absorb};
    return step;
}

// 矩形の 1 行目が INSERT の前より伸びたぶん（ほかの行へ写す本文）と、回数の繰り返しの 1 行目の
// 編集（#1042・#1093）。回数の繰り返しは本文の後ろへ足す VimInsertAt か、消す鍵を含む流し直しの
// VimReplaceRange で、流し直しは置き換えた後の 1 行目から伸びたぶんを読む（Vim の op_insert も
// 回数の周を終えた 1 行目を写す）。1 行目の中に収まらない流し直し（前の行とつないだ・改行を
// 打った）と、伸びていない 1 行目は写さない（空）。
[[nodiscard]] std::optional<std::pair<std::string, std::vector<VimBlockEdit>>>
first_line_growth(const VimBlockInsert &insert, const TextBuffer &text, const VimEffect &effect)
{
    const Offset start = text.line_start(insert.first);
    const Offset end = text.line_end(insert.first);
    std::string line = text.text_range(start, end);
    std::vector<VimBlockEdit> edits;
    std::string tail;
    if (const auto *replaced = std::get_if<VimReplaceRange>(&effect))
    {
        if (replaced->range.begin.value < start.value || end.value < replaced->range.end.value ||
            replaced->utf8.find('\n') != std::string::npos)
        {
            return std::nullopt;
        }
        line.replace(replaced->range.begin.value - start.value,
                     replaced->range.end.value - replaced->range.begin.value, replaced->utf8);
        edits.push_back(VimBlockEdit{replaced->range, replaced->utf8});
    }
    if (line.size() <= insert.line_bytes)
    {
        return std::nullopt;
    }
    if (const auto *repeat = std::get_if<VimInsertAt>(&effect))
    {
        edits.push_back(VimBlockEdit{OffsetRange{repeat->at, repeat->at}, repeat->utf8});
        tail = repeat->utf8;
    }
    std::string body = line.substr(insert.at.value - start.value, line.size() - insert.line_bytes);
    body += tail;
    return std::pair{std::move(body), std::move(edits)};
}

// 矩形の INSERT の Esc（ADR 0035 の追補 #1042）。1 行目が入る前より伸びたぶんを打った本文として
// 読み（回数の繰り返しもその後ろに足す）、残りの行の同じ桁へ写して 1 行目の INSERT と 1 つの undo
// 単位にする。何も打たなかったら写さない。`I` `A` は Esc の行が 1 行目でなければ（改行を打った）
// 写さず（固定 Vim の op_insert）、`c` `C` は行を問わず今の行番号の行へ写す（op_change・どちらも
// 実測）。キャレットは `I` `A` なら矩形の左上（Esc の位置が手前ならそこ）、`c` `C` なら Esc
// のふつうの位置で、`'^` は打ち終えた 所のまま。
[[nodiscard]] VimStep block_copied(const VimBlockInsert &insert, const VimEditorView &view,
                                   VimStep step)
{
    const TextBuffer &text = view.text;
    const bool left_first_line = line_of(text, view.selection.caret) != insert.first;
    if (insert.caret.has_value() && left_first_line)
    {
        return step;
    }
    auto growth = first_line_growth(insert, text, step.effect);
    if (!growth.has_value())
    {
        return step;
    }
    std::string body = std::move(growth.value().first);
    std::vector<VimBlockEdit> edits = std::move(growth.value().second);
    Offset resting = view.selection.caret;
    if (const auto *repeat = std::get_if<VimInsertAt>(&step.effect))
    {
        resting = repeat->caret;
    }
    if (const auto *replaced = std::get_if<VimReplaceRange>(&step.effect))
    {
        resting = replaced->caret;
    }
    if (const auto *moved = std::get_if<VimMoveTo>(&step.effect))
    {
        resting = moved->caret;
    }
    const std::size_t last = std::min(insert.first.value + insert.lines, text.line_count() + 1);
    for (std::size_t index = single_step; insert.first.value + index < last; ++index)
    {
        auto edit = block_insert_edit(text, LineNumber{insert.first.value + index}, insert, body);
        if (edit.has_value())
        {
            edits.push_back(std::move(edit).value());
        }
    }
    // `I` `A` は矩形の左上へ戻る。打つ間に左上より前へ動いて打ったなら Esc の位置のまま（実測
    // `<Home>`）。
    const Offset caret{std::min(insert.caret.value_or(resting).value, resting.value)};
    const std::size_t inserted_end = resting.value + step.insert_left.value_or(0);
    step.insert_left = inserted_end > caret.value ? inserted_end - caret.value : 0;
    step.effect = VimInsertBlock{std::move(edits), caret, EditBoundary::absorb};
    return step;
}

[[nodiscard]] VimStep left_insert(const VimState &state, const VimEditorView &view)
{
    VimStep step = plain_left_insert(state, view);
    if (!state.block_insert.has_value())
    {
        return step;
    }
    return block_copied(state.block_insert.value(), view, std::move(step));
}

[[nodiscard]] VimStep insert_page_moved(const VimState &state, const VimEditorView &view,
                                        VimScrollDirection direction)
{
    const auto action = direction == VimScrollDirection::down ? VimAction::scroll_page_down
                                                              : VimAction::scroll_page_up;
    if (auto request = requested_viewport(state, view, action))
    {
        return std::move(request).value();
    }
    VimState next = state;
    next.insert_repeat = std::nullopt;
    // 格子の画面行の単位は NORMAL と同じ解決を通す（ADR 0144 の決定 5）。論理行の道は今日のまま。
    if (view.layout.enabled && view.layout.unit == VimScrollUnit::row)
    {
        const auto displayed = display_view(next, view, action);
        if (!displayed)
        {
            return requested_layout(state, displayed.error());
        }
        return page_step(next, displayed.value(), direction);
    }
    return page_step(next, view, direction);
}
} // namespace

namespace detail
{
[[nodiscard]] VimStep insert_special(const VimState &state, const VimEditorView &view,
                                     VimSpecialKey key)
{
    const TextBuffer &text = view.text;
    const Offset caret = view.selection.caret;
    switch (key)
    {
    case VimSpecialKey::escape:
        return left_insert(state, view);
    case VimSpecialKey::enter:
        return VimStep{insert_recorded(state, VimKey{key}), VimNewLine{}};
    case VimSpecialKey::backspace:
        return erase_recorded(insert_erased(state, text, caret), key);
    case VimSpecialKey::arrow_left:
        return insert_left(state, text, caret);
    case VimSpecialKey::arrow_right:
        return insert_right(state, text, caret);
    case VimSpecialKey::arrow_up:
        return insert_moved(state, text, caret, CaretMotion::previous_line);
    case VimSpecialKey::arrow_down:
        return insert_moved(state, text, caret, CaretMotion::next_line);
    case VimSpecialKey::home:
        return insert_moved(state, text, caret, CaretMotion::line_start);
    case VimSpecialKey::end:
        return insert_moved(state, text, caret, CaretMotion::line_end);
    case VimSpecialKey::control_w:
        return erase_recorded(insert_cut(state, text, caret, true), key);
    case VimSpecialKey::control_u:
        return erase_recorded(insert_cut(state, text, caret, false), key);
    case VimSpecialKey::control_r:
        return started_prefix(state, VimPrefix::insert_register);
    case VimSpecialKey::control_d:
    case VimSpecialKey::control_f:
    case VimSpecialKey::control_b:
    // INSERT の Ctrl+V は窓が OS の貼付のまま扱うのでここへ来ない（ADR 0035 の決定 9）。
    case VimSpecialKey::control_v:
    // INSERT の Ctrl-a（直前の挿入）と Ctrl-x（補完のモード）は未対応で、何もしない（ADR 0015 の
    // 決定 14）。窓は INSERT では写さない。
    case VimSpecialKey::control_a:
    case VimSpecialKey::control_x:
    // INSERT の Ctrl-o（Vim では NORMAL の命令を 1 つ）は未対応で、失敗にせず何もしない（#1037）。
    case VimSpecialKey::control_o:
        return VimStep{state, VimNoEffect{}};
    case VimSpecialKey::page_up:
        return insert_page_moved(state, view, VimScrollDirection::up);
    case VimSpecialKey::page_down:
        return insert_page_moved(state, view, VimScrollDirection::down);
    }
    std::unreachable();
}

[[nodiscard]] VimStep insert_character(const VimState &state, VimCharacter key)
{
    if (key.code == line_feed)
    {
        return VimStep{insert_recorded(state, VimKey{key}), VimNewLine{}};
    }
    std::string utf8;
    append_utf8(utf8, key.code);
    return VimStep{insert_recorded(state, VimKey{key}), VimInsertString{std::move(utf8)}};
}

// INSERT の `Ctrl-r {name}`（#1040）。名前 → 選択は `"` `@` と同じ register_selection_of、読みは
// register_read の 1 本。本文は 1 回の挿入で入れ（undo は INSERT の 1 単位のまま）、回数の入力記録
// にも本文の文字として足す（#1093）。名前にならない鍵・空のレジスタは何も入れず INSERT
// を続ける（Vim の実測）。
[[nodiscard]] VimStep insert_register_step(const VimState &state, const VimEditorView &view,
                                           const VimKey &key)
{
    VimState next = finished_input_wait(state);
    const auto *const character = std::get_if<VimCharacter>(&key);
    const auto selection =
        register_selection_of(character == nullptr ? char32_t{} : character->code);
    if (!selection.has_value())
    {
        return VimStep{std::move(next), VimNoEffect{}};
    }
    std::string body = register_read(state, selection).text;
    if (body.empty())
    {
        return VimStep{std::move(next), VimNoEffect{}};
    }
    const Offset caret = view.selection.caret;
    const Offset rest{caret.value + body.size()};
    for (Offset at{0}; at.value < body.size(); at = next_code_point(body, at))
    {
        next = insert_recorded(next, VimKey{VimCharacter{code_point_at(body, at)}});
    }
    return VimStep{std::move(next),
                   VimInsertAt{caret, std::move(body), rest, EditBoundary::absorb}};
}

// INSERT の消す鍵（Backspace・Ctrl-w・Ctrl-u）か（#1093）。鍵の種類が増えたら std::visit が
// erased_nothing で落ちる（CPP-002）。
namespace
{
[[nodiscard]] bool erases_in_insert(VimCharacter) noexcept
{
    return false;
}

[[nodiscard]] bool erases_in_insert(const VimSearchPattern &) noexcept
{
    return false;
}

[[nodiscard]] bool erases_in_insert(VimSpecialKey key) noexcept
{
    switch (key)
    {
    case VimSpecialKey::backspace:
    case VimSpecialKey::control_w:
    case VimSpecialKey::control_u:
        return true;
    case VimSpecialKey::escape:
    case VimSpecialKey::enter:
    case VimSpecialKey::arrow_left:
    case VimSpecialKey::arrow_right:
    case VimSpecialKey::arrow_up:
    case VimSpecialKey::arrow_down:
    case VimSpecialKey::home:
    case VimSpecialKey::end:
    case VimSpecialKey::page_up:
    case VimSpecialKey::page_down:
    case VimSpecialKey::control_r:
    case VimSpecialKey::control_d:
    case VimSpecialKey::control_f:
    case VimSpecialKey::control_b:
    case VimSpecialKey::control_v:
    case VimSpecialKey::control_a:
    case VimSpecialKey::control_x:
    case VimSpecialKey::control_o:
        return false;
    }
    std::unreachable();
}
} // namespace

// 何も消せずに止まった消す鍵（本文の先頭の Backspace・Ctrl-w・Ctrl-u・Vim のビープ）か（#1093）。
// Vim の ins_bs はビープで返る鍵を redo に足さないので、回数の鍵の列にも `.` の記録にも載せない。
[[nodiscard]] bool erased_nothing(const VimKey &key, const VimEffect &effect) noexcept
{
    return std::holds_alternative<VimNoEffect>(effect) &&
           std::visit([](const auto &value) { return erases_in_insert(value); }, key);
}

// INSERT で記録を `i` から取り直す鍵（決定 4）。ADR 0028 の決定 3 が挿入の反復を切るのと
// 同じ境界で、Vim も移動のあとの入力を新しい挿入として扱う（Home / End / 矢印で実測）。
[[nodiscard]] bool restarts_insert(VimKey key) noexcept
{
    if (!std::holds_alternative<VimSpecialKey>(key))
    {
        return false;
    }
    switch (std::get<VimSpecialKey>(key))
    {
    case VimSpecialKey::arrow_left:
    case VimSpecialKey::arrow_right:
    case VimSpecialKey::arrow_up:
    case VimSpecialKey::arrow_down:
    case VimSpecialKey::home:
    case VimSpecialKey::end:
    case VimSpecialKey::page_up:
    case VimSpecialKey::page_down:
        return true;
    case VimSpecialKey::escape:
    case VimSpecialKey::enter:
    case VimSpecialKey::backspace:
    case VimSpecialKey::control_r:
    case VimSpecialKey::control_d:
    case VimSpecialKey::control_u:
    case VimSpecialKey::control_f:
    case VimSpecialKey::control_b:
    case VimSpecialKey::control_v:
    case VimSpecialKey::control_w:
    case VimSpecialKey::control_a:
    case VimSpecialKey::control_x:
    case VimSpecialKey::control_o:
        return false;
    }
    std::unreachable();
}

namespace
{
// 移動の後に打った最初の鍵の前の始めの位置（Vim の stop_arrow と update_Insstart_orig・#1040）。
// キャレットの桁が始めの桁以下なら（行は比べない）始めをキャレットへ移し、右なら動かさない
// （`Axy<Left><Left><Left>z<C-w>` は z だけ・`jAxy<Up>z<C-u>` は行頭まで・oracle で実測）。
[[nodiscard]] VimInsertStart arrow_stopped(VimInsertStart start, VimInsertStart caret) noexcept
{
    if (caret.byte_column > start.byte_column)
    {
        start.moved = false;
        return start;
    }
    return caret;
}
} // namespace

// INSERT の鍵の前に始めの位置を整える（#1040）。最初の鍵ではキャレットから置き（INSERT へ入る
// 命令の効果は controller が写すので、入った後の最初の鍵で読むのが Vim の Insstart と同じ位置）、
// 移動の鍵では印を立て、印の後の最初の移動でない鍵で arrow_stopped を通す。
[[nodiscard]] VimState insert_started(const VimState &state, const VimEditorView &view,
                                      const VimKey &key)
{
    if (state.mode != VimMode::insert)
    {
        return state;
    }
    VimState next = state;
    const Offset caret = view.selection.caret;
    const LineNumber line = line_of(view.text, caret);
    const VimInsertStart here{line, caret.value - view.text.line_start(line).value, false};
    const bool moving = !state.input_wait.has_value() && restarts_insert(key);
    VimInsertStart start = state.insert_start.value_or(here);
    if (moving)
    {
        start.moved = true;
    }
    else if (start.moved)
    {
        start = arrow_stopped(start, here);
    }
    next.insert_start = start;
    return next;
}
} // namespace detail
} // namespace nenenib::core
