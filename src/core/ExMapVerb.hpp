#pragma once

#include <cstdint>

namespace nenenib::core
{
// Vim の `map` の Ex の命令の種類（ADR 0176 の決定 6）。add は `:nmap` `:nnoremap` …、remove は
// `:unmap` `:nunmap` …、list は引数なし（か左辺だけ）の `:nmap` …、leader は
// `:let mapleader=`。値が増えたら写し先の switch が落ちる（CPP-002）。
enum class ExMapVerb : std::uint8_t
{
    add,
    remove,
    list,
    leader
};
} // namespace nenenib::core
