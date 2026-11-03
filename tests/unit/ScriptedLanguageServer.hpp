#pragma once

#include "LanguageServerPort.hpp"
#include "ServerEvent.hpp"
#include "ServerFailure.hpp"
#include "ServerLaunch.hpp"
#include "ServerMessage.hpp"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace nenenib::tests
{
// 言語サーバーと話す口の替え玉（ADR 0160 の決定 9）。子もパイプも持たない。start で受けた要求と
// send で受けた本文を順に覚え、stop と expire_retired と collect の呼ばれた回数を数え、collect
// が返す出来事を先に仕込める（仕込んだ分は次の collect が全部を返し、その後は空）。stop は
// 仕込んだ出来事を捨てる（本物と同じく古い子の本文を次の子の前に混ぜない）。refuse は次の start
// を断らせる。
class ScriptedLanguageServer final : public nenenib::application::LanguageServerPort
{
  public:
    [[nodiscard]] std::expected<void, nenenib::application::ServerFailure>
    start(nenenib::application::ServerLaunch launch) override
    {
        launches_.push_back(std::move(launch));
        const auto refusal = std::exchange(refusal_, std::nullopt);
        if (refusal.has_value())
        {
            return std::unexpected(refusal.value());
        }
        return {};
    }

    void send(std::string body) override
    {
        sent_.push_back(std::move(body));
    }

    void stop() override
    {
        ++stops_;
        events_.clear();
    }

    void expire_retired() override
    {
        ++expirations_;
    }

    [[nodiscard]] std::vector<nenenib::application::ServerEvent> collect() override
    {
        ++collects_;
        return std::exchange(events_, {});
    }

    // 次の collect が返す出来事を足す（数えない）。
    void serve(nenenib::application::ServerEvent event)
    {
        events_.push_back(std::move(event));
    }
    // 返す通知の台本（ADR 0161 の決定 8）。uri の文書の `publishDiagnostics` 1 通を次の collect
    // に足す。version が無ければ欄を書かない。diagnostics は `Diagnostic[]` の JSON。
    void publish(std::string_view uri, std::optional<std::uint64_t> version,
                 std::string_view diagnostics)
    {
        std::string body = R"({"jsonrpc":"2.0","method":"textDocument/publishDiagnostics",)";
        body += R"("params":{"uri":")" + std::string{uri} + "\"";
        if (version.has_value())
        {
            body += R"(,"version":)" + std::to_string(version.value());
        }
        body += R"(,"diagnostics":)" + std::string{diagnostics} + "}}";
        serve(nenenib::application::ServerMessage{std::move(body)});
    }
    // 受けた通知の記録（決定 8）。send で受けた本文のうち method の通知を順に返す。
    [[nodiscard]] std::vector<std::string> notifications(std::string_view method) const
    {
        const std::string key = R"("method":")" + std::string{method} + "\"";
        std::vector<std::string> found;
        for (const std::string &body : sent_)
        {
            if (body.find(key) != std::string::npos)
            {
                found.push_back(body);
            }
        }
        return found;
    }
    // 次の start を failure で断らせる（1 回だけ）。
    void refuse(nenenib::application::ServerFailure failure)
    {
        refusal_ = failure;
    }

    [[nodiscard]] const std::vector<nenenib::application::ServerLaunch> &launches() const noexcept
    {
        return launches_;
    }
    [[nodiscard]] const std::vector<std::string> &sent() const noexcept
    {
        return sent_;
    }
    [[nodiscard]] std::size_t stops() const noexcept
    {
        return stops_;
    }
    [[nodiscard]] std::size_t expirations() const noexcept
    {
        return expirations_;
    }
    [[nodiscard]] std::size_t collects() const noexcept
    {
        return collects_;
    }
    // どの口でも呼ばれた回数の和（verify_quiet_paths）。
    [[nodiscard]] std::size_t touches() const noexcept
    {
        return launches_.size() + sent_.size() + stops_ + expirations_ + collects_;
    }

  private:
    std::vector<nenenib::application::ServerLaunch> launches_;
    std::vector<std::string> sent_;
    std::vector<nenenib::application::ServerEvent> events_;
    std::optional<nenenib::application::ServerFailure> refusal_;
    std::size_t stops_ = 0;
    std::size_t expirations_ = 0;
    std::size_t collects_ = 0;
};
} // namespace nenenib::tests
