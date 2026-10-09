#pragma once

#include "TextEncoding.hpp"

#include <expected>
#include <string>
#include <string_view>

namespace nenenib::core
{
// 判定した列と文字コードを一緒に所有する（ADR 0080）。可変の列は貸さない。
class DetectedText final
{
  public:
    [[nodiscard]] static std::expected<DetectedText, EncodingFailure>
    from_bytes(std::string_view bytes);
    [[nodiscard]] TextEncoding encoding() const noexcept;
    [[nodiscard]] std::string_view bytes() const & noexcept;
    [[nodiscard]] std::string_view bytes() const && = delete;

  private:
    friend class TextBuffer;
    DetectedText(std::string bytes, TextEncoding encoding);
    std::string bytes_;
    TextEncoding encoding_;
};
} // namespace nenenib::core
