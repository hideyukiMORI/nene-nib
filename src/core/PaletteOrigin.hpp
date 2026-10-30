#pragma once

#include <cstdint>
#include <string_view>

namespace nenenib::core
{
// Ctrl+P の候補の出どころの印（ADR 0060 の決定 3）。folder は同じフォルダの候補（ADR 0062 の
// 決定 11）。bookmark は登録、bookmarked_tab は登録済みのタブ（ADR 0063）。document_format は
// 保存の形の候補（ADR 0010 の追補 #1049 の決定 2）で、その面の候補の列はこの印の候補だけ。Ex
// のコマンドの候補は印なし。値が増えたら 出どころで絞る関数と補足の文言の switch
// が落ちる（CPP-002）。
enum class PaletteOrigin : std::uint8_t
{
    tab,
    bookmarked_tab,
    bookmark,
    history,
    folder,
    document_format
};

// 行の右に添える補足の文言（ADR 0060 の決定 3）。
[[nodiscard]] std::string_view palette_origin_label(PaletteOrigin origin) noexcept;
} // namespace nenenib::core
