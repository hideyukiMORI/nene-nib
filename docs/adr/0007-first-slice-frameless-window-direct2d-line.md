# ADR 0007 — 最初の縦切り: 枠なし窓に Direct2D で 1 行描き、OS のライト／ダークに従う

- 状態: 受理
- 日付: 2026-09-15
- Issue: #3
- 影響する規則: ARC-002 / ARC-003 / ARC-004 / ARC-005 / ARC-006 / ARC-007 / ARC-011 / CPP-005 / CPP-007 / CPP-013 / CPP-017 / QLT-009 / QLT-013 / CNF-007

## 文脈

Issue #1 で規約とゲートは置いたが製品モジュールが無く、`eng/symbols.py` は「0 libraries checked」で何も守っていない。
INIT_PROCEDURE の Phase 3 は「起動して 1 つの値を表示する程度」の縦切りを憲章どおりの層で通し、規約が実装に耐えるかを確かめる段である。
Nib でその最小形は「枠なし窓に Direct2D で 1 行描き、OS のライト／ダーク設定に従う」で、Phase 0 の D1（[phase0-results.json](../quality/phase0-results.json)）で
実測した提示経路をそのまま製品に置く。

## 決定

**5 層すべてに正典の経路を 1 本ずつ作り、それ以上を作らない。**

| 層 | 置くもの | 正典の経路 |
| --- | --- | --- |
| `src/core` | `DisplayText`（検証済みの表示文字列。空・制御文字・不正 UTF-8 を `TextFailure` で拒否する唯一のファクトリ）、`Appearance`（`light` / `dark` の閉じた集合）、`RgbColor`、`Palette`（`Appearance` → 背景・文字色の純関数。dark は施主決定 D11 の茄子色 #300A24 / #EEEEEC、light は #F4F5F7 / #1B1F24） | 値は全部ファクトリ経由。`{}` で作れる公開 aggregate は `RgbColor` と `Palette` だけ（全メンバーが独立に妥当な値の集まり） |
| `src/application` | `AppearancePort`（`Appearance current() const noexcept`）、`EditorState`（`DisplayText` と `Appearance` の唯一の所有者）、`EditorIntent`（`refresh_appearance` の閉じた集合）、`EditorFrame`（表示値: 文字列と `Palette`）、`EditorController`（意図 → 次状態 → `EditorFrame`） | 状態遷移は `EditorController::apply(EditorIntent)` の 1 か所。UI は `EditorFrame` を写すだけ |
| `src/adapters/win32` | `Win32AppearanceAdapter`（`AppsUseLightTheme` を `RegGetValueW` で読む）。読めなければ `dark` に落とさず、`AppearanceReadFailure` を返して application が既定 `Appearance::dark` を選ぶ（ARC-009 の型のある失敗） | OS のテーマを読む唯一の場所。advapi32 はこのモジュールだけが結ぶ |
| `src/ui/win32` | `EditorWindow`（`WS_POPUP`・PMv2 DPI・`WM_NCHITTEST` は `HTCAPTION`・Esc で `DestroyWindow`・`WM_SETTINGCHANGE` で `refresh_appearance` の意図を発行。`create` は `std::unique_ptr` で返す＝HWND の `GWLP_USERDATA` が `this` を指すため動かさない）、`Direct2DRenderer`（D3D11 device → DXGI composition swap chain（flip・waitable）→ DirectComposition visual → D2D device context・DirectWrite `Segoe UI Variable`）、`render(EditorFrame)` | 描画は `render` だけ。`WM_PAINT` では `ValidateRect` して何もしない。描く必要は意図の結果（フレームが変わった）と `WM_SIZE` / `WM_DPICHANGED` でだけ生じる。タイマーは無い |
| `src/app` | `wWinMain`: adapter → controller → window を結び、メッセージループ、終了コード | 端末出力と終了コードを持つ唯一の場所。起動できなかった理由は `MessageBoxW` で 1 行 |

- 提示経路は D1 と同じ: WARP ではなく既定のアダプタで `D3D11CreateDevice`（失敗したら WARP に落ちる。どちらで動いたかは `EditorFrame` に載せない＝業務状態ではない）
- device lost（`DXGI_ERROR_DEVICE_REMOVED` / `D2DERR_RECREATE_TARGET`）は `RenderFailure` の閉じた結果で `EditorWindow` に返り、レンダラを作り直す。例外にしない
- 版の入力は `CMakeLists.txt` の `project(... VERSION)` だけ。manifest（PMv2 DPI・版）と VERSIONINFO は configure で生成し、CNF-007 が固定 manifest と版リテラルを拒否する（Loupe Issue #17 の移植）
- 単体テストは core / application だけを対象にし、OS 資源を使わない。`--coverage-negative` で失敗系を省いた実行が QLT-009 で落ちることを毎回確かめる
- `WaitForSingleObjectEx` で frame latency waitable object を待つのは提示経路の一部で、並行性の導入ではない（CPP-013 に明記）。`reinterpret_cast` は書かず、`IUnknown**` は `ComPtr<IUnknown>` ＋ `As()`、`GWLP_USERDATA` と `LPARAM` は `std::bit_cast`（CPP-009）
- 実機の確認は `eng/verify-window.py`（ctypes だけ・隔離した環境変数・窓クラス名で特定・矩形と中心画素・`WM_CLOSE`・終了コード）で行い、gate-proofs 第 5 節に環境と結果を書く。CI とテストは窓を作らない

## 強制

- ARC-002 / ARC-003 / ARC-007 / CPP-013: この Issue で core / application の静的ライブラリが生まれるので、`eng/symbols.py --require core application` を `eng/check.ps1` に結線して **active** にする（negative proof は実ライブラリで取り直す）
- QLT-009: `eng/coverage.py` を結線して **active**（分岐 90%・反例）
- CNF-007: `version_metadata_checks` を移植して **active**
- ARC-011 / CPP-017（意図と表示値の分離・`render` だけが描く）: **planned**（レビュー事項）

## 結果

- 得られるもの: 5 層の正典経路、シンボル検査とカバレッジが本物の対象を持つ、D1 の提示経路が製品に載る
- 失うもの: 1 行を描くだけの窓に D3D11 / DXGI / DComp / D2D / DWrite の 5 つの COM を持つ。これは意図的で、次の縦切り（テキスト表示・編集）が同じ経路に載る
- 正直に記録しておくこと: Mica・自前タイトルバー・IME・タブは入れない。dwmapi は結ばない（呼ぶ API が無い）。分岐カバレッジは `default` を書かない方針のため到達不能の辺が 3 本残り、64 分岐中 61（95.31%）で止まる。窓は生成直後に一瞬 (0,0) に出てから中央へ動く。device lost の再生成経路は書いたが実機で誘発していない。`WM_SETTINGCHANGE` でテーマに追従するが、Windows 11 の「アクセント色」は読まない。実 GPU の遅延は測らない（QLT-014 は planned のまま）

## 却下した選択肢

| 選択肢 | 却下の理由 |
| --- | --- |
| GDI で 1 行描いて D2D は次の縦切りで | 提示経路を 2 回作ることになる。ADR 0002 が GDI を使わないと決めている |
| `WM_PAINT` で描く | flip model の swap chain は `WM_PAINT` の枠に載らない。描く必要を意図とサイズ変更に限り、`WM_PAINT` は `ValidateRect` だけにする |
| タイマーで再描画する | 「描く必要が生じたときだけ描く」の逆。速さの原則（ADR 0006）に反する |
| テーマの読み取りに `ShouldAppsUseDarkMode`（非公開 API） | 非公開・順序が変わる。レジストリの `AppsUseLightTheme` は文書化されている |
| adapters を作らず ui でレジストリを読む | ARC-003 / ARC-007 の例外区画が増える。テーマは application が所有する状態で、UI は写すだけ |
| 単体テストの枠組みに Catch2 / GoogleTest | 依存が増える（DEVELOPMENT_WORKFLOW 第 6 節）。Loupe と同じ自前の最小ハーネス（`main` と `expect`）で足りる |

## 関連

- [ADR 0002](0002-plain-win32-with-direct2d.md)・[ADR 0003](0003-cpp23-clang-cl-foundation-and-measured-limits.md)
- nene-loupe ADR 0004（最初の縦切り）・nene-folio ADR 0004
