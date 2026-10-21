#pragma once

#include "ClipboardFailure.hpp"
#include "ClipboardPort.hpp"

#include <cstddef>
#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace nenenib::tests
{
using nenenib::application::ClipboardFailure;
using nenenib::application::ClipboardPort;
using Content = std::expected<std::string, ClipboardFailure>;
using HtmlContent = std::expected<std::optional<std::string>, ClipboardFailure>;

// 偽のクリップボード。OS を呼ばずに「置けた」「置けない」「空」を作り分ける（ARC-007）。
class ScriptedClipboard final : public ClipboardPort
{
  public:
    void hold(Content content)
    {
        content_ = std::move(content);
    }

    // HTML（CF_HTML）の fragment を置く。値なしは「HTML が無い」（ADR 0157 の決定 4）。
    void hold_html(HtmlContent html)
    {
        html_ = std::move(html);
    }

    // 書き込みを断る。断る理由は adapter が返しうる値から選ぶ（ADR 0134）。
    void refuse_writes(ClipboardFailure failure = ClipboardFailure::write_failed)
    {
        refusal_ = failure;
    }

    // 書かれた回数（置けなかった試みも数える）と読まれた回数（ADR 0051 の決定 9）。
    [[nodiscard]] std::size_t writes() const noexcept
    {
        return writes_;
    }

    [[nodiscard]] std::size_t reads() const noexcept
    {
        return reads_;
    }

    // HTML が読まれた回数（ADR 0157 の決定 9・verify_quiet_paths）。
    [[nodiscard]] std::size_t html_reads() const noexcept
    {
        return html_reads_;
    }

    [[nodiscard]] std::expected<void, ClipboardFailure> write(std::string_view utf8) override
    {
        ++writes_;
        if (refusal_.has_value())
        {
            return std::unexpected(refusal_.value());
        }
        content_ = std::string(utf8);
        return {};
    }

    [[nodiscard]] Content read() override
    {
        ++reads_;
        return content_;
    }

    [[nodiscard]] HtmlContent html() override
    {
        ++html_reads_;
        return html_;
    }

  private:
    Content content_{std::unexpected(ClipboardFailure::empty)};
    std::optional<ClipboardFailure> refusal_;
    std::size_t writes_ = 0;
    std::size_t reads_ = 0;
    HtmlContent html_{std::nullopt};
    std::size_t html_reads_ = 0;
};
} // namespace nenenib::tests
