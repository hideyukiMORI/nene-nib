#pragma once

#include <cstdint>
#include <utility>

namespace nenenib::core
{
// 鍵がどこから来たか（ADR 0046 の決定 2・ADR 0176 の追補 33）。typed は利用者の打鍵、mapped は
// `map` の表に当たった右辺から controller が流している鍵、replayed は controller が `.` と `@`
// の再生（VimReplay）で流している鍵。再生の中では `q` が効かない（Vim の「レジスタ実行中は q は
// 無効」）。マクロの録画に積むのは typed だけ（Vim の「録るのは打った鍵だけ」・右辺の鍵の代わりに
// controller が打った左辺を積む・#1312）。値を足したら下の 2 つの switch が落ちる（CPP-002）。
enum class VimKeySource : std::uint8_t
{
    typed,
    mapped,
    replayed
};

// 再生の中の鍵か（`q` を無効にし、VISUAL の `.` の選び直しの印を持ち越す）。
[[nodiscard]] constexpr bool vim_key_replayed(VimKeySource source) noexcept
{
    switch (source)
    {
    case VimKeySource::typed:
    case VimKeySource::mapped:
        return false;
    case VimKeySource::replayed:
        return true;
    }
    std::unreachable();
}

// engine がマクロの録画に積む鍵か（利用者が打った鍵だけ）。
[[nodiscard]] constexpr bool vim_key_recorded_in_macro(VimKeySource source) noexcept
{
    switch (source)
    {
    case VimKeySource::typed:
        return true;
    case VimKeySource::mapped:
    case VimKeySource::replayed:
        return false;
    }
    std::unreachable();
}
} // namespace nenenib::core
