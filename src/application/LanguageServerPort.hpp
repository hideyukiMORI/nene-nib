#pragma once

#include "LanguageServerEvent.hpp"
#include "LanguageServerSlot.hpp"
#include "ServerEvent.hpp"
#include "ServerFailure.hpp"
#include "ServerLaunch.hpp"

#include <expected>
#include <string>
#include <vector>

namespace nenenib::application
{
// 言語サーバーと話す口（ADR 0160 の決定 1・追補）。プロセス・パイプ・待ちの handle は adapters
// に閉じ、ここには出さない（ARC-007）。UI スレッドだけが呼ぶ。port は 1 本で、席の番号
// （LanguageServerSlot）で子を指す（2026-11-20 の追補・段 S2）。start はその席に子を起こして
// すぐ戻る（その席に前に起きていた子は先に stop する）。起こせなければ失敗の値。send はその席の
// 子へ JSON-RPC の本文 1 通を送る（見出しは adapter が付ける・送れなければその席の ServerFailed）。
// stop はその席の子へ shutdown も exit も送らず（呼ぶ側の役）、パイプを閉じて子を待たずに戻る。
// その席のまだ collect していない出来事は列から外して届いた順に返す（古い子の本文を次の子の前に
// 混ぜない・ほかの席の出来事は残す）。controller は返った出来事を記録にだけ写し、ほかには使わない
// （止める前の標準エラーと logMessage を失わない・2026-11-22 の追補の #1950 のレビュー M1）。
// collect は全部の席のたまった出来事を、出した席を添えて届いた順に受け取る（無ければ
// 空）。届いた合図は既存の work_message の道（新しい合図は足さない）。expire_retired は stop の後に
// まだ終わっていない退く子を全部の席の分まとめて片付ける（追補・controller が LanguageServerTick で
// 呼ぶ・待たない）。
class LanguageServerPort
{
  public:
    LanguageServerPort() = default;
    virtual ~LanguageServerPort() = default;
    LanguageServerPort(const LanguageServerPort &) = delete;
    LanguageServerPort(LanguageServerPort &&) = delete;
    LanguageServerPort &operator=(const LanguageServerPort &) = delete;
    LanguageServerPort &operator=(LanguageServerPort &&) = delete;

    [[nodiscard]] virtual std::expected<void, ServerFailure> start(LanguageServerSlot slot,
                                                                   ServerLaunch launch) = 0;
    virtual void send(LanguageServerSlot slot, std::string body) = 0;
    [[nodiscard]] virtual std::vector<ServerEvent> stop(LanguageServerSlot slot) = 0;
    virtual void expire_retired() = 0;
    [[nodiscard]] virtual std::vector<LanguageServerEvent> collect() = 0;
};
} // namespace nenenib::application
