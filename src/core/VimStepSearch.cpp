#include "VimStepDetail.hpp"

#include "LiteralPattern.hpp"
#include "VimAction.hpp"
#include "VimMarkEdits.hpp"
#include "VimMatchRequest.hpp"
#include "VimMotionRange.hpp"
#include "VimNumberedRule.hpp"
#include "VimSearch.hpp"
#include "VimSearchHit.hpp"
#include "VimSearchObject.hpp"
#include "VimSearchObjectRequest.hpp"
#include "VimSearchRequest.hpp"
#include "VimSmartcase.hpp"
#include "VimWordMotion.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace nenenib::core
{
using detail::characters_between;
using detail::combined_count;
using detail::context_marked;
using detail::exclusive_range;
using detail::operation_of;
using detail::operator_start;
using detail::opposite;
using detail::performed;
using detail::resolved_count;
using detail::rested_mode;
using detail::search_action;
using detail::searched_key;
using detail::visual_operator_applied;
using detail::visual_resting;

namespace detail
{
// ---------------------------------------------------------------- 検索（ADR 0032）

// 命令を食べ終わったあとのモード。VISUAL は VISUAL のまま残る（見つからなくても・実測）。
[[nodiscard]] VimMode rested_mode(VimMode mode)
{
    switch (mode)
    {
    case VimMode::normal:
    case VimMode::insert:
        return VimMode::normal;
    case VimMode::visual:
    case VimMode::visual_line:
    case VimMode::visual_block:
        return mode;
    }
    std::unreachable();
}
} // namespace detail

namespace
{
// 検索を食べ終わったあとの状態。
[[nodiscard]] VimState search_rested(const VimState &state)
{
    VimState next = vim_resting_from(state, state.unnamed_register);
    next.mode = rested_mode(state.mode);
    return next;
}

[[nodiscard]] VimSearchNotice notice_of(VimSearchNoticeKind kind)
{
    return VimSearchNotice{kind, std::string{}, std::nullopt};
}

// 報せを残して命令を取り消す（見つからない・直前が無い・語が無い・未対応の構文）。
[[nodiscard]] VimStep search_noticed(const VimState &state, VimSearchNotice notice)
{
    return VimStep{search_rested(state), VimNoEffect{}, std::move(notice),
                   VimRepeatFailure::not_found};
}

// 折り返したときだけ出る報せ（wrapscan は既定で有効）。
[[nodiscard]] std::optional<VimSearchNotice> wrap_notice(const VimSearchHit &hit,
                                                         VimSearchDirection direction)
{
    if (!hit.wrapped)
    {
        return std::nullopt;
    }
    return notice_of(direction == VimSearchDirection::forward
                         ? VimSearchNoticeKind::wrapped_to_top
                         : VimSearchNoticeKind::wrapped_to_bottom);
}

// 移動としての検索。NORMAL はキャレット、VISUAL は端点を動かす（決定 3）。
[[nodiscard]] VimStep search_moved(const VimState &state, const VimEditorView &view,
                                   Offset destination)
{
    VimState next = search_rested(state);
    next.wanted_column = VimWantedColumn{
        VimColumnWish::at_column,
        detail::landed_column(view.text, state.mode, view.selection.anchor, destination)};
    switch (state.mode)
    {
    case VimMode::normal:
    case VimMode::insert:
        return VimStep{std::move(next), VimMoveTo{destination}};
    case VimMode::visual:
    case VimMode::visual_line:
    case VimMode::visual_block:
        return VimStep{std::move(next), VimSelect{Selection{view.selection.anchor, destination}}};
    }
    std::unreachable();
}

// オペレータの後ろの検索は exclusive で、既存の 1 本の規則を通る（決定 3）。
// 範囲が空（回数が自分自身へ折り返した）ときも同じ道を通る（#1344）。Vim の do_pending_operator
// は空の範囲を oap->empty にするだけで（cpo-E が既定で無い）、`d` は op_delete が何も書かずに
// 返り、`c` は INSERT に入り、`y` は空の文字単位を yank する（実測）。1 桁目の `d0` `c0`
// `y0` と同じ規則なので、performed の空の範囲の扱いがそのまま当てはまる。
[[nodiscard]] VimStep search_operated(const VimState &state, const VimEditorView &view,
                                      const VimSearchRequest &request, Offset destination)
{
    VimMotionRange range =
        exclusive_range(view.text, characters_between(request.anchor, destination));
    // 検索の移動の削除は 1 行の中でも `"1` へ行く（ADR 0050 の決定 4）。
    range.numbered = VimNumberedRule::always;
    // 後ろ向きで行単位になった y は検索の当たりに着く（Vim の oap->start・#1294）。
    return performed(state, view.text, operator_start(request.anchor, destination), range);
}

// 検索の鍵は `:nohlsearch` で止めた強調を戻す（ADR 0037 の決定 1）。見つからなくても（E486）、
// 直前のパターンが無くても（E35）戻り、`:set nohlsearch` の off は戻らない（固定 Vim 9.1 の
// `v:hlsearch` で実測）。入力行を開くだけの `/` `?` は確定ではないので通らない。
[[nodiscard]] VimState search_highlighted(VimState state)
{
    if (state.highlight == VimSearchHighlight::suspended)
    {
        state.highlight = VimSearchHighlight::on;
    }
    return state;
}

// 検索が着く位置（Vim の normal_search の最後の check_cursor・#1606）。行の内容の終わり（NUL）の
// 当たり（`/$` `/\>` など幅の無い当たり）は、NORMAL とオペレータ待ちでは最後の文字の上に寄せ、
// その寄せた位置がオペレータの範囲の端になる（exclusive なので `d/$` は最後の文字を残す）。VISUAL
// と INSERT の `Ctrl-o` の命令を待つ間は Vim の check_cursor_col が NUL を許すので寄せない
// （`v/$d` は改行まで・`i<C-o>d/$` は行の全体を消す・実測）。
[[nodiscard]] Offset searched_landing(const VimState &state, const TextBuffer &text,
                                      const VimSearchHit &hit)
{
    const Offset at = text.offset_of(hit.position);
    return vim_insert_paused(state) ? at : detail::rested_in(text, at, state.mode);
}

// 解析済みの last_search が正本。回数ぶん探し、着いた先へ 1 本で流す（決定 3・4）。
[[nodiscard]] VimStep search_from(const VimState &state, const VimEditorView &view,
                                  const VimSearchRequest &request)
{
    if (!state.last_search.has_value())
    {
        return search_noticed(state, notice_of(VimSearchNoticeKind::no_previous_pattern));
    }
    const auto &remembered = state.last_search.value();
    const auto &parsed = remembered.parsed();
    if (!parsed)
    {
        return search_noticed(state, VimSearchNotice{VimSearchNoticeKind::unsupported_pattern,
                                                     std::string{}, parsed.error()});
    }
    const auto hit = vim_find_match(view.text, parsed.value(),
                                    VimMatchRequest{view.text.position_of(request.origin),
                                                    request.direction, resolved_count(state)});
    if (!hit.has_value())
    {
        return search_noticed(
            state, VimSearchNotice{hit.error(), std::string{remembered.text()}, std::nullopt});
    }
    const Offset destination = searched_landing(state, view.text, hit.value());
    VimStep step = state.pending.has_value() ? search_operated(state, view, request, destination)
                                             : search_moved(state, view, destination);
    step.notice = wrap_notice(hit.value(), request.direction);
    // 見つかった検索は飛ぶ鍵（Vim の jump-motions・ADR 0152 の決定 4）。
    return context_marked(std::move(step), view);
}
} // namespace

namespace detail
{
// 入力行の Enter（決定 3）。空のパターンは直前を使い直し、無ければ E35。
// 見つからない検索も last_search を更新する（次の `n` が同じ失敗を繰り返す・実測）。
[[nodiscard]] VimStep searched_key(const VimState &original, const VimEditorView &view,
                                   const VimSearchPattern &key)
{
    const VimState state = search_highlighted(original);
    const std::string_view previous =
        state.last_search.has_value() ? state.last_search.value().text() : std::string_view{};
    const std::string_view pattern = key.pattern.empty() ? previous : std::string_view{key.pattern};
    if (pattern.empty())
    {
        return search_noticed(state, notice_of(VimSearchNoticeKind::no_previous_pattern));
    }
    VimState remembered = state;
    // 空のパターンは直前の smartcase の扱いも引き継ぐ（`*` の後の `/<CR>`・Vim の no_scs）。
    const VimSmartcase smartcase = key.pattern.empty() && state.last_search.has_value()
                                       ? state.last_search.value().smartcase()
                                       : VimSmartcase::honored;
    remembered.last_search =
        VimSearchSnapshot::from(pattern, key.direction, view.search_case, smartcase);
    // incsearch の Ctrl-G / Ctrl-T が起点を動かしていればそこから探す。範囲の端は元のキャレット
    // （ADR 0043 の決定 3）。
    const Offset caret = view.selection.caret;
    const Offset from = key.from.has_value() ? view.text.offset_of(key.from.value()) : caret;
    return search_from(remembered, view, VimSearchRequest{from, caret, key.direction});
}
} // namespace detail

namespace
{
// `n` / `N`（決定 3）。覚えた向きのまま、または反対の向きで探す。last_search は変えない。
[[nodiscard]] VimStep repeated_search(const VimState &original, const VimEditorView &view,
                                      VimAction action)
{
    const VimState state = search_highlighted(original);
    if (!state.last_search.has_value())
    {
        return search_noticed(state, notice_of(VimSearchNoticeKind::no_previous_pattern));
    }
    const VimSearchDirection remembered = state.last_search.value().direction();
    const VimSearchDirection direction =
        action == VimAction::repeat_search ? remembered : opposite(remembered);
    const Offset caret = view.selection.caret;
    return search_from(state, view, VimSearchRequest{caret, caret, direction});
}

// `*` / `#` が拾った本文の magic の特殊文字を `\` で escape する（Vim の nv_ident の aux_ptr・
// Issue #961）。逃がし方は通常モードの検索語と同じ literal_pattern の 1 本で、`#` は区切りの
// `?` も逃がす（ADR 0174 の決定 4 と追補）。
[[nodiscard]] std::string escaped_search_text(std::string_view text, VimSearchDirection direction)
{
    return literal_pattern(text, direction);
}

// `*` / `#`（決定 3 と Issue #961 の追補）。キャレットの下か後ろの語を \<…\> のパターンにし、
// 語が無ければ空白でない文字列を \< \> なしで、同じ経路へ。どちらも無ければ E348。
[[nodiscard]] VimStep word_search(const VimState &original, const VimEditorView &view,
                                  VimAction action)
{
    const VimState state = search_highlighted(original);
    const auto word = vim_word_at(view.text, view.selection.caret);
    const auto found = word.has_value() ? word : vim_string_at(view.text, view.selection.caret);
    if (!found.has_value())
    {
        return search_noticed(state, notice_of(VimSearchNoticeKind::no_word_under_cursor));
    }
    const VimSearchDirection direction = action == VimAction::search_word_forward
                                             ? VimSearchDirection::forward
                                             : VimSearchDirection::backward;
    const std::string text = escaped_search_text(
        view.text.text_range(found.value().begin, found.value().end), direction);
    VimState remembered = state;
    remembered.last_search =
        VimSearchSnapshot::from(word.has_value() ? "\\<" + text + "\\>" : text, direction,
                                view.search_case, VimSmartcase::ignored);
    // 探し始めるのは語の先頭だが、範囲の端は元のキャレットである（実測）。
    return search_from(remembered, view,
                       VimSearchRequest{found.value().begin, view.selection.caret, direction});
}

// オペレータ待ちの `gn` `gN`（#1617）。Vim の current_search は当たりを VISUAL として立て、
// do_pending_operator がその VISUAL の範囲にオペレータを効かせる（is_VIsual）。同じ道として、選んだ
// 当たりを文字単位の選択に置いて VISUAL のオペレータの 1 本へ流す。回数は当たりを選ぶのに使い、
// `>` `<` では字下げの回数にもなる（Vim の op_shift の cap->count1・`2>gn` は 2 段・実測）。
// `.` の記録は NORMAL の鍵の列（`cgnX<Esc>`）のままで、再生は次の当たりを選び直す。抜けた選択は
// `gv` と `'<` `'>` が読む前の選択になる（`dgngv` は消した所を選ぶ・実測）。
[[nodiscard]] VimStep object_operated(const VimState &state, const VimEditorView &view,
                                      const Selection &selected)
{
    VimState visual = state;
    visual.mode = VimMode::visual;
    visual.count = combined_count(state);
    visual.pending = std::nullopt;
    visual.input_wait = std::nullopt;
    // 選択は借用の欄なので、ほかの欄を写した view を選んだ当たりで組み直す。欄を全部ほどいてから
    // 名指しで組むので、VimEditorView に欄が足されるとほどく数が合わずにコンパイルが落ちる
    // （足した欄を黙って既定値にしない・#1617 のレビュー L3）。
    const auto &[text, selection, viewport, source, tabs, layout, navigation, row_landing,
                 view_marks, mark_landing, search_case, scroll_margin, document_limit,
                 capture_limit] = view;
    static_cast<void>(selection);
    const VimEditorView over{.text = text,
                             .selection = selected,
                             .viewport = viewport,
                             .source = source,
                             .tabs = tabs,
                             .layout = layout,
                             .navigation = navigation,
                             .row_landing = row_landing,
                             .marks = view_marks,
                             .mark_landing = mark_landing,
                             .search_case = search_case,
                             .scroll_margin = scroll_margin,
                             .document_limit = document_limit,
                             .capture_limit = capture_limit};
    VimStep step = visual_operator_applied(visual, over, operation_of(state));
    VimMarks marks = step.marks.value_or(*view.marks);
    marks.last_visual =
        VimLastVisual{VimMode::visual, vim_mark_position_of(view.text, selected.anchor),
                      vim_mark_position_of(view.text, selected.caret), std::nullopt};
    step.marks = std::move(marks);
    return step;
}

// `gn` `gN`（#1617・Vim の nv_g_cmd の `n` `N` → current_search）。最後の検索のパターンで当たりを
// 選ぶ 1 本で、NORMAL は文字単位の VISUAL に入り、VISUAL は anchor を残して広げ（行単位と矩形も
// 文字単位になる）、オペレータ待ちはその選択に効かせる。範囲は vim_search_object の 1 本が決める。
// 欲しい列は Vim と同じく動かさない（current_search は curswant を書かない）。
// 検索の強調・ジャンプの列・覚えた向きは変えない（SEARCH_KEEP）。最後の検索が無ければ E35、
// 当たりが無ければ報せを出さずに失敗し、オペレータは消えて VISUAL は選択のまま残る（clearopbeep）。
[[nodiscard]] VimStep matched_object(const VimState &state, const VimEditorView &view,
                                     VimAction action)
{
    if (!state.last_search.has_value())
    {
        return search_noticed(state, notice_of(VimSearchNoticeKind::no_previous_pattern));
    }
    const auto &parsed = state.last_search.value().parsed();
    if (!parsed)
    {
        return search_noticed(state, VimSearchNotice{VimSearchNoticeKind::unsupported_pattern,
                                                     std::string{}, parsed.error()});
    }
    const bool visual = is_visual_mode(state.mode);
    const VimSearchObjectRequest request{
        visual ? view.selection : collapsed_at(view.selection.caret),
        visual ? VimSearchObjectFrom::selection : VimSearchObjectFrom::caret,
        action == VimAction::select_next_match ? VimSearchDirection::forward
                                               : VimSearchDirection::backward,
        resolved_count(state)};
    const auto selected = vim_search_object(view.text, parsed.value(), request);
    if (!selected.has_value())
    {
        return VimStep{search_rested(state), VimNoEffect{}, std::nullopt,
                       VimRepeatFailure::not_found};
    }
    if (state.pending.has_value())
    {
        return object_operated(state, view, selected.value());
    }
    VimState next = visual_resting(state, VimMode::visual);
    next.wanted_column = state.wanted_column;
    return VimStep{std::move(next), VimSelect{selected.value()}};
}

// 検索の入力行を開く（決定 2）。VimState は変えないので保留・回数・記録はそのまま残る。
[[nodiscard]] VimStep opened_search(const VimState &state, VimSearchDirection direction)
{
    return VimStep{state, VimOpenSearch{direction}};
}
} // namespace

namespace detail
{
[[nodiscard]] VimStep search_action(const VimState &state, const VimEditorView &view,
                                    VimAction action)
{
    if (action == VimAction::open_search_forward)
    {
        return opened_search(state, VimSearchDirection::forward);
    }
    if (action == VimAction::open_search_backward)
    {
        return opened_search(state, VimSearchDirection::backward);
    }
    if (action == VimAction::repeat_search || action == VimAction::repeat_search_opposite)
    {
        return repeated_search(state, view, action);
    }
    if (action == VimAction::select_next_match || action == VimAction::select_previous_match)
    {
        return matched_object(state, view, action);
    }
    return word_search(state, view, action);
}
} // namespace detail
} // namespace nenenib::core
