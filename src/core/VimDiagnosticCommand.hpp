#pragma once

#include "DiagnosticCommand.hpp"

#include <cstddef>

namespace nenenib::core
{
// `]d` `[d` `gl` が診断の跳びと札を頼む（#1939）。engine は文書の診断を知らないので、この値を
// 返すだけで、跳ぶ・札を出すのは controller（VimLanguageQuery と同じ形・ADR 0051）。状態と失敗の
// 印は表に行が無かったときと同じまま。count は跳びの回数（1 以上・札では 1）。公開 aggregate
// （CPP-003）。
struct VimDiagnosticCommand
{
    DiagnosticCommand command;
    std::size_t count;
};
} // namespace nenenib::core
