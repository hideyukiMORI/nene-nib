#pragma once

#include <cstddef>
#include <optional>
#include <string>

namespace nenenib::application
{
// 窓の保持の鍵（ADR 0189 の決定 4）。ファイルのパスの文字列と、窓の中心の行（先頭からなら無し）。
// 値だけを運ぶ公開 aggregate（CPP-003）。
struct PreviewKey
{
    std::string path;
    std::optional<std::size_t> line;
};

[[nodiscard]] inline bool operator==(const PreviewKey &left, const PreviewKey &right)
{
    return left.path == right.path && left.line == right.line;
}
} // namespace nenenib::application
