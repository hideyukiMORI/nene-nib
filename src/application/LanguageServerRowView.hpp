#pragma once

#include "LanguageServerRowAction.hpp"
#include "LanguageServerRowState.hpp"

#include <string>

namespace nenenib::application
{
// 設定の「言語サーバー」の節の 1 行の表示値（LSP 段 S4・#1945・案 A）。title は言語の名前（表の
// 固有名・表に無い言語の行は設定の名前）、server はサーバーの名前（等幅で小さく描く）、state は
// 状態（ui が語で書く）、detail は 2 行目（code なら命令の行・パス・入れ方の命令で等幅、そうで
// なければ表示言語の文）、action は右の押す物。language は行の身元（表の行は最初の言語の設定の
// 名前・表に無い言語の行は設定の名前・押してから離すまでに行がずれても同じ行かを比べる・レビュー
// R2）。
struct LanguageServerRowView
{
    std::string title;
    std::string server;
    LanguageServerRowState state;
    std::string detail;
    bool code;
    LanguageServerRowAction action;
    std::string language;
};
} // namespace nenenib::application
