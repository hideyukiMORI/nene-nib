#pragma once

#include "FilePath.hpp"
#include "HeldLanguageRoot.hpp"
#include "Language.hpp"
#include "LanguageRootAnswer.hpp"
#include "LanguageRootRequest.hpp"
#include "LanguageServerLink.hpp"
#include "LanguageServerLog.hpp"
#include "LanguageServerSlot.hpp"
#include "LanguageServerTrouble.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <tuple>
#include <vector>

namespace nenenib::application
{
// 印を探した答えを覚える（フォルダ・言語）の数の上限（段 S3・設定に出さない定数）。
inline constexpr std::size_t language_root_memory = 64;

// controller が持つ言語サーバーの全部の席（ADR 0160 の 2026-11-20 の追補・段 S2）。links は席ごとの
// つながり（slot_index で引く）、awake は idle でない席の印（席の番号のビット・立てるのは
// start_language_server・消すのは stop_language_server の 2 か所だけ・0 ならオフの道は欄を 1 つ
// 読むだけで抜ける）、next_id は要求の通し番号（席をまたいで一意）、troubles は言語ごとの失敗の
// 控え（root の違う同じ言語の席は数を共有する）、considered はアクティブな文書を受ける席が無い
// ときに起こすかを決めた（言語・パス・設定の root）（同じ文書では打鍵ごとに設定を引き直さない・
// 設定の root が変われば決め直す・どの席を止めても・root を探した答えが届いても空に戻る）、retired
// は stop した子が adapter に残っているかもしれないこと（次の tick で expire_retired を呼ぶ）、uses
// は used の 印の通し番号、idle は受ける席が無いときに読む空のつながり（能力も文書も無い）。roots
// は印を 探した答えの覚え（（フォルダ・言語）ごとに 1 つ・窓を閉じるまで・上限 language_root_memory
// で 古い方から捨てる・段 S3）、seeking は頼んでまだ答えの来ない（フォルダ・言語）（同じ頼みを 2 回
// 出さない・空なら collect を呼ばない）。answers は roots を書き換えた回数（held の鍵）、held は
// アクティブな文書について reconcile が 1 回決めた root（#1944 のレビュー L1）、log は席ごとの
// 標準エラーと知らせの記録（`:LspLog`・席を止めても残す・ADR 0160 の 2026-11-22 の追補）。公開
// aggregate（CPP-003）。
struct LanguageServers
{
    std::array<LanguageServerLink, language_server_slot_limit> links = {};
    std::uint8_t awake = 0;
    std::int64_t next_id = 1;
    std::vector<LanguageServerTrouble> troubles = {};
    std::optional<
        std::tuple<core::Language, std::optional<core::FilePath>, std::optional<core::FilePath>>>
        considered = std::nullopt;
    bool retired = false;
    std::uint64_t uses = 0;
    LanguageServerLink idle = {};
    std::vector<LanguageRootAnswer> roots = {};
    std::vector<LanguageRootRequest> seeking = {};
    std::uint64_t answers = 0;
    std::optional<HeldLanguageRoot> held = std::nullopt;
    LanguageServerLog log = {};
};
} // namespace nenenib::application
