#pragma once

#include "FilePath.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace nenenib::adapters::win32
{
// 保存の一時ファイルの名前と回収（ADR 0135）。名前は `<target>.nib-tmp-<プロセス ID の 16
// 進>-<連番>` で、保存先と同じフォルダに置く（ADR 0010 の決定 6）。
[[nodiscard]] std::wstring temporary_name(const std::wstring &target, std::uint32_t sequence);
// 連番はプロセスで 1 つ（文書・設定・履歴…の adapter が別でも衝突しない）。次の連番を払い出す。
[[nodiscard]] std::uint32_t next_temporary_sequence() noexcept;
// ここまでに払い出した最後の連番（まだ無ければ 0）。これ以下の自分の連番は古い（決定 3）。
[[nodiscard]] std::uint32_t issued_temporary_sequence() noexcept;
// 置き換えの直前に、一時ファイルがこのプロセスのその連番のものであることを名前で確かめる（決定
// 4）。
[[nodiscard]] bool owned_temporary(const std::wstring &temporary, const std::wstring &target,
                                   std::uint32_t sequence);
// プロセスが生きているかを答える関数。回収は OpenProcess で、試験は替え玉で答える。
using ProcessAlive = bool (*)(std::uint32_t pid) noexcept;
// OpenProcess の 1 本の判定（決定 3）。無い pid と終わったプロセスだけが偽で、拒否とほかの失敗は
// 生きている側に倒す。自動退避の残骸の判定も同じ 1 本を使う（ADR 0137 の決定 6）。
[[nodiscard]] bool process_alive(std::uint32_t pid) noexcept;
// 作る名前と同じ形（小文字・先頭の 0 なし・32 bit に収まる）の 16 進の pid を読む。形が違えば無し。
[[nodiscard]] std::optional<std::uint32_t> process_id_of(std::wstring_view text) noexcept;
// 回収してよい残骸の名前か（決定 3）。suffix は `.nib-tmp-` の後ろの全部。形が作る名前と同じ
// `<16 進の pid>-<10 進の連番>`（小文字・先頭の 0 なし・32 bit に収まる）で、(a) pid が self で
// なく alive が偽、または (b) pid が self で連番が issued 以下（今の保存より古い）のときだけ真。
[[nodiscard]] bool reclaimable_temporary(std::wstring_view suffix, std::uint32_t self,
                                         std::uint32_t issued, ProcessAlive alive);
// `<target>.nib-tmp-*` のうち reclaimable_temporary が真のものを DeleteFileW で消す。消せない
// もの（開かれている・pid が再利用された相手が使っている）は残す。失敗は無視する（決定 3 の
// (a)）。
void reclaim_temporaries_of(const std::wstring &target);
// file と同じフォルダの `*.nib-tmp-*` を同じ規則で消す。起動時に設定の置き場で呼ぶ（決定 3 の
// (b)）。
void reclaim_temporaries_beside(const core::FilePath &file);
} // namespace nenenib::adapters::win32
