#pragma once

#include "DisplayLanguage.hpp"
#include "UiLanguagePort.hpp"

#include <cstdint>
#include <optional>

namespace nenenib::adapters::win32
{
// OS の表示言語を読む唯一の場所（ARC-007・ADR 0123 の追補 2026-11-13 の決定 2）。
// GetUserDefaultUILanguage の LANGID を 1 回読んで写すだけで、ファイルとレジストリは読まない。
class Win32UiLanguage final : public application::UiLanguagePort
{
  public:
    [[nodiscard]] std::optional<core::DisplayLanguage> observe() override;
};

// LANGID の写し（ADR 0123 の改訂の決定 2 のうち列のある 3 言語）。PRIMARYLANGID が日本語なら
// japanese、中国語で SUBLANGID が簡体字（中国・シンガポール）なら simplified_chinese、ほか
// （繁体字・英語・列の無い言語）は english。
[[nodiscard]] std::optional<core::DisplayLanguage>
display_language_of(std::uint16_t language_id) noexcept;
} // namespace nenenib::adapters::win32
