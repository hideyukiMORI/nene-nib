#pragma once

#include "Appearance.hpp"
#include "DisplayText.hpp"

namespace nenenib::application
{
// 表示文字列と外観の唯一の所有者（ARC-004）。公開値は不変で、次状態を返す（ARC-005）。
class EditorState final
{
  public:
    [[nodiscard]] static EditorState create(core::DisplayText text, core::Appearance appearance);
    [[nodiscard]] const core::DisplayText &text() const noexcept;
    [[nodiscard]] core::Appearance appearance() const noexcept;
    [[nodiscard]] EditorState with_appearance(core::Appearance appearance) const;

  private:
    EditorState(core::DisplayText text, core::Appearance appearance);
    core::DisplayText text_;
    core::Appearance appearance_;
};
} // namespace nenenib::application
