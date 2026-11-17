#pragma once

#include <string>

namespace nenenib::adapters::win32
{
// 経路を OS の規則で絶対の形にする（相対・`.`・`..`・`/`・ドライブの相対・最後の段の末尾の空白と
// ドットを畳む）。拡張子つきの予約名（`nul.txt` `con.md` `com1.txt` など）を装置に写さず、どの
// Windows でも普通のファイルの経路にする（Issue #1805・ADR 0010 の追補・Windows 11 の振る舞い）。
// GetFullPathNameW が最後の段を装置（`\\.\nul`）に写し、かつ最後の段が装置の名前そのもの
// （`nul` `NUL.`）でないときだけ、フォルダの部分を絶対にして最後の段を後ろに付け直す。予約名の
// 一覧は持たない（写したかと装置の名前は OS の答えで見る）。入力が `\\?\` か `\\.\` で始まれば
// OS の答えのまま。失敗は空。起動引数・`:w` の名前（absolute_file_path）と native_path が共用する。
[[nodiscard]] std::wstring full_path(const std::wstring &path);
// Win32 のファイルの関数へ渡す直前の経路の形（Issue #498・#1805）。full_path で絶対にして、
// 長さに依らず必ず `\\?\`（UNC は `\\?\UNC\`）を付ける。この形は Win32 が装置の名前を解釈せず、
// レジストリの LongPathsEnabled に依らず 32,767 文字まで通る。既に `\\?\` か `\\.\` で始まる
// 経路と、絶対にできない経路はそのまま返す（後の Win32 の呼び出しが失敗する）。表示と一覧に
// 書く経路はこの形にしない。adapters/win32 のファイルの Win32 呼び出しは全部この 1 本を通す。
[[nodiscard]] std::wstring native_path(const std::wstring &path);
} // namespace nenenib::adapters::win32
