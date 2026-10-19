#pragma once

#include "SealedBytes.hpp"
#include "TextEncoding.hpp"

#include <expected>
#include <string>
#include <string_view>

namespace nenenib::core
{
// 判定した列と文字コードを一緒に所有する（ADR 0080）。可変の列は貸さない。UTF-16 だけは
// BOM を外して UTF-8 へ写した列を持つ（ADR 0149 の決定 2）。ほかは読んだ列のまま。
class DetectedText final
{
  public:
    [[nodiscard]] static std::expected<DetectedText, EncodingFailure>
    from_bytes(std::string_view bytes);
    // 封印済みの列は可変の別名を持たないので、判定した列をそのまま move で保持する（ADR 0141 決定
    // 3）。
    [[nodiscard]] static std::expected<DetectedText, EncodingFailure>
    from_sealed(SealedBytes &&bytes);
    [[nodiscard]] TextEncoding encoding() const noexcept;
    [[nodiscard]] std::string_view bytes() const & noexcept;
    [[nodiscard]] std::string_view bytes() const && = delete;

  private:
    friend class TextBuffer;
    DetectedText(std::string bytes, TextEncoding encoding);
    // 判定の済んだ列を持つ。UTF-16 はここで UTF-8 へ写す（ADR 0149 の決定 2）。
    [[nodiscard]] static std::expected<DetectedText, EncodingFailure>
    from_detected(std::string bytes, TextEncoding encoding);
    std::string bytes_;
    TextEncoding encoding_;
};
} // namespace nenenib::core
