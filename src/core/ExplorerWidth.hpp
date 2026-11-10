#pragma once

#include <cstdint>
#include <optional>

namespace nenenib::core
{
// 出ている面の幅の求め（ADR 0162 の追補・施主決定 D78）。dips は使う人が決めた幅（DIP・覚えた
// 設定 explorer_width か、ドラッグの間の幅）で、値なしは既定の式 min(240 DIP, client の 40%)。
// 面が出ているかは explorer_layout の引数が std::optional で持つ。単独で妥当な値なので公開
// aggregate（CPP-003）。
struct ExplorerWidth
{
    std::optional<std::int32_t> dips;
};
} // namespace nenenib::core
