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
#include <memory>
#include <vector>

namespace nenenib::application
{
// 1回の位置解決の間だけ現在の文書を借用する。スクロール/選択の状態は所有しない。
// 借用の間は文書が変わらないので、行の表示値（本文の写しと display_line）は行ごとに 1 回だけ
// 作って使い回す（#529）。保持は見えている行数 ＋ 数行までで、超えたら古いものから捨てる。
// 保持を書き換えるので位置解決の関数は const にしない（CPP-003 は mutable を書かない）。
class VisualTextView final
{
  public:
    VisualTextView(const EditorState &state, TextLayoutPort &layouts,
                   core::TextViewGeometry geometry);
    // 呼び手が選択と当たりを書き足す写し。保持を通さずに作る（巨大な行を複製しない）。
    [[nodiscard]] LineView line(core::LineNumber number) const;
    [[nodiscard]] std::expected<std::size_t, TextLayoutFailure> rows(core::LineNumber number);
    [[nodiscard]] std::expected<core::TextLocation, TextLayoutFailure>
    location(core::TextCaret caret);
    [[nodiscard]] std::expected<core::VisualPosition, TextLayoutFailure>
    position(core::TextCaret caret);
    [[nodiscard]] std::expected<core::VisualPosition, TextLayoutFailure>
    shifted(core::VisualPosition from, std::int64_t amount);
    [[nodiscard]] std::expected<core::TextCaret, TextLayoutFailure>
    caret_at(core::VisualPosition at, float x);
    [[nodiscard]] std::expected<core::VisualPosition, TextLayoutFailure>
    constrained(core::VisualPosition first, core::ScrollExtent extent);
    [[nodiscard]] std::expected<ScrollState, TextLayoutFailure>
    followed(core::TextCaret caret, core::ScrollFollow follow, core::ScrollExtent extent);

  private:
    using SharedLine = std::shared_ptr<const LineView>;
    [[nodiscard]] SharedLine shown(core::LineNumber number);
    [[nodiscard]] TextLayoutRequest request(const LineView &line) const;
    [[nodiscard]] std::expected<core::VisualPosition, TextLayoutFailure>
    forward(core::VisualPosition from, std::size_t amount);
    [[nodiscard]] std::expected<core::VisualPosition, TextLayoutFailure>
    backward(core::VisualPosition from, std::size_t amount);
    [[nodiscard]] std::expected<core::VisualPosition, TextLayoutFailure> last();
    [[nodiscard]] std::expected<std::size_t, TextLayoutFailure>
    distance(core::VisualPosition first, core::VisualPosition last, std::size_t limit);
    [[nodiscard]] std::expected<core::VisualPosition, TextLayoutFailure>
    followed_first(core::VisualPosition caret, core::ScrollFollow follow);

    const EditorState &state_;
    TextLayoutPort &layouts_;
    core::TextViewGeometry geometry_;
    core::TextPosition caret_;
    // 作った行の表示値。古い順に並び、上限を超えたら先頭を捨てる。
    std::vector<SharedLine> shown_;
};
} // namespace nenenib::application
