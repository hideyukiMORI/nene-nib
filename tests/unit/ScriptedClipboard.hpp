#pragma once

#include "ClipboardFailure.hpp"
#include "ClipboardPort.hpp"

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

    [[nodiscard]] std::expected<void, ClipboardFailure> write(std::string_view utf8) override
    {
        if (!writable_)
        {
            return std::unexpected(ClipboardFailure::write_failed);
        }
        content_ = std::string(utf8);
        return {};
    }

    [[nodiscard]] Content read() override
    {
        return content_;
    }

  private:
    Content content_{std::unexpected(ClipboardFailure::empty)};
    bool writable_ = true;
};
} // namespace nenenib::tests
