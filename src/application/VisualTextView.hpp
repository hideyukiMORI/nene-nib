#pragma once

#include "EditorState.hpp"
#include "GridCells.hpp"
#include "GridPoint.hpp"
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
// text の表示の巨大な行（ADR 0139）は錨の周りの窓だけを表示値に持ち、rows は 1。x は
// 「キャレットの行の窓の始まり」（origin・キャレットが巨大な行に無ければ行頭）からの相対で、
// 横の送り（ScrollState::horizontal）も同じ相対で読む（決定 5）。各行の x は行の原点からの x に
// 行のずれ（offset_of）を足した値で、location / caret_at / followed はこの x で答える。
// 格子の文書（ADR 0140 の決定 4・5）では、構築のときに 1 升を port の cell から 1 回だけ読み、
// rows / location / caret_at を本文が持つ仮想桁の索引（TextBuffer::virtual_columns）から GridCells
// で答える。索引は借りるだけで作らない（持たせるのは開く道と SelectDisplay の application の 2
// か所）。 文字組みの rows / locate / hit は呼ばない。
class VisualTextView final
{
  public:
    VisualTextView(const EditorState &state, TextLayoutPort &layouts,
                   core::TextViewGeometry geometry);
    // 見えている 1 行の表示値（変換中の文字列はキャレットの行に差し込む）。巨大な行の錨は
    // キャレットがその行にあればキャレット、無ければ横の送りから見積もった位置（決定 3）。
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
    // 格子の升の答え手（ADR 0140 の決定 4・5）。本文が索引を持たないか 1 升を読めなければ失敗。
    // frame の画面行（GridLineRows）は位置解決と同じ GridCells を 1 frame に 1 つ借りて切り出す。
    [[nodiscard]] std::expected<GridCells, TextLayoutFailure> grid_cells() const;
    // キャレットが立つ格子の画面行と升（cells は grid_cells の値）。行頭に立つ trailing
    // のキャレット は前の画面行の右端。
    [[nodiscard]] core::GridPoint grid_point(GridCells &cells, core::TextCaret caret) const;

  private:
    [[nodiscard]] TextLayoutRequest request(const LineView &line) const;
    // anchor を省けば line の既定の錨。巨大な行は錨の周りの窓だけを text_range で読む。
    [[nodiscard]] LineView line_at(core::LineNumber number,
                                   std::optional<core::Offset> anchor) const;
    [[nodiscard]] LineView giant_line(core::LineNumber number,
                                      const core::VirtualColumnIndex &index,
                                      std::optional<core::Offset> anchor) const;
    void compose(LineView &result) const;
    // 行の原点の x（行の中の x にこれを足すと位置解決の x）。描画のずれは left = これ − 横の送り。
    [[nodiscard]] float offset_of(const LineView &line) const noexcept;
    // 巨大な行で窓の外を指す x は、見積もった仮想桁から索引で解く（決定 4・Home / End）。
    [[nodiscard]] std::optional<core::TextCaret> outside_window(const LineView &line,
                                                                float local) const;
    // 巨大な行のキャレットを追う横の送り。前の送りが隣の窓からの相対なら、そう読み替えてから
    // 見えるかを確かめ、見えなければ今日どおり端へ寄せる（決定 5）。
    [[nodiscard]] float giant_horizontal(core::TextCaret caret, float x, float width) const;
    // キャレットを追う横の送り（located はキャレットの x）。
    [[nodiscard]] float horizontal_for(core::TextCaret caret, float located) const;
    [[nodiscard]] bool gridded() const noexcept;
    [[nodiscard]] std::expected<core::GridPoint, TextLayoutFailure>
    grid_point(core::TextCaret caret) const;
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
    // キャレットの行の窓の始まりの仮想桁（キャレットが巨大な行に無ければ 0）と見積もりの字幅。
    std::size_t origin_ = 0;
    float advance_ = 1;
    // 格子の 1 画面行の升の数と 1 升の幅（物理画素）。格子でなければ 0。cell が読めなければ
    // grid_failure_。
    std::size_t grid_columns_ = 0;
    float cell_width_ = 0;
    std::optional<TextLayoutFailure> grid_failure_;
};
} // namespace nenenib::application
