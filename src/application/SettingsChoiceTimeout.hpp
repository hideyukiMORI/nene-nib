#pragma once

namespace nenenib::application
{
// 選ぶ欄の一覧の頭文字の跳びの「1 秒空いた」の合図（#1926）。ui は一覧へ字を送るたびに 1 秒・1 回の
// タイマーを張り直し（MapTimeout と同じ形・時刻は読まず OS のタイマーに数えさせる）、切れたらこれを
// 1 回送る。controller は打った字を忘れる（選ぼうとしている位置と一覧はそのまま）。使う人の操作では
// ないので is_signal の 1 本に載り、歩き・知らせ・一覧と面を動かさない。
struct SettingsChoiceTimeout
{
};
} // namespace nenenib::application
