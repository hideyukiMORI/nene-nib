# 速度と安定性の向上の日 — 2026-10-27（設計席 Fable・hide 不在）

hide の指示「止めるまで速度向上・安定性向上の実験を続け、ほんの少しでも速度が上がるのを目標に最善を尽くす。試したこと・変えたことは全部文書に残す」。設計・判断は Fable の設計席、実装・調査・レビューは Opus / Sonnet の背景の席（ADR 0038 / 0039・実装席 22・レビュー席 14・調査席 1）。製品の性能改善は D41（独立レビュー・正式ゲート・CI・hide の試用）で、merge は hide の試用の後。試験と道具と文書と製品の破れの直しは merge の裁量で main に入れた。機械は hide の実機（i9-10850K / RTX 3090 / 120 dpi・指紋 `bc8a356f37c68491`）。席のビルドと重なった計測は「負荷あり」と書き、比較は ABAB の交互で読む。基準値（2026-10-18 採用）は変えていない。

## 1. 計測の道具が増えたこと

- #844 / PR #846（main `1284539`）: 打鍵の道に `frame_built`・`frame_drawn` の節目。単打のベンチの記録に「作る / 描く / 提示」の段が載る（独立レビュー R 2 件は試験の抜けで次の節目の席へ）。
- 診断の exe（`experiment/bimodal-marks`・main に入れない・`D:\NeNeNib\worktrees\bimodal-diag`）: 前日までの `key_delivered` / `paint_entered` / `text_layout_created` / `fallback_mapped` / `layout_created` / `draw_ended` / `latency_waited` / `presented` に、今日 `render_cleared` / `render_title_done` / `render_body_done` / `render_status_done` / `render_other_done` / `render_draw_ended`（d69a22a）を足した。`experiment/875-diag`（ada837a）は `fallback_begun` / `script_analyzed` / 保持の当たり・外れ / `fallback_assigned`。集計は `D:\NeNeNib\scripts\bimodal-trials.py`（試行ごとに raw marks を残し summary.md と csv に）。
- 対照の計測 `D:\NeNeNib\scripts\preview-control-trial.py`（同じ文書でプレビューあり / なしを ABAB）。
- 画面に出るまでの時刻（#873・`experiment/present-latency`・`D:\NeNeNib\worktrees\873-present-latency`）: `IDXGISwapChain::GetFrameStatistics` の `SyncQPCTime`（`PresentCount` が自分の番号に届いた frame・失敗時は DComp の `lastFrameTime`）で「input → 画面」を tsv に書く実験 exe と集計 `D:\NeNeNib\outputs\20261027\present-latency\summarize.py`。合成は 60 Hz（`EnumDisplaySettings` の 30 Hz は信用しない）。
- 起動の床の exe（#872・`D:\NeNeNib\experiments\startup-floor\`）: 製品と同じ import の空の `wWinMain` が process 生成からの時刻を書く。

## 2. 分かったこと（段の内訳・中央値 ms・静かな回）

| ベンチ | 計 | 作る | 描く | 提示 | 中で大きいもの |
|---|---:|---:|---:|---:|---|
| 空の文書の 1 打鍵 | 0.57 | 0.13 | 0.37 | 0.07 | 描く: EndDraw 0.19・タイトルバー 0.07・ステータスバー 0.055・本文 0.01 |
| 構文の色つき（cpp） | 0.90 | 0.38 | 0.46 | 0.07 | 字句器と色の run |
| 長い日本語行 | 1.93 | 1.41 | 0.43 | 0.08 | **字体選択 0.9〜1.1**（区切りごとの保持の当たり 1080 回）・AnalyzeScript 0.16 |
| プレビューを開いた 1 打鍵 | 2.61 | 1.24 | 1.30 | 0.06 | 作る: 文書の長い日本語行（同上）・描く: 全ブロックの `DrawTextLayout` 0.93 |
| Ctrl+P の面（診断 exe・古い base） | 3.0 | 0.5〜1.6（絞り込み） | 1.1 | — | 面の描画 0.9 |
| 格子の 1 行 16 MiB | 1.01 | 0.18 | 0.77 | 0.06 | 格子の描画 |

- **長い行の 2 山**（約 1.5 / 1.95・2026-10-22 から未解明）: 1 試行ずつ 12 回起動すると起動ごとに速い群か遅い群に入る（プロセスごとに決まる）。段は `paint_entered → layout_created`（字体選択）の中で、描く段と提示の段には無い。字体選択の中は `AnalyzeScript` 0.11〜0.16 と区切りごとの `MapCharacters` 0.68〜0.93 で、`FontFallbackCache` の当たりが 1 打鍵に 1080 回・外れ 1 回。日本語の文は漢字とかなが交互なので区切りが約 2 単位ごとになり、当たりでも 1 回 0.85 µs（`fallback_key` の COM の往復 4 回・`std::wstring` 3 本・線形探索）が積もる。さらに OS の `MapCharacters` は同じ family でも毎回別の `IDWriteFont` を返すので `joins` の `family_of` が区切りごとに `english_family`（COM 4 往復）を呼んでいた。2 山はこの 1080 回の費用の揺れ（保持の並び = 線形探索の深さ）と読める。
- **打鍵が画面に出るまで**: 現状 `Present(1, 0)` で中央値 37.2 ms（2 vblank 超）。`Present(0, 0)` 25.0・＋`SetMaximumFrameLatency(1)` 18.5・＋待機可能オブジェクトの待ちを描く前へ 14.5 ms。ベンチ（`input_received → frame_presented`）には現れない。
- **起動**: origin 11.3 ms は同じ import の空の exe で 9.9 ms（kernel32 だけ 5.15・user32 だけ 7.64）。製品の固有分は約 1.5 ms（CRT・`CoInitializeEx`・`CommandLineToArgvW`）。製品のコード由来の静的初期化子は 0。`/DEPENDENTLOADFLAG:0x800`・ole32 / shell32 / advapi32 の DELAYLOAD（最初の呼び出しが窓の前）・msctf の worker での先読みは効かない。`SW_SHOWNOACTIVATE` → 最初の frame の後に `SetForegroundWindow` で、最初の frame が 45.3〜48.8 → 35.2〜39.3 ms（約 10 ms・20%）。活性化（IME / TSF の文脈）は消えずに後ろへ移る。振る舞いが変わるので hide の判断（#895）。
- **プレビュー**: 作る側の +1.1 ms はプレビューではなく文書（`preview-1000.md`）の長い日本語行の字体選択（対照計測で確認）。描く側の +0.93 は全ブロックの `DrawTextLayout`（保持の探索・効果・矩形の 3 つを直しても差 0・PR #901 は閉じた）。
- **Ctrl+P の面**: 面の描画 0.9 ms は行ごとの text layout の生成（PR #904 で −12〜18%）。絞り込み 0.5〜1.6 ms は段 2（#900 の計画）。
- 字句器の表引きの門（#859・前日の続き）: Go 0.69〜0.74・C# 0.78・Rust 0.79〜0.84・Lua 0.78〜0.86 倍（ns/byte・probe・分類不変）。

## 3. 直したもの（PR・効果・状態）

| Issue | PR | 内容 | 効果（静かな回・中央値 ms） | 状態 |
|---|---|---|---|---|
| #859 | #874 | 字句器の表引きの門（表から `consteval` で作る先頭バイトと長さの門） | probe で表の大きい言語 0.69〜0.86 倍 | **main** `215162b` |
| #884 | #885 | 字体選択: 区切りの文字列 → 字体の覚え（`FallbackSegmentMemo`・FNV-1a・開番地法・4096 件） | 長い行 1.5〜1.9 → 1.37〜1.50 | Ready・レビュー済み・試用待ち |
| #889 | #891 | 字体選択: 覚えの字体を family・weight・style・stretch で正規化し `joins` を値で比べる（#885 の上） | 長い行 → **0.84〜1.18**（ゲート 0.837・基準 1.574 の 53%） | Ready・レビュー済み・ゲート 0 regression・試用待ち |
| #888 | #892 | 描く前に待機可能オブジェクトを待つ・`SetMaximumFrameLatency(1)`・`Present(0, 0)`（ADR 0007 の追補） | 画面に出るまで 37.2 → 14.5 ms（実験の同じ形）・ゲート 0 regression・PNG 差 0 | Ready・レビュー済み・試用待ち |
| #900 | #904 | Ctrl+P の面の行の text layout の保持（`PaletteRowLayouts`・32 件・LRU） | 面 1.78〜1.85 → 1.46〜1.57（ゲート 1.548・基準 1.729） | Ready・レビュー済み・ゲート 0 regression・試用待ち |
| #903 | #907 | プレビューの枠をオフスクリーンの `ID2D1Bitmap1` に描き、鍵が同じ frame は `DrawBitmap` 1 回（ADR 0155 の追補） | プレビューを開いた 1 打鍵 2.5 → **1.5**（描く段 1.4 → 0.36）・PNG 3 枚とも差 0 | Ready・レビュー済み・ゲート 0 regression・試用待ち |
| #912 | #914 | Ctrl+P の照合に ASCII だけの query のバイト列の速道（核は `SubsequenceScore` の 1 つ・同値は性質の試験 10000 件 × 2）と `reserve`（ADR 0091 の追補） | 面 1.81〜2.12 → 1.42〜1.52（−20%・PNG 差 0） | Ready・レビュー済み・試用待ち |
| #924 | #925 | タブの題名の text layout を `PaletteRowLayouts` と同じ保持から引く（毎 frame 作っていた唯一の layout・ADR 0056 の追補・#904 の上） | 空の文書の 1 打鍵 0.57 → 0.48〜0.52（−12%・描く段 0.36 → 0.29〜0.31・PNG 4 枚差 0） | Ready・レビュー済み・試用待ち |
| #887 | #890 | `parse_markdown` が CR だけの改行の後ろの空の項目に本文の末尾の位置を置く（性質の試験で検出） | 製品の破れの直し | **main** `a250e4f` |
| #875 | #880 | 片の切り目を内容で決める | 差 0（原因が違った） | 閉じた |
| #894 | #896 | ASCII の行で `AnalyzeScript` を省く | 差が測れず（数 µs の桁） | 閉じた |
| #876 | #901 | プレビューの保持の探索・効果・矩形 | 差 0 | 閉じた（段 2 の #903 へ） |
| #906 | #909 | `VimPattern::parse` が不正な UTF-8 の pattern を閉じた失敗で断る（性質の試験で検出・製品には来ない） | 入口の守り | **main** `4dd64fe` |

重ね版（main `9456132` + #885 #891 #892 #904 #907・`experiment/perf-stack-20261027` ba91ab7）のゲート（負荷 5/4/10%）: 起動 46.6・窓 36.3・単打 0.547・連打 2.143・16 MiB open 57.6・16 MiB 連打 2.047・面 1.445・長い行 1.118 → 0 regression。任意ベンチ: プレビューを開いた 1 打鍵 1.004・cpp 0.970・格子の 1 行 16 MiB 1.190。
夕方に #914 を足した重ね版（7a9e394・exe `D:\NeNeNib\worktrees\perf-stack-20261027\build\release-7a9e394\NeNeNib.exe`・SHA-256 `36532F2B2DFC82B323E821BD0B341B6DDDC747671833A30050B585066B43B381`）のゲート（負荷 3/3/4%）: 起動 45.5・窓 35.0・単打 0.650・連打 2.491・16 MiB open 54.4・16 MiB 連打 2.752・**面 1.217**（基準 1.729 の 70%）・**長い行 1.125**（基準 1.574 の 71%）→ **0 regression**。プレビューを開いた 1 打鍵 **0.927**（朝 2.61）。見た目（打鍵・面）は main 相当と差 0 画素。hide の試用の表は `D:\NeNeNib\evidence\20261027-trial\README.md`（この exe）。

## 4. 安定性の性質の試験（固定 seed の決定的な生成器・既定実行に載る・50 倍の走りで破れを探した）

| Issue | PR | 対象 | checks / 所要（Debug） | 見つけた破れ |
|---|---|---|---|---|
| #868 | #877 **main** | 字句器 25 言語 × 不変量 4 × 入力 5 族 | 54 / 0.4 s | 不正な UTF-8 で 7 言語が 0 桁の区間（製品には来ない・#878） |
| #869 | #882 **main** | TextBuffer / EditHistory と `std::string` の模型（行・位置・仮想桁・undo の往復） | 7842 / 1.5 s | 0 |
| #870 | #883 **main** | Vim のランダムな鍵列（3 本・50 倍 = 300 本・seed 2 つ） | 11 / 0.5 s | 0 |
| #871 | #886 **main** | `parse_markdown` / `markdown_from_html` の構造と上限・線形性の probe（16 KiB 152 µs・256 KiB 2.3 ms・4 MiB 38.8 ms・9.5 µs/KiB） | 18 / 0.4 s | **CR だけの改行の `source_begin`（#887・直した）** |
| #897 | #902 **main** | 設定 v1〜v3・session v1 / v2・履歴・ブックマーク・自動退避の台帳の復号（変異・往復・上限・1 MiB の境界） | 22 / 0.7 s（別 target `nib_persisted_property_tests`） | 0 |
| #898 | #908 **main** | DetectedText の判定と復号（9 族・往復・替え玉の CP932） | 142 / 0.5 s | 0（先頭が NUL の UTF-16 LE は UTF-32 の BOM として断られる・#734 の決め方どおり） |
| #899 | #905 **main** | 検索の pattern の parse と matched（断片 24 種の閉じた失敗・範囲・決定性・後戻り） | 15 / 0.45 s | **末尾で切れた多バイトで終端の先を読む（#906・PR #909・main）** |
| #911 | #913 **main** | Ex と検索の入力行のランダムな文字列（核 300 行 + controller 4 試行・補完と履歴の鍵・替え玉への書き込みは Ex の名前の正本の形だけ） | 338 / 0.3〜0.6 s | 0（50 倍 = 核 15000 行・controller 200 試行・seed 2 つ） |
| #917 | #920 **main** | `CF_HTML` の見出しと断片の切り出し（`html_fragment_of`・位置の外・逆順・欠け・巨大・BOM・切り詰め・見出しから素朴に計算した期待 2 本と照合） | 12 / 0.5 s（別 target） | 0（50 倍 = 225,000 入力。19 桁以上のゼロ埋めの数は先頭 18 桁で読む・範囲は入力の内・直さない） |
| #923 | #926 | 構文の色の覚え `SyntaxStateCache`: 5 言語 × ランダムな編集・undo / redo・移動（4096 行超を含む）の後の見えている行の色が、遡りの基準か模型の覚えから続けた基準と一致し `syntax_lexed` も等しい（覚えと遡りの両方の道を通ることも expect） | 8 / 0.7 s | 0（50 倍 = 250 trial・7,817 frame。変異: 捨て方を緩める → 落ちる・続きを使わない → 落ちる・過剰に捨てる → 色は正しく性能だけの差なので通る） |
| #929 | #932 **main** | タブの帯の配置 `title_bar_layout` と hit test の一致（幅 360〜4000 DIP・DPI 4 種・タブ 0〜64・送り量・載せた要素。矩形の重なり・順・帯の内・× は自分のタブの内・見えていないタブに当たらない・全点走査） | 11 / 0.35 s | 0（50 倍 = 約 17 s・境目の見本 6 件を固定） |
| #930 | #931 **main** | `autosave_plan` / `clears_autosave`（12 種の操作・タブ 16 本・帳面の改訂と残骸。変わっていないタブを書かない・読んでいないタブを書かない・固定点・決定性・枝ごとの覆いを expect） | 1403 / 0.66 s | 0（50 倍 = 4,800 組。変異 3 本（store と keep の入れ替え・改訂を位置に・残骸の discard を keep）はどれも落ちる） |

ほかに道具の直し: `eng/verify-window.py` の typing の節が main でも落ちていたのは、ステータスバーの右の項目を 3 つ（言語の欄 #776 の前）として数えていたため → 4 項目の正本に合わせた（#893・PR #918・main）。palette の節は ADR 0150（面の最初の候補の配色のプレビュー）の後の振る舞いと合わず落ちる → #919。

## 5. beta.7

main `c5e7285`（#859 まで・記録 [beta7-release-2026-10-27.md](beta7-release-2026-10-27.md)）から梱包（`D:/NeNeNib/releases/v0.1.0-beta.7/`・exe SHA-256 `9cba0f717b028bc85389506e1cb63d8084100e500875ec5eeaf029e8a61f1cbb`）・smoke pass・ゲート 8 本 0 regression（負荷 32/25/19%）。**tag と pre-release の作成は自動の権限（Create Public Surface）で止まった** → hide の手か権限で出す。

## 6. 効かなかったこと・決めなかったこと

- 片の切り目を内容で決める（#880）・ASCII 行の `AnalyzeScript` 省略（#896）・プレビューの保持の 3 点（#901）: 測って差が無かったので入れない。
- 起動の origin: 床が 9.9 ms で縮める余地は 1〜2 ms。`DEPENDENTLOADFLAG`・DELAYLOAD の追加・msctf の先読みは効かない。
- `SW_SHOWNOACTIVATE`（#895）: 数字は出たが振る舞いの変更なので hide の判断。
- `eng/verify-window.py` の既定の節の `verify_typing` は main でも落ちていた（#893・右の項目の数が 3 のまま）→ 直した（PR #918）。palette の節は #919。
- 描く段の EndDraw 0.18〜0.20 ms: 1 frame の命令は約 34（Fill 12・Line 7・Rect 1・DrawTextLayout 12・DrawTextW 1・GlyphRun 1）と clip 8 組・SetColor 約 26 で、命令の数に比例するか未検証（一定の提出費なら削っても効かない）。毎 frame 作る layout はタブの題名の `DrawTextW` だけ（調査 `D:/NeNeNib/outputs/20261027/probe-draw-primitives.md`）→ 今日は追わない。
- Ctrl+P の絞り込みを前の結果から続ける（#910）は ADR 0091 で実装済みだった → 閉じ、残りの費用（名前で外れた候補の場所の再照合）は #912 / PR #914 で縮めた。

- `tests/unit/NibTests.cpp` の契約の表は 54 行で `verify_vim_scope_contracts()` の関数長の上限（clang-tidy・QLT-010 で閾値は触らない）に当たった → 表を関数の外の `constexpr` の配列に出した（PR #931・CPP-012）。席のビルドの確かめで PowerShell の `$LASTEXITCODE` が pipeline の後ろの値になり「build exit=0」と出ていたのに `.obj` は FAILED だった → ビルドのログを `FAILED|error:` で grep する。

## 7. 記録の所在

`D:/NeNeNib/outputs/20261027/`（計測のログ・レビュー 14 本・診断の summary・A/B）、`D:/NeNeNib/briefs/20261027/`（依頼書と Issue の原稿）、`D:/NeNeNib/evidence/beta7-release/`・`20261027-trial/`。
