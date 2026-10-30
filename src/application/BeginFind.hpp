#pragma once

namespace nenenib::application
{
// 「検索」の操作（Ctrl+F と F1 の一覧・ADR 0174 の決定 2・3）。通常モードは
// 「検索: 」の入力行を前回の語つきで開き、Vim モードは `/` の入力行を開く
// （INSERT なら先に NORMAL へ戻る）。
struct BeginFind
{
};
} // namespace nenenib::application
