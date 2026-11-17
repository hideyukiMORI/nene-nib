#pragma once

namespace nenenib::core
{
// 印の鍵がどの印にも当たらなかったか、Esc・文字でない鍵だったとき（ADR 0163 の決定 5）。何も
// 変えない。鳴らすか（Esc は鳴らさない）は engine が鍵から決める（escaped_or_refused）。
struct EasyMotionCancel
{
};
} // namespace nenenib::core
