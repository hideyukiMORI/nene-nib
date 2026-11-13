// #1594（#1019 の T1c）・#1602（T1f）: ui が自分で出す文を表示言語の表から引き、画面の文字の書式の
// locale を言語から決め、固定の枠に出る文が 3 言語とも枠に収まる（ADR 0123 の追補 2026-11-13 の
// 決定 9〜11）。文の確かめは窓を作らない。幅は本物の DirectWrite で、製品の renderer と同じ字体・
// 大きさ（UiTextSizes.hpp）・太さ・locale の書式で測り、枠は製品と同じ core の配置から引く（96
// DPI・物理画素 = DIP）。字体に結果が依るので、Segoe UI も無い機械では幅を測らない。zh-cn の書式で
// 漢字が日本語の字体に落ちないことも本物の MapCharacters で確かめる。
#include "BodyGuideLayout.hpp"
#include "BodyLayout.hpp"
#include "DevicePixels.hpp"
#include "DirectWriteFactory.hpp"
#include "DisplayLanguage.hpp"
#include "EditMode.hpp"
#include "EditorOperation.hpp"
#include "ExplorerLayout.hpp"
#include "ExplorerWidth.hpp"
#include "FallbackRunSource.hpp"
#include "FileDialog.hpp"
#include "FileFailure.hpp"
#include "FileFailureText.hpp"
#include "FontSize.hpp"
#include "GuideContext.hpp"
#include "GuideRequest.hpp"
#include "OperationGuide.hpp"
#include "OperationGuideLayout.hpp"
#include "PaletteLayout.hpp"
#include "SettingsFailure.hpp"
#include "SettingsNotice.hpp"
#include "StartEntryKind.hpp"
#include "StartScreen.hpp"
#include "StartScreenLayout.hpp"
#include "StatusBarLayout.hpp"
#include "StatusGuideLayout.hpp"
#include "UiLocale.hpp"
#include "UiMessage.hpp"
#include "UiTextSizes.hpp"
#include "UiTexts.hpp"
#include "Utf16.hpp"
#include "WindowChecks.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <variant>
#include <wrl/client.h>
#include <wrl/implements.h>

namespace nenenib::tests::ui
{
namespace
{
using Format = Microsoft::WRL::ComPtr<IDWriteTextFormat>;
using core::DisplayLanguage;
using core::UiMessage;
using nenenib::ui::win32::ui_locale_name;

constexpr std::uint32_t dpi = 96;
constexpr std::array<DisplayLanguage, 3> languages{
    DisplayLanguage::japanese, DisplayLanguage::english, DisplayLanguage::simplified_chinese};
// 字の大きさと鍵の枠の余白は renderer と同じ出どころ（UiTextSizes.hpp）から読む。
using nenenib::ui::win32::guide_key_dips;
using nenenib::ui::win32::guide_key_padding_dips;
using nenenib::ui::win32::guide_label_dips;
using nenenib::ui::win32::guide_note_dips;
using nenenib::ui::win32::tab_text_dips;
using nenenib::ui::win32::ui_text_dips;

[[nodiscard]] const char *language_name(DisplayLanguage language) noexcept
{
    switch (language)
    {
    case DisplayLanguage::japanese:
        return "ja";
    case DisplayLanguage::english:
        return "en";
    case DisplayLanguage::simplified_chinese:
        return "zh-hans";
    }
    return "?";
}

// ---- 文（窓を作らない） ----

constexpr std::array<application::FileFailure, 11> file_failures{
    application::FileFailure::not_found,          application::FileFailure::access_denied,
    application::FileFailure::unreadable,         application::FileFailure::unwritable,
    application::FileFailure::too_large,          application::FileFailure::too_large_to_decode,
    application::FileFailure::undecodable,        application::FileFailure::unsupported_utf32,
    application::FileFailure::unencodable,        application::FileFailure::already_exists,
    application::FileFailure::insufficient_memory};

// 移す前の reason_of の日本語（file_failures と同じ順）。
constexpr std::array<std::wstring_view, 11> japanese_file_failures{
    L"ファイルが見つかりませんでした。",
    L"ファイルを開く権限がありません。",
    L"ファイルを読み取れませんでした。",
    L"ファイルを保存できませんでした。元のファイルは変わっていません。",
    L"1 GB を超えるファイルは開けません。",
    L"64 MiB を超える Shift_JIS / UTF-16 は開けません。",
    L"文字コードを判別できませんでした。",
    L"UTF-32 のファイルは扱えません。UTF-8 か UTF-16 で保存し直してください。",
    L"この文字コードでは保存できない文字があります。",
    L"ファイルは既に存在します。",
    L"メモリが足りず、ファイルを開けませんでした。"};

constexpr std::array<application::SettingsFailure, 11> settings_failures{
    application::SettingsFailure::location_unavailable,
    application::SettingsFailure::unreadable,
    application::SettingsFailure::too_large,
    application::SettingsFailure::malformed,
    application::SettingsFailure::unsupported_version,
    application::SettingsFailure::unknown_theme,
    application::SettingsFailure::invalid_font_family,
    application::SettingsFailure::invalid_font_size,
    application::SettingsFailure::unwritable,
    application::SettingsFailure::changed_externally,
    application::SettingsFailure::not_loaded};

// 移す前の settings_notice の日本語（settings_failures と同じ順）。
constexpr std::array<std::wstring_view, 11> japanese_settings_notices{
    L"設定の保存先を取得できませんでした。設定は保存されません。",
    L"設定ファイルを読めませんでした。元の設定は保持します。",
    L"設定ファイルが上限の 16 KiB（16384 バイト）を超えています。元の設定は保持します。",
    L"設定ファイルの形式が正しくありません。元の設定は保持します。",
    L"設定ファイルの版に対応していません。元の設定は保持します。",
    L"設定ファイルのテーマ名が見つかりません。元の設定は保持します。",
    L"設定ファイルのフォント名が正しくありません。元の設定は保持します。",
    L"設定ファイルの文字サイズは 8〜40 pt で指定してください。元の設定は保持します。",
    L"設定を保存できませんでした。別の起動で使用中、または書き込みが許可されていません。",
    L"別の処理で設定ファイルが変更されました。上書きせずに保持します。再起動で読み直してください。",
    L"設定を読み込めていないため、保存しませんでした。"};

// 置き場が残らず埋まり、空でない文か。
[[nodiscard]] bool filled(const std::wstring &text)
{
    return !text.empty() && text.find(L"{0}") == std::wstring::npos &&
           text.find(L"{1}") == std::wstring::npos;
}

void verify_dialog_texts()
{
    namespace win32 = nenenib::ui::win32;
    for (std::size_t index = 0; index < file_failures.size(); ++index)
    {
        const auto failure = file_failures.at(index);
        expect(win32::file_failure_text(failure, DisplayLanguage::japanese) ==
                   japanese_file_failures.at(index),
               "the Japanese file failure text is the product's text before #1019");
        for (const auto language : languages)
        {
            expect(filled(win32::file_failure_text(failure, language)),
                   "every file failure has a filled text in every language");
        }
    }
    expect(win32::file_failure_text(application::FileFailure::too_large, DisplayLanguage::english)
                   .find(L"1 GB") != std::wstring::npos,
           "the English open limit is filled from core::maximum_document_bytes");
    for (std::size_t index = 0; index < settings_failures.size(); ++index)
    {
        const auto failure = settings_failures.at(index);
        expect(win32::settings_notice(failure, DisplayLanguage::japanese) ==
                   japanese_settings_notices.at(index),
               "the Japanese settings notice is the product's text before #1019");
        for (const auto language : languages)
        {
            expect(filled(win32::settings_notice(failure, language)),
                   "every settings notice has a filled text in every language");
        }
    }
}

void verify_os_texts()
{
    namespace win32 = nenenib::ui::win32;
    const auto japanese = win32::file_type_names(DisplayLanguage::japanese);
    expect(japanese.at(0) == L"対応するファイル" && japanese.at(1) == L"すべてのファイル",
           "the Japanese filter names are the product's names before #1019");
    for (const auto language : languages)
    {
        const auto names = win32::file_type_names(language);
        expect(!names.at(0).empty() && !names.at(1).empty() && names.at(0) != names.at(1),
               "both filter names are shown in every language");
        expect(!win32::wide_ui_text(UiMessage::default_file_name, language).empty() &&
                   !win32::wide_ui_text(UiMessage::os_shutdown_block_reason, language).empty(),
               "the default file name and the shutdown reason exist in every language");
    }
    expect(win32::wide_ui_text(UiMessage::default_file_name, DisplayLanguage::japanese) ==
               L"無題.txt",
           "the Japanese default file name is 無題.txt");
    expect(win32::wide_ui_text(UiMessage::os_shutdown_block_reason, DisplayLanguage::japanese) ==
               L"保存されていない変更があります",
           "the Japanese shutdown reason is the product's text before #1019");
    expect(std::wstring_view(ui_locale_name(DisplayLanguage::japanese)).empty() &&
               std::wstring_view(ui_locale_name(DisplayLanguage::english)) == L"en-us" &&
               std::wstring_view(ui_locale_name(DisplayLanguage::simplified_chinese)) == L"zh-cn",
           "the UI locale is empty for Japanese (unchanged), en-us and zh-cn otherwise");
}

// 画面に出る文（renderer・起動画面・エクスプローラー）の名前と、移す前の日本語。
constexpr std::array<std::pair<UiMessage, std::string_view>, 16> japanese_screen_texts{{
    {UiMessage::prompt_find, "検索: "},
    {UiMessage::prompt_go_to_line, "行: "},
    {UiMessage::prompt_replace_with, "置換後: "},
    {UiMessage::guide_note_disappears, "打ち始めると消えます"},
    {UiMessage::status_mode_basic, "通常"},
    {UiMessage::palette_no_candidates, "候補なし"},
    {UiMessage::palette_footer_hint, "↑↓ 選択   Enter 決定   Esc 閉じる"},
    {UiMessage::explorer_no_root, ":set root= でルートを決めてください"},
    {UiMessage::start_recent_files, "最近使ったファイル"},
    {UiMessage::label_bookmarks, "ブックマーク"},
    {UiMessage::start_get_started, "はじめる"},
    {UiMessage::start_shortcuts, "ショートカット"},
    {UiMessage::start_no_history,
     "まだ履歴がありません。ファイルを開くか、そのまま打ち始めてください"},
    {UiMessage::start_new_file, "新しいファイル"},
    {UiMessage::start_new_file_detail, "そのまま打ち始める"},
    {UiMessage::start_open_folder, "フォルダを開く"},
}};

void verify_screen_texts()
{
    for (const auto &[message, japanese] : japanese_screen_texts)
    {
        expect(core::ui_text(message, DisplayLanguage::japanese) == japanese,
               "the Japanese screen text is the product's text before #1019");
        for (const auto language : languages)
        {
            expect(!core::ui_text(message, language).empty(),
                   "every screen text exists in every language");
        }
    }
    expect(core::ui_text(UiMessage::start_open_folder_detail, DisplayLanguage::japanese) ==
               "ルートを選んでエクスプローラーを出す",
           "the Japanese open folder detail is the product's text before #1019");
    expect(core::ui_text_filled(UiMessage::prompt_replace_with_pattern, DisplayLanguage::japanese,
                                {"abc"}) == "置換後 (abc): ",
           "the Japanese replace prompt keeps the pattern in the product's form");
}

// ---- 幅（本物の DirectWrite） ----

using Face = const wchar_t *;
// 書式の組（factory・字体・大きさ DIP・太さ）。製品の renderer の書式と同じ値を渡す。
using Pen = std::tuple<IDWriteFactory2 *, Face, float, DWRITE_FONT_WEIGHT>;
// 測った文（言語と文）。外れたときの 1 行に書く。
using Said = std::pair<DisplayLanguage, std::string_view>;

// 製品と同じ書式（字体・大きさ・太さ・表示言語の locale・折り返さない）で測った文の幅（DIP）。
// 測れなければ負。
[[nodiscard]] float width_in(const Pen &pen, const Said &said)
{
    const auto &[factory, face, size, weight] = pen;
    Format format;
    if (FAILED(factory->CreateTextFormat(face, nullptr, weight, DWRITE_FONT_STYLE_NORMAL,
                                         DWRITE_FONT_STRETCH_NORMAL, size,
                                         ui_locale_name(said.first), format.GetAddressOf())) ||
        FAILED(format->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP)))
    {
        return -1.0F;
    }
    const std::wstring wide = core::to_utf16(said.second).value_or(std::wstring{});
    Microsoft::WRL::ComPtr<IDWriteTextLayout> layout;
    DWRITE_TEXT_METRICS metrics{};
    if (FAILED(factory->CreateTextLayout(wide.c_str(), static_cast<UINT32>(wide.size()),
                                         format.Get(), 100000.0F, 100.0F, layout.GetAddressOf())) ||
        FAILED(layout->GetMetrics(&metrics)))
    {
        return -1.0F;
    }
    return metrics.width;
}

// 幅 width の文が枠 room（物理画素 = DIP）に収まるか。外れたら言語と文と数字を 1 行書く。
void expect_fits(float width, std::int32_t room, const Said &said, const char *frame)
{
    const bool fit = width >= 0.0F && width <= static_cast<float>(room);
    if (!fit)
    {
        std::printf("  not fitting [%s] %.1f > %d DIP: %.*s\n", language_name(said.first),
                    static_cast<double>(width), room, static_cast<int>(said.second.size()),
                    said.second.data());
    }
    expect(fit, frame);
}

// 書式 pen で測った文 said が枠 room に収まるか。
void expect_fits_in(const Pen &pen, std::int32_t room, const Said &said, const char *frame)
{
    expect_fits(width_in(pen, said), room, said, frame);
}

// 文の名前 message を 3 言語で測り、どれも枠 room に収まるか。
void expect_all_fit(const Pen &pen, std::int32_t room, UiMessage message, const char *frame)
{
    for (const auto language : languages)
    {
        expect_fits_in(pen, room, Said{language, core::ui_text(message, language)}, frame);
    }
}

// 「通常」のトグル（44 DIP・semibold 12 DIP）とモードの枠（72 DIP・bold 12 DIP）。英語の候補 4 つの
// 幅を並べて書く（decisions の 2・設計席が hide に見せる数字）。
void verify_status_left(IDWriteFactory2 &factory, Face face)
{
    const auto status = core::status_bar_layout(1280, 800, dpi);
    const auto toggle = core::width_of(status.toggle_ordinary);
    const Pen semibold{&factory, face, ui_text_dips, DWRITE_FONT_WEIGHT_SEMI_BOLD};
    expect_all_fit(semibold, toggle, UiMessage::status_mode_basic,
                   "the basic label fits the 44 DIP toggle segment");
    expect_all_fit(Pen{&factory, face, ui_text_dips, DWRITE_FONT_WEIGHT_BOLD},
                   core::width_of(status.mode), UiMessage::status_mode_basic,
                   "the basic label fits the 72 DIP mode frame");
    constexpr std::array<std::string_view, 4> candidates{"Basic", "Standard", "Normal", "Std"};
    for (const auto candidate : candidates)
    {
        const auto width = width_in(semibold, Said{DisplayLanguage::english, candidate});
        std::printf("  toggle %d DIP (en-us, semibold 12 DIP): %.*s = %.1f DIP\n", toggle,
                    static_cast<int>(candidate.size()), candidate.data(),
                    static_cast<double>(width));
    }
}

// 右の最初の項目（行・桁・96 DIP）。数は 4 桁の行と 3 桁の桁で比べる（3 言語で同じ値）。
void verify_status_items(IDWriteFactory2 &factory, Face face)
{
    const auto status = core::status_bar_layout(1280, 800, dpi);
    const Pen pen{&factory, face, ui_text_dips, DWRITE_FONT_WEIGHT_NORMAL};
    for (const auto language : languages)
    {
        const auto text =
            core::ui_text_filled(UiMessage::status_line_column, language, {"9999", "999"});
        expect_fits_in(pen, core::width_of(status.items.front()), Said{language, text},
                       "the line and column item fits its 96 DIP frame");
    }
}

// Ctrl+P の面の「候補なし」（行の名前の欄）と下の案内（件数の 64 DIP を引いた残り）。
void verify_palette_texts(IDWriteFactory2 &factory, Face face)
{
    const auto layout = core::palette_layout(960, 640, dpi, 0);
    const Pen pen{&factory, face, ui_text_dips, DWRITE_FONT_WEIGHT_BOLD};
    expect_all_fit(
        pen, core::width_of(core::palette_row_label(core::palette_row(layout, 0), dpi, false)),
        UiMessage::palette_no_candidates, "the no-candidate line fits the palette row");
    expect_all_fit(pen, core::width_of(layout.footer) - core::to_pixels(64, dpi),
                   UiMessage::palette_footer_hint, "the palette footer hint fits beside the count");
}

// 空の文書の案内の説明の列（144 DIP・13.5 DIP）と注記（11.5 DIP）。
void verify_body_guide(IDWriteFactory2 &factory, Face face)
{
    const auto body = core::body_layout(960, 560, dpi, core::default_font_size());
    const auto guide =
        core::operation_guide_layout(core::GuideRequest{core::GuideContext::untouched_untitled, 0},
                                     body, core::status_bar_layout(960, 560, dpi), dpi);
    const auto *layout = std::get_if<core::BodyGuideLayout>(&guide);
    expect(layout != nullptr, "a 960 x 560 DIP window shows the body guide");
    if (layout == nullptr)
    {
        return;
    }
    constexpr std::array<UiMessage, 3> names{UiMessage::operation_list_files_name,
                                             UiMessage::operation_open_file_name,
                                             UiMessage::operation_list_operations_short_name};
    const Pen label{&factory, face, guide_label_dips, DWRITE_FONT_WEIGHT_NORMAL};
    for (std::size_t index = 0; index < names.size(); ++index)
    {
        expect_all_fit(label, core::width_of(layout->rows.at(index).label), names.at(index),
                       "the body guide name fits its 144 DIP column");
    }
    expect_all_fit(Pen{&factory, face, guide_note_dips, DWRITE_FONT_WEIGHT_NORMAL},
                   core::width_of(layout->note), UiMessage::guide_note_disappears,
                   "the body guide note fits under it");
}

// ステータスバーの操作の案内（短い名前の 2 枠・24 DIP と 36 DIP）。
void verify_status_guide(IDWriteFactory2 &factory, Face face)
{
    const auto guide =
        core::operation_guide_layout(core::GuideRequest{core::GuideContext::other, 0},
                                     core::body_layout(1280, 800, dpi, core::default_font_size()),
                                     core::status_bar_layout(1280, 800, dpi), dpi);
    const auto *layout = std::get_if<core::StatusGuideLayout>(&guide);
    expect(layout != nullptr, "a 1280 DIP wide window shows the status guide");
    if (layout == nullptr)
    {
        return;
    }
    constexpr std::array<UiMessage, 2> names{UiMessage::operation_list_files_short_name,
                                             UiMessage::operation_list_operations_short_name};
    const Pen pen{&factory, face, ui_text_dips, DWRITE_FONT_WEIGHT_NORMAL};
    for (std::size_t index = 0; index < names.size(); ++index)
    {
        expect_all_fit(pen, core::width_of(layout->rows.at(index).label), names.at(index),
                       "the status guide short name fits its frame");
    }
}

// エクスプローラーのルートが無い面の先頭の行（既定の幅 240 DIP の面のルート名の欄・12.5 DIP）。
void verify_explorer_text(IDWriteFactory2 &factory, Face face)
{
    const auto layout =
        core::explorer_layout(core::LayoutRect{0, 0, 1280, 800}, dpi, core::ExplorerWidth{});
    expect_all_fit(Pen{&factory, face, tab_text_dips, DWRITE_FONT_WEIGHT_NORMAL},
                   core::width_of(core::explorer_root_name(layout, core::explorer_line(layout, 0))),
                   UiMessage::explorer_no_root, "the explorer no-root line fits the 240 DIP panel");
}

// 起動画面の行の名前と補足。
[[nodiscard]] std::pair<UiMessage, std::optional<UiMessage>>
start_row_texts(core::StartEntryKind kind)
{
    switch (kind)
    {
    case core::StartEntryKind::new_file:
        return {UiMessage::start_new_file, UiMessage::start_new_file_detail};
    case core::StartEntryKind::open_folder:
        return {UiMessage::start_open_folder, UiMessage::start_open_folder_detail};
    case core::StartEntryKind::open_file:
    case core::StartEntryKind::recent:
    case core::StartEntryKind::bookmark:
        return {UiMessage::operation_open_file_name, std::nullopt};
    }
    return {UiMessage::operation_open_file_name, std::nullopt};
}

[[nodiscard]] core::EditorOperation start_row_operation(core::StartEntryKind kind) noexcept
{
    switch (kind)
    {
    case core::StartEntryKind::new_file:
        return core::EditorOperation::new_tab;
    case core::StartEntryKind::open_folder:
        return core::EditorOperation::choose_explorer_root;
    case core::StartEntryKind::open_file:
    case core::StartEntryKind::recent:
    case core::StartEntryKind::bookmark:
        return core::EditorOperation::open_file;
    }
    return core::EditorOperation::open_file;
}

// 鍵の枠の幅（字の幅と左右の余白・DIP）。通常と Vim の広いほう。
[[nodiscard]] float keycap_room(const Pen &key_pen, core::EditorOperation operation)
{
    float widest = 0.0F;
    for (const auto mode : {core::EditMode::ordinary, core::EditMode::vim})
    {
        const auto key =
            core::operation_guide_entry(operation, mode, DisplayLanguage::japanese).key;
        const float width = key.empty() ? 0.0F
                                        : width_in(key_pen, Said{DisplayLanguage::japanese, key}) +
                                              (guide_key_padding_dips * 2.0F);
        widest = std::max(widest, width);
    }
    return widest;
}

// 「はじめる」の 1 行: 名前 ＋ 12 DIP ＋ 補足が、行の欄から鍵の枠と 12 DIP を引いた残りに収まるか。
void verify_start_row(const Pen &item, const Pen &key_pen, const core::StartRowLayout &row,
                      core::StartEntryKind kind)
{
    const auto gap = static_cast<float>(core::start_detail_gap_dips);
    const auto [name, detail] = start_row_texts(kind);
    const auto room =
        core::width_of(row.label) -
        static_cast<std::int32_t>(keycap_room(key_pen, start_row_operation(kind)) + gap);
    for (const auto language : languages)
    {
        const Said named{language, core::ui_text(name, language)};
        const float added =
            detail.has_value()
                ? gap + width_in(item, Said{language, core::ui_text(detail.value(), language)})
                : 0.0F;
        expect_fits(width_in(item, named) + added, room, named,
                    "the start screen row name and detail fit before the key");
    }
}

// 起動画面の見出しと空の知らせ（12 DIP）と「はじめる」の行（列 480 DIP）。
void verify_start_screen(IDWriteFactory2 &factory, Face face, Face key_face)
{
    const auto body =
        core::body_layout_in(core::LayoutRect{0, 0, 1300, 900}, dpi, core::default_font_size());
    const auto entries = core::start_entries({}, {});
    const auto layout =
        core::start_screen_layout(body, dpi, core::start_screen_shape(entries, true), 0);
    expect(layout.has_value() && layout.value().empty_note.has_value(),
           "the start screen with no history shows the empty note");
    if (!layout.has_value() || !layout.value().empty_note.has_value())
    {
        return;
    }
    constexpr std::array<UiMessage, 4> headings{
        UiMessage::start_recent_files, UiMessage::label_bookmarks, UiMessage::start_get_started,
        UiMessage::start_shortcuts};
    const Pen heading{&factory, face, ui_text_dips, DWRITE_FONT_WEIGHT_NORMAL};
    for (const auto message : headings)
    {
        expect_all_fit(heading, core::width_of(layout.value().start_heading), message,
                       "the start screen heading fits the column");
    }
    expect_all_fit(heading, core::width_of(layout.value().empty_note.value()),
                   UiMessage::start_no_history, "the start screen empty note fits the column");
    const Pen item{&factory, face, tab_text_dips, DWRITE_FONT_WEIGHT_NORMAL};
    const Pen key_pen{&factory, key_face, guide_key_dips, DWRITE_FONT_WEIGHT_NORMAL};
    for (std::size_t index = 0; index < entries.size() && index < layout.value().rows.size();
         ++index)
    {
        verify_start_row(item, key_pen, layout.value().rows.at(index), entries.at(index).kind);
    }
}

void verify_widths()
{
    const auto factory = directwrite_factory();
    expect(factory != nullptr, "the DirectWrite factory is created");
    if (factory == nullptr)
    {
        return;
    }
    // 製品の renderer の family と同じ順に落ちる（ADR 0008 の決定 6）。
    const Face face = installed(*factory.Get(), L"Segoe UI Variable Text")
                          ? L"Segoe UI Variable Text"
                      : installed(*factory.Get(), L"Segoe UI") ? L"Segoe UI"
                                                               : nullptr;
    const Face key_face = installed(*factory.Get(), L"Cascadia Code") ? L"Cascadia Code"
                          : installed(*factory.Get(), L"Consolas")    ? L"Consolas"
                                                                      : nullptr;
    if (face == nullptr || key_face == nullptr)
    {
        unmeasured("the UI fonts are not installed; the fixed-frame widths are not measured");
        return;
    }
    verify_status_left(*factory.Get(), face);
    verify_status_items(*factory.Get(), face);
    verify_palette_texts(*factory.Get(), face);
    verify_body_guide(*factory.Get(), face);
    verify_status_guide(*factory.Get(), face);
    verify_explorer_text(*factory.Get(), face);
    verify_start_screen(*factory.Get(), face, key_face);
}

// ---- zh-cn の字体の選び（本物の MapCharacters・追補の決定 9・V-1598 の C-F1） ----

// 字体の family の英語の名前（無ければ最初の名前）。取れなければ空。
[[nodiscard]] std::wstring family_name_of(IDWriteFont &font)
{
    Microsoft::WRL::ComPtr<IDWriteFontFamily> family;
    Microsoft::WRL::ComPtr<IDWriteLocalizedStrings> names;
    if (FAILED(font.GetFontFamily(family.GetAddressOf())) ||
        FAILED(family->GetFamilyNames(names.GetAddressOf())))
    {
        return {};
    }
    UINT32 index = 0;
    BOOL exists = FALSE;
    if (FAILED(names->FindLocaleName(L"en-us", &index, &exists)) || exists == FALSE)
    {
        index = 0;
    }
    UINT32 length = 0;
    if (FAILED(names->GetStringLength(index, &length)))
    {
        return {};
    }
    std::wstring name(length + 1U, L'\0');
    if (FAILED(names->GetString(index, name.data(), length + 1U)))
    {
        return {};
    }
    name.resize(length);
    return name;
}

// 漢字 1 字を言語の札 tag つきで OS の字体選択に渡し、選ばれた字体の名前を返す（取れなければ空）。
[[nodiscard]] std::wstring fallback_family(IDWriteFactory2 &factory, const wchar_t *tag)
{
    Microsoft::WRL::ComPtr<IDWriteFontFallback> fallback;
    if (FAILED(factory.GetSystemFontFallback(fallback.GetAddressOf())))
    {
        return {};
    }
    auto source = Microsoft::WRL::Make<nenenib::ui::win32::FallbackRunSource>(
        tag, DWRITE_READING_DIRECTION_LEFT_TO_RIGHT);
    // 「直」（U+76F4）は日本語と簡体字で字形が違う字。
    source->reset(L"直");
    UINT32 mapped = 0;
    Microsoft::WRL::ComPtr<IDWriteFont> font;
    FLOAT scale = 0.0F;
    if (FAILED(fallback->MapCharacters(source.Get(), 0, source->length(), nullptr, L"Segoe UI",
                                       DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
                                       DWRITE_FONT_STRETCH_NORMAL, &mapped, font.GetAddressOf(),
                                       &scale)) ||
        font == nullptr)
    {
        return {};
    }
    return family_name_of(*font.Get());
}

// 日本語の字体の family か（Windows に入っている和文の字体の名前の頭）。
[[nodiscard]] bool japanese_family(std::wstring_view name)
{
    constexpr std::array<std::wstring_view, 5> heads{L"Yu Gothic", L"Yu Mincho", L"Meiryo", L"MS ",
                                                     L"BIZ UD"};
    return std::ranges::any_of(heads,
                               [name](std::wstring_view head) { return name.starts_with(head); });
}

void verify_chinese_fallback()
{
    const auto factory = directwrite_factory();
    if (factory == nullptr)
    {
        unmeasured("the DirectWrite factory is not available; the zh-cn fallback is not checked");
        return;
    }
    const auto japanese =
        fallback_family(*factory.Get(), ui_locale_name(DisplayLanguage::japanese));
    const auto chinese =
        fallback_family(*factory.Get(), ui_locale_name(DisplayLanguage::simplified_chinese));
    std::printf("  fallback of U+76F4: ja (L\"\") = %ls, ja-jp = %ls, zh-cn = %ls\n",
                japanese.c_str(), fallback_family(*factory.Get(), L"ja-jp").c_str(),
                chinese.c_str());
    if (chinese.empty())
    {
        unmeasured("no font maps U+76F4 for zh-cn; the zh-cn fallback is not checked");
        return;
    }
    expect(!japanese_family(chinese),
           "the zh-cn locale of the screen text falls back to a non-Japanese (simplified) font");
}
} // namespace

void verify_language_texts()
{
    verify_dialog_texts();
    verify_os_texts();
    verify_screen_texts();
    verify_widths();
    verify_chinese_fallback();
}
} // namespace nenenib::tests::ui
