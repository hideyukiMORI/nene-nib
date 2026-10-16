#pragma once

#include "EditorState.hpp"
#include "LineView.hpp"
#include "ScrollExtent.hpp"
#include "ScrollFollow.hpp"
#include "ScrollState.hpp"
#include "TextCaret.hpp"
#include "TextLayoutPort.hpp"
#include "VirtualColumnIndex.hpp"
#include "VisualPosition.hpp"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>

namespace nenenib::application
{
// 1回の位置解決の間だけ現在の文書を借用する。スクロール/選択の状態は所有しない。
// 格子の文書（ADR 0140 の決定 4・5）では、構築のときに 1 升を port の cell から 1 回だけ読んで
// 升の累積（VirtualColumnIndex::for_grid）を作り、rows / location / caret_at を core の格子の
// 純関数で答える。文字組みの rows / locate / hit は呼ばない。
class VisualTextView final
{
  public:
    VisualTextView(const EditorState &state, TextLayoutPort &layouts,
                   core::TextViewGeometry geometry);
    // 見えている 1 行の表示値（変換中の文字列はキャレットの行に差し込む）。位置解決をしない
    // frame の行はこちらを直接呼ぶ（格子の累積を作らない）。
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
    // 格子の升の累積と 1 升の幅（物理画素）。格子でなければ空。cell が読めなければ grid_failure_。
    std::optional<core::VirtualColumnIndex> grid_;
    float cell_width_ = 0;
    std::optional<TextLayoutFailure> grid_failure_;
};
} // namespace nenenib::application
