#pragma once

namespace nenenib::application
{
// 起動の問いに「いいえ」と答えた意図（ADR 0137 の決定 6）。controller が autosave_remnants() で
// 覚えた残骸を全部忘れる。使う人の操作なので、歩きを確定し 1 意図の知らせを消す。
struct DiscardAutosave
{
};
} // namespace nenenib::application
