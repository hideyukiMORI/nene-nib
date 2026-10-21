#pragma once
#include "BodyTextLayout.hpp"

#include <dwrite.h>

namespace nenenib::tests::ui
{
// 字形を保持できない行の替え玉（#816・ADR 0156 の決定 12）。BodyTextPainter に渡すと、どの行も
// 保持が無い行として DrawTextLayout と HitTestTextRange の切り抜きの道を通る。
class RefusingGlyphLayouts final
{
  public:
    [[nodiscard]] static const nenenib::ui::win32::BodyTextLayout *
    glyphs_of(IDWriteTextLayout * /*layout*/, float /*left*/, float /*top*/,
              float /*bottom*/) noexcept
    {
        return nullptr;
    }
};
} // namespace nenenib::tests::ui
