#pragma once

#include "LspId.hpp"
#include "LspProgressKind.hpp"

#include <cstdint>
#include <optional>
#include <string>

namespace nenenib::core
{
// `$/progress` の 1 通（LSP 3.17 の WorkDoneProgressBegin / Report / End・ADR 0160 の 2026-11-22 の
// 追補）。token は ProgressToken（整数か文字列）、title は begin だけが持つ、message と percentage
// （0〜100 に丸めた値）は欠けてよい。application は token ごとの今の値（題は begin の値のまま）も
// この形で持つ。どの欄も互いに独立して妥当なので公開 aggregate（CPP-003）。
struct LspProgressValue
{
    LspId token;
    LspProgressKind kind;
    std::optional<std::string> title;
    std::optional<std::string> message;
    std::optional<std::uint64_t> percentage;
};
} // namespace nenenib::core
