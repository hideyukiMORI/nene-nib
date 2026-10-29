#pragma once

namespace nenenib::application
{
// 「行へ移動」の操作（Ctrl+G と F1 の一覧・ADR 0174 の決定 2・8）。通常モードは「行: 」の入力行を
// 開き、Vim モードは `:` の入力行を空で開く（`:N`・INSERT なら先に NORMAL へ戻る）。
struct BeginGoToLine
{
};
} // namespace nenenib::application
