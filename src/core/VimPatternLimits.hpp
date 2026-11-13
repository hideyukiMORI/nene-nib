#pragma once

#include <cstddef>

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

// parse の上限（製品は既定の値・試験だけが小さな値を渡す）。値は単独で妥当なので公開 aggregate
// （CPP-003）。
struct VimPatternLimits
{
    std::size_t atoms = vim_pattern_atom_limit;
    std::size_t instructions = vim_pattern_instruction_limit;
};
} // namespace nenenib::core
