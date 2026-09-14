#pragma once

#include "ClipboardFailure.hpp"
#include "ClipboardPort.hpp"

#include <windows.h>

#include <expected>
#include <string>
#include <string_view>

namespace nenenib::adapters::win32
{
// OS のクリップボードを触る唯一の場所（ARC-003 / ARC-007 / ADR 0009 の決定 5）。
// UTF-8 ↔ UTF-16 の変換もここで閉じる（CPP-014）。OpenClipboard に要る HWND は bind で受け取る。
class Win32ClipboardAdapter final : public application::ClipboardPort
{
  public:
    void bind(HWND owner) noexcept;
    [[nodiscard]] std::expected<void, application::ClipboardFailure>
    write(std::string_view utf8) override;
    [[nodiscard]] std::expected<std::string, application::ClipboardFailure> read() override;

  private:
    [[nodiscard]] std::expected<void, application::ClipboardFailure> store(HGLOBAL handle);
    HWND owner_ = nullptr;
};
} // namespace nenenib::adapters::win32
