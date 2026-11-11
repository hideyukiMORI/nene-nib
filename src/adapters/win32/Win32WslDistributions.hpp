#pragma once

#include "WslDistribution.hpp"
#include "WslDistributionPort.hpp"
#include "WslListFailure.hpp"

#include <expected>
#include <string>
#include <vector>

namespace nenenib::adapters::win32
{
// WSL が登録したディストリの鍵（HKCU の下）。製品はここを読み、試験は自分の試験の根を渡す。
inline constexpr wchar_t wsl_lxss_key[] = LR"(Software\Microsoft\Windows\CurrentVersion\Lxss)";

// 登録された WSL のディストリをレジストリから読む唯一の場所（ADR 0162 の追補 #1450・ARC-007）。
// `wsl.exe` を起こさず、VM にも触れない。鍵が無ければ空の一覧（WSL が入っていない）。
class Win32WslDistributions final : public application::WslDistributionPort
{
  public:
    explicit Win32WslDistributions(std::wstring key_path);

    [[nodiscard]] std::expected<std::vector<core::WslDistribution>, application::WslListFailure>
    distributions() const override;

  private:
    std::wstring key_path_;
};
} // namespace nenenib::adapters::win32
