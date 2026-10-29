#pragma once

#include <cstddef>
#include <optional>

namespace nenenib::core
{
// 通常モードの「行へ移動」の入力を読んだ値（ADR 0174 の決定 8）。line は書いたままの行番号（`:N` と
// 同じく先頭の 0 を読み飛ばし、size_t に収まらなければ最大に飽和する）で、範囲の外かは engine が
// `{N}G` と同じ着地で決める。column は `行:桁` の桁（1 始まりのコードポイント・ステータスバーの
// 「桁」と同じ単位・飽和も同じ）で、`行` だけなら値なし。どちらも単独で妥当な公開 aggregate。
struct GoToLineTarget
{
    std::size_t line;
    std::optional<std::size_t> column;
};
} // namespace nenenib::core
