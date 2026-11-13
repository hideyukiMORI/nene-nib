# 画面に出る文の棚卸し（2026-11-13・表示言語 T1 の下ごしらえ）

Issue #1019・ADR 0123 の追補（2026-11-13）の材料。下ごしらえの席 P1 が作った棚卸しを、T1a（文の表
`src/core/UiMessage.hpp` と列 `UiTextsJapanese.hpp` `UiTextsEnglish.hpp` `UiTextsSimplifiedChinese.hpp`）
の時点で収載した。一覧は同じフォルダの [inventory-2026-11-13.tsv](inventory-2026-11-13.tsv)（10 列・UTF-8・LF・空の欄は `-`）。

- TSV の `proposed_id` と `en` は案で、表の値の名前と英語の文の正本は列のファイル。T1a で名前を変えた所
  （例: `status_mode_ordinary` → `status_mode_basic`・同じ語の 2 か所を 1 値にした `label_bookmarks`
  `label_history`・接頭辞と理由を 1 文にした `notice_print_*` `notice_shell_registered`）は TSV を直さない。
- いま英語で出している Nib の文（§9 の問い 6）とモード名（問い 1）は、設計席の決めで表に入れない。
- §9 の問いへの答えは ADR 0123 の追補に入る（T1e）。

以下は P1 の棚卸しのまま（見出しを 1 段下げた）。

## 画面に出る文の棚卸し（表示言語 T1 の下ごしらえ P1）

読んだ木: `D:/NeNeNib/worktrees/1563-integration`（HEAD `f2742e728e30fe849bb5f5bd004546f069cf562d`・まとめ枝 27）。読むだけ。一覧は同じフォルダの `inventory.tsv`（337 行・10 列・UTF-8・LF）。
方法: `src/**/*.{cpp,hpp}` 7,797 個の文字列リテラル（連結は 1 つ・コメントと文字リテラルは除く）を字句で抜き（道具は `D:/NeNeNib/temp/p1/ext.py`）、日本語を含む 251 個と、英語で画面に出そうな約 2,800 個を目で振り分けた。TSV の `text` は `\"` `\n` を戻し、差し込む値は `{}` で示す。ソースの現状: 表示言語の希望値 `DisplayLanguageChoice`（7 値）と `settings.v4` の `ui_language` はあるが、解決済みの言語・`UiMessage`・`ui_texts` はまだ無い（`grep` で確認）。
コメントだけに日本語がある所（`//` 1 行 / `/* */` 1 個の数）: 13,746（数えない）。`u8""` と `\u` の日本語は 0。`.rc` は英語だけ（`040904b0`・FileDescription/ProductName は "NeNe Nib"）。

### 1. 件数

TSV の行（日本語の連結 1 つ・英語の 1 文を 1 行）。`vim_message` と `key_name` は個々の行を全部は挙げていない（下の「訳さない」）。

| module | screen | operation | vim_message | key_name | data | comment_like | 計 |
|---|---|---|---|---|---|---|---|
| core | 77 | 113 | 5 | 0 | 1 | 5 | 201 |
| application | 61 | 0 | 7 | 0 | 0 | 1 | 69 |
| ui | 54 | 0 | 0 | 0 | 0 | 0 | 54 |
| app | 5 | 0 | 0 | 0 | 0 | 0 | 5 |
| adapters | 8 | 0 | 0 | 0 | 0 | 0 | 8 |
| 計 | 205 | 113 | 12 | 0 | 1 | 6 | 337 |

- operation 113 = 37 操作 × (名前・読み・説明) + 短い名前 2（`list_files`=一覧, `list_operations`=ヘルプ）。`en` は読み 37 行を除く全部を埋めた（読みは英語で不要）。`en` が空なのは 37 行（すべて読み）。
- 日本語リテラル 251 の内訳: operation 113・screen 133（連結で 103 行）・comment_like 5（`static_assert` の文: ThemeDerivation.hpp:93,95 / VimKeyTable.cpp:514,516,537）。
- 英語で画面に出る screen: 約 100 行（モード名 10・Nib の Ex/map/検索の断り 約 25・テーマの失敗 15+9・知らせ 8・状態枠の規格名 8 ほか）。
- ファイルごと（多い順）: OperationTexts.hpp 113 / EditorControllerDocuments.cpp 15 / ThemeLookupFailure.cpp 15 / EditorWindowOperations.cpp 14 / EditorControllerShell.cpp 13 / SettingsNotice.hpp 12 / ModeLabel.cpp 11 / VimSearchNotice.cpp 11 / Direct2DRenderer.cpp 10 / StartScreenPainter.cpp 10 / Win32ThemeAdapter.cpp 8 / EditorControllerPalette.cpp 8 / ExResult.cpp 8 / Command・Preview・CommandChoice・ExMapCommand・PaletteMarks 各 6。全 43 ファイル。
- 訳さない（Vim と同じ英語・命令名・鍵名）で TSV に個別に挙げていないもの: `ExResult.cpp` の設定名と echo（`colorscheme=` `guide=on` `set wrap` など約 185 個）、`ExMapCommand.cpp` の map 命令名 約 42、`VimKeyNames.hpp` 27、`KeyChord.cpp` 25（`Ctrl+` `Shift+` と鍵名）、`LanguageTable.cpp:66` の filetype 名 26（`cpp` `ps1` など。ステータスバーの右端にそのまま出る）、`VimSearchNotice.cpp` と controller の E 番号つき文 約 20（TSV に 12 行だけ代表を挙げた）、`ExResult.cpp` の `on/off/suspended`。日本語で出している Vim の報せ（`vim_japanese`）は 0 件（controller の `ブックマーク…` などは Nib の文で screen）。

### 2. 文が画面に届くまでの道

どれも「言語を受け取る形」ではない。引数に言語は無く、純関数か `constexpr` の表で、`std::string_view` は静的な記憶域を指す。言語は `EditorSettings.ui_language`（application の状態）にだけある。

| 文を作るもの | 定義 | 呼び出し元 | 返す型 | 言語を渡せるか |
|---|---|---|---|---|
| `operation_texts`（37 行の表）/ `operation_text(op)` | core/OperationTexts.hpp:13 / .cpp:87 | `operation_text`: OperationGuide.cpp:16・StartScreenPainter.cpp:219。表の直接走査: CommandChoice.cpp:310 | `const OperationText&`（string_view の組） | `inline constexpr std::array<OperationText,37>`。`static_assert` が行数と順（OperationTexts.cpp:83-85）。2 次元にすれば可 |
| `mode_label` | core/ModeLabel.cpp:48 | 1（EditorController.cpp:1146）→ `EditorFrame.mode_label`(string_view) | `std::string_view` | switch の純関数。frame が string_view を持つので静的な記憶域が前提 |
| `status_items_for` / `diagnostic_count_item` | core/StatusItems.cpp:82 / :52 | 1（EditorController.cpp:143）/ 1 | `std::array<DisplayText,4>` / `optional<DisplayText>` | 純関数。「行 N, 桁 M」は固定長 buffer に 2 リテラルを `std::copy`（:42） |
| `encoding_label` `line_ending_label` `language_name` `display_form_label` | TextEncoding.hpp:25 / LineEnding.hpp:45 / LanguageTable.cpp:302 / DisplayFormLabel.cpp:7 | 各 1〜2（StatusItems.cpp・Palette.cpp:154,159・Direct2DRenderer.cpp:1946） | `string_view` | 規格名と filetype 名で訳さない。表は `constexpr` |
| `palette_marks`（6 行）/ `palette_mark_hint` | core/PaletteMarks.hpp:16 / .cpp:33 | PaletteMarks.cpp が 3 回走査 | `std::array<PaletteMark,6>` / `DisplayText` | `constexpr` の表に name（日本語）。鍵の `#` `*` `@` `/` `:` `?` は言語に依らない |
| `palette_origin_label` | core/CommandChoice.cpp:335 | 1（Direct2DRenderer.cpp:2197） | `string_view` | core の関数を renderer が直接呼ぶ |
| `save_question_text` | core/SaveQuestionText.cpp:7 | 1（ui/SaveQuestion.cpp:22 → TaskDialog） | `string_view` | 純 switch（5 値）。`SaveQuestionText.hpp` が言語に触れている |
| `settings_notice`（3 オーバーロード） | ui/win32/SettingsNotice.hpp:11,43,49 | 1（EditorWindowOperations.cpp:199 → MessageBoxW） | `const wchar_t*` / `std::wstring` | ui の inline 関数。日本語直書き 11 文 + 英語の理由文（下記） |
| `reason_of(FileFailure)` / `reason_of(WindowFailure)` | EditorWindowOperations.cpp:89 / Main.cpp:61 | 2（:192,:240）/ 1 | `const wchar_t*` | 日本語の `L""` 直書き（11 文 / 3 文） |
| `ex_failure_message(ExFailure)` | core/ExResult.cpp:1825 | application 3（Command.cpp:332,780・Palette.cpp:513）。ExEvaluationFailure.cpp:11-38 が visit | `DisplayText` | 英語直書き 13 文。`DisplayText::parse(...).value()` |
| `theme_failure_message` | core/ThemeLookupFailure.cpp:50 | 2（ExEvaluationFailure.cpp:16・SettingsNotice.hpp:45） | `DisplayText`（`{name}: {理由}`） | 理由 14 文の英語 switch（:15-47） |
| `ex_map_failure_message` / `vim_search_message` | ExMapCommand.cpp:375 / VimSearchNotice.cpp:72 | controller 経由（件数は未確認） | `DisplayText` | 英語直書き。E 番号つきは Vim の文 |
| `tab_title_for` | core/TabTitle.cpp:73 | 12（controller 9・EditorState 2・StartScreenPainter 1） | `DisplayText` | 「無題」は `untitled` 定数（:14）。試験が直接比べる |
| `print_failure_notice` | EditorControllerDocuments.cpp:1523 | 2（:1546,:1573） | `std::string` | 前置き + 理由の連結 |
| `named_notice` / `named_line` | EditorControllerDetail.cpp:148 / :154 | 3（Autosave:80・Documents:339 ほか） | `DisplayText` | prefix + 名前 + suffix。`prefix.size()` で名前の長さを決める |
| `failure_text` `plan_failure_text` | EditorControllerShell.cpp:35,46 | 1 | `string_view` | switch の日本語 |
| `prompt_text` | ui/win32/Direct2DRenderer.cpp:206 | 1（:2034） | `std::string` | 入力行の前置き（検索: 行: 置換後:）を renderer が持つ |
| controller の `constexpr string_view …_message/_notice` と `DisplayText::parse("…")` | application/EditorController*.cpp | 約 70 箇所（`with_command_message` と `reject` を合わせた grep で 74 行） | `DisplayText` | 状態の知らせ。言語は state_ から取れる |
| ui の `wchar_t[]` 直書き | EditorWindow.cpp:32・EditorWindowOperations.cpp:64,67・EditorWindowLifecycle.cpp:562 | 各 1 | `const wchar_t[]` | 静的初期化。言語を切るなら関数にする |

要点: 画面の文を決める層は core（表と switch）・application（constexpr と parse）・ui（wchar_t 直書き）・app/adapters（起動の失敗とテーマの知らせ）の 4 つに散らばる。frame を作る application が `ui_language` を持つので、「application で解いて frame に渡す」形が最小の変更で届く（最終判断は設計席）。

### 3. 差し込みのある文（params ≥ 1・screen 21 文 + vim_message 3 文）

組み立ては全部 `+` 連結・`std::to_string`・`append`（`std::format` は 0 件）。語順が言語で変わると困る形:
- 数が文の途中（前後 2 リテラルで挟む）: EditorController.cpp:610-611「同じフォルダは N 件まで。残りは…」、EditorControllerPalette.cpp:443-444「ブックマークは N 件まで登録できます」、EditorControllerDetail.cpp:152「（ほか N 件）」。
- 「前置き: 値」を別々に連結（値が後ろ・前置きの長さで名前を切る）: Autosave.cpp:74・Documents.cpp:291,292・LanguageServer.cpp:40-44（4 文）・Documents.cpp:1521（前置き + 理由の 2 段）・Shell.cpp:32,33（前置き + 理由）。
- 値が前、名詞が後ろ: ShellRegistration.cpp:28（拡張子 + 「 ファイル（NeNe Nib）」）・Direct2DRenderer.cpp:226（「置換後 (」+ 検索語 + 「): 」）・MarkdownBuilder.cpp:23（「[画像: 」+ alt +「]」）・StatusItems.cpp:42-43（「行 」N「, 桁 」M・固定長 buffer）・Direct2DRenderer.cpp:2016（`recording @` + 名前）。
- 理由文と日本語が 1 つの箱に混ざる: SettingsNotice.hpp:43-46（`theme_failure_message` の英語 + 日本語の続き）。
- 英語の単複を連結で作る: VimSearchNotice.cpp:60-66（`match`/`es`・`line`/`lines`）。Vim の文なので訳さない側。
- 文に埋め込まれた固定の数（0 個差し込みだが定数と手で揃える）: 1 GiB（Command.cpp:172・`static_assert` あり）、100,000 鍵（Vim.cpp:231・`static_assert` あり）、64 MiB（Autosave.cpp:72・無し）、16 MiB（Documents.cpp:1520・Preview.cpp:58）、1 GB / 64 MiB / 16 KiB / 8〜40 pt（EditorWindowOperations.cpp:102,104・SettingsNotice.hpp:21,31）、Ex の "8 to 40 pt" "256 bytes" "limited to 3"。

### 4. 同じ文の重複

- 同じ日本語: 「ブックマークを読めませんでした」Palette.cpp:231,435。「ブックマーク」CommandChoice.cpp:344・PaletteMarks.hpp:18・StartScreenPainter.cpp:28。「履歴」CommandChoice.cpp:346・PaletteMarks.hpp:19。「通常」ModeLabel.cpp:53・Direct2DRenderer.cpp:1916（枠の幅が違う）。`INSERT` ModeLabel.cpp:16,36。「この文字コードでは保存できない文字があります。」EditorWindowOperations.cpp:110 と :207 の前半。「元の設定は保持します。」SettingsNotice.hpp の 6 文。ThemeDerivation.hpp:93,95 の `static_assert` 文。
- 製品名 "NeNe Nib": Main.cpp:57・EditorWindowDetail.hpp:17（10 箇所で caption）・EditorWindowLifecycle.cpp:48,117・StartScreenPainter.cpp:26・EditorControllerShell.cpp:24,25・ShellRegistration.cpp:28。
- 同じ英語: "Settings could not be saved" 5 箇所（Command.cpp:809,929・Explorer.cpp:425,463・LanguageServer.cpp:468）、"E32: No file name" 2、"E474: Invalid argument" 2（ExResult.cpp:1848・ExMapCommand.cpp:383）。
- 日英の同じ意味: ファイルが書けない文字（`dialog_file_unencodable` と Documents.cpp:86）、フォント範囲（SettingsNotice.hpp:31 と ExResult.cpp:1838）。

### 5. 試験が文を直接比べている所

`tests/**`（.cpp .hpp .py）を、棚卸しの screen 文（4 文字以上・無題.txt を除く）の部分一致で数えた: **73 ファイル・678 箇所**。別に「無題」21 ファイル 84 箇所。英語: `"NORMAL"` 3 ファイル 7、`VISUAL BLOCK` 1・2、`recording @` 1・2、`(insert)` 2・5、`Unknown command` 2・3、`Settings could not be saved` 3・3、`"Written"` 3・4、`Could not write` 2・4、`Not supported` 5・6、`Theme file` 1・1（`Clipboard unavailable` は 0）。部分一致なので試験の入力データに偶然同じ語がある分が混ざる（未確認）。
上位 20 ファイル（箇所数）: performance/OperationChoiceProbe.cpp 290 / unit/BackgroundWorkTests.cpp 54 / unit/ShellRegistrationTests.cpp 19 / ui/StartScreenPaintTests.cpp 18 / unit/ExplorerTests.cpp 15 / adapters/AutosavePairPropertyTests.cpp 14 / unit/SessionTests.cpp 14 / unit/OpenAsyncTests.cpp 13 / unit/SavedStatePropertyTests.cpp 12 / unit/CommandPaletteTests.cpp 11 / unit/HistoryTests.cpp 11 / unit/OperationsTests.cpp 11 / unit/PrintTests.cpp 11 / unit/StartScreenTests.cpp 9 / unit/BellTests.cpp 8 / unit/ExplorerTreeTests.cpp 8 / unit/PreviewFullTests.cpp 8 / ui/SaveQuestionTests.cpp 7 / unit/PreviewImageTests.cpp 7 / adapters/FolderAdapterTests.cpp 6。
表へ移すと壊れる見立て:
1. 試験が日本語の文字列で等価を比べているので、既定の言語を試験の中で ja に固定するか、`ui_text(message, ja)` を通して比べる形に直す必要がある（直書きのままだと言語を足した瞬間に落ちる）。
2. `OperationChoiceProbe.cpp` は操作名・読みの日本語で絞り込みを 290 箇所検証している。名前が言語で引かれると、言語ごとの期待（読みは ja だけ・en は名前の部分一致）を表から導く形に作り直す。
3. 差し込みのある文（数・名前・拡張子）の試験は連結の形に依存する。書式（位置指定の `{}`）に変えると文は変わらなくても組み立ての検査が落ちる。性質の試験（SavedState / AutosavePair）が「無題」を使うので `tab_untitled` を引く形にそろえる。

### 6. 幅が決まっている枠（文と定数の場所）

| 枠 | 出る文 | 幅を決める定数 |
|---|---|---|
| ステータスバーのモード枠 | NORMAL…VISUAL BLOCK・`(insert) VISUAL BLOCK`（最長 21 文字）・通常 | `mode_width_dips = 72`（core/StatusBarLayout.cpp:20）。文字の高さ 12 DIP（Direct2DRenderer.cpp:65） |
| 「通常 \| Vim」トグル | 通常 / Vim | `segment_width_dips = 44`（StatusBarLayout.cpp:18） |
| 右の項目（行・桁 / 文字コード / 改行 / 言語） | 行 N, 桁 M / UTF-16 LE / CRLF / filetype 名 | `item_width_dips{96,72,36,72}`（StatusBarLayout.cpp:26） |
| 診断の枠・表示形式の枠 | E2 W1+ / grid | `diagnostics_width_dips = 96`（:28）・`display_width_dips = 24`（:32） |
| タブの帯 | 題名（無題・● ・…） | `tab_minimum_dips = 120`・`tab_maximum_dips = 200`・`tab_padding_dips = 14`（core/TitleBarLayout.cpp:18-31）。文は `DisplayText::maximum_bytes = 256`（DisplayText.hpp:16） |
| 起動画面 | 見出し 4・説明 9・空の文・操作名 | 列 `start_column_dips = 480`（core/StartScreenLayout.hpp:43）・鍵の列の最小 64（StartScreenLayout.cpp:15）・測る幅 8192 物理画素（StartScreenPainter.cpp:~41） |
| エクスプローラー | `:set root= でルートを決めてください`・（読めません） | `panel_width_dips = 240`・`explorer_width_minimum_dips = 120`（core/ExplorerLayout.cpp:13・.hpp:44） |
| Ctrl+P の面 | 候補なし・案内 `↑↓ 選択   Enter 決定   Esc 閉じる`・出どころ 6・記号の案内 | 件数の欄 64 DIP（Direct2DRenderer.cpp:~2381 `to_pixels(64, dpi_)`）・`palette_row_limit`（PaletteLayout.cpp:23） |
| 本文の操作の案内 | 3 操作の名前（一覧・ファイルを開く・ヘルプ）と注記 | 鍵の列 ≥64・説明 144 DIP（core/OperationGuideLayout.cpp:16-18）。状態の案内（ステータスバー）の幅は未確認（StatusGuideLayout.hpp） |

### 7. 文字の `IDWriteTextFormat` の locale

実際に作っている 5 か所すべて `L""`（空 = DirectWrite が OS の既定）: BodyTextLayouts.cpp:207,237（本文）・Direct2DRenderer.cpp:462,470（`make_format` / `make_italic_format`。tab / toggle / status / command / mode / guide / palette 等の UI の format が全部ここ）・PreviewTextLayouts.cpp:128・PrintComposition.cpp:212（行番号）。`ja-jp` の直書きは 0。`FallbackSegmentMemo.cpp:40` の `L"en-us"` は字体名の探索で画面の言語ではない。`PrimaryFontCoverage.cpp:91` が format の locale を読み、`FallbackRunSource` 経由で字体選択のキー（`FontFallbackKey.cpp:42-49` の `locale_name`）に入る。

### 8. OS が持つ文

- 絞り込み名（`IFileDialog::SetFileTypes`）: FileDialog.cpp:34「対応するファイル」「すべてのファイル」（`static const` 配列に生ポインタ）。ほかに `SetDefaultExtension(L"txt")`（:117）・`SetFileName`（既定の名前 `無題.txt`: EditorWindowOperations.cpp:64,263）。ダイアログの題名とボタンは OS に任せている（`SetTitle` `SetOkButtonLabel` は 0）。
- TaskDialog（保存しますか）: 本文とボタン 3 つは core の `save_question_text`（`保存する(&Y)` `保存しない(&N)` `キャンセル`）。窓題名は `product_name`（SaveQuestion.cpp:96）。アクセラレータ文字が日本語の Y/N（英語の標準は &S / Do&n't）。
- `MessageBoxW`: 7 呼び出し（EditorWindow.cpp:200・EditorWindowOperations.cpp:148,192,199,206,240・Main.cpp:75 ほか）。本文は日本語直書き、題は `product_name`、ボタンは OS の言語（Nib の言語と食い違い得る）。
- 窓の題名: `SetWindowTextW` 題 + `" - NeNe Nib"`（EditorWindowLifecycle.cpp:48,453）・初期題 `L"NeNe Nib"`（:117）。
- `ShutdownBlockReasonCreate`: 「保存されていない変更があります」（EditorWindowLifecycle.cpp:562）。
- レジストリに書く表示名（`core/ShellRegistration.cpp` の計画・`EditorControllerShell.cpp`）: 右クリックの項目「NeNe Nib で開く」・アプリ名 "NeNe Nib"・ProgId の説明「{拡張子} ファイル（NeNe Nib）」（ShellRegistration.cpp:28,132,134 の `ApplicationName` `ApplicationDescription` `FriendlyAppName` の値の出どころは未確認）。言語を切り替えても登録済みの値は古い言語のまま。
- 印刷（`PrintDlgExW`）: OS のダイアログ。Nib の文は controller の知らせ 5 文。
- `NeNeNib.rc` の VERSIONINFO（英語 1 言語）。

### 9. 設計席への問い（8）

1. **モード名と録画の文** `NORMAL` `INSERT` `VISUAL…` `(insert)` `recording @a` `Vim`（TSV の ModeLabel 10 行 + 2 行）。A: Vim と同じ英語のまま全言語で固定（表に入れない・「訳さない」）。B: 表の行にして 3 言語とも同じ値を入れる（将来の言語で変えられる）。おすすめ B（行は 1 個増えるだけで網羅性に乗る）。
2. **「通常」の英語**（ModeLabel.cpp:53・トグル枠 44 DIP）。A: `Standard`（D82 の「Windows の標準」に合う・NORMAL と紛れない・トグルは短く `Std` か `Normal`）。B: `Normal`（NORMAL と大文字小文字だけの差で紛らわしい）。おすすめ A。トグルの枠の幅を測って決める。
3. **解く場所と静的な記憶域**。A: application の frame を作る所で `ui_language` から引き、`EditorFrame` の `string_view` は静的な表を指す（`mode_label` のまま）。B: ui の renderer / window が引く（core の関数に言語を渡す）。おすすめ A（`state_` が言語を持ち、試験が renderer なしで通る）。
4. **差し込みのある文 21+3**（§3）。A: 表に位置指定の `{0}` `{1}` の書式を持たせ、core に小さな書式関数 1 本（連結をやめる）。B: 文を「前置き」「後置き」の行に割って今の連結を残す（語順は直せない）。おすすめ A。数が埋め込みの定数（1 GiB など）の `static_assert` を表のどこに残すかも決める。
5. **設定を読む前・OS が出す文**（Main.cpp 4 文・ShutdownBlockReason・レジストリの表示名）。A: `GetUserDefaultUILanguage` で決めた言語で出す（設定を読めない失敗は OS の言語しか使えない）。B: 設定の言語に従い、読めない失敗だけ英語固定。レジストリは言語変更で書き直すか据え置くかも。おすすめ A（レジストリは据え置きで注記）。
6. **英語で書かれた Nib 自身の文**（Ex の断り 13・map 6・検索の未対応 8・`Settings could not be saved` など 約 45 文）。A: すべて訳す（Vim の E 番号つきだけ残す）。B: Ex 系は Vim の文化に合わせ英語のまま。おすすめ A（`Not supported: {命令}` の命令名は訳さず埋め込む）。
7. **テーマの失敗**（ThemeLookupFailure.cpp の理由 14 文・Win32ThemeAdapter.cpp の英語 9 文＝adapters が画面の文を持つ・SettingsNotice の日英混在）。A: adapters は型のある失敗の enum を返し、文は core の表で作る（ARC 上きれい・変更が大きい）。B: 文を表に移すだけで adapters は `UiMessage` の値を持つ。おすすめ A（今日の範囲は B で足りる）。
8. **幅と `IDWriteTextFormat` の locale**。A: 全言語の全文を DirectWrite で測り枠に収まることを `nib_window_tests` で確かめる（計画どおり）・format の locale は UI 用だけ言語に合わせる（簡体字の字形のため）。B: locale は `L""` のまま・枠は固定。おすすめ A。ただし本文の format（BodyTextLayouts）はドキュメントの言語であって UI の言語ではないので locale を変えない。

### 10. 未確認

- 各 `ex_map_failure_message` `vim_search_message` の呼び出し数。`status_guide`（ステータスバーの操作の案内）の幅の定数。`ShellRegistration.cpp:132-136` の値の出どころ。`Win32ThemeAdapter` の知らせが最終的に出る画面の道（`ThemeInventory.notice`）。試験の部分一致のうち偶然の一致の数。Controller 内の `autosave_*` 系の知らせの呼び出し先。以上は TSV の note に「未確認」と書いた。
