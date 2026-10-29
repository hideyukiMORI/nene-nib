#pragma once

namespace nenenib::core
{
// 表を引いた結果の「表に無い」（ADR 0176 の決定 2）。待ちは空のままで、呼び手は今の鍵を
// そのまま engine へ渡す。
struct KeyMapPass
{
};
} // namespace nenenib::core
