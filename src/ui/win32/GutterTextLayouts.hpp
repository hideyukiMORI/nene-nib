#pragma once

#include "GutterTextLayout.hpp"

#include <optional>
#include <string_view>
#include <vector>

namespace nenenib::ui::win32
{
// 現在と直前の描画だけの行番号資源。生成は renderer の text_layout が受け持つ。
class GutterTextLayouts final
{
  public:
    void begin();
    void end();
    void clear();
    [[nodiscard]] std::optional<Microsoft::WRL::ComPtr<IDWriteTextLayout>>
    lookup(std::string_view text, const core::LayoutRect &area);
    void retain(std::string_view text, const core::LayoutRect &area,
                Microsoft::WRL::ComPtr<IDWriteTextLayout> layout);

  private:
    std::vector<GutterTextLayout> current_;
    std::vector<GutterTextLayout> previous_;
};
} // namespace nenenib::ui::win32
