#pragma once

#include "CodePageFailure.hpp"
#include "CodePagePort.hpp"

#include <windows.h>

#include <expected>
#include <string>
#include <string_view>

namespace nenenib::adapters::win32
{
// CP932 ↔ UTF-8 を OS の表で変換する唯一の場所（ADR 0010 の決定 2 / CPP-014）。
// 表せない文字は unencodable で返し、'?' へ勝手に置き換えない。
class Win32CodePageAdapter final : public application::CodePagePort
{
  public:
    [[nodiscard]] std::expected<std::string, application::CodePageFailure>
    to_utf8(std::string_view cp932) override;
    [[nodiscard]] std::expected<std::string, application::CodePageFailure>
    from_utf8(std::string_view utf8) override;
};
} // namespace nenenib::adapters::win32
