#pragma once

#include "FontFamilyFailure.hpp"
#include "FontFamilyPort.hpp"

#include <expected>
#include <string>
#include <vector>

namespace nenenib::adapters::win32
{
// 字体の family 名を OS の照合順（使う人の既定ロケールの CompareStringEx）に並べる（#696・ADR 0022
// 決定 5 と ADR 0140 決定 6 の追補）。既定ロケールを読むので adapters
// に置く（ARC-007）。一覧そのものは 借りた port（製品は ui/win32 の
// SystemFontFamilies）が読み、失敗はそのまま返す。core は並びを変えない。
class Win32FontCollationAdapter final : public application::FontFamilyPort
{
  public:
    explicit Win32FontCollationAdapter(application::FontFamilyPort &families) noexcept;
    [[nodiscard]] std::expected<std::vector<std::string>, application::FontFamilyFailure>
    list() override;

  private:
    application::FontFamilyPort &families_;
};

// OS の照合順に並べた名前。照合が同じ（または比べられない）名前どうしはバイト順で決める。
[[nodiscard]] std::vector<std::string> collated_font_names(std::vector<std::string> names);
} // namespace nenenib::adapters::win32
