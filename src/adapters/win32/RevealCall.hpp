#pragma once

#include "RevealFailure.hpp"

#include <windows.h>

#include <expected>
#include <string>
#include <string_view>

namespace nenenib::adapters::win32
{
// エクスプローラーで場所を開く呼び出しの前後（ADR 0078 の追補 #1790）。シェルを呼ぶ直前までと
// 結果の読み替えは純関数で、試験は本物のエクスプローラーを開かずにここを試す。

// シェルに渡す経路（UTF-16）。UTF-8 から変換できない・空なら failed。`\?\` は付けない（シェルの
// 名前の解釈は付けた形を受けない）。
[[nodiscard]] std::expected<std::wstring, application::RevealFailure>
reveal_target(std::string_view utf8);

// シェルの失敗（HRESULT）→ 理由。ファイル・フォルダ・ネットワークの場所が無いは not_found、ほかは
// failed。
[[nodiscard]] application::RevealFailure reveal_failure_of(HRESULT result) noexcept;

// 本物のシェルを呼ぶ（SHParseDisplayName と SHOpenFolderAndSelectItems）。呼んだスレッドで STA を
// 作って壊す。ワーカーの仕事の中だけが呼ぶ（UI スレッドを止めない）。
[[nodiscard]] std::expected<void, application::RevealFailure>
reveal_in_shell(const std::wstring &path);
} // namespace nenenib::adapters::win32
