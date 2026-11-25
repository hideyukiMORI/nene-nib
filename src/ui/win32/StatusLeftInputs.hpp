#pragma once

namespace nenenib::ui::win32
{
// ステータスバーの左側の分かれ目（status_left_of）が読む frame の値（#2025）。frame を作らずに
// 優先の式を試すため、値の有無だけを分けて持つ。
struct StatusLeftInputs
{
    // Ctrl+P の一覧が開いている（`:` の入力も知らせも診断の文もトグルの側へ落ちる）。
    bool palette = false;
    // 入力行（`:`・検索）が開いている。
    bool command_line = false;
    // 1 行の知らせ（command_message）がある。
    bool message = false;
    // キャレットの上の診断の文がある（ADR 0161 の 2026-11-24 の追補）。
    bool diagnostic = false;
    // マクロを録画している。
    bool recording = false;
};
} // namespace nenenib::ui::win32
