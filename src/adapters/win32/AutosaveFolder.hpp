#pragma once

#include "AutosaveFailure.hpp"
#include "AutosaveFolderName.hpp"
#include "AutosaveId.hpp"
#include "AutosaveRemnant.hpp"
#include "AutosaveSnapshot.hpp"
#include "FilePath.hpp"
#include "FilePort.hpp"
#include "ProcessInstance.hpp"
#include "ProcessInstances.hpp"

#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace nenenib::adapters::win32
{
// 自動退避の置き場の中身を扱う（ADR 0137 の決定 4〜6・追補 #1634）。root の下の
// `<pid の小文字 16 進>-<作成時刻の小文字 16 進>` が 1 回の起動のフォルダで、一覧 `autosave.v1` と
// 本文 `<番号>.body` を持つ。書くのは files の原子的な書き。一覧は読む → 直す → 書く で、空になれば
// 一覧とフォルダを消す。前の版が書いた `<pid の 16 進>` だけのフォルダは読むだけで、書かない。

// root の下の、この起動のフォルダ（UTF-8 の経路）。自分の置き場の名前を作るのはここだけ。
[[nodiscard]] std::string autosave_folder(const core::FilePath &root, ProcessInstance self);
// フォルダ名を読む。作る形（小文字・先頭の 0 なし・pid は 32 bit・作成時刻は 64 bit）の今の形か
// 前の形だけを受け、ほかは無し。
[[nodiscard]] std::optional<AutosaveFolderName> autosave_folder_name_of(std::wstring_view name);
// 前回の起動の残骸のフォルダの名前か（#1634）。形の外は偽。
// 前の形: pid が self なら真（同じ pid の前の process は必ず終わっている）。
//   ほかは created_of が gone のときだけ真。
// 今の形: self と同じ印なら偽。pid が self で作成時刻が違えば真。
//   ほかは created_of が gone か、生きていて作成時刻が名前と違う（pid が再利用された）とき真。
//   denied は生きている本人の側に倒して偽。
[[nodiscard]] bool autosave_remnant_name(std::wstring_view name, ProcessInstance self,
                                         ProcessCreatedOf created_of);
// フォルダを作って本文を書き、一覧の同じ番号の行を置き換える（無ければ足す）。
[[nodiscard]] std::expected<void, application::AutosaveFailure>
autosave_stored(application::FilePort &files, const std::optional<std::string> &folder,
                const application::AutosaveSnapshot &snapshot);
// 本文を消して一覧から外す。無い番号は何もしない。
[[nodiscard]] std::expected<void, application::AutosaveFailure>
autosave_dropped(application::FilePort &files, const std::string &folder,
                 application::AutosaveId id);
// フォルダの中のファイルを全部消してからフォルダを消す。消せないものは残す。
void remove_autosave_folder(const std::string &folder);
// root の直下の、autosave_remnant_name が真のフォルダの一覧の行。一覧を読めないフォルダは飛ばす。
[[nodiscard]] std::vector<application::AutosaveRemnant>
autosave_remnants_in(application::FilePort &files, const core::FilePath &root, ProcessInstance self,
                     ProcessCreatedOf created_of);
// remnant の location（`<フォルダ名>/<番号>`・フォルダ名は今の形か前の形）が作る形と一致する
// ときだけ、そのフォルダ。
[[nodiscard]] std::optional<std::string>
autosave_remnant_folder(const core::FilePath &root, const application::AutosaveRemnant &remnant);
} // namespace nenenib::adapters::win32
