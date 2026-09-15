#pragma once

#include "CodePageFailure.hpp"

#include <expected>
#include <string>
#include <string_view>

namespace nenenib::application
{
// CP932 ↔ UTF-8 の変換の唯一の入口（ADR 0010 の決定 2）。表を core に持ち込まず、OS に任せる。
class CodePagePort
{
  public:
    CodePagePort() = default;
    virtual ~CodePagePort() = default;
    CodePagePort(const CodePagePort &) = delete;
    CodePagePort(CodePagePort &&) = delete;
    CodePagePort &operator=(const CodePagePort &) = delete;
    CodePagePort &operator=(CodePagePort &&) = delete;

    [[nodiscard]] virtual std::expected<std::string, CodePageFailure>
    to_utf8(std::string_view cp932) = 0;
    [[nodiscard]] virtual std::expected<std::string, CodePageFailure>
    from_utf8(std::string_view utf8) = 0;
};
} // namespace nenenib::application
