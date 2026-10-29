#include "EditorWindow.hpp"

#include "CancelComposition.hpp"
#include "ClauseEmphasis.hpp"
#include "CommitText.hpp"
#include "ComposeText.hpp"
#include "Composition.hpp"
#include "CompositionClause.hpp"
#include "Offset.hpp"
#include "OffsetRange.hpp"
#include "Utf16.hpp"

#include <imm.h>

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace nenenib::ui::win32
{
namespace
{
// ---------------------------------------------------------------- IMM32（ADR 0014）

// 長さを先に取ってからバッファに読む。ImmGetCompositionStringW はバイト数を返す。
[[nodiscard]] std::wstring composition_string(HIMC context, DWORD kind, DWORD flags)
{
    if ((flags & kind) == 0)
    {
        return {};
    }
    const LONG bytes = ImmGetCompositionStringW(context, kind, nullptr, 0);
    if (bytes <= 0)
    {
        return {};
    }
    std::wstring wide(static_cast<std::size_t>(bytes) / sizeof(wchar_t), L'\0');
    ImmGetCompositionStringW(context, kind, wide.data(), static_cast<DWORD>(bytes));
    return wide;
}

[[nodiscard]] std::vector<std::uint8_t> composition_bytes(HIMC context, DWORD kind)
{
    const LONG bytes = ImmGetCompositionStringW(context, kind, nullptr, 0);
    if (bytes <= 0)
    {
        return {};
    }
    std::vector<std::uint8_t> read(static_cast<std::size_t>(bytes), 0);
    ImmGetCompositionStringW(context, kind, read.data(), static_cast<DWORD>(bytes));
    return read;
}

// GCS_COMPCLAUSE は DWORD の並び（UTF-16 の単位で、先頭は 0・末尾は長さ）。バイト列から
// DWORD へ戻すのは std::bit_cast（reinterpret_cast は書けない・CPP-009）。
[[nodiscard]] std::vector<std::size_t> composition_boundaries(HIMC context)
{
    const auto bytes = composition_bytes(context, GCS_COMPCLAUSE);
    std::vector<std::size_t> boundaries;
    for (std::size_t at = 0; at + sizeof(DWORD) <= bytes.size(); at += sizeof(DWORD))
    {
        std::array<std::uint8_t, sizeof(DWORD)> word{};
        std::copy_n(bytes.begin() + static_cast<std::ptrdiff_t>(at), word.size(), word.begin());
        boundaries.push_back(static_cast<std::size_t>(std::bit_cast<DWORD>(word)));
    }
    return boundaries;
}

// UTF-16 の単位の位置 → UTF-8 のバイト位置の表。文節の境界も属性も GCS_CURSORPOS も
// どれも UTF-16 の単位なので、写しはこの 1 本を通す（CPP-014 / Issue #13）。
[[nodiscard]] std::vector<std::size_t> byte_offsets(std::wstring_view wide)
{
    // 先頭は必ず 0 で始める（空の prefix を変換しに行かない）。
    std::vector<std::size_t> offsets{0};
    offsets.reserve(wide.size() + 1);
    for (std::size_t units = 1; units <= wide.size(); ++units)
    {
        // サロゲートの途中で切った prefix は変換できないので、直前の値を伸ばさずに使う。
        const auto converted = core::to_utf8(wide.substr(0, units));
        offsets.push_back(converted.has_value() ? converted.value().size() : offsets.back());
    }
    return offsets;
}

[[nodiscard]] std::size_t byte_at(const std::vector<std::size_t> &offsets, std::size_t unit)
{
    if (offsets.empty())
    {
        return 0;
    }
    return offsets.at(std::min(unit, offsets.size() - 1));
}

// 文節の強さは文節の先頭の属性が決める（ADR 0014 の決定 7）。属性の集合は IME が決める
// 開いた集合なので、閉じた enum に畳むのはここ 1 か所だけ。
[[nodiscard]] core::ClauseEmphasis emphasis_at(const std::vector<std::uint8_t> &attributes,
                                               std::size_t unit) noexcept
{
    if (unit >= attributes.size())
    {
        return core::ClauseEmphasis::other;
    }
    const std::uint8_t attribute = attributes.at(unit);
    if (attribute == ATTR_TARGET_CONVERTED || attribute == ATTR_TARGET_NOTCONVERTED)
    {
        return core::ClauseEmphasis::target;
    }
    return core::ClauseEmphasis::other;
}

[[nodiscard]] std::vector<core::CompositionClause>
clauses_of(const std::vector<std::size_t> &boundaries, const std::vector<std::uint8_t> &attributes,
           const std::vector<std::size_t> &offsets)
{
    std::vector<core::CompositionClause> clauses;
    for (std::size_t index = 0; index + 1 < boundaries.size(); ++index)
    {
        const std::size_t from = boundaries.at(index);
        const std::size_t to = boundaries.at(index + 1);
        clauses.push_back(
            core::CompositionClause{core::OffsetRange{core::Offset{byte_at(offsets, from)},
                                                      core::Offset{byte_at(offsets, to)}},
                                    emphasis_at(attributes, from)});
    }
    return clauses;
}

[[nodiscard]] core::Composition composition_of(const std::wstring &wide,
                                               const std::vector<std::uint8_t> &attributes,
                                               const std::vector<std::size_t> &boundaries,
                                               LONG cursor)
{
    const auto offsets = byte_offsets(wide);
    // 変換中の文字列は IME が作った正しい UTF-16 なので、空になるのは本当に空のときだけ。
    std::string utf8 = core::to_utf8(wide).value_or(std::string{});
    const std::size_t unit = cursor < 0 ? wide.size() : static_cast<std::size_t>(cursor);
    return core::Composition{std::move(utf8), clauses_of(boundaries, attributes, offsets),
                             core::Offset{byte_at(offsets, unit)}};
}

// GCS_COMPSTR が立っていなければ変換中の文字列は変わっていない（確定だけのメッセージ）。
[[nodiscard]] std::optional<core::Composition> composition_in(HIMC context, DWORD flags)
{
    if ((flags & GCS_COMPSTR) == 0)
    {
        return std::nullopt;
    }
    const std::wstring wide = composition_string(context, GCS_COMPSTR, flags);
    return composition_of(wide, composition_bytes(context, GCS_COMPATTR),
                          composition_boundaries(context),
                          ImmGetCompositionStringW(context, GCS_CURSORPOS, nullptr, 0));
}
} // namespace

// ---------------------------------------------------------------- IMM32（ADR 0014）

// IMM32 の 3 通（決定 3）。WM_IME_STARTCOMPOSITION と WM_IME_COMPOSITION は DefWindowProcW に
// 渡さない＝ IME の既定の変換窓を出さず、確定文字を WM_CHAR に流さない。WM_IME_ENDCOMPOSITION は
// 残った変換を捨ててから OS にも見せる。変換中の鍵は IME が食い、WM_KEYDOWN は VK_PROCESSKEY で
// 来る＝ vim_specials の表に無いので Vim の鍵にならない（決定 5・既存の形のまま）。
LRESULT EditorWindow::compose_message(UINT message, WPARAM word, LPARAM data)
{
    switch (message)
    {
    case WM_IME_STARTCOMPOSITION:
        place_candidate_window();
        return 0;
    case WM_IME_COMPOSITION:
        compose(data);
        return 0;
    // OS のメッセージ番号は開いた集合なので、既定分岐だけは許される（CPP-017）。
    default:
        break;
    }
    end_composition();
    return DefWindowProcW(window_, message, word, data);
}

void EditorWindow::compose(LPARAM data)
{
    const auto flags = static_cast<DWORD>(data);
    const HIMC context = ImmGetContext(window_);
    if (context == nullptr)
    {
        return;
    }
    // 確定と変換中は 1 通のメッセージに同居しうるので、文脈を離す前に両方を読む（決定 3）。
    const std::string committed =
        core::to_utf8(composition_string(context, GCS_RESULTSTR, flags)).value_or(std::string{});
    const auto composing = composition_in(context, flags);
    ImmReleaseContext(window_, context);
    if (!committed.empty())
    {
        send(application::CommitText{committed});
    }
    if (composing.has_value())
    {
        send_composition(composing.value());
    }
    place_candidate_window();
}

void EditorWindow::send_composition(const core::Composition &composition)
{
    // 空の変換文字列は「変換をやめた」の合図（決定 3）。
    if (composition.utf8.empty())
    {
        send(application::CancelComposition{});
        return;
    }
    send(application::ComposeText{composition});
}

void EditorWindow::end_composition()
{
    if (!controller_.is_composing())
    {
        return;
    }
    send(application::CancelComposition{});
}

// 候補窓はキャレットの直下。ImmSetCompositionWindow は使わない（変換文字列は自前で描く・決定 6）。
void EditorWindow::place_candidate_window()
{
    if (renderer_ == nullptr)
    {
        return;
    }
    const HIMC context = ImmGetContext(window_);
    if (context == nullptr)
    {
        return;
    }
    const RECT caret = renderer_->caret_rectangle();
    CANDIDATEFORM form{};
    form.dwStyle = CFS_CANDIDATEPOS;
    form.ptCurrentPos = POINT{caret.left, caret.bottom};
    ImmSetCandidateWindow(context, &form);
    ImmReleaseContext(window_, context);
}

// 構えは application の ime_stance_of が決めて frame に載せる。ui はその値を実行するだけで、
// モードや入力行の有無から IME を決めない（ADR 0061 の決定 2）。
void EditorWindow::follow_ime(const application::EditorDelivery &delivery)
{
    const auto previous = input_line_;
    input_line_ = delivery.input_line;
    switch (delivery.ime)
    {
    case application::ImeStance::as_left:
        restore_ime();
        return;
    case application::ImeStance::closed:
        close_ime();
        return;
    case application::ImeStance::closed_once:
        // 入力行の種類が変わったとき（無し → Ex・Ex → 面・無し → 面・無し → 検索）に
        // 1 度だけ閉じる。同じ入力行の続く意図では、使う人が開いた IME を閉じ直さない。
        if (previous != delivery.input_line)
        {
            close_ime();
        }
        return;
    }
    std::unreachable();
}

void EditorWindow::close_ime()
{
    const HIMC context = ImmGetContext(window_);
    if (context == nullptr)
    {
        return;
    }
    // 控えが在れば切る前の値を上書きしない（ADR 0014 の決定 5）。閉じるのは控えの有無に依らない
    // ＝面の中で開いた IME も、Vim の NORMAL へ戻るときに閉じ直す（ADR 0061 の決定 2）。
    const bool open = ImmGetOpenStatus(context) != FALSE;
    if (ime_open_ == ImeOpenState::unrecorded)
    {
        ime_open_ = open ? ImeOpenState::open : ImeOpenState::closed;
    }
    // 閉じているときは OS に何も送らない（NORMAL の打鍵ごとに通知を起こさない）。
    if (open)
    {
        ImmSetOpenStatus(context, FALSE);
    }
    ImmReleaseContext(window_, context);
}

// 入力行が閉じて変換が消えたとき、IME の側に変換を残さない（ADR 0061 の決定 3）。
void EditorWindow::cancel_ime_composition()
{
    const HIMC context = ImmGetContext(window_);
    if (context == nullptr)
    {
        return;
    }
    ImmNotifyIME(context, NI_COMPOSITIONSTR, CPS_CANCEL, 0);
    ImmReleaseContext(window_, context);
}

void EditorWindow::restore_ime()
{
    if (ime_open_ == ImeOpenState::unrecorded)
    {
        return;
    }
    const HIMC context = ImmGetContext(window_);
    if (context == nullptr)
    {
        return;
    }
    ImmSetOpenStatus(context, ime_open_ == ImeOpenState::open ? TRUE : FALSE);
    ime_open_ = ImeOpenState::unrecorded;
    ImmReleaseContext(window_, context);
}
} // namespace nenenib::ui::win32
