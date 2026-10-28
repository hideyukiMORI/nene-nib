#pragma once

#include <cstdint>

namespace nenenib::core
{
// 表示言語の希望値（ADR 0123 の改訂の決定 1）。system は OS の観測から解決する
// 印で、ほかの 6 値は解決済みの言語と同じ並び。保存の ASCII 値（`system` `ja`
// `en` `zh-hans` `ko` `es` `de`）への写しは adapters の設定の codec の 1 か所。
// いまは設定 v4 の欄として読み書きするだけで、解決と表示は後の PR（#1019）。
// 値を足すと写し先の switch がコンパイルで落ちる（CPP-002）。
enum class DisplayLanguageChoice : std::uint8_t
{
    system,
    japanese,
    english,
    simplified_chinese,
    korean,
    spanish,
    german
};
} // namespace nenenib::core
