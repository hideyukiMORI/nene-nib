#pragma once

namespace nenenib::application
{
// 開いているタブの一覧を Ctrl+P の面で開く（ADR 0057 の決定 7）。帯の「∨」と Ex の `:tabs` と
// Ctrl+P の候補 `tabs` が同じ所へ着く。一覧（または Ctrl+P の面）が開いている間は閉じる。
struct OpenTabList
{
};
} // namespace nenenib::application
