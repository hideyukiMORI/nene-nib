#pragma once

#include <array>
#include <dwrite.h>
#include <utility>
#include <wrl/client.h>

namespace nenenib::ui::win32
{
// リンクの札の書式の持ち主（施主決定 D106・#1869 の段 L2・SettingsPanelFormats と同じ形）。札を
// 最初に描くときに作り、DPI・表示言語・字体を作り直すときに捨てる（札を出さない間は書式を持たない）。
// 字体は faces（UI の字体と等幅の字体の組）、大きさは採用案の見本の値（題 13・宛先 13 の等幅・
// 開き方 12.5・案内 11.5・ボタン 13）に倍率 scale（DPI / 96）を掛ける。tag は書式の言語の名前。
class LinkCardFormats final
{
  public:
    // 作ってあれば何もしない。作れなければ偽（途中まで作った書式は捨てる）。
    [[nodiscard]] bool made(IDWriteFactory &dwrite,
                            std::pair<const wchar_t *, const wchar_t *> faces, float scale,
                            const wchar_t *tag);
    void reset() noexcept;
    // made が真を返した後だけ呼ぶ。
    [[nodiscard]] IDWriteTextFormat *title() const noexcept;
    [[nodiscard]] IDWriteTextFormat *target() const noexcept;
    [[nodiscard]] IDWriteTextFormat *way() const noexcept;
    [[nodiscard]] IDWriteTextFormat *hint() const noexcept;
    [[nodiscard]] IDWriteTextFormat *button() const noexcept;

  private:
    // title・target・way・hint・button の順。
    std::array<Microsoft::WRL::ComPtr<IDWriteTextFormat>, 5> formats_;
    bool ready_ = false;
};
} // namespace nenenib::ui::win32
