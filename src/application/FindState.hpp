#pragma once

#include "FindPhase.hpp"
#include "Offset.hpp"
#include "OffsetRange.hpp"
#include "TextBuffer.hpp"
#include "VimPattern.hpp"

#include <optional>
#include <string>

namespace nenenib::application
{
// 通常モードの検索の状態（ADR 0174 の決定 3）。窓に 1 つで、保存しない。pattern は最後に「検索: 」
// で確定した語（literal に逃がす前の打ったまま）で、Ctrl+F が開くときに置く。照合と最後の検索は Vim
// の last_search の 1 つ（逃がした形・決定 5）で、ここは打ったままの語だけを持つ。置換後の語
// （replacement）は N1b が足す。どの欄も単独で妥当な公開 aggregate。
struct FindState
{
    std::string pattern;
    FindPhase phase = FindPhase::find;
};

// 通常モードの「今の当たり」（ADR 0174 の決定 7・N1b の置換の 1 件が使う）。
// キャレットを含む当たり（始まり <= キャレット < 終わり）があればそれ、無ければ
// キャレットから前向きの次の当たり（確定の検索と同じ vim_find_match の 1 本・
// 折り返しも同じ）。数え方は vim_line_matches の 1 本で、長さ 0 の一致は当たりに
// しない。当たりが無ければ値なし。
[[nodiscard]] std::optional<core::OffsetRange> current_find_match(const core::TextBuffer &text,
                                                                  const core::VimPattern &pattern,
                                                                  core::Offset caret);
} // namespace nenenib::application
