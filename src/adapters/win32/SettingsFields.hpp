#pragma once

#include <optional>
#include <string_view>
#include <utility>
#include <vector>

namespace nenenib::adapters::win32
{
// 版付き設定の直列化 DTO。domain へ変換した後は保持しない（ARC-009）。
struct SettingsFields
{
    std::optional<std::string_view> version;
    std::optional<std::string_view> colorscheme;
    std::optional<std::string_view> font_family;
    std::optional<std::string_view> font_size;
    std::optional<std::string_view> guide;
    std::optional<std::string_view> wrap;
    // v4 だけの鍵（ADR 0170 の決定 3）。
    std::optional<std::string_view> ui_language;
    std::optional<std::string_view> lsp;
    std::optional<std::string_view> explorer;
    std::optional<std::string_view> root;
    std::optional<std::string_view> features;
    // 検索の大小（#1041・v4 の鍵に足す。v4 は未公開なので版は上げない）。
    std::optional<std::string_view> ignorecase;
    std::optional<std::string_view> smartcase;
    // `lsp.<language>.command` の行（鍵の中の言語の名前と値・読んだ順）。
    std::vector<std::pair<std::string_view, std::string_view>> lsp_commands;
    // Vim の `map.<modes>.<lhs>` の行（鍵の全体と値・ファイルの中の順）と `mapleader`
    // （ADR 0176 の決定 1）。
    std::vector<std::pair<std::string_view, std::string_view>> maps;
    std::optional<std::string_view> mapleader;
};
} // namespace nenenib::adapters::win32
