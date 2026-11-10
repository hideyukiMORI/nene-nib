#pragma once

#include "FilePath.hpp"
#include "ImageReferenceRefusal.hpp"

#include <expected>
#include <optional>
#include <string_view>

namespace nenenib::core
{
// Markdown の画像の参照先（md4c の src・実体参照は解いた後）をローカルのファイルの経路に解く純関数
// （施主決定 D79・ADR 0155 の追補 48・#1381）。ファイルに触れない（文字列の計算だけ・ARC-007）。
// document は開いている Markdown のファイルの経路（無題なら nullopt）。
//
// - ドライブの絶対パス（`C:\a.png` `C:/a.png`）は綴りのまま（percent-encoding を解かない・`?` と
//   `#` で切らない）。
// - `file:` の URL（scheme は大小を問わない）は `file:///C:/a.png`・`file://localhost/C:/a.png`・
//   `file:/C:/a.png` を受け、`?` と `#` の後ろを捨てて percent-encoding を解く。host のある URL は
//   network。
// - 相対パス（`a.png` `./img/a.png` `../a.png` `img%20x.png`）は `?` と `#` の後ろを捨てて
//   percent-encoding を解き、文書のフォルダから解く。`/` か `\` で始まる参照は文書のドライブの根か
//   ら解く。
// - 結果は区切りを `\` に揃え、`.` を落として `..` を 1 つ上へ畳む（根より上へは出ない）。結果が
//   `\\` で始まる（文書がネットワークの場所にある）なら network。
// - 不完全な percent-encoding（`%` の後ろが 16 進 2 桁でない）は `%` のまま残す。
[[nodiscard]] std::expected<FilePath, ImageReferenceRefusal>
resolve_image_reference(std::string_view reference, const std::optional<FilePath> &document);
} // namespace nenenib::core
