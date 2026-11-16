#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace nenenib::core
{
// プロジェクトの全文検索の固定の値（ADR 0167 の決定 2〜4）。adapters（木を歩く・読む）と
// application（件数で打ち切る）と core（塊を照合する）は同じこの値を読む。設定にしない。

// 1 つの仕事で読む塊の大きさ（バイト・1 MiB）。読む側は塊を行の境で切らなくてよい
// （塊の終わりで切れた行は ProjectScan が次の塊へ繰り越す）。
inline constexpr std::size_t project_search_chunk_bytes = std::size_t{1} << 20U;

// 照合する 1 行の長さの上限（バイト・1 MiB・改行の `\n` を含まない）。超える行は照合せずに飛ばし、
// 行の番号だけを数える。塊に切っても切らなくても同じ行を飛ばす。
inline constexpr std::size_t project_search_line_limit = std::size_t{1} << 20U;

// 探すファイルの大きさの上限（バイト・16 MiB）。超えるファイルは読まない（project_file_fits）。
inline constexpr std::uint64_t project_search_file_limit = std::uint64_t{16} << 20U;

// バイナリの判定に読む先頭の長さ（バイト・8 KiB）。
inline constexpr std::size_t project_search_binary_probe = std::size_t{8} << 10U;

// 当たりの件数の上限（件・全部のファイルを合わせて）。届いたら探すのを止める（`truncated`）。
inline constexpr std::size_t project_search_hit_limit = 1000;

// 当たりの行の本文の窓の長さ（バイト・ProjectHit::line_text）。
inline constexpr std::size_t project_hit_text_bytes = 256;

// 当たりが行頭からの窓に収まらないとき、窓を当たりの側へ寄せて当たりの前に残す長さ（バイト・
// 窓の 4 分の 1）。
inline constexpr std::size_t project_hit_context_bytes = 64;

// 歩かないフォルダの名前（固定の表・設定にしない・ADR 0167 の決定 2）。隠し属性のフォルダは
// adapter が属性で飛ばす。
inline constexpr std::array<std::string_view, 4> project_skipped_folders{".git", "node_modules",
                                                                         "build", "out"};

// 名前（UTF-8・パスではなく 1 段の名前）が表のフォルダか。Windows の名前と同じく ASCII の大小を
// 区別しない（`Build` も飛ばす）。
[[nodiscard]] bool project_folder_skipped(std::string_view name) noexcept;

// 大きさ（バイト）のファイルを探すか（project_search_file_limit 以下なら探す）。
[[nodiscard]] bool project_file_fits(std::uint64_t bytes) noexcept;

// ファイルの先頭（最初に読んだ塊。長くても最初の project_search_binary_probe バイトだけを見る）に
// NUL があればバイナリとして飛ばす。UTF-16 の本文も NUL を含むので飛ぶ。
[[nodiscard]] bool project_text_is_binary(std::string_view head) noexcept;
} // namespace nenenib::core
