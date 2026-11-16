#pragma once

#include "TextBuffer.hpp"

#include <cstdint>

namespace nenenib::application
{
// 文書の Vim の eol に当たる旗（ADR 0188 の決定 1・3）。文書ごとの値で、設定にもタブの一覧にも
// 入れない。up は決めた時点に本文が空でなく最後が改行で終わったか。entry は決めたときの「通常
// モードから Vim のモードへ入った回数」で、置いた束を広げるときに今の回数と違えば（決めた後に
// 通常モードを通った）本文から決め直す。全部の行を消しても旗は下ろさない（空で旗が立っている
// 文書は、Vim のモードの編集で空でなくなったときに最後の改行を足し直す）。どの欄も単独で妥当な
// 値なので公開 aggregate（CPP-003）。
struct VimEndOfLine
{
    bool up = false;
    std::uint64_t entry = 0;
};

// 開いたとき・Vim のモードへ入った後に文書が最初に Vim のモードで見えるときの旗（決定 1）。
// 決める口はこの 1 本。
[[nodiscard]] VimEndOfLine vim_end_of_line_decided(const core::TextBuffer &text,
                                                   std::uint64_t entry);
} // namespace nenenib::application
