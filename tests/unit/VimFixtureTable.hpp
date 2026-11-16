#pragma once

#include "../vim/VimFixture.hpp"

#include <span>

namespace nenenib::tests
{
// 生成物 VimFixtures.hpp の表を 1
// つの翻訳単位（VimFixtureTable.cpp）だけが読み、ほかはここから引く（#1753）。
[[nodiscard]] std::span<const VimFixture> vim_fixture_table();
} // namespace nenenib::tests
