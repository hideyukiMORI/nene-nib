# ADR 0123 — 表示言語は版付き設定に持ち、OSの観測値から明示操作時に解決する

- 状態: **受理（2026-10-28・施主決定 D69「言語は 6 言語目標」。下の改訂の節が 2 言語の案を 6 言語に広げる。実装は #1019 の T1〜T3。2026-11-13 の追補で T1（日本語・英語・中国語（簡体字））を実装）**
- 日付: 2026-10-12
- Issue: #456
- 影響する規則: ARC-001/003/004/007/008/009/010/011/012、CPP-002/003/011/013、QLT-001/012/013、FR-016/017/018/019

## 文脈

#438/#454で設定モーダルと日本語/Englishの表示切替を計画した。現行main `5b7dbb2`の`EditorSettings`に言語はなく、保存はsettings.v3。`Win32SettingsAdapter`はv3→v2→v1の順で不在の版だけを通過し、読込元までのlockとbytes比較で移行の競合を拒否する。新しいキーをv3へ任意追加すると、既存の未知キー拒否と旧版の読み取りを壊す。

言語の希望値と、その起動で実際に表示する言語は同じではない。「Windowsに合わせる」を日本語に解決した結果だけ保存すると、次回のOS変更に追従できない。一方、描画や打鍵ごとにOSを読むと、純粋性と見た目の切替時機が曖昧になる。

本ADRは試験保留中の設計案である。以下は採用候補であり、現行settings.v3や受理済みADR 0020/0079/0114の実装契約を、この文書の統合だけで置き換えない。

## 決定案

### 1. 希望値と表示値を閉じた型で分ける

coreに保存対象の`UiLanguageChoice`（system / japanese / english）と、解決済みの`UiLanguage`（japanese / english）を分ける案とする。公開型を導入するときは1ファイル1型に従う。`EditorSettings`は希望値を1つ持ち、設定の比較へ含める。表示する言語は希望値と注入されたOS観測値から純関数1本で求める。

保存用のASCII値はadapterのcodecで写し、画面上の「日本語」「English」から逆算しない。core/applicationへLANGIDやWindowsのhandleを持ち込まず、実行時ロケールを読まない。本文の文字コード・改行・言語解析と、このUI言語を混同しない。

### 2. 保存の正本候補はsettings.v4

同じ設定ディレクトリに置くUTF-8、最大4096 bytes、`version=4`。必須キーは次の7個で、どれも1回だけ。値・未知/重複/欠落キー・版不一致は厳密に検証する。

```text
version=4
colorscheme=system
font_family=Cascadia Code
font_size=13.5
guide=on
wrap=on
ui_language=system
```

`ui_language`はASCIIの`system` / `ja` / `en`だけを受ける案。大文字や`ja-JP`、表示ラベルは保存値にしない。BOM・LF/CRLF・キーと値の周りの空白・空行は既存codecと同じ扱いにする。字体・サイズ・テーマ・案内・折り返しの検証器を複製しない。不正な言語値は閉じた設定失敗へ値を追加して表す案とし、読み手の網羅性をコンパイラに検査させる。

### 3. 旧版は不在の場合だけ順に読む

| 読み取った版 | 従来の必須キー数 | 新しく補う値 |
| --- | --- | --- |
| v4 | 7 | なし |
| v3 | 6 | ui_language=ja |
| v2 | 5 | wrap=on、ui_language=ja |
| v1 | 4 | guide=on、wrap=on、ui_language=ja |
| すべて不在 | — | 既存の既定値とui_language=system |

読み順はv4→v3→v2→v1。上位版が存在して壊れている・読めない場合はそこで失敗し、下位版へ戻らない。旧版に`ui_language`があれば未知キーとして拒否する。ファイル名と`version`は一致を要求し、settings.v4にversion=3を置いても受けない。

旧版の言語をjaへ補うのは、更新しただけで既存利用者の主な表示を変えないための製品上の選択。新規利用のsystemと区別する。起動・読込・モーダルの表示・無変更の保存では移行書込をしない。最初の実変更でv4だけを書き、v1/v2/v3は元のbytesで残す。v4成立後に旧版を同期したり取り込んだりしない。

読込失敗時は既存どおり作業を続けられる既定値を用いるが、元ファイルを保持して設定保存を拒否する。壊れた設定から`ui_language`だけを抜き出して使わない。表示する理由の言語は、正常に得た設定または既定値から決める。

### 4. lockと比較は読込元まで

| 読込元 | 保存前に共有なし・待機なしで取得するlock | 比較するsnapshot |
| --- | --- | --- |
| v4 | v4 | v4のbytes |
| v3 | v4→v3 | v4の不在、v3のbytes |
| v2 | v4→v3→v2 | v4/v3の不在、v2のbytes |
| v1 | v4→v3→v2→v1 | v4/v3/v2の不在、v1のbytes |
| 全不在 | v4→v3→v2→v1 | 全版の不在 |

ロック名と固定順は既存の`Win32SettingsAdapter::write_through`を拡張する。`SettingsVersion`、`SettingsPaths`、`settings_path`、codec、読込順を一度に移行し、v4専用adapterや別の保存関数を増やさない。

上位版の作成、読込元の変更/削除、lock/読込/書込の失敗は拒否する。書込は同じ`FilePort`へ一度渡す。成功してからv4のbytesを新しいsnapshotにし、以後はv4だけを読む/lockする。失敗を成功として記録せず、元の確定値と言語を保持する。

保護範囲は、同じlock手順を守るプロセス間と保存前のsnapshot照合。lockを無視する外部エディタが比較後に変更する競合を原子的に防ぐ保証へ広げない（ADR 0020/0079の限界を継ぐ）。

### 5. OSの観測はadapterの1か所

WindowsのユーザーUI言語は`GetUserDefaultUILanguage`から取得する案とする。このAPIは現在ユーザーのUI言語のLANGIDを返し、未設定時のOS側fallbackも持つ。一部の補助ロケールでは専用値を返す。[MicrosoftのAPI仕様](https://learn.microsoft.com/en-us/windows/win32/api/winnls/nf-winnls-getuserdefaultuilanguage)

`PRIMARYLANGID`はそのLANGIDから主言語を取り出す。[Microsoftのマクロ仕様](https://learn.microsoft.com/en-us/windows/win32/api/winnt/nf-winnt-primarylangid)

ここからの**製品上の選択**として、主言語が日本語の場合だけjapanese、それ以外・専用値・未知の値はenglishへ写す。キーボード配列、IMEの入力言語、地域、環境変数LANGからは決めない。OSの言語設定を書き換えず、OS所有のダイアログやボタンまで同じ言語へ強制しない。

applicationの型付き`UiLanguagePort`とwin32 adapterへ閉じ、合成は既存`EditorPorts`に束ねる案とする。注入するのは2択の観測値で、純粋な関数が希望値と合わせて解決する。rendererや設定codecがOSを読む経路を作らない。

### 6. 再解決を明示操作に限る

| 時機 | 観測と反映の案 |
| --- | --- |
| 起動 | OSを1回観測し、読み取った希望値と合わせて確定表示を作る。ファイルは書かない |
| モーダルを開く | OSを1回観測して候補の文脈に保持。「Windowsに合わせる」の見本はこの観測値から解決する。外側の確定言語は変えない |
| モーダルで言語候補を変える | 保持した観測値を使う。選択のたびにOSへ問い合わせない |
| モーダルで保存 | 候補に使った同じ観測値を使い、永続化成功後に一度反映する。保存直前の再観測で見本と結果を変えない |
| Ctrl+P/Exからsystemへ明示変更 | 操作時に1回観測し、同じ設定変更/保存経路へ渡す。具体的なコマンド追加は実装範囲で定める |
| 他の設定・打鍵・描画・タブ切替 | OSを再観測しない。直前に確定した言語を維持 |
| 取消・保存失敗 | 候補の観測値を確定状態へ移さない。失敗した候補はモーダル内で保持できる |

OS観測値は、確定表示で使用する値とモーダル候補の値を所有者ごとに区別する。設定保存の共通入口へ、設定候補と解決に使った観測値を一つの変更要求として渡す案とし、入口ごとに保存成功/失敗の規則を複製しない。実装時に候補の型と全呼出し元を同じADRに追記して移行する。

希望値を含む設定が同じなら書込は省く。systemの観測だけが変わった場合、明示した保存操作で表示の解決値だけを更新できる案。起動時に設定を読めなかった状態ではこれを「保存成功」にせず、設定のエラーは保持する。保存中の外部変更を検査するのは実際に書き込む時点であり、無変更で閉じたことをディスクとの再照合成功とは扱わない。

この案は#454の「systemの保存時に解決する」を、**モーダルで見せた観測値を保存成功時に採用する**形へ具体化する。OS言語変更の常時監視や、他の窓の言語を自動で変える機能は加えない。

## 強制と実装の区切り

- **planned:** v4の厳密codec、旧版の未知キー拒否、読込優先順と保存禁止、lock/snapshotの移行、希望値を含む同値判定、失敗時の表示保持。対象の実装と契約が成立してから技術受理を判断する。ここではゲートをactiveへ変更しない。
- **planned:** 言語/設定失敗の閉じた型の網羅性、意味IDごとの日英の全件対応。翻訳の生成と既存通知の型への移行は別の実装設計で確定する。
- **既存規範を維持:** OS/環境/ファイルはadapter、設定の所有はEditorState、保存はSettingsPort/FilePort、操作名と鍵は既存の操作表。これは新しい強制を実装したという宣言ではない。

まず言語の型と保存/移行の対象契約、次にモーダルの候補と同じ保存入口、最後に全表示と保持結果の更新を接続する。使われない公開APIだけを先に製品へ残さない。移行中の部分的な日英化はdraftの開発状態として扱い、全対象が揃うまで日英対応の製品完成を宣言しない。

試験再開後の最小対象は、v4の往復/欠落/重複/未知値、v1〜v3の値補完とbytes保持、壊れた上位版、全不在、各移行元の競合/lock/書込失敗、純粋な言語選択、モーダルの取消/失敗/無変更、OS観測回数と結果の適用時機。OSやファイルは替え玉で指定し、通常編集の全件試験をこの変更だけで選ばない。今回はすべて未実施。

## 結果と未決点

設定ファイルの正本を一つにし、既存利用者の言語を保ちつつ新規利用者にOS追従を用意できる。費用は版付きパス/codec/port/状態の移行と、言語依存の候補・通知・描画保持の更新である。設定を読む失敗、本文保存の失敗、モーダル入力エラーは別の結果として扱う。

このIssueでは画面資料とADRだけを収載する。設定ファイル・製品コード・品質ゲートを変更せず、#446/PR447と#452/PR453の試験保留を維持する。実際の入力session/IME/保存要求の型、全翻訳の欠落検査、読み上げへの接続は実装設計へ残す。保存遅延#424の解消、D12の画面採用、Win32での操作成立は未了。

## 却下する案

| 案 | 理由 |
| --- | --- |
| settings.v3へ任意の言語キーを足す | 旧版の厳密なschemaと読込互換性を壊す |
| 解決済みのja/enだけ保存する | systemという希望を失い、次回のOS追従ができない |
| 既存設定の言語を無条件でsystemにする | 製品更新だけで表示言語が変わり得る |
| 描画時にOSを読み直す | 純粋性を失い、設定保存/取消と無関係な時機に画面が変わる |
| 上位版が読めなければ旧版へ戻る | 正本の破損を隠し、古い値で上書きする危険がある |
| モーダルが独自に設定ファイルを保存する | 共通入口の失敗時保持・競合・版移行と分岐する |
| 旧版のファイルを削除・同期する | 旧製品で使っていた設定を失い、複数の正本を維持することになる |

## 関連

[操作設計案](../design/2026-10-12-settings-dialog-and-language.md)、[画面案](../design/2026-10-12-settings-dialog-boards.md)、ADR 0020/0078/0079/0114、[EditorSettings](../../src/core/EditorSettings.hpp)、[SettingsCodec](../../src/adapters/win32/SettingsCodec.cpp)、[Win32SettingsAdapter](../../src/adapters/win32/Win32SettingsAdapter.cpp)、[LocalSettingsPath](../../src/adapters/win32/LocalSettingsPath.cpp)。

## 改訂（2026-10-28・D69・#1019）: 6 言語と文の表

上の決定案 1〜5 は形（希望値と表示値を分ける・保存は v4・旧版は不在のときだけ・lock と比較は読込元まで・OS の観測は adapter の 1 か所）をそのまま保ち、次を改める。

1. **言語は 6 つ。** `DisplayLanguage`（解決済み・閉じた enum）は `japanese` `english` `simplified_chinese` `korean` `spanish` `german`。希望値 `DisplayLanguageChoice` は `system` ＋ 6 値。保存の ASCII 値は `system` `ja` `en` `zh-hans` `ko` `es` `de`（これ以外は閉じた設定失敗）。
2. **OS の観測の写し。** `GetUserDefaultUILanguage` の `PRIMARYLANGID` が `LANG_JAPANESE` → japanese・`LANG_CHINESE` で `SUBLANGID` が簡体字（`SUBLANG_CHINESE_SIMPLIFIED` / `SUBLANG_CHINESE_SINGAPORE`）→ simplified_chinese（繁体字は english・限界）・`LANG_KOREAN` → korean・`LANG_SPANISH` → spanish・`LANG_GERMAN` → german・ほかは english。adapter は `std::optional<DisplayLanguage>` の観測値を注入し、`system` の解決は core の純関数 1 本（観測値が無ければ english）。
3. **文の表は core の 1 つ。** `UiMessage`（画面の文の名前・閉じた enum・1 ファイル 1 型）× `DisplayLanguage` の `constexpr` の 2 次元の表 `ui_texts`。行が欠けていればコンパイルが落ちる（`std::array` の大きさ）。読む口は `ui_text(UiMessage, DisplayLanguage) -> std::string_view` の 1 本。操作の名前と説明（`operation_texts`・ADR 0078）は同じ表に畳む（表を 2 本にしない）。鍵の表示名（`key_chord_label`）と Vim の文（鍵・Ex の命令名・`E486` などのエラー）は訳さない（英語のまま・学習の連続性）。
4. **引く所の規則（CNF-016・planned → T1 で active）。** `src/**` の描画の道（ui / application / core の frame と知らせ）に画面の文のリテラルを書かない。`eng/conformance.py` が `src/**/*.cpp` `*.hpp` の日本語（ひらがな・カタカナ・漢字）を含む文字列リテラルを数え、`ui_texts` の表のファイルと試験の外では 0 であることを守る。例外は表のファイル・`docs`・コメント（リテラルでない）。
5. **幅。** ステータスバーの枠とタブの帯と面の文は言語で幅が変わる。`nib_window_tests` に「全言語の全文を DirectWrite で測り、各枠の幅に収まる」試験を足す（収まらない文は短い言い回しに直す・枠の幅は変えない・ドイツ語が要注意）。
6. **訳の責任。** 日本語と英語は設計席が書き hide が見る。残り 4 言語は設計席の機械の訳で、`ui_texts` の見出しと `docs/i18n/REVIEW.md`（未レビューの一覧・言語ごと）に「機械の訳・レビュー歓迎」と明示する。
7. **v4 の鍵の集合。** 決定案 2 の 7 個の鍵は例で、v4 の鍵の集合は設定モーダルの ADR（D68・#1016）が **1 回で**決める（`ui_language` はその 1 つ・LSP の `lsp`・エクスプローラーの `explorer` `root`・機能のオン・オフの `features`）。v4 を最初に要る PR が定義し、後の PR は欄を使う。
8. **切り替え。** `:set language=ja|en|zh-hans|ko|es|de|system`・`:set language?`・設定モーダル。切り替えは即時（frame を作り直し・layout の保持は書式の鍵に言語を含めて捨てる・打鍵の道は変えない）。
9. **限界。** 繁体字中国語・フランス語などの追加は表に列を足すだけ（`DisplayLanguage` に 1 値・`std::array` が落ちる所を埋める）。OS 所有のダイアログ（ファイルの選択）の文は OS の言語のまま。Markdown プレビューの本文・Vim のエラー・鍵の名前は訳さない。

## 追補（2026-11-13・D88・#1019 の T1）: 1.0.0 は 3 言語・文の表の形・言語が画面に届く道

施主決定 D88（Store の 1.0.0 は日本語・英語・中国語（簡体字）の表示で申請する・韓国語・スペイン語・ドイツ語は 1.1.0）。改訂（2026-10-28）の決定 1〜9 の形を保ち、実装の形を次のとおり決める（実装は PR #1592 T1a・#1593 T1b・#1594 T1c・#1595 T1d・#1602 T1f）。

1. **解決済みの言語はいま 3 値。** `core::DisplayLanguage` は `japanese` `english` `simplified_chinese`。希望値 `DisplayLanguageChoice`（設定 v4 の欄・7 値）は変えない。`korean` `spanish` `german` の希望値は、列ができるまで `english` に解決する（設定ファイルは読めて書ける・別の `:set` を保存しても値は残る・`:set language=` の候補には出さない）。列を足すときは `DisplayLanguage` に 1 値を足し、落ちた表と `switch` を埋める。
2. **解決は core の純関数 1 本。** `resolved_language(DisplayLanguageChoice, std::optional<DisplayLanguage> observed) -> DisplayLanguage`。`system` は観測値（無ければ `english`）、ほかは 1 の写し。OS の観測は application の port `UiLanguagePort::observe()`（`std::optional<DisplayLanguage>`）と `src/adapters/win32` の `Win32UiLanguage` の 1 か所（`GetUserDefaultUILanguage`・`LANG_JAPANESE` → 日本語・`LANG_CHINESE` の簡体字とシンガポールと中立（`0x0004`）→ 簡体字・繁体字とほかの言語は英語）。観測するのは起動のとき（controller の構築の 1 回）と、`:set language=system` と `:set language&` のときだけ（決定案 6）。`ja` `en` `zh-hans` を選ぶ・`:set language?`・打鍵・描画・タブの切り替えでは読まない（契約 `verify_quiet_observation` が port の回数を数える）。起動の前の失敗の箱（`src/app/Main.cpp`）は、失敗の道でだけ観測して出す。
3. **状態が持つのは解決済みの値 1 つ。** `EditorState::display_language()`（既定は `japanese` = いままでの表示・試験と fixture の文が変わらない）。controller が構築のときに設定と観測から解決して置き、`:set language=` が置き直す。frame は `EditorFrame.language` に同じ値を載せ、renderer と painter（起動画面・エクスプローラー）はそこから引く。ui が自分で出す箱（MessageBox・TaskDialog・ファイルの選択・ShutdownBlockReason）は frame を作らずに `EditorController::display_language()` から引く（ui は設定も OS も読まない）。
4. **文の表は列ごとのファイル・行は名前つき。** `core/UiMessage.hpp`（閉じた enum・1 ファイル 1 型）、列は `core/UiTextsJapanese.hpp` `UiTextsEnglish.hpp` `UiTextsSimplifiedChinese.hpp` の `inline constexpr UiTextColumn`（`std::array<UiTextRow, ui_message_count>`・`UiTextRow{UiMessage, std::string_view}`）。`UiTexts.cpp` の `static_assert` が「全値を 1 回ずつ・enum の順・空でない・正しい UTF-8」と「3 列で同じ置き場の集合」を守り、C++ では数えられない「enum の列挙子の数・`ui_message_count` が名指しする値が最後の列挙子であること・3 列の行の数と順」は `eng/conformance.py` の CNF-016 が守る（末尾に値だけ足して行を書かない形が `static_assert` を通る穴・レビュー V-1598 の A-M1）。読む口は `ui_text(UiMessage, DisplayLanguage) noexcept -> std::string_view` の 1 本（`UiTexts.hpp`）。列をファイルで分けるのは、言語を足す PR が 1 ファイルを足すだけで済み、ほかの列と衝突しないため。
5. **差し込みは番号つきの置き場。** 文の中に `{0}` `{1}` を書き、`ui_text_filled(UiMessage, DisplayLanguage, std::initializer_list<std::string_view>) -> std::string` が埋める（core・純関数・差し込んだ値は読み直さない）。語順は言語ごとの文が決める（呼び出し元で文の断片を連結しない）。前置きの長さで名前を切る知らせ（`named_line` `named_notice`）は `first_slot_split` で文を置き場の前後に割って使う。文の中の固定の数は定数から作る（`binary_size_text` が `64 MiB` `1 GiB` `16 MiB`・ui の箱は `1 GB` `16 KiB` `16384` `8`〜`40`）。桁区切りは付けないが、日本語の今の文を保つ `notice_replay_limit` だけは `grouped_decimal_text`（`100,000`）で埋める。
6. **操作の表は文の名前を持つ。** `OperationText` は名前・説明・短い名前を `UiMessage` で持ち、F1 の一覧と案内は `ui_text` で引く。読み（ひらがな）は画面に出ない検索の鍵なので、日本語のときだけ使う別の小さな表 `operation_readings`（`OperationReadings.hpp`）に残す。英語と中国語は名前・短い名前・鍵の表示名で探す（拼音の読みは持たない = 限界）。
7. **訳さないもの。** Vim のエラーと報せ（`E486: Pattern not found`・`recording @a`・`-- INSERT --` の類）・モードの名前（`NORMAL` `INSERT` `VISUAL`）・Ex の命令名・鍵の名前（`key_chord_label`）・テーマの名前・字体の名前・文字コードと改行の名前（`UTF-8` `CRLF`）・製品名 `NeNe Nib`・言語の名前の自称（`日本語` `English` `简体中文` はどの言語でも同じ綴りで出す・`DisplayLanguageNames.cpp`）。いま英語で出している Nib 自身の文（Ex の断り・`Settings could not be saved`・テーマの失敗など）は、今回は全言語で英語のまま（表に入れない・後続 #1585）。
8. **切り替え。** `:set language=ja|en|zh-hans|system`・`:set language?`（今の希望値と、`system` と列の無い希望値では解決済みの値を `language=system (ja)` の形で添える・保存を通さない）・`:set language&`（既定の `system` へ）。`language!` と `invlanguage` は旗でないので E518（Vim と同じ）。F1 の一覧の操作「表示言語を選ぶ」（`EditorOperation::choose_language`・鍵なし・両モード）は面を `:set language=` の候補で開く（`choose_encoding` と同じ形・候補は自称と「Windows に合わせる」の説明つき）。反映は即時（状態の値が変わり frame が変わる）。保存は既存の設定の入口（`persist_settings` → `settings.v4`）で、保存できなければ言語を変えない。
9. **文字組みの locale。** 画面の文字（ステータスバー・帯・面・起動画面・エクスプローラー・案内）の `IDWriteTextFormat` の locale は言語から決める（`UiLocale.hpp` の `ui_locale_name`: 日本語は今までと同じ `L""`・英語 `en-us`・中国語 `zh-cn`）。renderer は作るときに言語を受け、最初から正しい locale で書式を作る（作り直しは言語が変わったときだけ・保持の枠は書式の再作成の入口で先に消える）。本文・プレビュー・印刷の書式の locale は変えない（文書の中身は表示言語と無関係）。`zh-cn` の書式で漢字が簡体字の字体に落ちることは `nib_window_tests --language` が本物の `MapCharacters` で確かめる。
10. **幅。** 固定の枠（ステータスバーのトグルとモードの枠・行と桁・面の案内と「候補なし」・本文とステータスバーの案内・エクスプローラーの見出し・起動画面）に出る文は、3 言語とも枠に収まることを `nib_window_tests --language` が製品と同じ字体・大きさ（`UiTextSizes.hpp` を renderer と共用）・locale で測る。収まらない文は短い言い回しに直す（枠の幅は変えない）。「通常」は英語 `Basic`・中国語 `标准`（`Normal` と `普通` は Vim の NORMAL と紛れる・`Standard` は 44 DIP に入らない）。
   - 追補（2026-11-13・#1640）: 入力行と行を分け合う補助の文（Ctrl+P の検索欄の記号の案内）は固定の枠に入れず、幅を文に合わせ、入らなければ組の単位で落とす（ADR 0060 の追補 #1640）。言い回しを短くする直しはこの文には当てない。
11. **OS の持ち物。** ファイルの選択の絞り込みの名前・TaskDialog の本文とボタン・既定のファイル名（`無題.txt` / `Untitled.txt` / `无标题.txt`）・窓の題名とタブの「無題」・ShutdownBlockReason は表から引く。OS が描く部品（ダイアログの枠・「開く」「キャンセル」）は OS の言語のまま。exe の VERSIONINFO は英語のまま。エクスプローラーへの登録の表示名（ADR 0178）は登録の操作の時点の表示言語で書き、言語を変えても書き直さない（もう一度「登録する」を選ぶと書き直る）。設定の上限の数（16384）は core の `SettingsByteLimit.hpp` の 1 つで、adapter の読みと ui の箱の文が同じ値を読む。
12. **強制（CNF-016・active）。** `eng/conformance.py` が `src/**/*.cpp` `*.hpp` の文字列リテラル（コメントは数えない）の中のひらがな・カタカナ・漢字を数え、許した場所（`eng/conformance-rules.json` の `uiTextLiteralAllowedPaths`: 日本語と中国語の列・読みの表 `OperationReadings.hpp`・言語の自称 `DisplayLanguageNames.cpp`）の外では 0 件であることを守る。英語の列は許した場所に入れない。`static_assert` の文は英語で書く。英語のリテラルが残っていないか（画面に出る英語の直書き）は機械では見分けられないので、棚卸しの表（`docs/i18n/inventory-2026-11-13.md`）と独立レビューで守る。
13. **訳の責任。** 日本語と英語は設計席が書き hide が見る。中国語（簡体字）は機械の訳で、列のファイルの見出しと `docs/i18n/REVIEW.md` に「機械の訳・レビュー歓迎」と書く。英語と中国語は独立レビュー（V-1597）の必須と推奨を入れた。
14. **旧い設定。** 決定案 3 のとおり（v1〜v3 から読んだときは `ja`・全部不在なら `system`）。v4 は 2026-10-28 から `ui_language` を持ち、既定は `system`。日本語の Windows では表示は変わらない。

### 限界（2026-11-13）

- 文の取り違え（名前と文が入れ替わった 2 行）は型で見えない。日本語の列は製品の文と照合した（V-1598）うえで、`--ui-texts` と `--language` の代表の文が守る。
- 呼び出し元が渡す値の数と文の置き場の数の照合は無い（3 列とも同じ置き場なら落ちない）。
- Markdown プレビューの画像の代わりの文字 `[画像: …]` と知らせは、解析した時点の言語のまま（言語を切り替えても本文が変わるまで前の言語）。
- 拼音の読みは持たない（中国語で F1 の一覧を読みで探せない）。
- 繁体字中国語と、韓国語・スペイン語・ドイツ語の列は無い（英語に解決する）。

### 工程（2026-11-13）

| 段 | 中身 | PR |
|---|---|---|
| T1a | `DisplayLanguage`・`UiMessage`・3 列（日英は全行・中国語は英語の写しを仮に置く）・`ui_text` `ui_text_filled`・`resolved_language`・`EditorState::display_language()`・`EditorFrame.language`・試験 | #1592 |
| T1b | core と application の文を表へ（操作の表・知らせ・面・ステータス・保存の問い）・`UiLanguagePort` と adapter・`:set language=`・操作「表示言語を選ぶ」・起動の解決 | #1593 |
| T1c | ui の文を表へ（ダイアログ・起動画面・エクスプローラー・絞り込みの名前）・文字組みの locale・幅の試験 | #1594 |
| T1d | 中国語（簡体字）の列・`docs/i18n/REVIEW.md` | #1595 |
| T1f | 4 本を合わせる・レビュー（V-1597・V-1598・V-1601）の直し・CNF-016 を active に・この追補・仕様と README | #1602 |
