#pragma once

#include "FilePath.hpp"
#include "Language.hpp"
#include "LanguageServerDocument.hpp"
#include "LanguageServerPhase.hpp"
#include "LspPositionEncoding.hpp"
#include "PendingLspRequest.hpp"
#include "ServerAbilities.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace nenenib::application
{
// 自動の再起動の上限（ADR 0160 の決定 6・L-3）。超えたら 1 行知らせて止まり、`:set lsp` で手で
// 再開する。
inline constexpr std::size_t language_server_restart_limit = 3;
// 答えを待つ要求を古いとみなす tick の回数（決定 6）。tick は 30 秒ごとなので、2 回の tick を
// 越えた要求は必ず 30 秒を超えている（1 回だと送った直後の tick で捨てる）。
inline constexpr std::size_t language_server_stale_ticks = 2;
// 言語サーバーへ送る文書の大きさの上限（ADR 0161 の決定 1・L-4・設定に出さない定数）。これを
// 超える本文は `didOpen` せず、編集で超えたら `didClose` する。
inline constexpr std::size_t language_server_document_limit = std::size_t{4} * 1024 * 1024;
// 1 文書で覚えておく送った版の数（ADR 0161 の決定 3）。これより古い版で届いた診断は置かずに
// 捨てる（サーバーは新しい版の診断を続けて送る）。
inline constexpr std::size_t language_server_kept_revisions = 16;

// controller が持つ言語サーバー 1 席とのつながりの控え（ADR 0160 の決定 5・6・2026-11-20 の追補）。
// 席ごとに 1 つで、language はその子の言語（idle なら空）。pending は答えを待っている要求（id
// の表は 席ごと・id は席をまたいで一意）、encoding はサーバーが選んだ位置の単位、generation は stop
// の たびに進む数（止めた後の同じ集めの残りを読まない）。root
// はその子を起こした作業フォルダ（自動の 再起動は同じ root で起こす）、configured_root
// は起こしたときの設定の root（設定の root があれば
// それが同じ席だけが文書を受ける・無ければ同じ言語の席が受ける・段 S2 の root の鍵）、root_uri は
// root の URI（`initialize` の rootUri・ADR 0161 の決定 1）。documents は `didOpen` した文書
// （running の間だけ・止めると空）、abilities は `initialize` の答えから読んだサーバーの能力
// （ADR 0190 の決定 3・止めると空）、used
// はアクティブな文書で最後に使った順の印（大きいほど新しい・ 席が満ちたら最も小さい席を譲る）。公開
// aggregate（CPP-003）。
struct LanguageServerLink
{
    LanguageServerPhase phase = LanguageServerPhase::idle;
    std::optional<core::Language> language = std::nullopt;
    std::vector<PendingLspRequest> pending = {};
    core::LspPositionEncoding encoding = core::LspPositionEncoding::utf16;
    std::uint64_t generation = 0;
    std::optional<core::FilePath> root = std::nullopt;
    std::optional<core::FilePath> configured_root = std::nullopt;
    std::string root_uri = {};
    std::vector<LanguageServerDocument> documents = {};
    ServerAbilities abilities = {};
    std::uint64_t used = 0;
};
} // namespace nenenib::application
