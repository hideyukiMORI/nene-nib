#pragma once

#include <cstddef>
#include <string>

namespace nenenib::application
{
// 席の鍵の命令の行ごとの失敗の控え（ADR 0160 の決定 6 の 2026-11-20 の追補・段 S2・2026-11-24 の
// 追補で言語ごとから命令の行ごとへ・#1952）。command は server_command_key で正規化した命令の行、
// failures は続けて失敗した回数（`initialized` まで届くか `:set lsp` で 0 に戻る）、given_up
// は上限を 超えて止まったこと。同じ命令の行を共有する言語（c と cpp
// など）は同じ控えを読む。ある命令の誤りで ほかの命令のサーバーを止めない。公開
// aggregate（CPP-003）。
struct LanguageServerTrouble
{
    std::string command;
    std::size_t failures = 0;
    bool given_up = false;
};
} // namespace nenenib::application
