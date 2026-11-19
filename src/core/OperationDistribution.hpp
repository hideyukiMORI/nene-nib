#pragma once

#include "Distribution.hpp"
#include "EditorOperation.hpp"

#include <utility>

namespace nenenib::core
{
// その形の製品で操作が効くか（ADR 0178 の追補 #1809）。パッケージの中の HKCU の書き込みは外から
// 見えないので、Store 版ではエクスプローラーへの登録の 2 つは効かない。F1 の一覧から外し、別の道で
// 届いても controller が書かずに 1 行で知らせる。
[[nodiscard]] constexpr bool operation_distributed(EditorOperation operation,
                                                   Distribution distribution) noexcept
{
    switch (distribution)
    {
    case Distribution::unpackaged:
        return true;
    case Distribution::packaged:
        return operation != EditorOperation::register_shell &&
               operation != EditorOperation::unregister_shell;
    }
    std::unreachable();
}
} // namespace nenenib::core
