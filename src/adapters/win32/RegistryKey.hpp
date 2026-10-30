#pragma once

#include <windows.h>

namespace nenenib::adapters::win32
{
// エクスプローラーへの登録（ADR 0178）で開いたレジストリの鍵の唯一の所有者（CPP-016）。
// nullptr は「持っていない」として閉じない。
class RegistryKey final
{
  public:
    RegistryKey() noexcept = default;

    ~RegistryKey()
    {
        if (key_ != nullptr)
        {
            RegCloseKey(key_);
        }
    }

    RegistryKey(const RegistryKey &) = delete;
    RegistryKey(RegistryKey &&) = delete;
    RegistryKey &operator=(const RegistryKey &) = delete;
    RegistryKey &operator=(RegistryKey &&) = delete;

    [[nodiscard]] HKEY get() const noexcept
    {
        return key_;
    }

    // RegCreateKeyExW / RegOpenKeyExW の出力の置き場。空の鍵にだけ使う。
    [[nodiscard]] HKEY *receive() noexcept
    {
        return &key_;
    }

  private:
    HKEY key_ = nullptr;
};
} // namespace nenenib::adapters::win32
