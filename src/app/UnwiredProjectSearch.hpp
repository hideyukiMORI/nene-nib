#pragma once

#include "ProjectSearchBatch.hpp"
#include "ProjectSearchPort.hpp"
#include "ProjectSearchRequest.hpp"

#include <cstdint>
#include <vector>

namespace nenenib::app
{
// 全文検索の adapter（worker の上で木を歩いて塊を照合する）は段 G3（#1003・ADR 0167 の決定 4）で
// 置く。それまでの合成ルートは要求を受けて何もしない口を結ぶ（面は「探しています… 0 / 0
// ファイル」のまま）。段 G2 だけを main へ入れない（G1〜G3 を同じまとめ枝で入れる・設計席の決め）。
class UnwiredProjectSearch final : public nenenib::application::ProjectSearchPort
{
  public:
    void request(nenenib::application::ProjectSearchRequest /*request*/) override {}
    void cancel(std::uint64_t /*ticket*/) override {}
    [[nodiscard]] std::vector<nenenib::application::ProjectSearchBatch> collect() override
    {
        return {};
    }
};
} // namespace nenenib::app
