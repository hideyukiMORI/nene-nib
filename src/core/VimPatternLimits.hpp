#pragma once

#include <cstddef>
#include <optional>

namespace nenenib::core
{
// 量（`\+` `\=` `\?` `\{n,m}` `\{-n,m}`）を原子の写しに開いた後の原子の数の上限（ADR 0184 の
// 決定 2）。照合の仕事は 1 文字ごとに原子の数に比例するので、写しで膨らむ数を parse で止める
// （時計を使わない守り）。超える量は too_complex で断る（Vim の E363 と同じ知らせ）。量の無い
// パターンには掛けない（決定 1・今のパターンの受け方を変えない）。
inline constexpr std::size_t vim_pattern_atom_limit = 1024;

// 命令の列（ADR 0184 の決定 3）の命令の数の上限。
// 照合の仕事は 1 文字ごとに命令の数に比例するので、
// 群の量の写しで膨らむ数を parse で止める（時計を使わない守り）。
// 原子の写しの上限の原子 1 つは命令が多くとも 3 つ（split・atom・jump）なので、
// その 4 倍に群の端と選択の分を見込んだ値。
// 超えるパターンは too_complex で断る（Vim の E363 と同じ知らせ）。
// 選択と群の量を含まないパターンには掛けない（決定 1）。
inline constexpr std::size_t vim_pattern_instruction_limit = 4096;

// 群の入れ子の深さの上限（Nib の上限・ADR 0184 の段 2 の強制）。命令の列の組み立ては群ごとに
// 再帰するので、深さを止めて呼び出しの積みを守る（`\%(` の入れ子は番号の上限 9 に掛からない）。
// Vim 9.1 の parse（nfa_reg → nfa_regatom の再帰）には深さの上限が無く、65 段の `\%(` も受ける
// （V-1614 の実測）。入力行の上限 4,096 バイトでは `\%(` を 1,365 段まで書けるので、積みの
// 大きさを読める値で止める。超えるパターンは too_complex で断る（選択も群の量も無いパターンは
// 組まないので掛からない）。
inline constexpr std::size_t vim_pattern_nesting_limit = 64;

// Vim 9.1 が「後戻りの照合器（BT）」へ移る `\{n,m}` の境（regexp_nfa.c の nfa_regpiece・
// `(maxval > 500 || maxval > minval + 200) && (maxval != MAX_LIMIT && minval < 200)`）。
inline constexpr std::size_t vim_backtracking_maximum = 500;
inline constexpr std::size_t vim_backtracking_span = 200;
inline constexpr std::size_t vim_backtracking_minimum = 200;

// 量 1 つ（最小・最大・最大が無ければ上限なし）で、Vim 9.1 がパターン全体を BT で照合するか
// （regexpengine=0 の RE_AUTO で nfa_regpiece が FAIL を返し、vim_regcomp が BT で組み直す）。
// 式は Vim の source のとおりに書く（minimum < 200 のとき maximum > 500 は
// maximum > minimum + 200 に含まれる）。`\+` `\=` `\{n,}` `\{0}` は当たらない。Vim の wants_nfa
// （`[[:upper:]]` `[[:lower:]]` が手前にあると NFA のまま）は、Nib が文字の類を受けないので
// 読まない。Nib は BT の照合器を持たないので、命令の列の道に乗るパターンがこれに当たる量を
// 1 つでも含めば断る（ADR 0184 の段 2 の強制）。
[[nodiscard]] constexpr bool vim_quantity_backtracks(std::size_t minimum,
                                                     std::optional<std::size_t> maximum) noexcept
{
    if (!maximum.has_value())
    {
        return false;
    }
    const std::size_t most = maximum.value();
    return (most > vim_backtracking_maximum || most > minimum + vim_backtracking_span) &&
           minimum < vim_backtracking_minimum;
}

// parse の上限（製品は既定の値・試験だけが小さな値を渡す）。値は単独で妥当なので公開 aggregate
// （CPP-003）。
struct VimPatternLimits
{
    std::size_t atoms = vim_pattern_atom_limit;
    std::size_t instructions = vim_pattern_instruction_limit;
    std::size_t nesting = vim_pattern_nesting_limit;
};
} // namespace nenenib::core
