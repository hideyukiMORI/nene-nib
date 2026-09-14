#pragma once

#include <string>

namespace nenenib::application
{
// 文字の入力。窓が WM_CHAR の UTF-16 を UTF-8 へ直してから渡す（ADR 0009 の決定 2）。
struct InsertText
{
    std::string utf8;
};
} // namespace nenenib::application
