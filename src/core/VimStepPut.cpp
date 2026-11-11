#include "VimStepDetail.hpp"

#include "DocumentCaret.hpp"
#include "LineEnding.hpp"
#include "Utf8.hpp"
#include "VimBlockRange.hpp"
#include "VimLineColumns.hpp"
#include "VimMarkEdits.hpp"
#include "VimPutSide.hpp"
#include "VimVisualRange.hpp"
#include "VirtualColumn.hpp"

#include <algorithm>
#include <cstddef>
#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>

namespace nenenib::core
{
using detail::block_of;
using detail::block_operated;
using detail::cancelled;
using detail::count_of;
using detail::count_room;
using detail::failed;
using detail::fits_repeated;
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

// ---------------------------------------------------------------- 矩形の VISUAL の p / P（#1461）

// 矩形を消した後の本文。行ごとの置き換えを下の行から写す（上の行の位置を動かさない）。
[[nodiscard]] TextBuffer block_removed_text(const TextBuffer &text, const VimRemoveBlock &removal)
{
    TextBuffer after = text;
    for (std::size_t index = removal.edits.size(); index > 0; --index)
    {
        const VimBlockEdit &edit = removal.edits.at(index - single_step);
        after = after.replaced(edit.range.begin, edit.range.end, edit.utf8);
    }
    return after;
}

// 貼る所と向き。removed は消した後の本文で、キャレットが消した所。消した所が行末に残ると Vim の
// check_cursor はキャレットを最後の文字へ寄せ、nv_put_opt はそれが b_op_start（矩形の 1 行目の左の
// 文字の頭）より左なら前向き（FORWARD）に貼る。左の文字が端で切れた Tab
// や全角で、寄せたキャレットが その頭に戻るときは後ろ向き（`x<Tab>b` の `fb<C-v>jjp` は Tab
// の跡の空白の前・実測）。
using PutPlace = std::pair<Offset, VimPutSide>;

[[nodiscard]] PutPlace block_put_place(const VimState &state, const VimEditorView &view,
                                       const VimEditorView &removed)
{
    const TextBuffer &after = removed.text;
    const Offset caret = removed.selection.caret;
    const LineNumber line = line_of(after, caret);
    const Offset start = after.line_start(line);
    if (caret.value < after.line_end(line).value || start.value >= after.line_end(line).value)
    {
        return PutPlace{caret, VimPutSide::before};
    }
    const VimLineColumns columns(after, line);
    const Offset clamped{start.value + columns.character_start(columns.size(), single_step)};
    const Offset left = block_of(state, view).lines.front().range.begin;
    return PutPlace{clamped, clamped.value < left.value ? VimPutSide::after : VimPutSide::before};
}

// 1 行の文字単位の本文を矩形の各行へ入れる編集（Vim の do_put の VIsual_active の繰り返し）。1
// 行目は 消した所へ入れ、下の行は 1
// 行目の桁で始まる文字の前（行の終わりがその桁ならその後ろ）へ入れる。
// その桁で始まる文字が無い行（短い行・Tab や全角の途中）は飛ばす（getvpos の FAIL・実測）。
[[nodiscard]] std::vector<VimBlockEdit> block_lines_put(const TextBuffer &after, Offset at,
                                                        LineNumber last, const std::string &body)
{
    const LineNumber first = line_of(after, at);
    const std::size_t column =
        VimLineColumns(after, first).column_of(at.value - after.line_start(first).value).value;
    std::vector<VimBlockEdit> edits{VimBlockEdit{OffsetRange{at, at}, body}};
    for (std::size_t number = first.value + single_step; number <= last.value; ++number)
    {
        const VimLineColumns line(after, LineNumber{number});
        const std::size_t byte = line.byte_at(VirtualColumn{column});
        if (line.column_of(byte).value != column)
        {
            continue;
        }
        const Offset where{line.start().value + byte};
        edits.push_back(VimBlockEdit{OffsetRange{where, where}, body});
    }
    return edits;
}

// 消した後の本文の上の貼付の編集と、貼った後のキャレット。
using BlockPut = std::pair<std::vector<VimBlockEdit>, Offset>;
// それに貼った後の前の選択（gv）を添えたもの。
using PutResult = std::tuple<std::vector<VimBlockEdit>, Offset, VimLastVisual>;

// 文字単位と行単位の本文を NORMAL の `p` `P` と同じ put_step で貼った効果を、行ごとの編集に直す。
// キャレットは文書の改行で数える（CRLF の文書では入れた改行のぶん後ろ）。
[[nodiscard]] BlockPut inserted_put(const VimInsertAt &insert, LineEnding ending)
{
    return BlockPut{
        std::vector<VimBlockEdit>{VimBlockEdit{OffsetRange{insert.at, insert.at}, insert.utf8}},
        document_caret_after(insert.at, insert.utf8, insert.caret, ending)};
}

// 行単位の本文の前の選択。貼った最初の行の頭から最後の行の最後の文字まで（Vim の do_put の
// MLINE）。 leading は最後の行の下へ改行から書いた形（put_lines の最後の行の `p`）。
[[nodiscard]] VimLastVisual lines_put_visual(const VimState &state, const TextBuffer &after,
                                             const VimInsertAt &insert, bool leading)
{
    const std::string_view body = insert.utf8;
    const LineNumber first{line_of(after, insert.at).value + (leading ? single_step : 0)};
    const std::string_view last = leading ? trailing_line_of(body) : last_line_of(body);
    return put_visual(state, VimMarkPosition{first, 0},
                      VimMarkPosition{LineNumber{first.value + newlines_in(body) - single_step},
                                      last_character_byte(last)});
}

// 改行を含む文字単位の本文の前の選択。入れた所から最後の行の最後の文字まで。
[[nodiscard]] VimLastVisual characters_put_visual(const VimState &state, const TextBuffer &after,
                                                  const VimInsertAt &insert)
{
    const VimMarkPosition start = vim_mark_position_of(after, insert.at);
    return put_visual(state, start,
                      VimMarkPosition{LineNumber{start.line.value + newlines_in(insert.utf8)},
                                      last_character_byte(trailing_line_of(insert.utf8))});
}

// 貼った後の本文の上の位置を、消した後の本文の行 line の上の位置として読む（貼付は line より上の
// 行を変えないので行頭は同じ・行の終わりは伸びている）。
[[nodiscard]] VimMarkPosition mark_on(const TextBuffer &after, LineNumber line, Offset at)
{
    return VimMarkPosition{line, at.value - after.line_start(line).value};
}

// 改行を含まない文字単位の本文の矩形の各行への貼付。put は消した後の本文で、キャレットが入れる所。
// キャレットと前の選択の終わりは 1 行目に入れた本文の最後の文字（Vim は 1 行目のキャレットだけを
// 進める・実測）。
[[nodiscard]] std::expected<BlockPut, VimRepeatFailure>
lines_of_block_put(const VimState &state, const VimEditorView &put, LineNumber last,
                   const VimRegister &source)
{
    const TextBuffer &after = put.text;
    const Offset at = put.selection.caret;
    auto body = repeated(source.text, count_of(state.count), after, put.document_limit);
    if (!body.has_value())
    {
        return std::unexpected(body.error());
    }
    std::vector<VimBlockEdit> edits = block_lines_put(after, at, last, body.value());
    if (!fits_repeated(body.value().size(), edits.size(), count_room(after, put.document_limit)))
    {
        return std::unexpected(VimRepeatFailure::too_large);
    }
    const Offset caret{at.value + body.value().size() - last_code_point_size(body.value())};
    return BlockPut{std::move(edits), caret};
}

// 矩形を消す行ごとの置き換え（消す前の本文の位置）に、消した後の本文の上の貼付の編集を重ねて、消す前の
// 本文の上の行ごとの置き換えの列に畳む（上の行から順・重ならない）。消した所に触れる貼付はその行の
// 置き換えと 1 つに合わせ、触れない貼付は手前の置き換えが伸び縮みしたぶんだけ位置をずらす。
[[nodiscard]] std::vector<VimBlockEdit> block_put_merged(const TextBuffer &after,
                                                         const std::vector<VimBlockEdit> &removal,
                                                         const std::vector<VimBlockEdit> &puts)
{
    std::vector<VimBlockEdit> merged;
    std::size_t next = 0;
    // 通り過ぎた置き換えの消す前の長さの和と、消した後の長さの和。
    std::size_t before = 0;
    std::size_t after_size = 0;
    for (const VimBlockEdit &put : puts)
    {
        while (next < removal.size() && removal.at(next).range.begin.value + after_size - before +
                                                removal.at(next).utf8.size() <
                                            put.range.begin.value)
        {
            merged.push_back(removal.at(next));
            before += removal.at(next).range.end.value - removal.at(next).range.begin.value;
            after_size += removal.at(next).utf8.size();
            ++next;
        }
        if (next >= removal.size() ||
            removal.at(next).range.begin.value + after_size - before > put.range.end.value)
        {
            merged.push_back(
                VimBlockEdit{OffsetRange{Offset{put.range.begin.value + before - after_size},
                                         Offset{put.range.end.value + before - after_size}},
                             put.utf8});
            continue;
        }
        const VimBlockEdit &removed = removal.at(next);
        const std::size_t span_begin = removed.range.begin.value + after_size - before;
        const std::size_t span_end = span_begin + removed.utf8.size();
        const std::size_t window_begin = std::min(span_begin, put.range.begin.value);
        const std::size_t window_end = std::max(span_end, put.range.end.value);
        std::string utf8 = after.text_range(Offset{window_begin}, put.range.begin);
        utf8 += put.utf8;
        utf8 += after.text_range(put.range.end, Offset{window_end});
        const std::size_t origin_begin = window_begin + before - after_size;
        before += removed.range.end.value - removed.range.begin.value;
        after_size += removed.utf8.size();
        merged.push_back(VimBlockEdit{
            OffsetRange{Offset{origin_begin}, Offset{window_end + before - after_size}},
            std::move(utf8)});
        ++next;
    }
    merged.insert(merged.end(), removal.begin() + static_cast<std::ptrdiff_t>(next), removal.end());
    return merged;
}

// NORMAL の `p` `P` と同じ put_step で消した後の本文へ貼った効果を、行ごとの編集とキャレットと前の
// 選択に直す。put
// は消した後の本文で、キャレットが貼る所。矩形の本文の前の選択は貼った矩形の左上から
// 最後の行の最後に入れたバイトまで、文字単位と行単位は characters_put_visual と lines_put_visual。
// 何も入れない効果なら値が無い。
[[nodiscard]] std::expected<std::optional<PutResult>, VimRepeatFailure>
stepped_block_put(const VimState &state, const VimEditorView &put, VimPutSide side,
                  VimRegisterKind kind)
{
    const TextBuffer &after = put.text;
    const VimStep step = put_step(state, put, side);
    if (step.failure.has_value())
    {
        return std::unexpected(step.failure.value());
    }
    if (const auto *const insert = std::get_if<VimInsertAt>(&step.effect))
    {
        auto [edits, caret] = inserted_put(*insert, after.line_ending());
        const bool leading = side == VimPutSide::after &&
                             line_of(after, put.selection.caret).value >= after.line_count();
        const VimLastVisual visual = kind == VimRegisterKind::lines
                                         ? lines_put_visual(state, after, *insert, leading)
                                         : characters_put_visual(state, after, *insert);
        return PutResult{std::move(edits), caret, visual};
    }
    const auto *const block = std::get_if<VimInsertBlock>(&step.effect);
    if (block == nullptr || block->edits.empty())
    {
        return std::nullopt;
    }
    const LineNumber first = line_of(after, block->edits.front().range.begin);
    return PutResult{block->edits, block->caret,
                     put_visual(state, mark_on(after, first, block->caret),
                                block_put_end(after, block->edits.back()))};
}

// 消した後の本文 after の at
// にキャレットを置いた見方（貼る一歩に渡す・文書の上限は元の見方のまま）。
[[nodiscard]] VimEditorView put_view_of(const VimEditorView &view, const TextBuffer &after,
                                        const Selection &at)
{
    VimEditorView put{after, at, std::nullopt};
    put.document_limit = view.document_limit;
    return put;
}

// 矩形を消した後の本文へ貼る一歩（Vim の nv_put_opt の後の do_put）。removed は消した後の本文で、
// キャレットが消した所。行単位の本文は `p`
// なら選択の終わりの角（b_visual.vi_end＝キャレット）の行の 下へ（PUT_LINE_FORWARD）、`P`
// なら消した後のキャレットの向きで行の上か下へ。改行を含まない文字単位の
// 本文は矩形の各行へ、改行を含む文字単位と矩形の本文は NORMAL の `p` `P` と同じに消した所へ貼る。
// 値が無ければ何も貼らない（空の本文と `"_`）。
[[nodiscard]] std::expected<std::optional<PutResult>, VimRepeatFailure>
block_put_edits(const VimState &state, const VimEditorView &view, const VimEditorView &removed,
                VimPutSide key)
{
    const TextBuffer &after = removed.text;
    const VimRegister source = visual_put_source(state);
    if (source.kind == VimRegisterKind::lines && key == VimPutSide::after)
    {
        const Selection end =
            collapsed_at(after.line_start(line_of(view.text, view.selection.caret)));
        return stepped_block_put(state, put_view_of(view, after, end), VimPutSide::after,
                                 source.kind);
    }
    const auto [place, side] = block_put_place(state, view, removed);
    if (source.kind != VimRegisterKind::characters || source.text.contains('\n'))
    {
        const Selection placed = collapsed_at(place);
        return stepped_block_put(state, put_view_of(view, after, placed), side, source.kind);
    }
    if (source.text.empty())
    {
        return std::nullopt;
    }
    // 前向きなら寄せた最後の文字の後ろ（行の終わり）へ入れる（do_put の MCHAR の FORWARD）。
    const Offset at = side == VimPutSide::after ? after.line_end(line_of(after, place)) : place;
    const VimBlockRange block = block_of(state, view);
    const LineNumber last{block.first.value + block.lines.size() - single_step};
    const Selection inserted = collapsed_at(at);
    auto lines = lines_of_block_put(state, put_view_of(view, after, inserted), last, source);
    if (!lines.has_value())
    {
        return std::unexpected(lines.error());
    }
    auto [edits, caret] = std::move(lines).value();
    const LineNumber first = line_of(after, at);
    return PutResult{std::move(edits), caret,
                     put_visual(state, mark_on(after, first, at), mark_on(after, first, caret))};
}

// 矩形の VISUAL の `p` `P`（#1461・Vim の nv_put_opt）。矩形を VISUAL の `d` と同じ道で消し（`p` は
// 無名と `"1` `"-` へ、`P` は `"_` へ）、消した後の本文へ block_put_edits
// で貼り、消す置き換えと貼る 編集を 1 つの矩形の編集に畳む（undo 1 単位・戻り先は矩形の `d`
// と同じ左上）。貼れないレジスタ （E353）は文字単位の VISUAL と同じく消してから鳴らす。
[[nodiscard]] VimStep block_visual_put(const VimState &state, const VimEditorView &view,
                                       VimPutSide side)
{
    VimState removing = state;
    removing.selected_register =
        side == VimPutSide::before ? register_selection_of(U'_') : std::nullopt;
    VimStep removal = block_operated(removing, view, VimOperator::remove);
    const auto *const removed = std::get_if<VimRemoveBlock>(&removal.effect);
    if (removal.failure.has_value() || removed == nullptr)
    {
        return removal;
    }
    if (refuses_put(state, visual_put_source(state)))
    {
        return failed(std::move(removal), VimRepeatFailure::refused);
    }
    const TextBuffer after = block_removed_text(view.text, *removed);
    const Selection at = collapsed_at(removed->caret);
    const VimEditorView put_view = put_view_of(view, after, at);
    auto put = block_put_edits(state, view, put_view, side);
    if (!put.has_value())
    {
        return failed(cancelled(state), put.error());
    }
    std::optional<PutResult> found = std::move(put).value();
    if (!found.has_value())
    {
        // 何も貼らないときの前の選択は、消した後に寄せたキャレットの 1 文字（Vim の b_op_start と
        // b_op_end はどちらもキャレット）。
        const VimMarkPosition rest =
            vim_mark_position_of(after, block_put_place(state, view, put_view).first);
        removal.visual_after = put_visual(state, rest, rest);
        return removal;
    }
    auto [edits, caret, visual] = std::move(found).value();
    removal.effect = VimInsertBlock{block_put_merged(after, removed->edits, edits), caret,
                                    EditBoundary::separate};
    removal.visual_after = visual;
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
    if (state.mode == VimMode::visual_block)
    {
        return block_visual_put(state, view, side);
    }
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
