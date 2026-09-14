# ADR 0002 — UI ライブラリを使わず、素の Win32 ＋ Direct2D / DirectWrite で描く

- 状態: 受理
- 日付: 2026-09-15
- Issue: #1
- 影響する規則: ARC-002 / ARC-003 / ARC-006 / ARC-011 / CPP-017 / QLT-013 / QLT-014

## 文脈

NeNe Nib は「Windows 11 で考えうる限り速い、単体 exe のテキストエディタ」である（[SPECIFICATION.md](../../SPECIFICATION.md) 第 1 節）。
施主の優先順位は 1 に速さとモダンな見た目、2 に Vim の再現度。決定 D1（Windows 11 専用）と D2（Qt / X11 / WinUI 3 / Electron / WebView を
使わない）は施主決定として与えられている。

先行の Loupe と Folio は素の Win32 ＋ GDI で描いた。Nib は 1 GB のファイルの表示・キー入力からの即時描画・Mica・滑らかなスクロールを
要求するので、GDI の CPU 描画と `WM_PAINT` 駆動では「キー→画面」の遅延と大画面の再描画で天井が見える。

Phase 0 で実測した（[phase0-results.json](../quality/phase0-results.json) D1-directx-static-clang-cl / D1-directx-static-cl）:
WARP の D3D11 device・Direct2D 1.3・DirectWrite 3・DXGI 1.6 の **composition 用 flip model（`FLIP_SEQUENTIAL`）＋ waitable swap chain**・
DirectComposition・DWM を静的リンクの exe から窓なしで順に呼び、Clear して Present するまで終了 0。exe の import は
`d2d1` `DWrite` `dxgi` `d3d11` `dcomp` `dwmapi` `KERNEL32` だけで、CRT の DLL は無い。Mica の `DWMWA_SYSTEMBACKDROP_TYPE` は
定数の存在だけを確認した（適用には HWND が要る）。

## 決定

**UI ライブラリを使わない。素の Win32（`WS_POPUP` 相当の枠なし窓・自前のタイトルバーとタブ・`WM_NCHITTEST`・IMM32 / TSF）を土台に、
描画は Direct2D ＋ DirectWrite、提示は DXGI の flip model ＋ waitable swap chain を DirectComposition で窓へ結び、背景は DWM の Mica で作る。
GDI では描かない。**

- 描画は `src/ui/win32` の唯一の区画に閉じる。application は表示値（どの行に何を、どの色で）を作り、UI はそれを写すだけ（ARC-011）
- 変わった行だけ再描画する。グリフはキャッシュし、フレームは waitable object で刻む。「キー→画面」はベンチ（QLT-014）で見張る
- OS ライブラリの許可表（`eng/architecture.json` の `platformLibraries`）に `ui_win32` の `user32` `d2d1` `dwrite` `dxgi` `d3d11` `dcomp`
  `dwmapi` `imm32` を最初から書く。これ以外を結ぶには ADR が要る
- COM の所有は RAII（`Microsoft::WRL::ComPtr`。SDK 同梱・ヘッダのみ）で閉じる。device lost は型のある結果として application へ返し、
  例外にしない（ARC-010）

## 強制

- **planned**: CMake のターゲットグラフで `core` と `application` が DirectX の lib を結べず、`d2d1.h` 等を含められないこと（ARC-002 / ARC-003）。
  `eng/symbols.py` が中核の `__imp_*` を拒否する。字句検査が中核の DirectX ヘッダを拒否する
- **不能**: 「UI ライブラリを後から足さない」こと自体。依存の追加は ADR 事項（DEVELOPMENT_WORKFLOW 第 6 節）で担保する
- 実機の描画（DPI・複数モニタ・Mica の見え・IME の候補窓）は QLT-013 の環境依存の確認として `docs/quality/gate-proofs.md` に別に記録する。
  CI とテストは WARP でしか描かない

## 結果

得られるもの:

- 単体 exe（ランタイム依存なし）、GPU の文字描画、flip model による最小の提示遅延、Mica と OS テーマの追従
- 憲章の「中核をプラットフォームから独立させる」が守りやすい。COM が全域に染みない

失うもの:

- 標準部品（メニュー・ダイアログ・アクセシビリティ）を自前で描く。UI Automation は初版の範囲外
- Direct2D の device lost・DPI 変更・モニタ跨ぎを自分で扱う。描画の実機証明はゲートに入らない
- GDI と違い、単体テストで画素を読む経路が無い。表示値（application）を厚くテストし、描画は実機で見る

正直に記録しておくこと:

- flip model と waitable swap chain は WARP で「呼べる」ことしか測っていない。実 GPU の遅延は Phase 3 のベンチで測る
- Mica は Windows 11 22H2 以降。D1 の決定どおり Windows 10 は対応しない

## 却下した選択肢

| 選択肢 | 却下の理由 |
| --- | --- |
| C++ ＋ WinUI 3 / WPF / Electron / WebView2 | 施主決定 D2。ランタイム依存・起動時間・配布物サイズが「爆速の単体 exe」と両立しない |
| 素の Win32 ＋ GDI（Loupe / Folio と同じ） | CPU 描画で GPU の文字描画とグリフキャッシュが無く、flip model の提示経路と Mica を組み合わせられない。1 GB のファイルの再描画と「キー→画面」の遅延で天井が見える |
| Direct3D の自前レンダラ（文字をアトラスで描く） | DirectWrite が持つ字形・合字・IME・フォールバックを自分で書き直すことになる。速さの利益が要件に見合わない |
| Skia / その他の描画ライブラリ | 実行時依存が増え、静的リンクしても exe が数 MB 増える。DirectX は OS 同梱で依存が増えない（D1） |
| Rust ＋ windows-rs | 規約の機械強制は最強だが、本リポジトリの言語は C++23（施主決定） |

## 関連

- SPECIFICATION.md 第 3 節（技術の土台）
- [ADR 0003](0003-cpp23-clang-cl-foundation-and-measured-limits.md)（COM の `__uuidof` と `-Wno-language-extension-token`）
- nene-loupe ADR 0002（素の Win32 を選んだ理由。Nib はそこから描画だけを変える）
