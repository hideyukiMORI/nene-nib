#pragma once

namespace nenenib::application
{
// Esc。通常モードでは選択を解くだけで、何も選んでいなければ何も起きない。
// Vim の NORMAL への遷移は Vim エンジンの縦切りで同じ意図に載せる（ADR 0009 の決定 3）。
struct CancelSelection
{
};
} // namespace nenenib::application
