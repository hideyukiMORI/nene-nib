#pragma once

#include "AutosaveFailure.hpp"
#include "AutosaveId.hpp"
#include "AutosavePort.hpp"
#include "AutosaveRemnant.hpp"
#include "AutosaveSnapshot.hpp"

#include <cstddef>
#include <expected>
#include <vector>

namespace nenenib::adapters::win32
{
// 何もしない自動退避の口（ADR 0137・#515 の 1 工程目）。合成ルートを結ぶためだけに置き、2 工程目
// で置き場に書く本物に置き換える。頼みは捨て、失敗も残骸も無い。
class NullAutosaveAdapter final : public application::AutosavePort
{
  public:
    void store(application::AutosaveSnapshot) override {}
    void discard(application::AutosaveId) override {}
    [[nodiscard]] std::vector<application::AutosaveFailure> collect() override
    {
        return {};
    }
    void clear() override {}
    [[nodiscard]] std::expected<std::vector<application::AutosaveRemnant>,
                                application::AutosaveFailure>
    remnants() override
    {
        return std::vector<application::AutosaveRemnant>{};
    }
    [[nodiscard]] std::expected<std::vector<std::byte>, application::AutosaveFailure>
    read(const application::AutosaveRemnant &) override
    {
        return std::unexpected(application::AutosaveFailure::unreadable);
    }
    void forget(const application::AutosaveRemnant &) override {}
};
} // namespace nenenib::adapters::win32
