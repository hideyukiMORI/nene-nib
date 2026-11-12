# ADR 0185 — アプリアイコンの正本は SVG 1 つで、ICO と MSIX のロゴは道具 1 本の生成物。一致は画素で確かめる

- 状態: 提案（実装席 ICON2・2026-11-13・設計席の受理待ち）
- 日付: 2026-11-13
- Issue: #1580
- 規則: ARC-001 / ARC-012、CPP-009、CNF-005 / CNF-007、QLT-007 / QLT-010 / QLT-014、施主決定 D87 / D88

## 文脈

施主決定 D87（案 A・角の半径 24）と D88（面 `#461136`）で図案は決まった（`D:/NeNeNib/design/app-icon-20261111/round3/candidates/A-best.svg`）。製品は exe にアイコンを持たず、窓の class も `hIcon` を持たないので、エクスプローラー・タスクバー・Alt+Tab は既定の絵を出していた。Store の申請（D88）は MSIX のロゴ一式を要る。

NeNe Loupe（`eng/render-app-icon.py`・25 枚）と NeNe Folio（ADR 0088・`eng/windows-icons.json`）は同じ問題を「SVG 1 つ → headless Chrome → ICO と PNG」で解いている。描画の道具の版で PNG のバイト列が揺れること、CI に Chrome と Pillow が無いことが残る。

## 決定

1. **正本は `docs/design/app-icon.svg` の 1 つ**（A-best.svg とバイト列が同じ・SHA-256 `d4eb442e…fbea7`）。図形や色を C++・rc・別の絵に写さない。図案を変えるときは SVG を変えて生成し直す。
2. **生成の入口は `eng/render-app-icon.py` の 1 本**。置き場・大きさ・描画の道具の版・生成物の SHA-256 は `eng/app-icon.json` が持つ（CNF-007 の `config-bindings.json` に登録）。
   - ICO `src/app/NeNeNib.ico`: 16 / 20 / 24 / 32 / 40 / 48 / 64 / 256。256 は PNG、ほかは 32 bit の DIB と透明の AND mask（Loupe と同じ形）。
   - MSIX `src/app/msix/`: Loupe の `package-msix.ps1` が要求する 25 枚と同じ名前と大きさ（`Square44x44Logo` `Square150x150Logo` `StoreLogo` の scale 100 / 125 / 150 / 200 / 400 と、`Square44x44Logo.targetsize-{16,24,32,48,256}_altform-{unplated,lightunplated}`）。
   - Store の掲載の 300 × 300: `docs/images/store/store-logo-300.png`。
3. **描画は design の記録と同じ道具と設定**（Chrome headless・背景は透明・device scale 1・root の width / height だけを大きさに替える）。版は `eng/app-icon.json` の `renderer.version` に記録する（初回 157.0.8103.52）。Chrome は自動で更新されるので版は「固定」できず、記録にとどめる。
4. **画素を道具の中で符号化し直す**。Chrome の PNG はそのまま置かず、標準ライブラリだけの決定的な符号化（行ごとに差の小さい filter・zlib 9）で書く。生成物のバイト列は画素だけで決まり、道具は Pillow を要らない。
5. **一致の確かめは 2 段**。
   - `--verify`（Chrome 不要・CI の `test-conformance.py` が回す）: 生成物の集合・大きさ・ICO の目次・SHA-256 が `eng/app-icon.json` と同じ。
   - `--check`（Chrome が要る・手元）: 正本から描き直した画素と、commit 済みの PNG と ICO の各 frame の画素が 1 画素も違わない。Chrome の版が記録と違えば 1 行知らせ、比べるのは画素。違ったら閾値を足さず、原因を調べてから生成し直す（QLT-010）。
6. **exe と窓**: `src/app/NeNeNib.rc` が `NENENIB_ICON_RESOURCE ICON "NeNeNib.ico"` を持ち、番号は `src/ui/win32/AppIconResource.hpp` の 1 か所（rc の前処理器が読むので `#define`）。窓の class（`EditorWindowLifecycle.cpp` の `register_editor_class`）は `LoadImageW(…, LR_DEFAULTSIZE | LR_SHARED)` で `hIcon` だけを結び、`hIconSm` は空にして OS に同じ resource から小さい大きさを選ばせる。`LR_SHARED` は resource の名前で cache するので、大きさ違いを 2 回引くと同じ handle が返る（Folio の ADR 0088 が避けた点）。共有の handle は解放しない。ファイルは読まない。覆いの子窓 `CoverWindow` にはアイコンを持たせない。タイトルバーは自前の描画で、帯にアイコンを描かない（見た目の採用案に無い）。
7. CMake は rc に `OBJECT_DEPENDS` で ICO とヘッダを名指しする（rc の依存の走査は ICON のファイルを拾わない）。

## 結果

- 起動の道に足したのは `LoadImageW` 1 回（exe の resource から既定の大きさの 1 frame を作る）。速さは設計席が測る（QLT-014・この ADR の時点で未測定）。
- `--verify` は CI で守られる。`--check` は Chrome のある機械でだけ回る（Chrome は CI に入れない）。
- DPI ごとの大きいアイコン（Folio の決定 3 の `WM_SETICON`）はしない。タスクバーと Alt+Tab は OS が exe の resource と class のアイコンから選ぶ。拡大で滲むと分かったら、Folio の形を後続で検討する。
- MSIX を作る道具・`resources.pri`・Store の申請は別の Issue。

## 確かめていないこと

- エクスプローラー・タスクバー・Alt+Tab での実際の見え方（窓を前面に出す道具を回していない・施主の試用で確かめる）。
- 125 % 以外の DPI での大きさの選ばれ方。
