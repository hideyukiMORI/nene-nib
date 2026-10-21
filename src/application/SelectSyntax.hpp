#pragma once

namespace nenenib::application
{
// 現在の文書の構文の色を入れ切りする（ADR 0156 の追補・#829）。Ex の `:syntax on` `:syntax off`
// が送る。設定にもセッションにも保存しない。使う人の操作なので、歩きを終え 1 意図の知らせを消す側
// （SelectLanguage と同じ）。
struct SelectSyntax
{
    bool enabled;
};
} // namespace nenenib::application
