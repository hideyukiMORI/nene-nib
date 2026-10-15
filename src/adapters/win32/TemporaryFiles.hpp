#pragma once

#include "FilePath.hpp"

#include <cstdint>
#include <string>

namespace nenenib::adapters::win32
{
// 保存の一時ファイルの名前と回収（ADR 0135）。名前は `<target>.nib-tmp-<プロセス ID の 16
// 進>-<連番>` で、保存先と同じフォルダに置く（ADR 0010 の決定 6）。
[[nodiscard]] std::wstring temporary_name(const std::wstring &target, std::uint32_t sequence);
// 置き換えの直前に、一時ファイルがこのプロセスのその連番のものであることを名前で確かめる（決定
// 4）。
[[nodiscard]] bool owned_temporary(const std::wstring &temporary, const std::wstring &target,
                                   std::uint32_t sequence);
// `<target>.nib-tmp-*` のうち DeleteFileW できるものを消す。生きている一時ファイルは共有 0 で
// 開かれているので消えない。失敗は無視する（決定 3 の (a)）。
void reclaim_temporaries_of(const std::wstring &target);
// file と同じフォルダの `*.nib-tmp-*` を同じ規則で消す。起動時に設定の置き場で呼ぶ（決定 3 の
// (b)）。
void reclaim_temporaries_beside(const core::FilePath &file);
} // namespace nenenib::adapters::win32
