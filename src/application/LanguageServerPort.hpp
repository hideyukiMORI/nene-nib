#pragma once

#include "ServerEvent.hpp"
#include "ServerFailure.hpp"
#include "ServerLaunch.hpp"

#include <expected>
#include <string>
#include <vector>

namespace nenenib::application
{
// 言語サーバーと話す口（ADR 0160 の決定 1・追補）。プロセス・パイプ・待ちの handle は adapters
// に閉じ、ここには出さない（ARC-007）。UI スレッドだけが呼ぶ。start
// は子を起こしてすぐ戻る（前に起きていた子は先に stop する）。起こせなければ失敗の値。send は
// JSON-RPC の本文 1 通を送る（見出しは adapter が付ける・送れなければ ServerFailed）。stop は
// shutdown も exit も送らず（呼ぶ側の役）、パイプを閉じて子を待たずに戻る。まだ collect
// していない出来事も捨てる（古い子の本文を次の子の前に混ぜない）。collect
// はたまった出来事を全部受け取る（無ければ空）。届いた合図は既存の work_message の道。
class LanguageServerPort
{
  public:
    LanguageServerPort() = default;
    virtual ~LanguageServerPort() = default;
    LanguageServerPort(const LanguageServerPort &) = delete;
    LanguageServerPort(LanguageServerPort &&) = delete;
    LanguageServerPort &operator=(const LanguageServerPort &) = delete;
    LanguageServerPort &operator=(LanguageServerPort &&) = delete;

    [[nodiscard]] virtual std::expected<void, ServerFailure> start(ServerLaunch launch) = 0;
    virtual void send(std::string body) = 0;
    virtual void stop() = 0;
    [[nodiscard]] virtual std::vector<ServerEvent> collect() = 0;
};
} // namespace nenenib::application
