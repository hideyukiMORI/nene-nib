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
#include "MinimapSource.hpp"
#include "TextBuffer.hpp"

#include <cstddef>
#include <optional>

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
    const auto top = core::minimap_top(result.first_visible, visible, total, rows);
    const auto window = core::minimap_window(minimap_.window_of(source), top, rows, total);
    if (!minimap_.holds(source, window))
    {
        minimap_.rebuild(text, source, window);
    }
    result.minimap = minimap_.framed(top, result.first_visible, visible);
}
} // namespace nenenib::application
