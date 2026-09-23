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
// 記法の名前が指す鍵。多くは特殊鍵で、`<NL>` だけは文字（Vim の Ctrl-J・ADR 0048 の決定 9）。
using VimNamedKey = std::variant<nenenib::core::VimCharacter, VimSpecialKey>;

// fixture の記法（<Esc> <CR> <NL> <BS> <C-r> <Space> 矢印など）と鍵の対応。写す場所は
// eng/vim-oracle.py とここの 2 つで、どちらも「fixture の書き方」という 1 つの約束の
// 両端である（ARC-012）。
struct VimKeyName
{
    std::string_view text;
    VimNamedKey key;
};

inline constexpr std::array<VimKeyName, 19> vim_key_names{
    {{"<Esc>", VimSpecialKey::escape},
     {"<CR>", VimSpecialKey::enter},
     {"<NL>", nenenib::core::VimCharacter{U'\n'}},
     {"<BS>", VimSpecialKey::backspace},
     {"<C-r>", VimSpecialKey::control_r},
     {"<C-d>", VimSpecialKey::control_d},
     {"<C-u>", VimSpecialKey::control_u},
     {"<C-f>", VimSpecialKey::control_f},
     {"<C-b>", VimSpecialKey::control_b},
     {"<C-v>", VimSpecialKey::control_v},
     {"<Home>", VimSpecialKey::home},
     {"<End>", VimSpecialKey::end},
     {"<PageUp>", VimSpecialKey::page_up},
     {"<PageDown>", VimSpecialKey::page_down},
     {"<Space>", nenenib::core::VimCharacter{U' '}},
     {"<Left>", VimSpecialKey::arrow_left},
     {"<Right>", VimSpecialKey::arrow_right},
     {"<Up>", VimSpecialKey::arrow_up},
     {"<Down>", VimSpecialKey::arrow_down}}};

// fixture はどれも数行なので、全部の行が表示値に載る高さで再生する。
inline constexpr std::size_t vim_visible_lines = 64;

[[nodiscard]] std::vector<nenenib::core::VimKey> vim_keys_of(std::string_view keys);
[[nodiscard]] std::string vim_body(const nenenib::application::EditorFrame &frame);
void vim_replay(nenenib::application::EditorController &controller, std::string_view keys);
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
