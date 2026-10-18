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
#include "VimRowRequest.hpp"
#include "VimRowSpan.hpp"
#include "VisualPosition.hpp"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <variant>
#include <vector>

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
// 借用の間は文書が変わらないので、行の表示値（本文の写しと display_line）は行ごとに 1 回だけ
// 作って使い回す（#529）。保持は見えている行数 ＋ 数行までで、超えたら古いものから捨てる。
// 保持を書き換えるので位置解決の関数は const にしない（CPP-003 は mutable を書かない）。
class VisualTextView final
{
  public:
    VisualTextView(const EditorState &state, TextLayoutPort &layouts,
                   core::TextViewGeometry geometry);
    // 見えている 1 行の表示値（変換中の文字列はキャレットの行に差し込む）。巨大な行の錨は
    // キャレットがその行にあればキャレット、無ければ横の送りから見積もった位置（決定 3）。
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
    // engine の問い（ADR 0144 の決定 2）の答え手。位置の問いはその位置の画面行、番号の問いは
    // その画面行（行の画面行数へ丸める）の本文の範囲と行の画面行数。格子は GridCells、text の形は
    // 文字組みの row（段の番号と範囲を同じ 1 つの layout から・#664）、巨大な行の窓は 1 画面行で、
    // 行の全体を写さない。位置の問いの答えは必ずその位置を含む（最後の段は行末も）。含まなければ
    // 隣の段で引き直し、それでも含まなければ失敗。
    [[nodiscard]] std::expected<core::VimRowSpan, TextLayoutFailure>
    row_span(const core::VimRowRequest &request);
    // 横送りの答え手（ADR 0145 の決定 2・8・9）。horizontal_columns は本文の欄に入る升の数（平均の
    // 字幅で割る）で、キャレットの行を横に送らないとき（wrap on の普通の行・格子・幅 0）は 0。
    // horizontal_of_column と column_of_horizontal は升の leftcol と横の送り（画素）の
    // 1 対で、互いに逆。巨大な行は origin からの見積もりの x、折り返さない普通の行は
    // 同じ文字組みの locate / hit。
    [[nodiscard]] std::size_t horizontal_columns() const;
    // 1 画面行の升の数（ADR 0144 の決定 10）。格子は GridCells の columns、text の形は
    // horizontal_columns と同じ式（横に送るかの条件を除く）。折り返さないとき・幅 0・キャレットが
    // text の形の巨大な行の窓（1 画面行・ADR 0144 の決定 8）にあるときは 0。
    [[nodiscard]] std::size_t row_columns() const;
    [[nodiscard]] std::expected<float, TextLayoutFailure>
    horizontal_of_column(core::LineNumber number, std::size_t column);
    [[nodiscard]] std::expected<std::size_t, TextLayoutFailure>
    column_of_horizontal(core::LineNumber number, float horizontal);
    // 今の横の送りからキャレットが見える最小の送り（縦は動かさない・ADR 0145 の決定 8）。
    [[nodiscard]] std::expected<float, TextLayoutFailure>
    followed_horizontal(core::TextCaret caret);
    // 格子の升の答え手（ADR 0140 の決定 4・5）。本文が索引を持たないか 1 升を読めなければ失敗。
    // frame の画面行（GridLineRows）は位置解決と同じ GridCells を 1 frame に 1 つ借りて切り出す。
    [[nodiscard]] std::expected<GridCells, TextLayoutFailure> grid_cells() const;
    // キャレットが立つ格子の画面行と升（cells は grid_cells の値）。画面行の頭に立つ
    // trailing のキャレットは前の画面行の右端。
    [[nodiscard]] core::GridPoint grid_point(GridCells &cells, core::TextCaret caret) const;

  private:
    using SharedLine = std::shared_ptr<const LineView>;
    [[nodiscard]] SharedLine shown(core::LineNumber number);
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
    // 行 number を横に送るか（折り返さない設定か巨大な行で、格子でも幅 0 でもない）。
    [[nodiscard]] bool scrolls_horizontally(core::LineNumber number) const;
    // 本文の欄の幅 ÷ 平均の字幅（1 以上・ADR 0145 の決定 2）。
    [[nodiscard]] std::size_t window_columns() const;
    [[nodiscard]] bool gridded() const noexcept;
    [[nodiscard]] std::expected<core::GridPoint, TextLayoutFailure>
    grid_point(core::TextCaret caret) const;
    [[nodiscard]] std::expected<std::size_t, TextLayoutFailure>
    grid_rows(core::LineNumber number) const;
    [[nodiscard]] std::expected<core::TextLocation, TextLayoutFailure>
    grid_location(core::TextCaret caret) const;
    [[nodiscard]] std::expected<core::TextCaret, TextLayoutFailure>
    grid_caret_at(core::VisualPosition at, float x) const;
    // 表示の桁 column（変換中の文字列を差し込んだ桁）が指す本文の位置。
    [[nodiscard]] core::TextPosition source_position(const LineView &line,
                                                     core::Column column) const;
    // 行 number の問い target の画面行の [start, end) と画面行数（仮想桁はまだ埋めない）。
    [[nodiscard]] std::expected<core::VimRowSpan, TextLayoutFailure>
    row_bounds(core::LineNumber number, const std::variant<core::Offset, std::size_t> &target);
    [[nodiscard]] std::expected<core::VimRowSpan, TextLayoutFailure>
    grid_row_bounds(core::LineNumber number,
                    const std::variant<core::Offset, std::size_t> &target) const;
    [[nodiscard]] std::expected<core::VimRowSpan, TextLayoutFailure>
    text_row_bounds(core::LineNumber number, const std::variant<core::Offset, std::size_t> &target);
    // 番兵（#664）。found が at を含まなければ隣の段（前・後）で引き直す。
    [[nodiscard]] std::expected<core::VimRowSpan, TextLayoutFailure>
    containing(core::LineNumber number, const core::VimRowSpan &found, core::Offset at);
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
    // キャレットの行の窓の始まりの仮想桁（キャレットが巨大な行に無ければ 0）と見積もりの字幅。
    std::size_t origin_ = 0;
    float advance_ = 1;
    // 格子の 1 画面行の升の数と 1 升の幅（物理画素）。格子でなければ 0。cell が読めなければ
    // grid_failure_。
    std::size_t grid_columns_ = 0;
    float cell_width_ = 0;
    std::optional<TextLayoutFailure> grid_failure_;
    // 作った行の表示値。古い順に並び、上限を超えたら先頭を捨てる。
    std::vector<SharedLine> shown_;
};
} // namespace nenenib::application
