#pragma once

#include "EditorState.hpp"
#include "GridCells.hpp"
#include "LineView.hpp"
#include "ScrollExtent.hpp"
#include "ScrollFollow.hpp"
#include "ScrollState.hpp"
#include "TextCaret.hpp"
#include "TextLayoutPort.hpp"
#include "VisualPosition.hpp"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>

namespace nenenib::application
{
// 1回の位置解決の間だけ現在の文書を借用する。スクロール/選択の状態は所有しない。
// 格子の文書（ADR 0140 の決定 4・5）では、構築のときに 1 升を port の cell から 1 回だけ読み、
// rows / location / caret_at を本文が持つ仮想桁の索引（TextBuffer::virtual_columns）から GridCells
// で答える。索引は借りるだけで作らない（持たせるのは開く道と SelectDisplay の application の 2
// か所）。 文字組みの rows / locate / hit は呼ばない。
class VisualTextView final
{
  public:
    VisualTextView(const EditorState &state, TextLayoutPort &layouts,
                   core::TextViewGeometry geometry);
    // 見えている 1 行の表示値（変換中の文字列はキャレットの行に差し込む）。位置解決をしない
    // frame の行はこちらを直接呼ぶ（1 升を port から読まない）。
    [[nodiscard]] static LineView line_of(const EditorState &state, core::LineNumber number);
    [[nodiscard]] LineView line(core::LineNumber number) const;
    [[nodiscard]] std::expected<std::size_t, TextLayoutFailure> rows(core::LineNumber number) const;
    [[nodiscard]] std::expected<core::TextLocation, TextLayoutFailure>
    location(core::TextCaret caret) const;
    [[nodiscard]] std::expected<core::VisualPosition, TextLayoutFailure>
    position(core::TextCaret caret) const;
    [[nodiscard]] std::expected<core::VisualPosition, TextLayoutFailure>
    shifted(core::VisualPosition from, std::int64_t amount) const;
    [[nodiscard]] std::expected<core::TextCaret, TextLayoutFailure>
    caret_at(core::VisualPosition at, float x) const;
    [[nodiscard]] std::expected<core::VisualPosition, TextLayoutFailure>
    constrained(core::VisualPosition first, core::ScrollExtent extent) const;
    [[nodiscard]] std::expected<ScrollState, TextLayoutFailure>
    followed(core::TextCaret caret, core::ScrollFollow follow, core::ScrollExtent extent) const;

  private:
    [[nodiscard]] TextLayoutRequest request(const LineView &line) const;
    [[nodiscard]] bool gridded() const noexcept;
    // 格子の升の答え手。本文が索引を持たないか 1 升を読めなければ失敗。
    [[nodiscard]] std::expected<GridCells, TextLayoutFailure> grid() const;
    [[nodiscard]] std::expected<std::size_t, TextLayoutFailure>
    grid_rows(core::LineNumber number) const;
    [[nodiscard]] std::expected<core::TextLocation, TextLayoutFailure>
    grid_location(core::TextCaret caret) const;
    [[nodiscard]] std::expected<core::TextCaret, TextLayoutFailure>
    grid_caret_at(core::VisualPosition at, float x) const;
    [[nodiscard]] std::expected<core::VisualPosition, TextLayoutFailure>
    forward(core::VisualPosition from, std::size_t amount) const;
    [[nodiscard]] std::expected<core::VisualPosition, TextLayoutFailure>
    backward(core::VisualPosition from, std::size_t amount) const;
    [[nodiscard]] std::expected<core::VisualPosition, TextLayoutFailure> last() const;
    [[nodiscard]] std::expected<std::size_t, TextLayoutFailure>
    distance(core::VisualPosition first, core::VisualPosition last, std::size_t limit) const;
    [[nodiscard]] std::expected<core::VisualPosition, TextLayoutFailure>
    followed_first(core::VisualPosition caret, core::ScrollFollow follow) const;

    const EditorState &state_;
    TextLayoutPort &layouts_;
    core::TextViewGeometry geometry_;
    core::TextPosition caret_;
    // 格子の 1 画面行の升の数と 1 升の幅（物理画素）。格子でなければ 0。cell が読めなければ
    // grid_failure_。
    std::size_t grid_columns_ = 0;
    float cell_width_ = 0;
    std::optional<TextLayoutFailure> grid_failure_;
};
} // namespace nenenib::application
