// ミニマップの群（ADR 0171・施主決定 D70・FR-030）。設定で編集と frame の口を差し替え、オンの間だけ
// 窓の行の塊を覚えて frame に載せる。オフの間はどの関数もここを通らない（何もしない関数を除く）。
#include "AutosaveId.hpp"
#include "Edit.hpp"
#include "EditorController.hpp"
#include "EditorFrame.hpp"
#include "LayoutRect.hpp"
#include "Lexer.hpp"
#include "Minimap.hpp"
#include "MinimapLayout.hpp"
#include "MinimapSource.hpp"
#include "TextBuffer.hpp"

#include <cstddef>

namespace nenenib::application
{
bool EditorController::minimap_shown() const noexcept
{
    return minimap_shown_;
}

// 設定が変わった後と起動の 1 か所（決定 1）。口の差し替えはここだけで、消したときは覚えを捨てる。
void EditorController::sync_minimap()
{
    minimap_shown_ = core::minimap_shown(state_.settings());
    minimap_edited_ = minimap_shown_ ? &EditorController::note_minimap_edit
                                     : &EditorController::skip_minimap_edit;
    minimap_framed_ =
        minimap_shown_ ? &EditorController::add_minimap : &EditorController::skip_minimap_frame;
    if (!minimap_shown_)
    {
        minimap_.forget();
    }
}

void EditorController::skip_minimap_edit(const core::TextBuffer &, const core::Edit &) noexcept {}

// 編集の 1 か所（replace・undo・redo）が本文を置く前に呼ぶ。state_ の本文はまだ編集の前。
void EditorController::note_minimap_edit(const core::TextBuffer &after, const core::Edit &edit)
{
    minimap_.edited(state_.text(), after, edit, state_.autosave_ids().at(state_.active_tab()));
}

void EditorController::skip_minimap_frame(EditorFrame &) noexcept {}

// 面に入る行（窓の client と DPI と字の大きさ・core::minimap_layout の 1 本）から面の最上段の行と
// 窓を決め、覚えが同じ文書の同じ窓でなければ窓ごと読み直す（決定 4・段 1 はスクロールで窓を出たら
// 窓ごと）。frame に載るのは窓の塊の借りと帯だけ（本文の全部を写さない）。
void EditorController::add_minimap(EditorFrame &result)
{
    const auto &text = state_.text();
    const core::Language language = state_.language();
    const MinimapSource source{state_.autosave_ids().at(state_.active_tab()), language,
                               state_.syntax_enabled() && core::has_lexer(language)};
    const std::size_t rows =
        core::minimap_layout(core::LayoutRect{0, 0, window_width_, window_height_},
                             view_geometry_.dpi, state_.settings().font_size, true)
            .rows;
    const std::size_t total = text.line_count();
    const std::size_t visible = state_.scroll().visible_lines;
    const auto top = core::minimap_top(result.first_visible, visible, total, rows);
    const auto window = core::minimap_window(minimap_.window_of(source), top, rows, total);
    if (!minimap_.holds(source, window))
    {
        minimap_.rebuild(text, source, window);
    }
    result.minimap = minimap_.framed(top, result.first_visible, visible);
}
} // namespace nenenib::application
