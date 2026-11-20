#pragma once

#include <cstdint>

namespace nenenib::application
{
// 言語サーバーを起こせない・話せない理由の閉じた集合（ADR 0160 の決定 6）。例外にしない。
// not_found は実行ファイルが無い、cannot_start は在るが起こせない（Job Object を含む）、
// pipe_failed は標準入出力のパイプを作れない、write_failed は送れない（終わった後の send も）、
// read_failed は受け取れない・枠の見出しが壊れている、message_too_long は本文が上限（16 MiB）
// を超える、header_too_long は見出しの終わり（空行）が上限（8 KiB）の内に来ない（core の
// LspFrameFailure の同じ名前の値・ADR 0160 の決定 3）。not_absolute は命令の実行ファイルが絶対
// パスでない、script_refused は `.cmd` / `.bat`（cmd.exe を挟むので起こさない）で、どちらも起こす
// 前に断る（ADR 0160 の 2026-11-20 の追補・#1897）。
enum class ServerFailure : std::uint8_t
{
    not_found,
    cannot_start,
    pipe_failed,
    write_failed,
    read_failed,
    message_too_long,
    header_too_long,
    not_absolute,
    script_refused
};
} // namespace nenenib::application
