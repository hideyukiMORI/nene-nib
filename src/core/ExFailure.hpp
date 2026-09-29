#pragma once

#include "DisplayText.hpp"

#include <cstdint>

namespace nenenib::core
{
enum class ExFailure : std::uint8_t
{
    unknown_command,
    unknown_option,
    unknown_theme,
    invalid_font_size,
    invalid_font_family,
    invalid_text,
    too_long,
    // Vim が受ける形のうち Nib がまだ受けない引数（ADR 0057 の決定 4・`:tabnext +1` など）。
    // 文言は Vim の E 番号ではなく Nib の言葉で、入力を添えるのは ex_failure_message の 2 引数版。
    unsupported_argument
};

[[nodiscard]] DisplayText ex_failure_message(ExFailure failure);
} // namespace nenenib::core
