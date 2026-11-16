#pragma once

#include "EditHistory.hpp"
#include "TextBuffer.hpp"

#include <optional>

namespace nenenib::application
{
// 文書の Vim の eol に当たる旗（ADR 0188 の決定 1・3）。文書ごとの値で、設定にもタブの一覧にも
// 入れない。opened は決めた時点（開いた・通常モードから Vim のモードへ入った）に本文が空でなく
// 最後が改行で終わったか。lowered は全部の行を消して旗を下ろした直後の履歴で、その単位が履歴の
// 位置より前に残っている間だけ旗は下りている（undo で消す前に戻れば旗も戻り、戻した後に別の
// 編集を積めば下ろした単位は消えて旗は立ったまま）。どの欄も単独で妥当な値なので公開
// aggregate（CPP-003）。
struct VimEndOfLine
{
    bool opened = false;
    std::optional<core::EditHistory> lowered = std::nullopt;
};

// 開いた・Vim のモードへ入ったときの旗（決定 1）。決める口はこの 1 本。
[[nodiscard]] VimEndOfLine vim_end_of_line_decided(const core::TextBuffer &text);

// 履歴が history のときに旗が立っているか。
[[nodiscard]] bool vim_end_of_line_up(const VimEndOfLine &flag, const core::EditHistory &history);
} // namespace nenenib::application
