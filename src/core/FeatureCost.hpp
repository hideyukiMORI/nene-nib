#pragma once

#include "EditorFeature.hpp"
#include "FeatureStorage.hpp"
#include "UiMessage.hpp"

#include <cstdint>
#include <string_view>

namespace nenenib::core
{
// 機能の表の 1 行（ADR 0170 の決定 1・2・2026-11-18 の追補 1・3）。name は `features=` と
// `:set` の ASCII の名前。single_key_delta_us と bench は docs/quality/feature-costs.md に測った
// 行がある機能だけが持ち、bench が空の行は未測定（段の印を出さない・推測の段を出さない）。
// background は裏で CPU・メモリ・子プロセスを使う機能で、測った行があれば段を high に上げる。
struct FeatureCost
{
    EditorFeature feature;
    std::string_view name;
    bool default_on;
    FeatureStorage storage;
    std::uint16_t single_key_delta_us;
    std::string_view bench;
    bool background;
    UiMessage title;
    UiMessage description;
};
} // namespace nenenib::core
