#pragma once

#include "Language.hpp"

namespace nenenib::application
{
// 現在の文書の言語を選ぶ（ADR 0154 の決定 4・5）。Ex の `:set filetype=` `:set ft=` が送る。
// 設定にもセッションにも保存しない。使う人の操作なので、歩きを終え 1 意図の知らせを消す側。
struct SelectLanguage
{
    core::Language language;
};
} // namespace nenenib::application
