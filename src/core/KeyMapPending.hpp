#pragma once

namespace nenenib::core
{
// 表を引いた結果の「待つ」（ADR 0176 の決定 2）。今の鍵で左辺の途中なので、呼び手は鍵を
// 待ちの列の後ろに積み、engine へは渡さない。
struct KeyMapPending
{
};
} // namespace nenenib::core
