# 表示言語の訳のレビュー

NeNe Nib の画面の文は `src/core/UiTexts*.hpp` の列（`UiMessage` の値ごとに 1 行）にあります（ADR 0123 の追補 2026-11-13）。
訳の誤り・不自然な言い回し・より定着した用語に気づいたら、GitHub の Issue で知らせてください。

- 題に言語と値の名前を書く（例: `i18n(zh-hans): operation_preview_full_name`）。
- 本文に「今の文」「案」「理由（どの製品の定訳か・どこが不自然か）」を書く。
- 画面のどこに出る文かは値の名前の頭で分かります（`operation_` は F1 の一覧と案内・`notice_` はステータスバーの知らせ・`dialog_` はダイアログ・`palette_` は Ctrl+P の面・`start_` は起動画面・`status_` はステータスバー）。
- 置き場 `{0}` `{1}` は消さずに残します（中身はファイル名・数・理由の文など）。

## 言語ごとの状態

| 言語 | 列のファイル | 書いた人 | 状態 |
|---|---|---|---|
| 日本語 | `UiTextsJapanese.hpp` | 製品の文そのもの | 正本 |
| English | `UiTextsEnglish.hpp` | 設計席が書いた | hide が見る |
| 简体中文 | `UiTextsSimplifiedChinese.hpp` | **全行が機械の訳**（2026-11-13・#1595） | レビュー歓迎 |

## 中国語（簡体字）

全 197 行が機械の訳です。日本語の文の意味から起こし、Windows 11 の簡体字版・记事本・VS Code の簡体字の言語パックの定訳に合わせました。
句読点は全角（，。：（）“”）、英数字と漢字の間に半角の空白を入れます（`Markdown 预览`・`16 MiB`）。
訳さないもの: Vim・Markdown・HTML・LSP・UTF-8・CRLF・NeNe Nib・鍵の名前（`Enter` `Esc` `F5`）・モードの名前（`INSERT`）・命令（`:set root=` `:set lsp`）。

### 主な用語

标签页（タブ）・书签（ブックマーク）・历史记录（履歴）・窗格（分割の枠）・预览（プレビュー）・光标（キャレット）・换行符（改行）・编码（文字コード）・网格视图（格子表示）・自动备份（自動退避）・根文件夹（ルート）・资源管理器（Nib のエクスプローラー）・文件资源管理器（Windows のエクスプローラー）・命令（操作）・普通（通常のモード）・无标题（無題）。

### 迷った訳

| 値 | 原文（日本語） | 英語 | 置いた訳 | 別の候補 | 迷った理由 |
|---|---|---|---|---|---|
| `status_mode_basic` | 通常 | Basic | 普通 | 标准・常规 | トグルの枠が 44 DIP。「标准」は D82 の「Windows の標準」に合うが、`NORMAL`（Vim）と並ぶ枠で 2 字に収めた |
| `operation_toggle_mode_name` | 通常 / Vim を切り替える | Switch between Basic and Vim | 切换普通 / Vim | 在普通和 Vim 模式间切换 | 一覧で探しやすい短さを優先した |
| `operation_preview_full_name` | Markdown プレビュー（全面） | Markdown Preview (Full) | Markdown 预览（占满） | （全屏）・（全区域）・（整个编辑区） | 本文の面の全部で、窓の全画面ではない。「全屏」は意味がずれる |
| `operation_toggle_preview_description` | …もう一度で全面・もう一度で閉じる | …again for full view… | 再按一次占满正文区域 | 再按一次全屏显示 | 上と同じ語に揃えた |
| `operation_font_larger_name` ほか 2 行 | 文字を大きく / 小さく / 大きさを戻す | Zoom In / Zoom Out / Restore Default Zoom | 放大文字 / 缩小文字 / 恢复文字大小 | 放大 / 缩小 / 恢复默认缩放（记事本） | 日本語は拡大率ではなく字の大きさ。记事本の「缩放」は拡大率 |
| `operation_paste_html_name` | HTML のまま貼り付け | Paste HTML As Is | 按 HTML 原样粘贴 | 粘贴 HTML 源代码・保留 HTML 粘贴 | タグごと貼る意味。「源代码」は伝わりやすいが原文に無い |
| `operation_list_operations_name` / `palette_mark_operations` | 操作の一覧 / 操作 | All Commands / Commands | 命令列表 / 命令 | 操作列表 / 操作 | VS Code の「命令面板」に合わせた。「操作」も簡体字で通じる |
| `operation_recent_tab_name` | 前に使ったタブへ | Go to Recent Tab | 转到最近使用的标签页 | 上一个使用的标签页 | Ctrl+Tab の動き（最近使った順）を名前に残した |
| `notice_autosave_failed` ほか | 自動退避 | Autosave | 自动备份 | 自动保存・自动暂存 | 「自动保存」はファイルへの保存と紛れる。退避は別の置き場への写し |
| `notice_preview_task_stale` | プレビューが古いので切り替えられません | …so the task cannot be toggled | 预览已过期，无法切换任务 | 预览已过期，无法切换 | 原文は「タスク」を言わないが、チェックボックスの切り替えの知らせなので補った |
| `shell_verb_open` | NeNe Nib で開く | Open with NeNe Nib | 通过 NeNe Nib 打开 | 用 NeNe Nib 打开・使用 NeNe Nib 打开 | VS Code の右クリックの「通过 Code 打开」に合わせた |
| `notice_shell_empty_path` | exe のパスがありません | The exe path is empty | 缺少 exe 的路径 | exe 路径为空 | 原文は「無い」。英語は「空」 |
| `notice_replay_limit` | 再生を {0} 鍵で止めました（残りは捨てました） | Stopped the replay at {0} keys… | 回放到 {0} 个键时已停止（其余已丢弃） | 已在第 {0} 个键处停止回放 | 「鍵」は打鍵の数。「回放」はマクロの再生 |
| `palette_footer_hint` | ↑↓ 選択   Enter 決定   Esc 閉じる | ↑↓ Select   Enter OK   Esc Close | ↑↓ 选择   Enter 确定   Esc 关闭 | Enter 打开 | 「決定」は开く以外（設定の選択）にも使う |
| `prompt_replace_with_pattern` | 置換後 ({0}): | Replace ({0}) with: | 将（{0}）替换为： | 替换（{0}）为： | 入力行の前置き。語順を中国語に合わせた |
| `dialog_settings_unwritable` | 別の起動で使用中… | …in use by another window… | 设置正被另一个实例使用 | 另一个窗口 | 「別の起動」は別のプロセス。「实例」はやや硬い |
| `dialog_settings_invalid_font_size` | {0}〜{1} pt | from {0} to {1} pt | {0} 到 {1} pt | {0}～{1} pt・{0}-{1} pt | 「～」も通じるが「到」が読み違えない |
| `palette_origin_bookmarked_tab` | タブ・ブックマーク | Tabs · Bookmarks | 标签页 · 书签 | 标签页、书签 | 区切りは中国語の間隔号（U+00B7）。GB2312 の表では片仮名の中点（U+30FB）に写るが、GBK と GB/T 15834 では U+00B7 |
