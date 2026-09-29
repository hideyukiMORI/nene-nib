#pragma once

#include <string>

namespace nenenib::adapters::win32
{
// path の親のフォルダが無ければ 1 段だけ作る。設定と前回のタブの一覧の書きが使う（ADR 0020 /
// ADR 0059 の決定 2）。親の親は作らない。
[[nodiscard]] bool ensure_parent_directory(const std::wstring &path);
} // namespace nenenib::adapters::win32
