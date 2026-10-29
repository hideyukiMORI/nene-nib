#include "VimStepDetail.hpp"

#include "VimAction.hpp"
#include "VimMatchRequest.hpp"
#include "VimMotionRange.hpp"
#include "VimNumberedRule.hpp"
#include "VimSearch.hpp"
#include "VimSearchHit.hpp"
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
using detail::context_marked;
using detail::exclusive_range;
using detail::opposite;
using detail::performed;
using detail::resolved_count;
using detail::rested_mode;
using detail::search_action;
using detail::searched_key;

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
// 範囲が空（回数が自分自身へ折り返した）ときは本文もレジスタも変えない（実測）。
[[nodiscard]] VimStep search_operated(const VimState &state, const VimEditorView &view,
                                      const VimSearchRequest &request, Offset destination)
{
    if (destination == request.anchor)
    {
        return VimStep{search_rested(state), VimNoEffect{}};
    }
    VimMotionRange range =
        exclusive_range(view.text, characters_between(request.anchor, destination));
    // 検索の移動の削除は 1 行の中でも `"1` へ行く（ADR 0050 の決定 4）。
    range.numbered = VimNumberedRule::always;
    return performed(state, view.text, request.anchor, range);
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
    const Offset destination = view.text.offset_of(hit.value().position);
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
// Issue #961）。`#` は区切りの `?` も escape する。多バイト文字のバイトは ASCII と重ならない。
[[nodiscard]] std::string escaped_search_text(std::string_view text, VimSearchDirection direction)
{
    constexpr std::string_view special = "/.*~[^$\\";
    std::string escaped;
    for (const char byte : text)
    {
        if (special.contains(byte) || (byte == '?' && direction == VimSearchDirection::backward))
        {
            escaped.push_back('\\');
        }
        escaped.push_back(byte);
    }
    return escaped;
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
    return word_search(state, view, action);
}
} // namespace detail
} // namespace nenenib::core
