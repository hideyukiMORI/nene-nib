#pragma once

#include <string>
#include <string_view>

namespace nenenib::core
{
// URI の percent-encoding（`%XX`）を 1 バイトに解く唯一の写し（ADR 0190 の決定 4・ARC-012）。
// 画像の参照（ImageReference）と言語サーバーの URI（file_path_of_uri）が共用する。16 進 2 桁が
// 続かない `%` はそのまま残す。解いたバイト列が正しい UTF-8 かは呼び出し元が確かめる。
[[nodiscard]] std::string percent_decoded(std::string_view text);
} // namespace nenenib::core
