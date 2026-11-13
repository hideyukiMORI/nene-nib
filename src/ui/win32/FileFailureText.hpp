#pragma once

#include "DisplayLanguage.hpp"
#include "FileFailure.hpp"
#include "UiMessage.hpp"

#include <string>

namespace nenenib::ui::win32
{
// ファイルの失敗を 1 行の説明に写す（ADR 0010 の決定 9・ADR 0123 の追補 2026-11-13・#1019）。
// 文の名前は閉じた switch で決め（CPP-002）、文は表から言語で引く。開く上限（1 GB）と復号の
// 上限（64 MiB）は core の定数から差し込む（文と定数を手で揃えない）。
[[nodiscard]] core::UiMessage file_failure_message(application::FileFailure failure) noexcept;
[[nodiscard]] std::wstring file_failure_text(application::FileFailure failure,
                                             core::DisplayLanguage language);
} // namespace nenenib::ui::win32
