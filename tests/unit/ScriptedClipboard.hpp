#pragma once

#include "ClipboardFailure.hpp"
#include "ClipboardPort.hpp"

#include <cstddef>
#include <expected>
#include <string>
#include <string_view>
#include <utility>

namespace nenenib::tests
{
using nenenib::application::ClipboardFailure;
using nenenib::application::ClipboardPort;
using Content = std::expected<std::string, ClipboardFailure>;

// 偽のクリップボード。OS を呼ばずに「置けた」「置けない」「空」を作り分ける（ARC-007）。
class ScriptedClipboard final : public ClipboardPort
{
  public:
    void hold(Content content)
    {
        content_ = std::move(content);
    }

    void refuse_writes()
    {
        writable_ = false;
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

    [[nodiscard]] std::expected<void, ClipboardFailure> write(std::string_view utf8) override
    {
        ++writes_;
        if (!writable_)
        {
            return std::unexpected(ClipboardFailure::write_failed);
        }
        content_ = std::string(utf8);
        return {};
    }

    [[nodiscard]] Content read() override
    {
        ++reads_;
        return content_;
    }

  private:
    Content content_{std::unexpected(ClipboardFailure::empty)};
    bool writable_ = true;
    std::size_t writes_ = 0;
    std::size_t reads_ = 0;
};
} // namespace nenenib::tests
