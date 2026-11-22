#pragma once

#include "LanguageServerEvent.hpp"
#include "LanguageServerPort.hpp"
#include "LanguageServerSlot.hpp"
#include "ServerEvent.hpp"
#include "ServerFailure.hpp"
#include "ServerLaunch.hpp"
#include "ServerMessage.hpp"

#include <algorithm>
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
// 言語サーバーと話す口の替え玉（ADR 0160 の決定 9・2026-11-20 の追補）。子もパイプも持たない。
// start で受けた要求と席、send で受けた本文と席を順に覚え、stop と expire_retired と collect の
// 呼ばれた回数を数え、collect が返す出来事を先に仕込める（仕込んだ分は次の collect が全部を返し、
// その後は空）。席を名指さずに仕込んだ出来事は最後に start した席のもの。stop はその席に仕込んだ
// 出来事を捨てる（本物と同じく古い子の本文を次の子の前に混ぜない）。refuse は次の start を
// 断らせる。
class ScriptedLanguageServer final : public nenenib::application::LanguageServerPort
{
  public:
    [[nodiscard]] std::expected<void, nenenib::application::ServerFailure>
    start(nenenib::application::LanguageServerSlot slot,
          nenenib::application::ServerLaunch launch) override
    {
        launches_.push_back(std::move(launch));
        started_slots_.push_back(slot);
        last_slot_ = slot;
        const auto refusal = std::exchange(refusal_, std::nullopt);
        if (refusal.has_value())
        {
            return std::unexpected(refusal.value());
        }
        return {};
    }

    void send(nenenib::application::LanguageServerSlot slot, std::string body) override
    {
        sent_.push_back(std::move(body));
        sent_slots_.push_back(slot);
    }

    void stop(nenenib::application::LanguageServerSlot slot) override
    {
        ++stops_;
        stopped_slots_.push_back(slot);
        std::erase_if(events_, [slot](const nenenib::application::LanguageServerEvent &event)
                      { return event.slot == slot; });
    }

    void expire_retired() override
    {
        ++expirations_;
    }

    [[nodiscard]] std::vector<nenenib::application::LanguageServerEvent> collect() override
    {
        ++collects_;
        return std::exchange(events_, {});
    }

    // 次の collect が返す出来事を足す（数えない）。席を名指さなければ最後に start した席。
    void serve(nenenib::application::ServerEvent event)
    {
        serve(last_slot_, std::move(event));
    }
    void serve(nenenib::application::LanguageServerSlot slot,
               nenenib::application::ServerEvent event)
    {
        events_.push_back(
            nenenib::application::LanguageServerEvent{.slot = slot, .event = std::move(event)});
    }
    // 返す通知の台本（ADR 0161 の決定 8）。uri の文書の `publishDiagnostics` 1 通を次の collect
    // に足す。version が無ければ欄を書かない。diagnostics は `Diagnostic[]` の JSON。
    void publish(std::string_view uri, std::optional<std::uint64_t> version,
                 std::string_view diagnostics)
    {
        publish(last_slot_, uri, version, diagnostics);
    }
    void publish(nenenib::application::LanguageServerSlot slot, std::string_view uri,
                 std::optional<std::uint64_t> version, std::string_view diagnostics)
    {
        std::string body = R"({"jsonrpc":"2.0","method":"textDocument/publishDiagnostics",)";
        body += R"("params":{"uri":")" + std::string{uri} + "\"";
        if (version.has_value())
        {
            body += R"(,"version":)" + std::to_string(version.value());
        }
        body += R"(,"diagnostics":)" + std::string{diagnostics} + "}}";
        serve(slot, nenenib::application::ServerMessage{std::move(body)});
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
    // 送った要求の id（ADR 0190 の決定 11）。send で受けた本文のうち method の本文の最初の `"id":`
    // の数を順に返す（要求は id が method より前・`$/cancelRequest` は params の id）。
    [[nodiscard]] std::vector<std::int64_t> requests(std::string_view method) const
    {
        constexpr std::string_view key = R"("id":)";
        std::vector<std::int64_t> ids;
        for (const std::string &body : notifications(method))
        {
            const std::size_t at = body.find(key);
            if (at != std::string::npos)
            {
                ids.push_back(std::stoll(body.substr(at + key.size())));
            }
        }
        return ids;
    }
    // 取り消した要求の id（`$/cancelRequest` の params の id・決定 1）。
    [[nodiscard]] std::vector<std::int64_t> cancelled() const
    {
        return requests("$/cancelRequest");
    }
    // 要求 id に result（JSON の文字列）で答える本文を次の collect に足す（決定 11）。
    void answer(std::int64_t id, std::string_view result)
    {
        answer(last_slot_, id, result);
    }
    void answer(nenenib::application::LanguageServerSlot slot, std::int64_t id,
                std::string_view result)
    {
        serve(slot,
              nenenib::application::ServerMessage{R"({"jsonrpc":"2.0","id":)" + std::to_string(id) +
                                                  R"(,"result":)" + std::string{result} + "}"});
    }
    // 席 slot に送った本文だけを順に返す（2026-11-20 の追補）。
    [[nodiscard]] std::vector<std::string>
    sent_to(nenenib::application::LanguageServerSlot slot) const
    {
        std::vector<std::string> found;
        for (std::size_t index = 0; index < sent_.size(); ++index)
        {
            if (sent_slots_.at(index) == slot)
            {
                found.push_back(sent_.at(index));
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
    [[nodiscard]] const std::vector<nenenib::application::LanguageServerSlot> &
    started_slots() const noexcept
    {
        return started_slots_;
    }
    [[nodiscard]] const std::vector<nenenib::application::LanguageServerSlot> &
    stopped_slots() const noexcept
    {
        return stopped_slots_;
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
    std::vector<nenenib::application::LanguageServerSlot> sent_slots_;
    std::vector<nenenib::application::LanguageServerSlot> started_slots_;
    std::vector<nenenib::application::LanguageServerSlot> stopped_slots_;
    nenenib::application::LanguageServerSlot last_slot_ =
        nenenib::application::LanguageServerSlot::first;
    std::vector<nenenib::application::LanguageServerEvent> events_;
    std::optional<nenenib::application::ServerFailure> refusal_;
    std::size_t stops_ = 0;
    std::size_t expirations_ = 0;
    std::size_t collects_ = 0;
};
} // namespace nenenib::tests
