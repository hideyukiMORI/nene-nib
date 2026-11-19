#pragma once

#include <cstdint>
#include <string_view>
#include <utility>

namespace nenenib::core
{
// GitHub の alert の種類（`> [!NOTE]` ほか・#1848・D103）。いちばん外の引用の最初の行が
// `[!NOTE]` などだけの引用の要素が持つ。
enum class PreviewAlert : std::uint8_t
{
    note,
    tip,
    important,
    warning,
    caution
};

// 1 行目に出す種類の名前（GitHub の綴りの大文字・訳さない）。
[[nodiscard]] constexpr std::string_view preview_alert_name(PreviewAlert alert) noexcept
{
    switch (alert)
    {
    case PreviewAlert::note:
        return "NOTE";
    case PreviewAlert::tip:
        return "TIP";
    case PreviewAlert::important:
        return "IMPORTANT";
    case PreviewAlert::warning:
        return "WARNING";
    case PreviewAlert::caution:
        return "CAUTION";
    }
    std::unreachable();
}
} // namespace nenenib::core
