#pragma once

#include "FilePath.hpp"

#include <cstddef>
#include <optional>
#include <string>

namespace nenenib::application
{
// 席（命令の行・root）ごとの失敗の控え（ADR 0160 の決定 6 の 2026-11-20 の追補・段 S2・2026-11-24
// の追補で言語ごとから席の鍵ごとへ・#1952）。command は server_command_key で正規化した命令の行、
// root は席を起こした作業フォルダ（比べは core の same_folder）、failures は続けて失敗した回数
// （`initialized` まで届くか `:set lsp` で 0 に戻る）、given_up は上限を超えて止まったこと。同じ
// 席を共有する言語（c と cpp など）は同じ控えを読み、同じ命令でも root の違う席は別の控えを持つ。
// ある席の誤りでほかの席を止めない。公開 aggregate（CPP-003）。
struct LanguageServerTrouble
{
    std::string command;
    std::optional<core::FilePath> root;
    std::size_t failures = 0;
    bool given_up = false;
};
} // namespace nenenib::application
