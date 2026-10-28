#pragma once

#include "Language.hpp"
#include "LanguageServerPhase.hpp"
#include "LspPositionEncoding.hpp"
#include "PendingLspRequest.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace nenenib::application
{
// 自動の再起動の上限（ADR 0160 の決定 6・L-3）。超えたら 1 行知らせて止まり、`:set lsp` で手で
// 再開する。
inline constexpr std::size_t language_server_restart_limit = 3;
// 答えを待つ要求を古いとみなす tick の回数（決定 6）。tick は 30 秒ごとなので、2 回の tick を
// 越えた要求は必ず 30 秒を超えている（1 回だと送った直後の tick で捨てる）。
inline constexpr std::size_t language_server_stale_ticks = 2;

// controller が持つ言語サーバーとのつながりの控え（ADR 0160 の決定 5・6）。同時に起きているのは
// 1 本で、language はその子の言語（idle なら空）。next_id は要求の通し番号、pending は答えを
// 待っている要求、encoding はサーバーが選んだ位置の単位、failures は続けて失敗した回数（`:set
// lsp` が 0 に戻す）、given_up は上限を超えて止まったこと、retired は stop した子が adapter に
// 残っているかもしれないこと（次の tick で expire_retired を呼ぶ）。considered は子が居ない
// ときに起こすかを決めた言語（同じ言語では打鍵ごとに設定を引き直さない・止めると空に戻る）、
// generation は stop のたびに進む数（止めた後の同じ集めの残りを読まない）。公開 aggregate
// （CPP-003）。
struct LanguageServerLink
{
    LanguageServerPhase phase = LanguageServerPhase::idle;
    std::optional<core::Language> language = std::nullopt;
    std::int64_t next_id = 1;
    std::vector<PendingLspRequest> pending = {};
    core::LspPositionEncoding encoding = core::LspPositionEncoding::utf16;
    std::size_t failures = 0;
    bool given_up = false;
    bool retired = false;
    std::optional<core::Language> considered = std::nullopt;
    std::uint64_t generation = 0;
};
} // namespace nenenib::application
