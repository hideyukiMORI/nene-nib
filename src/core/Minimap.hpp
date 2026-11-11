#pragma once

#include "ColorSpan.hpp"
#include "EditorSettings.hpp"
#include "LineNumber.hpp"
#include "MinimapBlock.hpp"
#include "MinimapWindow.hpp"
#include "Palette.hpp"
#include "RgbColor.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

namespace nenenib::core
{
// ミニマップ（ADR 0171・施主決定 D70・FR-030）の値と純関数。描くのは ui/win32、窓と行の塊を作る
// のは application、ここはどちらも使う寸法と規則の 1 か所。

// 設定 v4 の features の名前（ADR 0170 の決定 3・既定オフ・`:set minimap`）。
inline constexpr std::string_view minimap_feature = "minimap";
// 面の幅（DIP）と 1 行の高さ（物理画素）と 1 行の桁の上限（決定 2）。1 桁 = 1 物理画素。
inline constexpr std::int32_t minimap_width_dips = 80;
inline constexpr std::int32_t minimap_row_pixels = 2;
inline constexpr std::size_t minimap_columns = 100;
// 窓は見えている所の前後 minimap_reach 行（決定 4）。編集で行が増えて窓が
// minimap_window_limit 行を超えたら、窓を作り直す（bitmap の高さを抑える）。
inline constexpr std::size_t minimap_reach = 2'000;
inline constexpr std::size_t minimap_window_limit = minimap_reach * 2 + minimap_reach / 2;
// 塊を作るのに 1 行から読むバイトの上限。上限を超える行は先頭だけを読み、色を付けない（巨大な
// 行の窓と同じく行の全体を写さない・限界）。100 桁は UTF-8 で 400 バイトに収まる。
inline constexpr std::size_t minimap_line_bytes = 4'096;

// 見えている範囲の面と縁の線の色（ADR 0171 の決定 2 の追補 #1487）を選ぶしきい値（コントラスト比の
// 10 倍・1.5）。accent が背景からこれだけ離れていなければ本文の字の色 text にする。
inline constexpr std::int64_t minimap_face_contrast_tenths = 15;

// 2 色のコントラスト比が tenths / 10 以上か。輝度は sRGB の線形化を 2 乗で近づけた整数の近似
// （Y = 2126·r² + 7152·g² + 722·b²・比は (Y + 0.05) の比・core で libm を呼ばない）。
[[nodiscard]] bool minimap_contrast_at_least(RgbColor first, RgbColor second,
                                             std::int64_t tenths) noexcept;
// 面と縁の線の色。accent と background が minimap_face_contrast_tenths 以上離れていれば accent、
// 未満なら text（accent が背景とほぼ同じテーマ・synthwave-84 など）。ui は色を選ばない。
[[nodiscard]] RgbColor minimap_face_color(const Palette &palette) noexcept;

// 設定でミニマップを出すか（features に名前がある）。設定が変わったときだけ読む（決定 1）。
[[nodiscard]] bool minimap_shown(const EditorSettings &settings) noexcept;
// features の名前を足す・外した設定（ほかの名前の順は変えない・足すときは末尾）。
[[nodiscard]] EditorSettings minimap_set(EditorSettings settings, bool shown);

// 1 行の塊の列（決定 3）。line は改行を含まない 1 行、colors はその行の構文の色の区間（桁は
// code point の序数）。空白（U+0020・Tab・U+3000）は塊にせず、Tab は次の 8 の倍数まで進み、全角は
// 2 桁。同じ役割で隣り合う文字は 1 つの塊にまとめ、minimap_columns 桁で切る。
[[nodiscard]] MinimapRow minimap_row(std::string_view line, std::span<const ColorSpan> colors);

// 面の最上段に来る行（VS Code と同じ比例の送り）。文書が面に収まれば 1 行目。見えている最初の行
// first_visible の位置の割合を、面に入りきらない行の範囲に写す（first_visible
// より下にはならない）。
[[nodiscard]] LineNumber minimap_top(LineNumber first_visible, std::size_t visible,
                                     std::size_t total, std::size_t rows) noexcept;

// 面の上の点 y（物理画素）が指す面の段（0 始まり・面の上端 panel_top から 1 段 =
// minimap_row_pixels）。面の外（ドラッグで上下へ出た点）は負か rows 以上のまま返す（切り詰めは
// minimap_pointed_first）。
[[nodiscard]] std::int32_t minimap_pointed_row(std::int32_t y, std::int32_t panel_top) noexcept;

// 面の段 row を押した・ドラッグした後の、見えている最初の行（ADR 0171 の決定 5 の追補）。見えて
// いる範囲の面（visible 段）の中央が row に来る所（VS Code と同じ「押した所へ跳ぶ・ドラッグで
// 追従」）。結果は今のスクロールに依らず（同じ段を 2 回押しても動かない）、row について単調で、
// [1, total - visible + 1] の中。文書が面に収まれば押した段の行を中央に、面が見えている行より
// 短ければ面の段の割合を文書に写す。見えている行の数以下の文書は 1 行目。
[[nodiscard]] LineNumber minimap_pointed_first(std::int32_t row, std::size_t visible,
                                               std::size_t total, std::size_t rows) noexcept;

// 面に出す行 [top, top + rows) を含む窓（決定 4）。held がそれを含み、大きさが
// minimap_window_limit 以内で文書の中にあれば held のまま。そうでなければ top の周りの前後
// minimap_reach 行（文書の端では寄せる）。
[[nodiscard]] MinimapWindow minimap_window(const std::optional<MinimapWindow> &held, LineNumber top,
                                           std::size_t rows, std::size_t total) noexcept;
} // namespace nenenib::core
