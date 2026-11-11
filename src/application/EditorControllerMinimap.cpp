// ミニマップの群（ADR 0171・施主決定 D70・FR-030）。オンの間だけ窓の行の塊を覚えて frame に載せる。
// オフの間は、編集と frame の口（note_minimap_edit・add_minimap）が最初の 1 行で返る（決定 1 の
// 追補・ほかの機能のオフと同じ早い return）。
#include "AutosaveId.hpp"
#include "Edit.hpp"
#include "EditorController.hpp"
#include "EditorFrame.hpp"
#include "LayoutRect.hpp"
#include "Lexer.hpp"
#include "LineNumber.hpp"
#include "Minimap.hpp"
#include "MinimapLayout.hpp"
#include "MinimapPointing.hpp"
#include "MinimapSource.hpp"
#include "PointMinimap.hpp"
#include "ScrollState.hpp"
#include "TextBuffer.hpp"

#include <algorithm>
#include <cstddef>
#include <optional>
#include <utility>

namespace nenenib::application
{
namespace
{
// 画面に一部でも出ている最後の論理行（frame の行か格子の画面行の最後）。行が無い frame（問いの
// 間）は値なし。
[[nodiscard]] std::optional<core::LineNumber> last_shown_line(const EditorFrame &frame)
{
    if (!frame.lines.empty())
    {
        return frame.lines.back().number;
    }
    if (!frame.grid.empty())
    {
        return frame.grid.back().number;
    }
    return std::nullopt;
}

// 見えている論理行の数（見えている最初の行から最後の行まで）。折り返す行は画面行を何本も使うので、
// 画面行の数（visible_lines）ではなく frame の行から数える（レビュー L2）。行が無い frame は画面行
// の数のまま。
[[nodiscard]] std::size_t visible_line_count(const EditorFrame &frame, std::size_t screen_rows)
{
    const auto last = last_shown_line(frame);
    if (!last.has_value() || last.value().value < frame.first_visible.value)
    {
        return screen_rows;
    }
    return last.value().value - frame.first_visible.value + 1;
}
} // namespace

bool EditorController::minimap_shown() const noexcept
{
    return minimap_shown_ && state_.full_preview() == nullptr;
}

// 設定が変わった後と起動の 1 か所（決定 1）。消したときは覚えを捨てる。
void EditorController::sync_minimap()
{
    minimap_shown_ = core::minimap_shown(state_.settings());
    if (!minimap_shown_)
    {
        minimap_.forget();
    }
}

// 編集の 1 か所（replace・undo・redo）が本文を置く前に呼ぶ。state_ の本文はまだ編集の前。
void EditorController::note_minimap_edit(const core::TextBuffer &after, const core::Edit &edit)
{
    if (!minimap_shown_)
    {
        return;
    }
    minimap_.edited(state_.text(), after, edit, state_.autosave_ids().at(state_.active_tab()));
}

// 面に入る行（窓の client と DPI と字の大きさ・core::minimap_layout の 1 本）から面の最上段の行と
// 窓を決め、覚えが同じ文書の同じ窓でなければ窓ごと読み直す（決定 4・段 1 はスクロールで窓を出たら
// 窓ごと）。frame に載るのは窓の塊の借りと帯だけ（本文の全部を写さない）。全面プレビューの間は
// 本文が隠れているので載せない（finish_frame は add_other_pane の後にここを呼ぶ・レビュー L6）。
void EditorController::add_minimap(EditorFrame &result)
{
    if (!minimap_shown_ || result.full_preview.has_value())
    {
        return;
    }
    const auto &text = state_.text();
    const core::Language language = state_.language();
    const MinimapSource source{state_.autosave_ids().at(state_.active_tab()), language,
                               state_.syntax_enabled() && core::has_lexer(language)};
    const std::size_t rows =
        core::minimap_layout(core::LayoutRect{0, 0, window_width_, window_height_},
                             view_geometry_.dpi, state_.settings().font_size, true)
            .rows;
    const std::size_t total = text.line_count();
    const std::size_t visible = visible_line_count(result, state_.scroll().visible_lines);
    minimap_visible_ = visible;
    const auto top = core::minimap_top(result.first_visible, visible, total, rows);
    const auto window = core::minimap_window(minimap_.window_of(source), top, rows, total);
    if (!minimap_.holds(source, window))
    {
        minimap_.rebuild(text, source, window);
    }
    result.minimap = minimap_.framed(top, result.first_visible, visible);
}

// 押しとドラッグで使う「見えている論理行の数」を決める（決定 10 の追補・レビュー V-1476 の M-2）。
// 押しは最後の frame の値を覚えるが、最後に面が送った位置（minimap_sent_）が今の位置のままなら
// 覚えた値を使い続ける（折り返す行で見えている数が送った先で変わっても、同じ点の押しが往復しない）。
// ドラッグは押したときの値のまま。押したタブと違うタブのドラッグ（ドラッグの間のタブの切り替え）は
// 偽を返し、何も送らない（レビュー L-6）。
bool EditorController::grab_minimap(MinimapPointing pointing, AutosaveId tab)
{
    const std::size_t framed = minimap_visible_ > 0
                                   ? minimap_visible_
                                   : std::max<std::size_t>(state_.scroll().visible_lines, 1);
    const auto &scroll = settled_scroll();
    const MinimapAt now{scroll.first_visible, scroll.first_row};
    switch (pointing)
    {
    case MinimapPointing::pressed:
    {
        const bool kept =
            minimap_grab_tab_ == tab && minimap_sent_ == now && minimap_grab_visible_ > 0;
        minimap_grab_visible_ = kept ? minimap_grab_visible_ : framed;
        minimap_grab_tab_ = tab;
        return true;
    }
    case MinimapPointing::dragged:
        return minimap_grab_tab_ == tab && minimap_grab_visible_ > 0;
    }
    std::unreachable();
}

// 面の上の押しとドラッグ（決定 5 の追補）。押した段が見えている範囲の面の中央に来るように、本文の
// 窓だけを送る（キャレット・選択・Vim の待ちと録画・`.`・undo は動かさない）。送り方はホイールの
// ScrollLines と同じく ScrollState を置いて clamp_scroll で収める 1 本で、Vim のモードでも
// キャレットを寄せない（ホイールと同じ）。面の段の数は frame と同じ core::minimap_layout の 1 本。
// 純関数が下端（見えている数で決まる最後の行）を返したら、文書の最後の行を頼んで clamp_scroll に
// ホイールと同じ本当の上限へ収めさせる（レビュー M-2 の (b)）。押した行が今の先頭の行と同じなら
// 何もしない（行の奥の画面行を保つ・L-5）。面が出ていなければ何もしない。
void EditorController::accept(const PointMinimap &intent)
{
    if (!minimap_shown())
    {
        return;
    }
    if (!grab_minimap(intent.pointing, state_.autosave_ids().at(state_.active_tab())))
    {
        return;
    }
    const std::size_t rows =
        core::minimap_layout(core::LayoutRect{0, 0, window_width_, window_height_},
                             view_geometry_.dpi, state_.settings().font_size, true)
            .rows;
    const std::size_t total = state_.text().line_count();
    const auto first = core::minimap_pointed_first(intent.row, minimap_grab_visible_, total, rows);
    const bool bottom =
        total > minimap_grab_visible_ && first.value == total - minimap_grab_visible_ + 1;
    const auto &scroll = settled_scroll();
    if (scroll.first_visible != first || bottom)
    {
        state_ = state_.with_scroll(ScrollState{bottom ? core::LineNumber{total} : first,
                                                scroll.visible_lines, 0, scroll.horizontal});
        clamp_scroll();
    }
    const auto &sent = settled_scroll();
    minimap_sent_ = MinimapAt{sent.first_visible, sent.first_row};
}
} // namespace nenenib::application
