#pragma once

#include "DisplayLanguage.hpp"
#include "DisplayLanguageChoice.hpp"

#include <optional>

namespace nenenib::core
{
// 希望値から解決済みの表示言語へ（ADR 0123 の追補 2026-11-13 の決定 2）。system は OS の観測値
// （無ければ english）、列のある言語はそのまま、列がまだ無い言語（korean spanish german）は
// english。observed を読むのは system のときだけ。
[[nodiscard]] DisplayLanguage resolved_language(DisplayLanguageChoice choice,
                                                std::optional<DisplayLanguage> observed) noexcept;
} // namespace nenenib::core
