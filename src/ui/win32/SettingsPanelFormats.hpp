#pragma once

#include "DisplayLanguage.hpp"
#include "SettingsPanelText.hpp"

#include <array>
#include <cstdint>
#include <dwrite.h>
#include <utility>
#include <wrl/client.h>

namespace nenenib::ui::win32
{
// 設定の面の書式の持ち主（施主決定 D102・#1836）。面を最初に描くときに作り、DPI・表示言語・字体を
// 作り直すときに捨てる（開いていない間は書式を持たない＝開いていないときの費用を増やさない）。
// 字体は faces（UI の字体と等幅の字体の組）、大きさは採用案の見本の値に倍率 scale（DPI /
// 96）を掛ける。 tag は書式の言語の名前（UI の書式と同じ値・ADR 0123）。作れなければ made
// が偽で、painter は 描かない。
class SettingsPanelFormats final
{
  public:
    // 作ってあれば何もしない。作れなければ偽（途中まで作った書式は捨てる）。
    [[nodiscard]] bool made(IDWriteFactory &dwrite,
                            std::pair<const wchar_t *, const wchar_t *> faces, float scale,
                            const wchar_t *tag);
    void reset() noexcept;
    // painter に渡す借用の組。made が真を返した後だけ呼ぶ。
    [[nodiscard]] SettingsPanelText text(IDWriteFactory *dwrite, std::uint32_t dpi,
                                         core::DisplayLanguage language) const;

  private:
    // title・strong・heading・note・label・value・mono の順。
    std::array<Microsoft::WRL::ComPtr<IDWriteTextFormat>, 7> formats_;
    bool ready_ = false;
};
} // namespace nenenib::ui::win32
