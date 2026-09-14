#pragma once

#include "AppearancePort.hpp"
#include "DisplayText.hpp"
#include "EditorFrame.hpp"
#include "EditorIntent.hpp"
#include "EditorState.hpp"

namespace nenenib::application
{
// 状態遷移はここだけ（ARC-004 / ARC-011）。UI は apply の返す EditorFrame を写す。
class EditorController final
{
  public:
    explicit EditorController(const AppearancePort &appearance, core::DisplayText text);
    [[nodiscard]] EditorFrame apply(EditorIntent intent);
    [[nodiscard]] EditorFrame frame() const;

  private:
    const AppearancePort &appearance_;
    EditorState state_;
};
} // namespace nenenib::application
