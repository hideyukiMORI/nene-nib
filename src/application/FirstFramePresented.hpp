#pragma once

namespace nenenib::application
{
// 窓が最初の frame を画面に出した、という合図（ADR 0172 の決定 3）。ui が 1 回だけ送る。中身は
// 無く、使う人の操作ではない（is_signal）。controller はこのとき起動画面が出ていれば履歴と
// ブックマークの読みを 1 回だけ頼み、以後は起動画面の鍵を受ける。
struct FirstFramePresented
{
};
} // namespace nenenib::application
