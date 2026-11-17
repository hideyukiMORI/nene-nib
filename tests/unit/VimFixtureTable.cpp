#include "VimFixtureTable.hpp"

#include "../vim/VimFixtures.hpp"

namespace nenenib::tests
{
std::span<const VimFixture> vim_fixture_table()
{
    return vim_fixtures;
}
} // namespace nenenib::tests
