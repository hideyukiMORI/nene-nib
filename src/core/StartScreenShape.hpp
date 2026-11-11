#pragma once

#include <cstddef>

namespace nenenib::core
{
// 起動画面の節の形（ADR 0172 の決定 2・5）。recent と bookmarks
// は最近使ったファイルとブックマークの 行の数（0 なら節ごと出さない）、empty_note
// は「まだ履歴がありません」の 1 行を出すか（履歴と
// ブックマークを読み終えて、どちらも空のとき）。配置（start_screen_layout）だけが読む。公開
// aggregate（CPP-003）。
struct StartScreenShape
{
    std::size_t recent = 0;
    std::size_t bookmarks = 0;
    bool empty_note = false;
};
} // namespace nenenib::core
