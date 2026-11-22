#pragma once

#include "FoundLanguageServer.hpp"
#include "LanguageServerProbe.hpp"
#include "ServerSearchPlaces.hpp"

#include <optional>
#include <stop_token>
#include <string>
#include <vector>

namespace nenenib::adapters::win32
{
// 言語サーバーを探す 1 回（LSP 段 S4・#1945・ADR 0160 の決定 7 の追補）。何も起こさず、ファイルが
// 在るかを見て `package.json` を読むだけ。結果は probes と同じ順・同じ数。
// - npm でない候補: SearchPathW に places.folders を `;` でつないだ値を明示して `<program>.exe` を
//   探す（作業フォルダと既定の探す道は見ない）。在ってフォルダでない物だけ。
// - npm の候補: places.npm_roots を順に `<root>\node_modules\<package>\package.json` を読み（1 MiB
//   まで）、core::npm_bin_script で `bin` のスクリプトを引き、包みのフォルダの下に在るファイル
//   なら、folders から探した node.exe と組にする。`.cmd` の入口は読まない。包みの名前の形が違う・
//   引けない・node.exe が無ければ見つからない。node.exe は 1 回の探しの中で 1 回だけ探す。
// - stop: 候補の間で見て、止める合図が来たら残りの候補を探さずに「見つからない」で埋める（窓を
//   閉じるときのワーカーの join を待たせない・#1945 のレビュー R1）。
[[nodiscard]] std::vector<std::optional<application::FoundLanguageServer>>
find_language_servers(const std::vector<application::LanguageServerProbe> &probes,
                      const ServerSearchPlaces &places, const std::stop_token &stop);

// folders から探さない段を除く: UNC（`\\` で始まる段・`\\?\` も含む）と、GetDriveTypeW が
// DRIVE_REMOTE と答えるドライブ（割り当てたネットワークのドライブ）の段。切れた共有の時間切れが
// 候補と段の数だけ重なってワーカーの仕事を止めないため（#1945 のレビュー R1）。
[[nodiscard]] std::vector<std::wstring> local_places(std::vector<std::wstring> folders);

// 製品の置き場（環境変数を読むのはここだけ）。folders は PATH の絶対パスの段（`"` を外す・
// 相対の段と `;` を含む段は捨てる）に、よくある置き場（`%USERPROFILE%\.cargo\bin`・
// `%USERPROFILE%\go\bin`・`%USERPROFILE%\.dotnet\tools`・`%ProgramFiles%\LLVM\bin`・Visual Studio
// の
// `<版>\<種類>\VC\Tools\Llvm\x64\bin`（`%ProgramFiles%` と `%ProgramFiles(x86)%` の
// `Microsoft Visual Studio` の下を 2 段だけ列挙））を後ろに足したもの。npm_roots は PATH の段と
// `%APPDATA%\npm`（nvm・fnm・既定の nodejs は包みを node.exe の段の node_modules に置くので、PATH
// の段も root として見る）。どちらも local_places で UNC とネットワークのドライブの段を除く。
[[nodiscard]] ServerSearchPlaces system_search_places();

// PATH の値を段に分ける（`;` で区切り・`"` を外し・前後の空白を落とし・絶対パスの段だけ）。
[[nodiscard]] std::vector<std::wstring> absolute_path_folders(const std::wstring &value);
} // namespace nenenib::adapters::win32
