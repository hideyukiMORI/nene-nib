#pragma once

#include "EditorState.hpp"
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

namespace nenenib::application
{
// 1回の位置解決の間だけ現在の文書を借用する。スクロール/選択の状態は所有しない。
class VisualTextView final
{
  public:
    VisualTextView(const EditorState &state, TextLayoutPort &layouts,
                   core::TextViewGeometry geometry);
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
};
} // namespace nenenib::application
