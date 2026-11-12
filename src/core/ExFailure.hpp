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
    unsupported_argument,
    // option の値が閉じた選択肢に無い（`:set filetype=<名前>` の名前が Nib の言語に無い・
    // ADR 0154 の決定 4）。文言は Vim の E474: Invalid argument。
    invalid_argument,
    // 範囲を取らない命令に範囲が付いた（VISUAL の `:'<,'>set` や回数の `:.,.+2noh`・ADR 0022 の
    // 追補 #1526）。文言は Vim の E481: No range allowed。
    no_range_allowed,
    // `:s` の区切りが英字（ADR 0168 の決定 1）。文言は Vim の E146。
    letter_delimiter,
    // `:s` の回数が 0（ADR 0168 の決定 2）。文言は Vim の E939: Positive count required。
    positive_count,
    // `:s` のフラグと回数の後ろの余り（ADR 0168 の決定 2）。文言は Vim の E488。
    trailing_characters
};

[[nodiscard]] DisplayText ex_failure_message(ExFailure failure);
} // namespace nenenib::core
