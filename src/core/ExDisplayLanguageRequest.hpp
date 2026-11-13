#pragma once

#include "DisplayLanguageChoice.hpp"

#include <optional>

namespace nenenib::core
{
// `:set language=<名前>`・`:set language?`（ADR 0123 の追補 2026-11-13 の決定 8・#1019）。chosen
// は選んだ希望値（`ja` `en` `zh-hans` `system` の 4 つのどれか）で、空なら今の値を知らせる問い。
// 設定 v4 の `ui_language` へ保存し、OS を観測して解決し直し、状態に置くのは controller（ADR 0160
// の lsp と同じ要求の形）。
struct ExDisplayLanguageRequest
{
    std::optional<DisplayLanguageChoice> chosen;
};
} // namespace nenenib::core
