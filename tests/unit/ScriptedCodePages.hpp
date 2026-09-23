#pragma once

#include "CodePageFailure.hpp"
#include "CodePagePort.hpp"
#include "Utf16.hpp"

#include <expected>
#include <string>
#include <string_view>
#include <utility>

namespace nenenib::tests
{
using nenenib::application::CodePageFailure;
using nenenib::application::CodePagePort;
using nenenib::core::to_utf8;
using Converted = std::expected<std::string, CodePageFailure>;

// 偽の CP932。実際の表は持たず、台本の答えを返すだけ（ADR 0010 の決定 2 の境界を測る）。
class ScriptedCodePages final : public CodePagePort
{
  public:
    void decode_to(Converted decoded)
    {
        decoded_ = std::move(decoded);
    }

    void encode_to(Converted encoded)
    {
        encoded_ = std::move(encoded);
    }

    [[nodiscard]] const std::string &encoded_from() const noexcept
    {
        return encoded_from_;
    }

    [[nodiscard]] Converted to_utf8(std::string_view cp932) override
    {
        decoded_from_ = std::string(cp932);
        return decoded_;
    }

    [[nodiscard]] Converted from_utf8(std::string_view utf8) override
    {
        encoded_from_ = std::string(utf8);
        return encoded_;
    }

  private:
    Converted decoded_{std::string{}};
    Converted encoded_{std::string{}};
    std::string decoded_from_;
    std::string encoded_from_;
};
} // namespace nenenib::tests
