#pragma once

#include <cstdint>

namespace nenenib::application
{
// 通常モードの検索の流れの段（ADR 0174 の決定 3）。「検索: 」の Enter が何をするかを決める。
// 値は流れの種類で、Ctrl+F の流れと Ctrl+H の置換の流れの 2 つ。増えたら落ちた switch
// を直す（CPP-002）。
enum class FindPhase : std::uint8_t
{
    // Ctrl+F の「検索: 」。Enter / F3 / Shift+F3 は当たりへ移り、入力行は閉じない（Esc で閉じる）。
    find,
    // Ctrl+H の置換の流れ。「検索: 」の Enter は当たりへ移ってから「置換後: 」へ進み、「置換後:
    // 」の Enter は今の当たりを 1 件、Ctrl+Enter は全件を置き換える（ADR 0174 の決定 3・7）。
    replace_with
};
} // namespace nenenib::application
