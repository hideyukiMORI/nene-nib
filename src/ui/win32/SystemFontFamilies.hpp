#pragma once

#include "FontFamilyPort.hpp"

namespace nenenib::ui::win32
{
// OS の字体の family 名（#597・D58）。描画と同じ共有の DirectWrite factory の system collection を
// 読む（ADR 0114 の TextLayoutPort と同じく ui/win32 が application の port を実装する）。名前は
// 各 family の全ロケールの名前（`BIZ UDゴシック` と `BIZ UDGothic` の両方・既定ロケールは読まない・
// ARC-007）。並びは合成ルートが adapters の Win32FontCollationAdapter で包んで OS の照合順にし
// （#696）、重複は core::FontFamilies が除く。
class SystemFontFamilies final : public application::FontFamilyPort
{
  public:
    [[nodiscard]] std::expected<std::vector<std::string>, application::FontFamilyFailure>
    list() override;
};
} // namespace nenenib::ui::win32
