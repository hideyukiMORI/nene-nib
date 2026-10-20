#pragma once

#include <string>
#include <string_view>

namespace nenenib::core
{
// md4c が MD_TEXT_ENTITY で渡す実体参照の綴り（"&amp;" "&#35;" "&#x22;"）を表示の UTF-8 に解く
// （ADR 0151 の決定 7）。数値は CommonMark と同じく 0・サロゲート・U+10FFFF 超を U+FFFD にする。
// 名前つきは CommonMark の例に出る範囲の表だけで、表に無い名前と読めない綴りは綴りのまま返す。
[[nodiscard]] std::string markdown_entity_text(std::string_view spelling);
} // namespace nenenib::core
