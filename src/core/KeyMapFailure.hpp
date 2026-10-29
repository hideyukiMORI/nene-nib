#pragma once

#include <cstdint>

namespace nenenib::core
{
// `map` の行と表が作れない理由（ADR 0176 の決定 1・ADR 0114 の厳密な読み）。どれも設定の全体を
// 拒む側で、呼び手は理由で道を分けない（知らせの文言だけが読む）。
enum class KeyMapFailure : std::uint8_t
{
    // `map.<modes>.<lhs>` の形でない鍵。
    malformed_key,
    // モードの文字が空・`n i v x o` の外・重複。
    bad_modes,
    // 記法の表に無い `<名前>` か、正しくない UTF-8。
    unknown_key,
    // 左辺か右辺が 0 打鍵。
    empty_keys,
    // 左辺が 4 打鍵以上。
    long_lhs,
    // `mapleader` が 1 打鍵でない。
    bad_leader,
    // 設定の行に書くと読み戻せない（左辺の `=`・端の空白・改行・`map` の右辺の先頭の `!`）。
    unwritable
};
} // namespace nenenib::core
