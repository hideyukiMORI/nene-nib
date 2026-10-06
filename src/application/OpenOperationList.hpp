#pragma once

namespace nenenib::application
{
// 操作の一覧を、入力が `?` の Ctrl+P の面で開く（ADR 0078 の決定 9）。F1 が送る。入力行や面が
// 開いていても `?` の面に置き換える。OpenCommandPalette・OpenTabList と同じく Ctrl+Tab の歩きを
// 確定し、1 行の知らせを消す。
struct OpenOperationList
{
};
} // namespace nenenib::application
