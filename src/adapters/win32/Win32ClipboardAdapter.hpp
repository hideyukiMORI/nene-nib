#pragma once

#include "ClipboardCalls.hpp"
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
// 開けなければ数回だけ開き直し、変換できない本文は書かない（ADR 0134）。
class Win32ClipboardAdapter final : public application::ClipboardPort
{
  public:
    Win32ClipboardAdapter() noexcept;
    // 試験だけが替え玉の束を渡す（ADR 0134 の決定 4）。
    explicit Win32ClipboardAdapter(ClipboardCalls calls) noexcept;
    void bind(HWND owner) noexcept;
    [[nodiscard]] std::expected<void, application::ClipboardFailure>
    write(std::string_view utf8) override;
    [[nodiscard]] std::expected<std::string, application::ClipboardFailure> read() override;

  private:
    [[nodiscard]] std::expected<void, application::ClipboardFailure> opened_clipboard();
    [[nodiscard]] std::expected<void, application::ClipboardFailure> store(HGLOBAL handle);
    ClipboardCalls calls_;
    HWND owner_ = nullptr;
};
} // namespace nenenib::adapters::win32
