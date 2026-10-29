#include "VimStepDetail.hpp"

#include "CaretMotion.hpp"
#include "CaretMove.hpp"
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
#include <utility>
#include <variant>
#include <vector>

namespace nenenib::core
{
using detail::backward_characters;
using detail::block_insert_edit;
using detail::display_view;
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
[[nodiscard]] VimStep insert_moved(const VimState &state, const TextBuffer &text, Offset caret,
                                   CaretMotion motion)
{
    VimState next = state;
    next.insert_repeat = std::nullopt;
    next.block_insert = std::nullopt;
    return VimStep{std::move(next),
                   VimMoveTo{moved_caret(
                       text, caret, CaretMoveRequest{motion, single_step, CaretUnit::code_point})}};
}

[[nodiscard]] VimState insert_recorded(const VimState &state, std::string_view input)
{
    VimState next = state;
    if (next.insert_repeat.has_value())
    {
        next.insert_repeat.value().text.append(input);
    }
    return next;
}

[[nodiscard]] VimState insert_record_erased(const VimState &state)
{
    VimState next = state;
    if (!next.insert_repeat.has_value())
    {
        return next;
    }
    std::string &input = next.insert_repeat.value().text;
    if (input.empty())
    {
        next.insert_repeat = std::nullopt;
        return next;
    }
    input.resize(previous_code_point(input, Offset{input.size()}).value);
    return next;
}

// `Ctrl-w` `Ctrl-u` が消した bytes バイトを回数の入力記録から外す（#1040）。記録の全部に届いたら
// 反復をやめる。Vim は記録の鍵を再生し、再生の `Ctrl-w` `Ctrl-u` は前の周の本文まで消すので、
// 本文の記録では写せない（`3Aab<C-u>c` は `xc` だけ・oracle で実測）。
[[nodiscard]] VimState insert_record_cut(const VimState &state, std::size_t bytes)
{
    VimState next = state;
    if (!next.insert_repeat.has_value())
    {
        return next;
    }
    std::string &input = next.insert_repeat.value().text;
    if (bytes >= input.size())
    {
        next.insert_repeat = std::nullopt;
        return next;
    }
    input.resize(input.size() - bytes);
    return next;
}

// 行頭で前の行とつなぐ（`backspace` の eol・#1040）。Backspace も `Ctrl-w` も `Ctrl-u` もここを
// 通り、1 鍵では改行だけを消す。INSERT を始めた行をつないだら、始めの位置はつないだ点へ移る
// （`jI<C-w>x<C-w>` は x だけを消す・oracle で実測）。
[[nodiscard]] VimStep joined_back(const VimState &state, const TextBuffer &text, Offset caret,
                                  LineNumber line)
{
    const LineNumber above{line.value - 1};
    const Offset end = text.line_end(above);
    VimState next = insert_record_erased(state);
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
        insert_record_erased(state),
        VimRemoveRange{OffsetRange{backward_characters(text, caret, single_step), caret}}};
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
// 境でも）消す。`.` の記録には鍵のまま残り、再生も同じ消し方をする（Vim の redo と同じ）。
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
    return VimStep{insert_record_cut(state, caret.value - begin.value),
                   VimRemoveRange{OffsetRange{begin, caret}}};
}

[[nodiscard]] VimStep plain_left_insert(const VimState &state, const VimEditorView &view)
{
    const Offset caret = view.selection.caret;
    const Offset back = backward_characters(view.text, caret, single_step);
    VimStep step{vim_resting_from(state, state.unnamed_register), VimMoveTo{back}};
    // `'^` は 1 つ左へ戻る前のキャレット（Vim の b_last_insert・#1039）。
    step.insert_left = caret.value - back.value;
    if (!state.insert_repeat.has_value() || state.insert_repeat.value().text.empty())
    {
        return step;
    }
    const VimInsertRepeat &repeat = state.insert_repeat.value();
    auto result = repeated(repeat.text, repeat.remaining);
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

// 矩形の INSERT の Esc（ADR 0035 の追補 #1042）。1 行目が入る前より伸びたぶんを打った本文として
// 読み（回数の繰り返しもその後ろに足す）、残りの行の同じ桁へ写して 1 行目の INSERT と 1 つの undo
// 単位にする。何も打たなかったら写さない。`I` `A` は Esc の行が 1 行目でなければ（改行を打った）
// 写さず（固定 Vim の op_insert）、`c` `C` は行を問わず今の行番号の行へ写す（op_change・どちらも
// 実測）。キャレットは `I` `A` なら矩形の左上、`c` `C` なら Esc のふつうの位置で、`'^` は打ち終えた
// 所のまま。
[[nodiscard]] VimStep block_copied(const VimBlockInsert &insert, const VimEditorView &view,
                                   VimStep step)
{
    const TextBuffer &text = view.text;
    const std::size_t bytes =
        text.line_end(insert.first).value - text.line_start(insert.first).value;
    const bool left_first_line = line_of(text, view.selection.caret) != insert.first;
    if ((insert.caret.has_value() && left_first_line) || bytes <= insert.line_bytes)
    {
        return step;
    }
    std::string body =
        text.text_range(insert.at, Offset{insert.at.value + bytes - insert.line_bytes});
    std::vector<VimBlockEdit> edits;
    Offset resting = view.selection.caret;
    if (const auto *repeat = std::get_if<VimInsertAt>(&step.effect))
    {
        edits.push_back(VimBlockEdit{OffsetRange{repeat->at, repeat->at}, repeat->utf8});
        body += repeat->utf8;
        resting = repeat->caret;
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
    const Offset caret = insert.caret.value_or(resting);
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
    next.block_insert = std::nullopt;
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
        return VimStep{insert_recorded(state, "\n"), VimNewLine{}};
    case VimSpecialKey::backspace:
        return insert_erased(state, text, caret);
    case VimSpecialKey::arrow_left:
        return insert_moved(state, text, caret, CaretMotion::previous_character);
    case VimSpecialKey::arrow_right:
        return insert_moved(state, text, caret, CaretMotion::next_character);
    case VimSpecialKey::arrow_up:
        return insert_moved(state, text, caret, CaretMotion::previous_line);
    case VimSpecialKey::arrow_down:
        return insert_moved(state, text, caret, CaretMotion::next_line);
    case VimSpecialKey::home:
        return insert_moved(state, text, caret, CaretMotion::line_start);
    case VimSpecialKey::end:
        return insert_moved(state, text, caret, CaretMotion::line_end);
    case VimSpecialKey::control_w:
        return insert_cut(state, text, caret, true);
    case VimSpecialKey::control_u:
        return insert_cut(state, text, caret, false);
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
        return VimStep{insert_recorded(state, "\n"), VimNewLine{}};
    }
    std::string utf8;
    append_utf8(utf8, key.code);
    return VimStep{insert_recorded(state, utf8), VimInsertString{std::move(utf8)}};
}

// INSERT の `Ctrl-r {name}`（#1040）。名前 → 選択は `"` `@` と同じ register_selection_of、読みは
// register_read の 1 本。本文は 1 回の挿入で入れ（undo は INSERT の 1 単位のまま）、回数の入力記録
// にも足す。名前にならない鍵・空のレジスタは何も入れず INSERT を続ける（Vim の実測）。
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
    next = insert_recorded(next, body);
    return VimStep{std::move(next),
                   VimInsertAt{caret, std::move(body), rest, EditBoundary::absorb}};
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
