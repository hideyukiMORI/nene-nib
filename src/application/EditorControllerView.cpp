#include "EditorController.hpp"
#include "EditorControllerDetail.hpp"

#include "CaretAffinity.hpp"
#include "DisplayInquiry.hpp"
#include "LineNumber.hpp"
#include "Offset.hpp"
#include "PlaceCaret.hpp"
#include "PointerClicks.hpp"
#include "PointerSelection.hpp"
#include "ScreenMotion.hpp"
#include "Selection.hpp"
#include "TextBuffer.hpp"
#include "TextCaret.hpp"
#include "TextPosition.hpp"
#include "VimCaret.hpp"
#include "VimCharacterBoundary.hpp"
#include "VimMode.hpp"
#include "VimTextObject.hpp"
#include "VimTextObjectRange.hpp"
#include "VimTextObjectRequest.hpp"
#include "VimTextObjectScope.hpp"
#include "VimTextObjectSpan.hpp"
#include "VimVisualRange.hpp"
#include "VisualPosition.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

namespace nenenib::application
{
using detail::scroll_extent_for;

namespace
{
// 押した点の手前の文字を決めるために読む幅（結合文字の連なりを含む Vim の 1 文字に足りる）。
constexpr std::size_t pointed_window_bytes = 64;

// 押した文字の語（`iw`）の VISUAL の選択（caret は含む側の端）。範囲にならない（空の本文）なら
// キャレットのまま。通常モードと Vim モードが同じ 1 本を使う。
[[nodiscard]] core::Selection clicked_word_visual(const core::TextBuffer &text, core::Offset rest)
{
    const auto outcome = core::vim_text_object_range(
        text, core::Selection{rest, rest},
        core::VimTextObjectRequest{core::VimTextObjectScope::inner, core::VimTextObject::word,
                                   core::VimMode::visual},
        1);
    const auto *span = std::get_if<core::VimTextObjectSpan>(&outcome);
    return span == nullptr ? core::Selection{rest, rest} : span->selection;
}

// 通常モードの語は VISUAL の選択を描く範囲（vim_visual_range）と同じ写しで半開区間にする。
[[nodiscard]] core::Selection clicked_word(const core::TextBuffer &text, core::Offset rest)
{
    const auto range =
        core::vim_visual_range(text, clicked_word_visual(text, rest), core::VimMode::visual).range;
    return core::Selection{range.begin, range.end};
}

// 押した行を行頭から次の行の頭まで（最終行は本文の終わり）。
[[nodiscard]] core::Selection clicked_line(const core::TextBuffer &text, core::Offset rest)
{
    const auto line = text.position_of(rest).line;
    const auto end = line.value < text.line_count()
                         ? text.line_start(core::LineNumber{line.value + 1})
                         : core::Offset{text.size_bytes()};
    return core::Selection{text.line_start(line), end};
}

[[nodiscard]] core::ScrollFollow scroll_follow_for(core::EditMode mode) noexcept
{
    switch (mode)
    {
    case core::EditMode::ordinary:
        return core::ScrollFollow::minimal;
    case core::EditMode::vim:
        return core::ScrollFollow::vim;
    }
    std::unreachable();
}

[[nodiscard]] std::optional<ScreenMotion> screen_motion(core::CaretMotion motion,
                                                        std::size_t height)
{
    switch (motion)
    {
    case core::CaretMotion::previous_line:
        return ScreenMotion{-1, std::nullopt};
    case core::CaretMotion::next_line:
        return ScreenMotion{1, std::nullopt};
    case core::CaretMotion::page_up:
        return ScreenMotion{-static_cast<std::int64_t>(height), std::nullopt};
    case core::CaretMotion::page_down:
        return ScreenMotion{static_cast<std::int64_t>(height), std::nullopt};
    case core::CaretMotion::line_start:
        // 行の原点より左の最小の x。巨大な行の窓の x は窓の始まりからなので、0 では行頭に
        // 着かない（ADR 0139 の決定 4）。
        return ScreenMotion{0, std::numeric_limits<float>::lowest()};
    case core::CaretMotion::line_end:
        return ScreenMotion{0, std::numeric_limits<float>::max()};
    case core::CaretMotion::previous_character:
    case core::CaretMotion::next_character:
    case core::CaretMotion::previous_word:
    case core::CaretMotion::next_word:
    case core::CaretMotion::document_start:
    case core::CaretMotion::document_end:
        return std::nullopt;
    }
    std::unreachable();
}

[[nodiscard]] bool page_motion(core::CaretMotion motion) noexcept
{
    return motion == core::CaretMotion::page_up || motion == core::CaretMotion::page_down;
}
} // namespace

void EditorController::follow_caret()
{
    follow_position(state_.text().position_of(state_.selection().caret));
}

// 位置が見える範囲に入るようにスクロールすることを頼む。キャレットと incsearch の preview が
// 同じ計算を使う（ADR 0041 の決定 3）。layout は作らず、解決はスクロールを読む所で 1 回だけ
// （ADR 0128 の決定 1・2）。
void EditorController::follow_position(core::TextPosition position)
{
    pending_follow_ = position;
}

// 保留を先に空にしてから view を作るので、visual_view() の先頭の解決へ再入しない。解決はその
// 時点の state で 1 回（ADR 0128 の決定 4）。保留の clamp は追従より先（決定 8）。問いの答えを
// 待つ間は文字組みせず、保留の追従と clamp を消費せずに残して戻る。答えて表示形式が決まった後の
// 最初のスクロールの読みで解決する（ADR 0140 の決定 2）。
void EditorController::settle_follow()
{
    if (display_pending(state_.display_inquiry()))
    {
        return;
    }
    const bool clamp = std::exchange(pending_clamp_, false);
    const auto pending = std::exchange(pending_follow_, std::nullopt);
    if (clamp)
    {
        clamp_scroll();
    }
    if (!pending.has_value())
    {
        return;
    }
    const auto followed =
        visual_view().followed(core::TextCaret{pending.value(), state_.caret_affinity()},
                               scroll_follow_for(state_.mode()), scroll_extent_for(state_.mode()));
    if (!followed)
    {
        layout_failure_ = followed.error();
        return;
    }
    if (!(followed.value() == state_.scroll()))
    {
        state_ = state_.with_scroll(followed.value());
    }
}

// 問いの答えを待つ間の保留は、追従も clamp も、その文書を離れるときに捨てる。問いの答え
// （SelectDisplay）が clamp_scroll と follow_caret を頼み直すので失うものは無い（ADR 0128 の
// 決定 7）。
void EditorController::drop_pending_follow()
{
    if (display_pending(state_.display_inquiry()))
    {
        pending_follow_.reset();
        pending_clamp_ = false;
    }
}

const ScrollState &EditorController::settled_scroll()
{
    settle_follow();
    return state_.scroll();
}

void EditorController::clamp_scroll()
{
    // 起動時は幅が未確定。保存した画面行を最初のVisibleLinesまで保つ。問いの答えを待つ間も保つ
    // （ADR 0140 の決定 2・答えの SelectDisplay が収め直す）。
    if ((view_geometry_.width <= 0 && settled_scroll().first_row > 0) ||
        display_pending(state_.display_inquiry()))
    {
        return;
    }
    const auto &scroll = settled_scroll();
    const auto within =
        visual_view().constrained(core::VisualPosition{scroll.first_visible, scroll.first_row},
                                  scroll_extent_for(state_.mode()));
    if (!within)
    {
        layout_failure_ = within.error();
        return;
    }
    state_ = state_.with_scroll(ScrollState{within.value().line, scroll.visible_lines,
                                            within.value().row, scroll.horizontal});
}

// view はスクロールを読むので、頼まれた追従を先に解決する（ADR 0128 の決定 2）。
// 問いの答えを待つ間は幅の無い窓として解決し、文字組みの port を呼ばない（ADR 0140 の決定 2）。
VisualTextView EditorController::visual_view()
{
    settle_follow();
    return VisualTextView{state_, ports_.text_layouts,
                          display_pending(state_.display_inquiry()) ? core::TextViewGeometry{}
                                                                    : view_geometry_};
}

bool EditorController::move_visual(const MoveCaret &intent, core::Offset origin)
{
    const auto motion = screen_motion(intent.motion, settled_scroll().visible_lines);
    if (view_geometry_.width <= 0 || state_.mode() != core::EditMode::ordinary ||
        !motion.has_value())
    {
        wanted_x_.reset();
        return false;
    }
    auto view = visual_view();
    const core::Offset caret_offset = state_.selection().caret;
    const auto point = view.location(
        core::TextCaret{state_.text().position_of(caret_offset), state_.caret_affinity()});
    if (!point)
    {
        layout_failure_ = point.error();
        return true;
    }
    // 横位置はキャレット（動く端）のもの、画面行は起点のもの（#1393）。
    const auto source = state_.text().position_of(origin);
    const auto from = origin_location(view, point.value(), origin);
    if (!from)
    {
        layout_failure_ = from.error();
        return true;
    }
    const float x = motion.value().x.value_or(wanted_x_.value_or(point.value().x));
    const auto moved =
        view.reached(core::VisualPosition{source.line, from.value().row}, motion.value().rows);
    if (!moved)
    {
        layout_failure_ = moved.error();
        return true;
    }
    if (moved.value().stopped)
    {
        move_to_text_edge(view, intent, motion.value().rows);
        return true;
    }
    wanted_x_ = motion.value().x.has_value() ? std::nullopt : std::optional{x};
    const auto caret = view.caret_at(moved.value().at, x);
    if (!caret)
    {
        layout_failure_ = caret.error();
        return true;
    }
    if (page_motion(intent.motion) && !page_top_shifted(view, motion.value().rows))
    {
        return true;
    }
    move_caret_to(state_.text().offset_of(caret.value().position), intent.anchoring,
                  caret.value().affinity);
    return true;
}

// 上下の移動の起点の位置（#1393）。起点がキャレットならキャレットの位置（at_caret）のまま。
std::expected<core::TextLocation, TextLayoutFailure>
EditorController::origin_location(VisualTextView &view, const core::TextLocation &at_caret,
                                  core::Offset origin) const
{
    if (origin == state_.selection().caret)
    {
        return at_caret;
    }
    return view.location(
        core::TextCaret{state_.text().position_of(origin), origin_affinity(origin)});
}

// 起点の画面行の側（#1393）。起点がキャレットならキャレットの affinity のまま。キャレットでない側の
// 端なら、折り返しの境目では先頭は後ろの画面行（leading）、末尾は前の画面行（trailing）＝選択を
// 描いていた側。
core::CaretAffinity EditorController::origin_affinity(core::Offset origin) const noexcept
{
    const core::Offset caret = state_.selection().caret;
    if (origin == caret)
    {
        return state_.caret_affinity();
    }
    return origin < caret ? core::CaretAffinity::leading : core::CaretAffinity::trailing;
}

// 本文の最初か最後の画面行で止まった上下の移動（↑ ↓ PgUp PgDn）は、本文の先頭か末尾へ着く
// （施主の要望 2026-11-10・#1386・ADR 0144 の追補）。もう着いていればキャレット・選択・狙う横位置を
// 変えず、画面がキャレットから離れていたときだけ見える所へ送り返す（追従を頼むだけで、見えていれば
// 画面も不変・2026-11-10 のレビューの後の決め）。着いた後の狙う横位置は着いた位置のもの（捨てて、
// 次の上下で location から読む）。通るのは端に当たったときだけ。上下の移動の道に足すのは歩きが持つ
// 印の読み 1 つで、文字組みの問い合わせは足さない（ADR 0128 の追従の遅延解決のまま）。
void EditorController::move_to_text_edge(VisualTextView &view, const MoveCaret &intent,
                                         std::int64_t rows)
{
    const core::Offset edge{rows < 0 ? 0 : state_.text().size_bytes()};
    const auto &selection = state_.selection();
    const bool collapsed = selection.anchor == selection.caret;
    if (selection.caret == edge &&
        (collapsed || intent.anchoring == core::SelectionAnchoring::extend))
    {
        follow_caret();
        return;
    }
    if (page_motion(intent.motion) && !page_top_shifted(view, rows))
    {
        return;
    }
    wanted_x_.reset();
    move_caret_to(edge, intent.anchoring);
}

// PgDn / PgUp は画面の先頭も同じ数の画面行だけ送る（ADR 0144 の決定 7）。端は本文の最初と最後の
// 画面行で止まり、追従（画面を埋める）がキャレットの見える範囲へ収める。失敗は layout_failure_。
bool EditorController::page_top_shifted(VisualTextView &view, std::int64_t rows)
{
    const auto &scroll = state_.scroll();
    const auto top =
        view.shifted(core::VisualPosition{scroll.first_visible, scroll.first_row}, rows);
    if (!top)
    {
        layout_failure_ = top.error();
        return false;
    }
    state_ = state_.with_scroll(
        ScrollState{top.value().line, scroll.visible_lines, top.value().row, scroll.horizontal});
    return true;
}

// 見えている範囲は先頭の (行, 画面行) から見えている行数ぶんの画面行。キャレットが外れていれば、
// 外れた側の端の画面行へ今の横の位置のまま入れる（#785）。位置の解決は上下の移動（move_visual）と
// 同じ location・shifted・caret_at で、格子と折り返しは view が答える。
std::expected<core::TextCaret, TextLayoutFailure> EditorController::caret_in_view()
{
    const ScrollState scroll = settled_scroll();
    auto view = visual_view();
    const core::TextCaret caret{state_.text().position_of(state_.selection().caret),
                                state_.caret_affinity()};
    const auto point = view.location(caret);
    if (!point)
    {
        return std::unexpected(point.error());
    }
    const core::VisualPosition at{caret.position.line, point.value().row};
    const core::VisualPosition first{scroll.first_visible, scroll.first_row};
    const auto rows = std::max<std::size_t>(scroll.visible_lines, 1) - 1;
    const auto last = view.shifted(first, static_cast<std::int64_t>(rows));
    if (!last)
    {
        return std::unexpected(last.error());
    }
    if (!(at < first) && !(last.value() < at))
    {
        return caret;
    }
    return view.caret_at(at < first ? first : last.value(), point.value().x);
}

void EditorController::accept(const PointText &intent)
{
    if (state_.composition().has_value())
    {
        return;
    }
    const auto &scroll = settled_scroll();
    const auto row = std::clamp<std::int64_t>(intent.row, 0,
                                              static_cast<std::int64_t>(scroll.visible_lines) - 1);
    auto view = visual_view();
    const auto at = view.shifted(core::VisualPosition{scroll.first_visible, scroll.first_row}, row);
    if (!at)
    {
        layout_failure_ = at.error();
        return;
    }
    const auto caret = view.caret_at(at.value(), intent.x + scroll.horizontal);
    if (!caret)
    {
        layout_failure_ = caret.error();
        return;
    }
    switch (intent.clicks)
    {
    case PointerClicks::single:
        accept(PlaceCaret{caret.value().position, intent.anchoring, caret.value().affinity});
        return;
    case PointerClicks::double_:
    case PointerClicks::triple:
        break;
    }
    const auto pointed = pointed_character(view, caret.value(), intent.x + scroll.horizontal);
    if (!pointed)
    {
        layout_failure_ = pointed.error();
        return;
    }
    select_clicked(pointed.value(), intent.clicks);
}

// 位置解決はキャレットの境目（近いほう）を返すので、点が文字の右半分にあれば境目は次の文字の前に
// なる。境目の x が点より右なら、手前の Vim の 1 文字（結合文字を含む・ADR 0053）へ戻す。手前の
// 文字は境目の前の数バイトだけを読んで決める（行を写さない）。
std::expected<core::Offset, TextLayoutFailure>
EditorController::pointed_character(VisualTextView &view, core::TextCaret caret, float x)
{
    const auto &text = state_.text();
    const auto offset = text.offset_of(caret.position);
    const auto located = view.location(caret);
    if (!located)
    {
        return std::unexpected(located.error());
    }
    const auto line_start = text.line_start(caret.position.line);
    if (located.value().x <= x || offset == line_start)
    {
        return offset;
    }
    const auto from = core::Offset{
        std::max(line_start.value, offset.value - std::min(offset.value, pointed_window_bytes))};
    const auto before = text.text_range(from, offset);
    // 窓の頭が code point の途中なら、次の code point の頭から読む（UTF-8
    // の続きのバイトを飛ばす）。
    const auto lead = std::ranges::find_if(
        before, [](char byte) { return (static_cast<unsigned char>(byte) & 0xC0U) != 0x80U; });
    const auto skipped = static_cast<std::size_t>(lead - before.begin());
    const std::string_view window = std::string_view{before}.substr(skipped);
    return core::Offset{from.value + skipped +
                        core::vim_character_start(window, core::Offset{window.size()}).value};
}

// 語は Vim の `iw` の 1 本（vim_text_object_range・ADR 0031）で、VISUAL の選択を描く範囲
// （vim_visual_range）と同じ写しで半開区間にする。空行の `iw` は改行を選ぶ（Vim 9.1 の `viw`
// と同じ・ 実測）。行は行頭から次の行の頭まで（最終行は本文の終わり・gvim の `<3-LeftMouse>`
// と同じ）。
void EditorController::select_clicked(core::Offset pointed, PointerClicks clicks)
{
    switch (state_.mode())
    {
    case core::EditMode::vim:
        select_clicked_in_vim(pointed, clicks);
        return;
    case core::EditMode::ordinary:
        break;
    }
    const auto &text = state_.text();
    const auto rest = core::vim_resting_caret(text, pointed);
    core::Selection selection{rest, rest};
    switch (clicks)
    {
    case PointerClicks::single:
        break;
    case PointerClicks::double_:
        selection = clicked_word(text, rest);
        break;
    case PointerClicks::triple:
        selection = clicked_line(text, rest);
        break;
    }
    // 押しが折り返しの段の境目（trailing）に置いた向きを残さない（キャレットは選択の端）。
    state_ = state_.with_selection(selection).with_caret_affinity(core::CaretAffinity::leading);
    follow_caret();
}

// Vim は鍵を流さず、engine の押しの入口（vim_pointer_selected）が VISUAL の種類と選択を直に置く
// （#1250・gvim の do_mouse と同じく `.` とマクロの記録に積まない）。待ち・回数・オペレータは捨て、
// INSERT は回数の反復なしで離れ、前の VISUAL は `gv` に残る。語は `v` `iw` と同じ選択、行は `V`
// と同じく押した文字に両端を置く。
void EditorController::select_clicked_in_vim(core::Offset pointed, PointerClicks clicks)
{
    const auto &text = state_.text();
    const auto rest = core::vim_resting_caret(text, pointed);
    switch (clicks)
    {
    case PointerClicks::single:
        return;
    case PointerClicks::double_:
        static_cast<void>(
            stepped_vim(PointerSelection{clicked_word_visual(text, rest), core::VimMode::visual}));
        break;
    case PointerClicks::triple:
        static_cast<void>(
            stepped_vim(PointerSelection{core::Selection{rest, rest}, core::VimMode::visual_line}));
        break;
    }
    // 押しが折り返しの段の境目（trailing）に置いた向きを残さない（perform(VimSelect) は gv などと
    // 共用なので、押しの道だけで戻す）。
    state_ = state_.with_caret_affinity(core::CaretAffinity::leading);
}
} // namespace nenenib::application
