#pragma once

#include "Appearance.hpp"
#include "DisplayText.hpp"
#include "EditMode.hpp"

namespace nenenib::application
{
// 表示文字列・外観・編集モードの唯一の所有者（ARC-004）。公開値は不変で、次状態を返す（ARC-005）。
class EditorState final
{
  public:
    [[nodiscard]] static EditorState create(core::DisplayText text, core::Appearance appearance,
                                            core::EditMode mode);
    [[nodiscard]] const core::DisplayText &text() const noexcept;
    [[nodiscard]] core::Appearance appearance() const noexcept;
    [[nodiscard]] core::EditMode mode() const noexcept;
    [[nodiscard]] EditorState with_appearance(core::Appearance appearance) const;
    [[nodiscard]] EditorState with_mode(core::EditMode mode) const;

  private:
    EditorState(core::DisplayText text, core::Appearance appearance, core::EditMode mode);
    core::DisplayText text_;
    core::Appearance appearance_;
    core::EditMode mode_;
};
} // namespace nenenib::application
