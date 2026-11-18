#pragma once

#include <cstdint>

namespace nenenib::core
{
// 機能のオン・オフをどこに持つか（ADR 0170 の 2026-11-18 の追補 1）。
// listed: 設定の `features=` の名前の列（既定オフはオンのとき名前、既定オンはオフのとき
//         `no_<名前>`）。
// own_key: 自分の鍵（`wrap` `guide` `autoindent` `explorer` `lsp`・決定 3）。
// always_on: 今は切れない（切る道を足すと打鍵の道に分岐が増える・設定に持たない）。
// per_document: 文書ごとの値で設定に入れない（決定 4 の `syntax`）。面に載せない。
enum class FeatureStorage : std::uint8_t
{
    listed,
    own_key,
    always_on,
    per_document
};
} // namespace nenenib::core
