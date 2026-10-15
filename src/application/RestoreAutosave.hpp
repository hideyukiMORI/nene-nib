#pragma once

namespace nenenib::application
{
// 起動の問いに「はい」と答えた意図（ADR 0137 の決定 6）。中身は無く、戻す残骸は controller が
// autosave_remnants() で引いて覚えた列。使う人の操作なので、歩きを確定し 1 意図の知らせを消す。
struct RestoreAutosave
{
};
} // namespace nenenib::application
