// Vim の単体テストが共有する足場（ADR 0042 決定 1）。fixture の記法と再生・表示値の読み方。
#pragma once

#include "Editing.hpp"
#include "EditorController.hpp"
#include "EditorFrame.hpp"
#include "VimCharacter.hpp"
#include "VimCharacterSearchKind.hpp"
#include "VimCount.hpp"
#include "VimKey.hpp"
#include "VimPrefix.hpp"
#include "VimRegister.hpp"
#include "VimSpecialKey.hpp"
#include "VimState.hpp"

#include "../vim/VimFixture.hpp"
#include "../vim/VimKeyNames.hpp"

#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace nenenib::tests
{
using nenenib::core::VimSpecialKey;
using MatchSpan = std::pair<std::size_t, std::size_t>;
// fixture の記法の表 vim_key_names は eng/vim-oracle.py の KEY_TABLE から作る生成物で、
// 手で書かない（ADR 0054）。記法を足すのは oracle の表の 1 か所だけ。

// fixture はどれも数行なので、全部の行が表示値に載る高さで再生する。
inline constexpr std::size_t vim_visible_lines = 64;

[[nodiscard]] std::vector<nenenib::core::VimKey> vim_keys_of(std::string_view keys);
[[nodiscard]] std::string vim_body(const nenenib::application::EditorFrame &frame);
// 打った鍵の意味で流す。窓で打った鍵と同じに、鍵が失敗しても次の鍵が走る（ADR 0046 の決定 3）。
void vim_replay(nenenib::application::EditorController &controller, std::string_view keys);
// fixture の鍵を Vim の :normal! の意味で流す。鍵が失敗で終わったら残りの鍵を流さない
// （Issue #230・oracle は鍵を 1 本の :normal! で Vim へ入れる）。
void vim_normal(nenenib::application::EditorController &controller, std::string_view keys);
[[nodiscard]] nenenib::core::VimState empty_vim_state();
[[nodiscard]] bool last_search_is(const nenenib::core::VimState &state,
                                  nenenib::core::VimCharacterSearchKind kind,
                                  char32_t target) noexcept;
[[nodiscard]] bool waits_for_character(const nenenib::core::VimState &state,
                                       nenenib::core::VimCharacterSearchKind kind) noexcept;
[[nodiscard]] bool waits_for_prefix(const nenenib::core::VimState &state,
                                    nenenib::core::VimPrefix prefix) noexcept;
void arrange_vim_viewport(nenenib::application::EditorController &controller,
                          const VimFixture &fixture);
void store_vim_fixture_macro(nenenib::application::EditorController &controller,
                             const VimFixture &fixture);
[[nodiscard]] std::string whole_vim_body(nenenib::application::EditorController &controller);
[[nodiscard]] std::string vim_register_kind(const nenenib::core::VimRegister &value);
void open_vim_document(Editing &editing, std::string text);
[[nodiscard]] bool dot_record_is(const nenenib::core::VimState &state,
                                 const std::optional<nenenib::core::VimCount> &count,
                                 std::string_view keys);
[[nodiscard]] bool caret_at(const nenenib::application::EditorFrame &frame, std::size_t line,
                            std::size_t column);
[[nodiscard]] std::vector<MatchSpan> frame_matches(const nenenib::application::EditorFrame &frame,
                                                   std::size_t index);
[[nodiscard]] bool current_match_is(const nenenib::application::EditorFrame &frame,
                                    std::size_t index, std::optional<MatchSpan> expected);
} // namespace nenenib::tests
