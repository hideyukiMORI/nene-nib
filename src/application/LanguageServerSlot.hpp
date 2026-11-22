#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <utility>

namespace nenenib::application
{
// 言語サーバーの席の番号（ADR 0160 の 2026-11-20 の追補・段 S2）。port は 1 本のまま、席の番号で
// 子を指す。閉じた値で、席を足すときは値を 1 つ足して language_server_slots に並べる（数は
// language_server_slot_limit の 1 つ・下の static_assert が並びの漏れを落とす）。
enum class LanguageServerSlot : std::uint8_t
{
    first,
    second,
    third,
    fourth
};

// 同時に起こせる言語サーバーの上限（席の数・設定に出さない定数）。
inline constexpr std::array language_server_slots{
    LanguageServerSlot::first, LanguageServerSlot::second, LanguageServerSlot::third,
    LanguageServerSlot::fourth};
inline constexpr std::size_t language_server_slot_limit = language_server_slots.size();
static_assert(std::to_underlying(language_server_slots.back()) + std::size_t{1} ==
                  language_server_slot_limit,
              "every slot value is listed once in order");

// 席の番号 → 席ごとの配列の添字。
[[nodiscard]] constexpr std::size_t slot_index(LanguageServerSlot slot) noexcept
{
    return std::to_underlying(slot);
}
} // namespace nenenib::application
