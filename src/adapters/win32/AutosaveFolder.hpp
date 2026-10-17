#pragma once

#include "AutosaveFailure.hpp"
#include "AutosaveId.hpp"
#include "AutosaveRemnant.hpp"
#include "AutosaveSnapshot.hpp"
#include "FilePath.hpp"
#include "FilePort.hpp"

#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <vector>

namespace nenenib::adapters::win32
{
// 自動退避の置き場の中身を扱う（ADR 0137 の決定 4〜6）。root の下の `<pid の小文字 16 進>` が
// 1 プロセスのフォルダで、一覧 `autosave.v1` と本文 `<番号>.body` を持つ。書くのは files の原子的な
// 書き。一覧は読む → 直す → 書く で、空になれば一覧とフォルダを消す。

// root の下の pid のフォルダ（UTF-8 の経路）。
[[nodiscard]] std::string autosave_folder(const core::FilePath &root, std::uint32_t pid);
// フォルダを作って本文を書き、一覧の同じ番号の行を置き換える（無ければ足す）。
[[nodiscard]] std::expected<void, application::AutosaveFailure>
autosave_stored(application::FilePort &files, const std::optional<core::FilePath> &root,
                const application::AutosaveSnapshot &snapshot);
// 本文を消して一覧から外す。無い番号は何もしない。
[[nodiscard]] std::expected<void, application::AutosaveFailure>
autosave_dropped(application::FilePort &files, const std::string &folder,
                 application::AutosaveId id);
// フォルダの中のファイルを全部消してからフォルダを消す。消せないものは残す。
void remove_autosave_folder(const std::string &folder);
// root の直下の、名前が作る形の 16 進の pid で、self でなく、生きていない（TemporaryFiles と
// 同じ 1 本の判定）フォルダの一覧の行。一覧を読めないフォルダは飛ばす。
[[nodiscard]] std::vector<application::AutosaveRemnant>
autosave_remnants_in(application::FilePort &files, const core::FilePath &root, std::uint32_t self);
// remnant の location（`<pid の 16 進>/<番号>`）が作る形と一致するときだけ、そのフォルダ。
[[nodiscard]] std::optional<std::string>
autosave_remnant_folder(const core::FilePath &root, const application::AutosaveRemnant &remnant);
} // namespace nenenib::adapters::win32
