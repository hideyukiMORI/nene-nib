#pragma once

namespace nenenib::application
{
// 言語サーバーの古さの合図（ADR 0160 の決定 6・追補）。ui のタイマーが 30 秒ごとに送り、時刻は
// 運ばない。controller は答えを待っている要求の古さを tick の数で数え、2 回の tick を越えた要求
// （30 秒を超えた要求）に `$/cancelRequest` を送って捨て、退く子の期限を adapter に告げる。
// AutosaveTick と同じく使う人の操作ではないので、歩き・知らせ・変換・入力行・Vim の待ちの状態・
// 記録・undo の単位を 1 つも動かさない。
struct LanguageServerTick
{
};
} // namespace nenenib::application
