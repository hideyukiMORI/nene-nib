#pragma once

#include "AutosaveId.hpp"
#include "Edit.hpp"
#include "Language.hpp"
#include "LexState.hpp"
#include "LineNumber.hpp"
#include "MinimapBand.hpp"
#include "MinimapFrame.hpp"
#include "MinimapRows.hpp"
#include "MinimapSource.hpp"
#include "MinimapWindow.hpp"
#include "TextBuffer.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

namespace nenenib::application
{
// ミニマップの窓の行の塊の覚え（ADR 0171 の決定 3・4）。controller が 1 つ持ち、アクティブな文書の
// 窓（見えている所の前後 core::minimap_reach 行）の行ごとに塊の列と行頭の字句の状態を持つ。
// 編集は replace の 1 か所（undo / redo も同じ道）から edited で受け、変わった行と、字句の状態が
// 前と揃うまでの下の行だけを読み直して、描き直す帯を積む。窓より上の編集・窓の終わりをまたぐ削除は
// 覚えを捨てる（次の frame で窓ごと作り直す）。オフのときは controller がこれを呼ばない（決定 1）。
class MinimapCache final
{
  public:
    // 文書 source の窓 window がこの覚えのままか。
    [[nodiscard]] bool holds(const MinimapSource &source,
                             const core::MinimapWindow &window) const noexcept;
    // 同じ文書の覚えの窓（無い・捨てた・別の文書なら値なし）。
    [[nodiscard]] std::optional<core::MinimapWindow>
    window_of(const MinimapSource &source) const noexcept;
    // 窓 window の行を text から読み直す（窓の手前は core::syntax_backfill_lines の遡り）。
    void rebuild(const core::TextBuffer &text, const MinimapSource &source,
                 const core::MinimapWindow &window);
    // 文書 tab の本文 before に edit を当てて after になった。覚えの文書でなければ何もしない。
    void edited(const core::TextBuffer &before, const core::TextBuffer &after,
                const core::Edit &edit, AutosaveId tab);
    // 覚えを捨てる（本文が丸ごと替わった・構文の切り替え）。
    void forget() noexcept;
    // frame に載せる値。前の frame からの帯を渡し、次の frame の起点を今の serial にする。
    [[nodiscard]] MinimapFrame framed(core::LineNumber top, core::LineNumber first_visible,
                                      std::size_t visible);
    // 窓を作り直すのに・編集で読み直すのに字句器へ通した行の数（試験が数える口）。
    [[nodiscard]] std::size_t lexed() const noexcept;

  private:
    // 行 line（1 始まり）の塊。state は行頭の状態で、次の行へ渡す状態に書き換える。
    [[nodiscard]] core::MinimapRow row_at(const core::TextBuffer &text, core::LineNumber line,
                                          core::LexState &state);
    void relex_band(const core::TextBuffer &after, std::size_t begin, std::size_t removed,
                    std::size_t inserted);

    std::shared_ptr<MinimapRows> rows_;
    // 窓の行ごとの行頭の字句の状態（rows_ の行の数 + 1・最後は窓の最後の行の終わりの状態）。
    std::vector<core::LexState> states_;
    MinimapSource source_{AutosaveId{0}, core::Language::plain, false};
    std::uint64_t serial_ = 0;
    std::uint64_t base_ = 0;
    std::optional<core::MinimapBand> dirty_;
    std::size_t lexed_ = 0;
};
} // namespace nenenib::application
