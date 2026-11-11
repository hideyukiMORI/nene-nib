#include "VimStepDetail.hpp"

#include "LineEnding.hpp"
#include "Utf8.hpp"
#include "VimMarkEdits.hpp"
#include "VimPutSide.hpp"
#include "VimVisualRange.hpp"

#include <algorithm>
#include <cstddef>
#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace nenenib::core
{
using detail::cancelled;
using detail::count_of;
using detail::failed;
using detail::forward_characters;
using detail::last_code_point_size;
using detail::line_of;
using detail::macro_name_of;
using detail::put_characters;
using detail::put_lines;
using detail::put_step;
using detail::refused_register;
using detail::refuses_put;
using detail::register_read;
using detail::register_selection_of;
using detail::removed_exactly;
using detail::removed_lines_range;
using detail::repeated;
using detail::selected_register;
using detail::single_step;
using detail::visual_restore;

namespace detail
{
// ---------------------------------------------------------------- p / P（決定 4）

[[nodiscard]] std::size_t count_room(const TextBuffer &text, std::size_t limit) noexcept
{
    const std::size_t size = text.size_bytes();
    return size < limit ? limit - size : 0;
}

[[nodiscard]] std::size_t written_bytes(std::string_view body, const TextBuffer &text) noexcept
{
    const auto newlines = static_cast<std::size_t>(std::ranges::count(body, '\n'));
    return body.size() + newlines * (newline_of(text.line_ending()).size() - single_step);
}

[[nodiscard]] bool fits_repeated(std::size_t piece, std::size_t count, std::size_t room) noexcept
{
    return piece == 0 || count <= room / piece;
}

// Vim は結果がメモリに入るかぎり貼る（`999999999p` は 1 GB の行を作り、`yy999999999p` は
// 止まらない・実測）。Nib は編集の後の文書が開ける上限を超える回数を、確保の前に閉じた失敗で断る
// （#1341）。CRLF の文書では本文の改行を 2 バイトに数える（controller が判定の後で写すため）。
[[nodiscard]] std::expected<std::string, VimRepeatFailure>
repeated(std::string_view body, std::size_t count, const TextBuffer &text, std::size_t limit)
{
    std::string result;
    if (body.empty())
    {
        return result;
    }
    if (!fits_repeated(written_bytes(body, text), count, count_room(text, limit)))
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
// 同じ）、`"a"b` は後勝ち。名前にならない鍵は回数ごと捨ててビープする（Esc は鳴らさずに打ち消す・
// `"<Esc>x` は x が効く・#1369）。それまでの回数は `"` の前の回数として覚え、後に打つ桁はそれに
// 掛ける（Vim の nv_regname の opcount・#1319）。
[[nodiscard]] VimStep selected_register(const VimState &state, const VimKey &key)
{
    const auto selection = register_selection_of(macro_name_of(key).value_or(char32_t{}));
    if (!selection.has_value())
    {
        return escaped_or_refused(state, key, refused_register(state));
    }
    VimState next = state;
    next.input_wait = std::nullopt;
    next.selected_register = selection.value();
    next.register_count = state.count.has_value()
                              ? std::optional{VimRegisterCount{state.count.value(), std::nullopt}}
                              : std::nullopt;
    return VimStep{std::move(next), VimNoEffect{}};
}
} // namespace detail

// ---------------------------------------------------------------- VISUAL の p / P（#1418）

namespace
{
// 貼る本文。Vim の nv_put_opt は選択を消す前に貼るレジスタを取っておき（reg1）、消した後に戻して
// から貼るので、読むのは消す前の状態（`"1p` `"-p` も消す前の本文・実測）。`"_` は空の文字単位
// （get_spec_reg が "" を返し、do_put は 1 行の空の本文として貼る＝行単位の VISUAL では空の行）。
[[nodiscard]] VimRegister visual_put_source(const VimState &state)
{
    const bool black_hole = state.selected_register.has_value() &&
                            state.selected_register.value().target == VimRegisterTarget::black_hole;
    return black_hole ? VimRegister{std::string{}, VimRegisterKind::characters}
                      : register_read(state, state.selected_register);
}

// 選択を消す一歩。`p` は名前なしの VISUAL の `d`（無名・`"1`・`"-` を書く）、`P` は `"_d`
// （レジスタを変えない・Vim の keep_registers）。範囲・戻り先・書き手は VISUAL の `d` と同じ道。
[[nodiscard]] VimStep visual_put_removal(const VimState &state, const VimEditorView &view,
                                         const VimMotionRange &range, VimPutSide side)
{
    VimState removing = state;
    removing.selected_register =
        side == VimPutSide::before ? register_selection_of(U'_') : std::nullopt;
    return removed_exactly(removing, view.text, range);
}

// 行として貼る本文（Vim の PUT_LINE は種類を行単位に読み替える）。文字単位と矩形は末尾に改行を
// 足す。
[[nodiscard]] std::string as_lines(const VimRegister &source)
{
    std::string body = source.text;
    if (source.kind != VimRegisterKind::lines)
    {
        body += '\n';
    }
    return body;
}

[[nodiscard]] std::size_t newlines_in(std::string_view body) noexcept
{
    return static_cast<std::size_t>(std::ranges::count(body, '\n'));
}

// 貼った本文の最後の行の最後の文字の頭のバイト（Vim の b_op_end.col・空の行は 0）。
[[nodiscard]] std::size_t last_character_byte(std::string_view line)
{
    return line.empty() ? 0 : line.size() - last_code_point_size(line);
}

// 行単位の本文（末尾が改行）の最後の行。
[[nodiscard]] std::string_view last_line_of(std::string_view lines) noexcept
{
    const std::string_view body = lines.substr(0, lines.size() - single_step);
    const std::size_t found = body.rfind('\n');
    return found == std::string_view::npos ? body : body.substr(found + single_step);
}

// 改行の後ろの最後の行（改行が無ければ全体）。
[[nodiscard]] std::string_view trailing_line_of(std::string_view body) noexcept
{
    const std::size_t found = body.rfind('\n');
    return found == std::string_view::npos ? body : body.substr(found + single_step);
}

// 前の選択（gv）は貼った本文の両端で、種類と欲しい列は鍵の前のまま（Vim の nv_put_opt は
// b_visual の vi_start と vi_end だけを置き直す）。
[[nodiscard]] VimLastVisual put_visual(const VimState &state, VimMarkPosition start,
                                       VimMarkPosition end)
{
    return VimLastVisual{state.mode, start, end, state.wanted_column};
}

// 選択を消す一歩と、その範囲（行単位は最後の行の改行を前の行から消す範囲）。
using PutRemoval = std::pair<VimStep, OffsetRange>;

// 行単位の VISUAL（Vim の PUT_LINE）。選んだ行を本文の行で置き換える。最後の行まで選んだときは
// 前の行の終わりから置き換え（消すと上の行の下へ貼る）、全部の行なら本文の終わりの空の行を残さない
// （nv_put_opt の empty の後始末・実測）。キャレットは貼った最初の行の最初の非空白。
[[nodiscard]] VimStep lines_replaced(const VimState &state, const VimEditorView &view,
                                     const VimRegister &source, PutRemoval put_removal)
{
    auto [removal, removed] = std::move(put_removal);
    const TextBuffer &text = view.text;
    const std::string line_body = as_lines(source);
    auto body = repeated(line_body, count_of(state.count), text, view.document_limit);
    if (!body.has_value())
    {
        return failed(cancelled(state), body.error());
    }
    const OffsetRange ordered = selection_range(view.selection);
    const LineNumber first = line_of(text, ordered.begin);
    const std::size_t lines = newlines_in(body.value());
    std::string utf8 = std::move(body).value();
    Offset start = removed.begin;
    if (line_of(text, ordered.end).value >= text.line_count())
    {
        utf8.pop_back();
        if (first.value > 1)
        {
            utf8.insert(utf8.begin(), '\n');
            start = Offset{start.value + single_step};
        }
    }
    const Offset caret{start.value + first_non_blank_index(line_body)};
    removal.effect = VimReplaceRange{removed, std::move(utf8), caret, EditBoundary::separate,
                                     VimLineMarks::shifted};
    removal.visual_after =
        put_visual(state, VimMarkPosition{first, 0},
                   VimMarkPosition{LineNumber{first.value + lines - single_step},
                                   last_character_byte(last_line_of(line_body))});
    return removal;
}

// 文字単位の VISUAL に行単位のレジスタ（Vim の PUT_LINE_SPLIT）。消した所で行を割り、その間に行を
// 貼る。キャレットは貼った最初の行の最初の非空白。
[[nodiscard]] VimStep line_split(const VimState &state, const VimEditorView &view,
                                 const VimRegister &source, PutRemoval put_removal)
{
    auto [removal, removed] = std::move(put_removal);
    const TextBuffer &text = view.text;
    auto body = repeated(source.text, count_of(state.count), text, view.document_limit);
    if (!body.has_value())
    {
        return failed(cancelled(state), body.error());
    }
    const LineNumber line = line_of(text, removed.begin);
    const std::size_t lines = newlines_in(body.value());
    std::string utf8 = "\n";
    utf8 += body.value();
    const Offset caret{removed.begin.value + single_step + first_non_blank_index(source.text)};
    removal.effect = VimReplaceRange{removed, std::move(utf8), caret, EditBoundary::separate,
                                     VimLineMarks::shifted};
    removal.visual_after =
        put_visual(state, VimMarkPosition{LineNumber{line.value + single_step}, 0},
                   VimMarkPosition{LineNumber{line.value + lines},
                                   last_character_byte(last_line_of(source.text))});
    return removal;
}

// 消した後の本文に貼った矩形の行ごとの編集を、消す前の本文の位置へ戻す。消した所に入る最初の
// 行の編集は消した範囲ごと置き換え、下の行の編集は消したバイト数だけ後ろへずらす。
[[nodiscard]] std::vector<VimBlockEdit> edits_before_removal(std::vector<VimBlockEdit> edits,
                                                             OffsetRange removed)
{
    const std::size_t removed_bytes = removed.end.value - removed.begin.value;
    for (VimBlockEdit &edit : edits)
    {
        if (edit.range.begin.value > removed.begin.value)
        {
            edit.range.begin = Offset{edit.range.begin.value + removed_bytes};
        }
        edit.range.end = Offset{edit.range.end.value + removed_bytes};
    }
    return edits;
}

// 矩形の貼付の最後の行の、最後に入れたバイト（Vim の do_put の MBLOCK が置く b_op_end）。after は
// 消した後の本文。
[[nodiscard]] VimMarkPosition block_put_end(const TextBuffer &after, const VimBlockEdit &last)
{
    const LineNumber line = line_of(after, last.range.begin);
    if (last.utf8.contains('\n'))
    {
        const std::string_view tail = trailing_line_of(last.utf8);
        return VimMarkPosition{LineNumber{line.value + newlines_in(last.utf8)},
                               tail.empty() ? 0 : tail.size() - single_step};
    }
    const std::size_t end =
        last.range.begin.value - after.line_start(line).value + last.utf8.size();
    return VimMarkPosition{line, end > 0 ? end - single_step : 0};
}

// 文字単位の VISUAL に文字単位か矩形のレジスタ。消した後の本文の消した所へ NORMAL の `P` と同じ
// put_step で貼り（消したら行末に残るときの Vim の `p` も同じ所へ入る）、その効果を消す前の本文の
// 位置へ戻して消す範囲と 1 つの編集に畳む。
[[nodiscard]] VimStep characters_put(const VimState &state, const VimEditorView &view,
                                     VimStep removal, OffsetRange removed)
{
    const TextBuffer &text = view.text;
    const TextBuffer after = text.replaced(removed.begin, removed.end, std::string_view{});
    const Selection at = collapsed_at(removed.begin);
    VimEditorView put_view{after, at, std::nullopt};
    put_view.document_limit = view.document_limit;
    VimStep put = put_step(state, put_view, VimPutSide::before);
    if (put.failure.has_value())
    {
        return put;
    }
    const VimMarkPosition start = vim_mark_position_of(text, removed.begin);
    removal.visual_after = put_visual(state, start, start);
    if (const auto *const insert = std::get_if<VimInsertAt>(&put.effect))
    {
        const std::string_view tail = trailing_line_of(insert->utf8);
        const std::size_t lines = newlines_in(insert->utf8);
        const std::size_t end =
            lines == 0 ? start.byte + insert->utf8.size() - last_code_point_size(insert->utf8)
                       : last_character_byte(tail);
        removal.visual_after.value().caret =
            VimMarkPosition{LineNumber{start.line.value + lines}, end};
        removal.effect = VimReplaceRange{removed, insert->utf8, insert->caret,
                                         EditBoundary::separate, VimLineMarks::shifted};
        return removal;
    }
    const auto *const block = std::get_if<VimInsertBlock>(&put.effect);
    if (block == nullptr || block->edits.empty())
    {
        // 空の本文（`"_` と空の文字単位）は消すだけで、キャレットは消した所に残る。
        return removal;
    }
    removal.visual_after.value().caret = block_put_end(after, block->edits.back());
    removal.effect = VimInsertBlock{edits_before_removal(block->edits, removed), block->caret,
                                    EditBoundary::separate};
    return removal;
}
} // namespace

namespace detail
{
// VISUAL の `p` `P`（#1418・Vim の nv_put_opt）。貼る本文を消す前に読み、選択を VISUAL の `d` と
// 同じ道で消して（`p` は無名と `"1` `"-` へ、`P` は `"_` へ）、消した所へ貼る。行単位の VISUAL は
// 本文を行として、文字単位の VISUAL は行単位の本文なら行を割って、文字単位と矩形の本文はそのまま
// 貼る。消して貼る全体を 1 つの編集で返すので undo 1 単位で、戻り先は VISUAL の `d` と同じ。
// 貼れないレジスタ（E353）でも Vim は先に消してから鳴らす（`lvp` は `b` を消す・実測）。
[[nodiscard]] VimStep visual_put(const VimState &state, const VimEditorView &view, VimPutSide side)
{
    const VimRegister source = visual_put_source(state);
    VimMotionRange range = vim_visual_range(view.text, view.selection, state.mode);
    range.restore = visual_restore(state, view.text, view.selection);
    VimStep removal = visual_put_removal(state, view, range, side);
    if (removal.failure.has_value())
    {
        return removal;
    }
    if (refuses_put(state, source))
    {
        return failed(std::move(removal), VimRepeatFailure::refused);
    }
    if (state.mode == VimMode::visual_line)
    {
        return lines_replaced(
            state, view, source,
            PutRemoval{std::move(removal), removed_lines_range(view.text, range)});
    }
    if (source.kind == VimRegisterKind::lines)
    {
        return line_split(state, view, source, PutRemoval{std::move(removal), range.range});
    }
    return characters_put(state, view, std::move(removal), range.range);
}
} // namespace detail
} // namespace nenenib::core
