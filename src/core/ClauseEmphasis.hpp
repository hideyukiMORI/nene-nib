#pragma once

#include <cstdint>

namespace nenenib::core
{
// 変換中の文節の強さ（ADR 0014 の決定 2・7）。IME の属性（GCS_COMPATTR）は開いた集合だが、
// 描き分けは「注目している文節」と「それ以外」の 2 つに畳む。閉じた enum なので、
// 増えたら switch がコンパイルで落ちる（CPP-002）。
enum class ClauseEmphasis : std::uint8_t
{
    target,
    other
};
} // namespace nenenib::core
